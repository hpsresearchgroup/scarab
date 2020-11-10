/* Copyright 2020 HPS/SAFARI Research Groups
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/***************************************************************************************
 * File         : dvfs.c
 * Author       : HPS Research Group
 * Date         : 3/28/2012
 * Description  : DVFS controller
 ***************************************************************************************/

#include "dvfs.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include "debug/debug_macros.h"
#include "dvfs.param.h"
#include "freq.h"
#include "globals/assert.h"
#include "memory/memory.param.h"
#include "optimizer2.h"
#include "perf_pred.h"
#include "power/power.param.h"
#include "power/power_intf.h"
#include "power_pred.h"
#include "ramulator.param.h"
#include "stat_mon.h"
#include "statistics.h"
#include "trigger.h"


/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_DVFS, ##args)

/**************************************************************************************/
/* Types */

/* A DVFS configuration: describes the state of the system affected by DVFS */
typedef struct Config_struct {
  uns core_cycle_times[MAX_NUM_PROCS];
} Config;

/* DVFS goodness metric: energy^energy_exp * delay^delay_exp */
typedef struct Metric_struct {
  int energy_exp;
  int delay_exp;
} Metric;

/* Per core info for BW sharing model */
typedef struct Proc_Info_struct {
  double orig_perf; /* measured performance, compute cycles per second */
  double perf_lat;  /* performance estimated by considering latency only */
  double perf_bw;   /* performance estimated by considering bandwidth only */
  double perf;      /* estimated performance */
  double s;         /* stall time per compute cycle */
  double f;         /* new frequency */
  double r;         /* number of requests per compute cycle */
} Proc_Info;

/**************************************************************************************/
/* Enumerations */

DEFINE_ENUM(DVFS_Metric, DVFS_METRIC_LIST);

/**************************************************************************************/
/* Global variables */

static Config*    configs;
static Config*    cur_config    = NULL;
static Config*    forced_config = NULL;
static Stat_Mon*  stat_mon;
static uns        num_configs;
static Counter    last_reconfig_inst_count;
static Metric     metric;
static FILE*      dvfs_log;
static FILE*      config_trace;
static Trigger*   start_trigger;
static Trigger*   trigger;
static Proc_Info* proc_infos;

/**************************************************************************************/
/* Local prototypes */

static void   init_static_config(void);
static void   init_configs_from_cmd(void);
static void   init_configs_from_file(void);
static void   set_config(Config* config);
static void   set_config_num(int num);
static uns    dvfs_read_config_trace(void);
static void   dvfs_reconfigure_oracle(void);
static void   dvfs_reconfigure_perf_pred(void);
static void   dvfs_reconfigure_dram_sharing(void);
static double dvfs_metric(double power, double delay);
static Metric get_metric(void);
static double gmean(const double* array, uns num);
static double compute_oracle_metric(void);
static void   invoke_dram_sharing_solver(double* pred_speedups, Config* config);
static void compute_stall_time_speedups(double* pred_speedups, Config* config);
static void compute_bw_sharing_speedups(double* pred_speedups, Config* config);

/**************************************************************************************/
/* dvfs_init: */

void dvfs_init(void) {
  if(DVFS_STATIC) {
    init_static_config();
  } else if(DVFS_CONFIG_FILE) {
    init_configs_from_file();
  } else {
    init_configs_from_cmd();
  }

  MESSAGEU(0, "Number of DVFS configs: %d\n", num_configs);

  if(DVFS_REPLAY_CONFIG_TRACE) {
    config_trace = fopen(DVFS_REPLAY_CONFIG_TRACE, "r");
    ASSERTM(0, config_trace, "Could not open config trace file\n");
    // skip first config since optimizer may not output the correct initial
    // config
    dvfs_read_config_trace();
  }

  Stat_Enum monitored_stats[] = {
    EXECUTION_TIME,
    DRAM_CYCLES,
    DRAM_BANK_IN_DEMAND,
    DRAM_BUS_DIR_SWITCHES,
    DRAM_GLOBAL_MLP,
    NODE_CYCLE,
    NODE_INST_COUNT,
    POWER_DRAM_ACTIVATE,
    RET_BLOCKED_L1_MISS,
    RET_BLOCKED_L1_MISS_BW_PREF,
    RET_BLOCKED_L1_ACCESS,
    RET_BLOCKED_MEM_STALL,
    RET_BLOCKED_OFFCHIP_DEMAND,
    MEM_REQ_COMPLETE_MEM,
    DRAM_CHANNEL_REQS,
    DRAM_CHANNEL_CRIT_REQS,
    DRAM_CHANNEL_CRIT_DIR_SWITCHES,
  };
  stat_mon = stat_mon_create_from_array(monitored_stats,
                                        NUM_ELEMENTS(monitored_stats));

  start_trigger = trigger_create("DVFS START", DVFS_START, TRIGGER_ONCE);
  trigger       = trigger_create("DVFS PERIOD", DVFS_PERIOD, TRIGGER_REPEAT);

  if(DVFS_LOG) {
    dvfs_log = file_tag_fopen(NULL, "dvfs", "w");
    ASSERTM(0, dvfs_log, "Could not open DVFS log file\n");
  }

  proc_infos = malloc(NUM_CORES * sizeof(Proc_Info));

  if(!DVFS_STATIC) {
    /* set the processor to the initial config */
    set_config(&configs[0]);

    metric = get_metric();

    if(DVFS_USE_ORACLE) {
      opt2_init(num_configs, 1, &set_config_num);
    }
  }
}

void init_static_config(void) {
  num_configs           = 1;  // only a single config is needed
  configs               = malloc(num_configs * sizeof(Config));
  uns* core_cycle_times = configs[0].core_cycle_times;
  uns  len = parse_uns_array(core_cycle_times, DVFS_STATIC, NUM_CORES);
  ASSERT(0, len == NUM_CORES);
}

void init_configs_from_cmd(void) {
  ASSERTM(0, DVFS_CONFIGS,
          "Please specify available configurations for the DVFS controller");

  /* DVFS_CONFIGS format is comma-separated core cycle times */

  /* first find the number of configs */
  char* token = DVFS_CONFIGS;
  for(num_configs = 1; (token = strchr(token, ',')); ++num_configs) {
    token += 1;  // skip the comma
  }
  ASSERT(0, num_configs > 0);

  uns  num_avail_core_cycle_times = num_configs;
  int* avail_core_cycle_times     = malloc(num_avail_core_cycle_times *
                                       sizeof(int));
  if(DVFS_INDIVIDUAL_CORES) {
    num_configs = 1;
    for(uns i = 0; i < NUM_CORES; i++)
      num_configs *= num_avail_core_cycle_times;
  }
  configs = malloc(num_configs * sizeof(Config));

  /* parse available core cycle times */
  uns len = parse_int_array(avail_core_cycle_times, DVFS_CONFIGS,
                            num_avail_core_cycle_times);
  ASSERT(0, len == num_avail_core_cycle_times);
  if(DVFS_INDIVIDUAL_CORES) {
    uns core_idx[MAX_NUM_PROCS] = {0};
    uns config_idx              = 0;
    while(TRUE) {
      for(uns k = 0; k < NUM_CORES; k++) {
        configs[config_idx].core_cycle_times[k] =
          avail_core_cycle_times[core_idx[k]];
      }
      config_idx++;
      int j;
      for(j = 0; j < NUM_CORES; j++) {
        if(core_idx[j] < num_avail_core_cycle_times - 1)
          break;
      }
      if(j == NUM_CORES)
        break;
      core_idx[j]++;
      for(j = j - 1; j >= 0; j--) {
        ASSERT(0, core_idx[j] == num_avail_core_cycle_times - 1);
        core_idx[j] = 0;
      }
    }
  } else {
    for(uns i = 0; i < num_configs; i++) {
      for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
        configs[i].core_cycle_times[proc_id] = avail_core_cycle_times[i];
      }
    }
  }

  if(DVFS_FORCE_CONFIG) {
    ASSERT(0, NUM_CORES == 1);
    uns core_cycle_time = atoi(DVFS_FORCE_CONFIG);
    uns i;
    for(i = 0; i < num_configs; i++) {
      if(avail_core_cycle_times[i] == core_cycle_time)
        break;
    }
    ASSERT(0, i < num_configs);
    forced_config = &configs[i];
  }
}

void init_configs_from_file(void) {
  ASSERT(0, !DVFS_CONFIGS);

  num_configs = 0;
  FILE* f     = fopen(DVFS_CONFIG_FILE, "r");
  ASSERTM(0, f, "Could not open DVFS config file %s\n", DVFS_CONFIG_FILE);
  char buf[MAX_STR_LENGTH + 1];
  while(fgets(buf, MAX_STR_LENGTH, f)) {
    num_configs++;
  }
  ASSERT(0, feof(f));
  ASSERT(0, !ferror(f));

  configs = malloc(num_configs * sizeof(Config));

  rewind(f);
  for(uns i = 0; i < num_configs; i++) {
    char* unused_ret = fgets(buf, MAX_STR_LENGTH, f);
    UNUSED(unused_ret);
    ASSERT(0, !ferror(f));
    uns len = parse_uns_array(configs[i].core_cycle_times, buf, NUM_CORES);
    ASSERT(0, len == NUM_CORES);
  }
  fclose(f);
}

Metric get_metric(void) {
  switch(DVFS_METRIC) {
    case DVFS_METRIC_DELAY:
      return (Metric){.energy_exp = 0, .delay_exp = 1};
      break;
    case DVFS_METRIC_ENERGY:
      return (Metric){.energy_exp = 1, .delay_exp = 0};
      break;
    case DVFS_METRIC_EDP:
      return (Metric){.energy_exp = 1, .delay_exp = 1};
      break;
    case DVFS_METRIC_ED2:
      return (Metric){.energy_exp = 1, .delay_exp = 2};
      break;
    default:
      FATAL_ERROR(0, "Unknown DVFS metric %s\n", DVFS_Metric_str(DVFS_METRIC));
      break;
  }
}

/**************************************************************************************/
/* dvfs_cycle: */

void dvfs_cycle(void) {
  if(DVFS_STATIC) {
    if(trigger_fired(start_trigger)) {
      set_config(&configs[0]);  // can only happen once
    }
    return;
  }

  if(trigger_fired(trigger)) {
    if(trigger_on(start_trigger)) {
      if(DVFS_REPLAY_CONFIG_TRACE) {
        set_config_num(dvfs_read_config_trace());
      } else if(DVFS_USE_ORACLE) {
        dvfs_reconfigure_oracle();
      } else if(DVFS_USE_BW_SHARING || DVFS_USE_DRAM_SHARING ||
                DVFS_USE_STALL_TIME) {
        dvfs_reconfigure_dram_sharing();
      } else {
        dvfs_reconfigure_perf_pred();
      }
    }

    stat_mon_reset(stat_mon);
    if(!DVFS_USE_BW_SHARING && !DVFS_USE_DRAM_SHARING && !DVFS_USE_STALL_TIME &&
       !DVFS_STATIC)
      perf_pred_reset_stats();
  }
}

/**************************************************************************************/
/* dvfs_done: */

void dvfs_done(void) {
  if(DVFS_USE_ORACLE) {
    opt2_comparison_barrier(compute_oracle_metric());
  }
  if(DVFS_REPLAY_CONFIG_TRACE) {
    fclose(config_trace);
  }
  free(configs);
}

static void set_config(Config* config) {
  if(DVFS_FORCE_CONFIG)
    config = forced_config;
  DEBUG(0, "Changing DVFS config to #%ld\n", (long int)(config - configs));
  if(DVFS_LOG)
    fprintf(dvfs_log, "New config #%ld:", (long int)(config - configs));
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    freq_set_cycle_time(FREQ_DOMAIN_CORES[proc_id],
                        config->core_cycle_times[proc_id]);
    if(DVFS_LOG)
      fprintf(dvfs_log, " %d", config->core_cycle_times[proc_id]);
  }
  if(DVFS_LOG)
    fprintf(dvfs_log, "\n");
  if(DVFS_CHIP_LEVEL) {
    ASSERT(0, NUM_CORES == 1);
    ASSERT(0, !L1_USE_CORE_FREQ);
    freq_set_cycle_time(FREQ_DOMAIN_L1, config->core_cycle_times[0]);
  }
  if(cur_config && cur_config != config)
    STAT_EVENT_ALL(DVFS_CONFIG_SWITCH);
  cur_config               = config;
  last_reconfig_inst_count = inst_count[0];
}

static void set_config_num(int num) {
  ASSERT(0, num < num_configs);
  set_config(&configs[num]);
}

static uns dvfs_read_config_trace(void) {
  uns config_idx;
  int matches = fscanf(config_trace, "%d", &config_idx);
  ASSERTM(0, matches == 1, "Error reading config trace: %s\n", strerror(errno));
  ASSERTM(0, config_idx < num_configs, "Config %u from trace is too big\n",
          config_idx);
  return config_idx;
}

static void dvfs_reconfigure_oracle(void) {
  double gmean_ipt = compute_oracle_metric();
  opt2_comparison_barrier(gmean_ipt);
  // we passed the barrier
  opt2_decision_point();
}

static void dvfs_reconfigure_perf_pred(void) {
  double min_metric     = 10.0;
  uns    min_metric_idx = num_configs;
  perf_pred_interval_done();
  if(metric.energy_exp != 0)
    power_intf_calc();
  if(DVFS_LOG)
    fprintf(dvfs_log, "Time: %llu\tInsts: %llu\tPredictions:", sim_time,
            inst_count[0]);

  for(uns i = 0; i < num_configs; ++i) {
    Config* config = &configs[i];
    double  pred_slowdown =
      // perf_pred_slowdown(0, PERF_PRED_MECH, config->core_cycle_times[0],
      // MEMORY_CYCLE_TIME);
      perf_pred_slowdown(0, PERF_PRED_MECH, config->core_cycle_times[0],
                         RAMULATOR_TCK);
    double pred_norm_power;
    double memory_access_frac = 1.0;
    if(POWER_INTF_ON) {
      // pred_norm_power = power_pred_norm_power(config->core_cycle_times,
      // MEMORY_CYCLE_TIME,
      pred_norm_power = power_pred_norm_power(
        config->core_cycle_times, RAMULATOR_TCK, &memory_access_frac,
        &pred_slowdown);
    } else {
      ASSERT(0, metric.energy_exp == 0);
      pred_norm_power = 1.0;
    }
    double metric = dvfs_metric(pred_norm_power, pred_slowdown);
    DEBUG(
      0,
      "Predicted metric for {\%d, \%d} is %f (norm. power %f, slowdown %f)\n",
      // configs[i].core_cycle_times[0], MEMORY_CYCLE_TIME,
      configs[i].core_cycle_times[0], RAMULATOR_TCK, metric, pred_norm_power,
      pred_slowdown);
    if(DVFS_LOG)
      fprintf(dvfs_log, " (%f, %f)", pred_norm_power, pred_slowdown);
    if(metric < min_metric) {
      min_metric     = metric;
      min_metric_idx = i;
    }
  }
  if(DVFS_LOG)
    fprintf(dvfs_log, "\n");
  ASSERT(0, min_metric_idx != num_configs);
  set_config(&configs[min_metric_idx]);
}

static void dvfs_reconfigure_dram_sharing(void) {
  double min_metric     = 10.0;
  uns    min_metric_idx = num_configs;
  if(metric.energy_exp != 0)
    power_intf_calc();
  if(DVFS_LOG)
    fprintf(dvfs_log, "Time: %llu\tInsts: %llu\tPredictions: (too many)\n",
            sim_time, inst_count[0]);
  for(uns i = 0; i < num_configs; ++i) {
    Config* config                       = &configs[i];
    double  pred_speedups[MAX_NUM_PROCS] = {0};
    if(DVFS_USE_BW_SHARING) {
      compute_bw_sharing_speedups(pred_speedups, config);
    } else if(DVFS_USE_DRAM_SHARING) {
      invoke_dram_sharing_solver(pred_speedups, config);
    } else {
      compute_stall_time_speedups(pred_speedups, config);
    }
    double pred_gmean_speedup            = gmean(pred_speedups, NUM_CORES);
    double pred_gmean_slowdown           = 1.0 / pred_gmean_speedup;
    double pred_slowdowns[MAX_NUM_PROCS] = {0};
    // convert speedups to slowdowns as expected by power_pred
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      pred_slowdowns[proc_id] = 1.0 / pred_speedups[proc_id];
    }
    double  memory_access_fracs[MAX_NUM_PROCS];
    Counter total_memory_accesses = 0;
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      total_memory_accesses += stat_mon_get_count(stat_mon, proc_id,
                                                  MEM_REQ_COMPLETE_MEM);
    }
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      memory_access_fracs[proc_id] = (double)stat_mon_get_count(
                                       stat_mon, proc_id,
                                       MEM_REQ_COMPLETE_MEM) /
                                     (double)total_memory_accesses;
    }
    double pred_norm_power;
    if(POWER_INTF_ON) {
      // TODO: fix the power prediction for multicore
      // pred_norm_power = power_pred_norm_power(config->core_cycle_times,
      // MEMORY_CYCLE_TIME,
      pred_norm_power = power_pred_norm_power(
        config->core_cycle_times, RAMULATOR_TCK, memory_access_fracs,
        pred_slowdowns);
    } else {
      ASSERT(0, metric.energy_exp == 0);
      pred_norm_power = 1.0;
    }
    double metric = dvfs_metric(pred_norm_power, pred_gmean_slowdown);
    DEBUG(
      0,
      "Predicted metric for config \%d is %f (norm. power %f, slowdown %f)\n",
      i, metric, pred_norm_power, pred_gmean_slowdown);
    // if (DVFS_LOG) fprintf(dvfs_log, " (%f, %f)", pred_norm_power,
    // pred_gmean_slowdown);
    if(metric < min_metric) {
      min_metric     = metric;
      min_metric_idx = i;
    }
  }
  if(DVFS_LOG)
    fprintf(dvfs_log, "\n");
  if(DVFS_DRAM_SHARING_SOLVER_STRICT) {
    ASSERT(0, min_metric_idx != num_configs);
  }
  if(min_metric_idx != num_configs) {
    set_config(&configs[min_metric_idx]);
  } else {
    WARNINGU(0, "No DVFS config chosen, skipping interval\n");
  }
}

static double dvfs_metric(double power, double delay) {
  double energy = power * delay;

  return pow(energy, metric.energy_exp) * pow(delay, metric.delay_exp);
}

static double gmean(const double* array, uns num) {
  double gmean = 1.0;
  for(uns i = 0; i < num; i++) {
    ASSERT(0, array[i] > 0.0);
    gmean *= array[i];
  }
  return pow(gmean, 1.0 / (double)num);
}

static void invoke_dram_sharing_solver(double* pred_speedups, Config* config) {
  ASSERT(0, DVFS_DRAM_SHARING_SOLVER_BIN);

  char  cmd[MAX_STR_LENGTH + 1];
  char* buf = cmd;
  // buf += sprintf(buf, "%s %d %d ", DVFS_DRAM_SHARING_SOLVER_BIN, NUM_CORES,
  // MEMORY_CHANNELS*MEMORY_BANKS);
  buf += sprintf(buf, "%s %d %d ", DVFS_DRAM_SHARING_SOLVER_BIN, NUM_CORES,
                 RAMULATOR_CHANNELS * RAMULATOR_BANKS);
  Counter dram_cycles      = stat_mon_get_count(stat_mon, 0, DRAM_CYCLES);
  Counter blp_times_cycles = stat_mon_get_count(stat_mon, 0,
                                                DRAM_BANK_IN_DEMAND);
  buf += sprintf(buf, " %.6f", (double)blp_times_cycles / (double)dram_cycles);
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Counter row_opens = stat_mon_get_count(stat_mon, proc_id,
                                           POWER_DRAM_ACTIVATE);
    buf += sprintf(buf, " %.10f", (double)row_opens / (double)dram_cycles);
  }
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Counter mlp_times_cycles = stat_mon_get_count(stat_mon, proc_id,
                                                  DRAM_GLOBAL_MLP);
    buf += sprintf(buf, " %.10f",
                   (double)mlp_times_cycles / (double)dram_cycles);
  }
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Counter core_cycles  = stat_mon_get_count(stat_mon, proc_id, NODE_CYCLE);
    Counter stall_cycles = stat_mon_get_count(stat_mon, proc_id,
                                              RET_BLOCKED_L1_MISS);
    buf += sprintf(buf, " %.10f", (double)stall_cycles / (double)core_cycles);
  }
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    buf += sprintf(buf, " %.4f",
                   (double)cur_config->core_cycle_times[proc_id] /
                     (double)config->core_cycle_times[proc_id]);
  }
  buf += sprintf(buf, " | grep SCARAB");
  FILE* solver_pipe                       = popen(cmd, "r");
  char  solver_output[MAX_STR_LENGTH + 1] = "";
  uns   num_matches = fscanf(solver_pipe, "SCARAB %s", solver_output);
  pclose(solver_pipe);
  if(DVFS_DRAM_SHARING_SOLVER_STRICT) {
    ASSERTM(0, num_matches == 1, "Could not parse solver output: '%s'\n",
            solver_output);
  }
  if(num_matches == 1) {
    // solver run successful
    int num_parsed = parse_double_array(pred_speedups, solver_output,
                                        NUM_CORES);
    ASSERT(0, num_parsed == NUM_CORES);
  } else {
    // solver failed, make sure this config will not get selected
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      pred_speedups[proc_id] = 0.01;
    }
  }
}

static void compute_stall_time_speedups(double* pred_speedups, Config* config) {
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Counter core_cycles  = stat_mon_get_count(stat_mon, proc_id, NODE_CYCLE);
    Counter stall_cycles = stat_mon_get_count(stat_mon, proc_id,
                                              RET_BLOCKED_L1_MISS);
    if(DVFS_COUNT_L1_ACCESS_STALL) {
      stall_cycles += stat_mon_get_count(stat_mon, proc_id,
                                         RET_BLOCKED_L1_ACCESS);
    }
    double stall_frac   = (double)stall_cycles / (double)core_cycles;
    double freq_speedup = (double)cur_config->core_cycle_times[proc_id] /
                          (double)config->core_cycle_times[proc_id];
    pred_speedups[proc_id] = 1.0 /
                             (stall_frac + (1.0 - stall_frac) / freq_speedup);
  }
}

static void compute_bw_sharing_speedups(double* pred_speedups, Config* config) {
  Counter total_mem_reqs = 0;
  DEBUG(0, "%7s %7s %7s %7s %11s\n", "f%", "stall%", "full%", "perf%", "r");
  /* Compute speedups due to latency */
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Proc_Info* info         = &proc_infos[proc_id];
    Counter    core_cycles  = stat_mon_get_count(stat_mon, proc_id, NODE_CYCLE);
    Counter    stall_cycles = stat_mon_get_count(stat_mon, proc_id,
                                              RET_BLOCKED_L1_MISS);
    if(DVFS_BW_SHARING_NO_PREF_STALL) {
      Counter pref_stall_cycles = stat_mon_get_count(
        stat_mon, proc_id, RET_BLOCKED_L1_MISS_BW_PREF);
      ASSERT(proc_id, pref_stall_cycles <= stall_cycles);
      stall_cycles -= pref_stall_cycles;
    }
    if(DVFS_COUNT_L1_ACCESS_STALL) {
      stall_cycles += stat_mon_get_count(stat_mon, proc_id,
                                         RET_BLOCKED_L1_ACCESS);
    }
    Counter mem_stall_cycles = stat_mon_get_count(stat_mon, proc_id,
                                                  RET_BLOCKED_MEM_STALL);
    Counter compute_cycles   = MAX2(core_cycles - mem_stall_cycles, 1);
    double  stall_frac       = (double)stall_cycles / (double)core_cycles;
    double  orig_freq        = 1.0e15 / cur_config->core_cycle_times[proc_id];
    double  time             = (double)core_cycles / (double)orig_freq;
    info->orig_perf          = (double)compute_cycles / time;
    info->f                  = 1.0e15 / config->core_cycle_times[proc_id];
    info->s                  = stall_frac * time / (double)compute_cycles;
    info->perf_lat           = 1.0 / (info->s + 1.0 / info->f);
    info->perf_bw            = 1.0e99;  // infinite
    info->perf               = info->perf_lat;
    Counter mem_reqs         = stat_mon_get_count(stat_mon, proc_id,
                                          MEM_REQ_COMPLETE_MEM);
    total_mem_reqs += mem_reqs;
    info->r = (double)mem_reqs / (double)compute_cycles;
    DEBUG(proc_id, "%7.4f %7.4f %7.4f %7.4f %11.8f\n", info->f / orig_freq,
          stall_frac, (double)mem_stall_cycles / (double)core_cycles,
          info->perf_lat / info->orig_perf, info->r);
  }
  if(total_mem_reqs == 0) {
    WARNINGU_ONCE(0, "total_mem_reqs == 0");
  }
  Counter bus_dir_switches = stat_mon_get_count(stat_mon, 0,
                                                DRAM_BUS_DIR_SWITCHES);
  // uns rtw_bus_cost = (DRAM_CL + DRAM_TCCD - DRAM_CWL + 2) + DRAM_CWL -
  // (DRAM_CL + DRAM_TCCD);
  uns rtw_bus_cost = (RAMULATOR_TCL + RAMULATOR_TCCD - RAMULATOR_TCWL + 2) +
                     RAMULATOR_TCWL - (RAMULATOR_TCL + RAMULATOR_TCCD);
  // uns wtr_bus_cost = (DRAM_CWL + DRAM_TBL + DRAM_TWTR) + DRAM_CL - (DRAM_CWL
  // + DRAM_TBL);
  uns wtr_bus_cost = (RAMULATOR_TCWL + RAMULATOR_TBL + RAMULATOR_TWTR) +
                     RAMULATOR_TCL - (RAMULATOR_TCWL + RAMULATOR_TBL);
  double bus_dir_switch_cost = ((double)rtw_bus_cost + (double)wtr_bus_cost) /
                               2.0;
  double bus_cycles_per_req =
    //((double)DRAM_TBL*(double)total_mem_reqs +
    // bus_dir_switch_cost*(double)bus_dir_switches)/
    ((double)RAMULATOR_TBL * (double)total_mem_reqs +
     bus_dir_switch_cost * (double)bus_dir_switches) /
    (double)total_mem_reqs;
  // double max_bw = (double)MEMORY_CHANNELS;
  double max_bw = (double)RAMULATOR_CHANNELS;
  if(DVFS_BW_SHARING_CRIT_STATS) {
    Counter reqs = stat_mon_get_count(stat_mon, 0, DRAM_CHANNEL_REQS);
    if(reqs == 0) {
      WARNINGU_ONCE(0, "reqs == 0\n");
    }
    Counter crit_reqs = stat_mon_get_count(stat_mon, 0, DRAM_CHANNEL_CRIT_REQS);
    if(crit_reqs == 0) {
      WARNINGU_ONCE(0, "crit_reqs == 0\n");
    }
    Counter crit_dir_switches = stat_mon_get_count(
      stat_mon, 0, DRAM_CHANNEL_CRIT_DIR_SWITCHES);
    if(reqs > 0 && crit_reqs > 0) {
      bus_cycles_per_req =
        //(double)(crit_reqs*DRAM_TBL+crit_dir_switches*bus_dir_switch_cost)/(double)crit_reqs;
        (double)(crit_reqs * RAMULATOR_TBL +
                 crit_dir_switches * bus_dir_switch_cost) /
        (double)crit_reqs;
      max_bw = (double)reqs / (double)crit_reqs;
    } else {
      // bus_cycles_per_req = DRAM_TBL;
      bus_cycles_per_req = RAMULATOR_TBL;
      // ASSERT(0, MEMORY_CHANNELS == 1 || MEMORY_CHANNELS == 2);
      ASSERT(0, RAMULATOR_CHANNELS == 1 || RAMULATOR_CHANNELS == 2);
      // max_bw = MEMORY_CHANNELS == 1 ? 1 : 1.5;
      max_bw = RAMULATOR_CHANNELS == 1 ? 1 : 1.5;
    }
  }
  // bus_cycles_per_req = MIN2(bus_cycles_per_req, (1.0 +
  // DVFS_BW_SHARING_MAX_RW_COST)*DRAM_TBL);
  bus_cycles_per_req = MIN2(
    bus_cycles_per_req, (1.0 + DVFS_BW_SHARING_MAX_RW_COST) * RAMULATOR_TBL);
  double avg_req_latency = 0.0;  // in DRAM cycles
  double bus_freq = 1.0e15 / (double)freq_get_cycle_time(FREQ_DOMAIN_MEMORY);
  while(TRUE) {
    /* compute bandwidth consumption */
    double total_bw = 0.0;
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      Proc_Info* info = &proc_infos[proc_id];
      double     bw   = info->perf * info->r * bus_cycles_per_req;
      total_bw += bw;
    }
    double bus_util = total_bw / bus_freq;

    /* If bandwidth is not over the maximum, we're done */
    if(bus_util < DVFS_BW_SHARING_BUS_UTIL_THRESH * max_bw)
      break;
    avg_req_latency += 1.0;
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      Proc_Info* info = &proc_infos[proc_id];
      if(info->r == 0.0) {
        info->perf_bw = 1.0e99;  // infinite
      } else {
        info->perf_bw = DVFS_BW_SHARING_MAX_REQS /
                        (info->r * avg_req_latency / bus_freq);
      }
      info->perf = MIN2(info->perf_lat, info->perf_bw);
    }
  }
  DEBUG(0, "Avg req latency: %.0f, avg req bus cycles: %.2f\n", avg_req_latency,
        bus_cycles_per_req);
  DEBUG(0, "%7s (%7s, %7s) %7s\n", "perf%", "lat", "bw", "bw%");
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Proc_Info* info        = &proc_infos[proc_id];
    pred_speedups[proc_id] = info->perf / info->orig_perf;
    DEBUG(proc_id, "%7.4f (%7.4f, %7.4f) %7.4f\n", info->perf / info->orig_perf,
          info->perf_lat / info->orig_perf,
          (info->perf_bw == 1.0e99 ? 0 : info->perf_bw) / info->orig_perf,
          info->perf * info->r * bus_cycles_per_req / bus_freq);
  }
}

double compute_oracle_metric(void) {
  double ipt_product = 1.0;
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Counter insts     = stat_mon_get_count(stat_mon, proc_id, NODE_INST_COUNT);
    Counter exec_time = stat_mon_get_count(stat_mon, proc_id, EXECUTION_TIME);
    ipt_product *= (double)insts / (double)exec_time;
  }
  return -pow(ipt_product, 1.0 / NUM_CORES);
}
