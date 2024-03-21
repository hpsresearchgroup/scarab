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
 * File         : perf_pred.c
 * Author       : HPS Research Group
 * Date         : 10/20/2011
 * Description  : Performance prediction counters.
 **************************************************************************************/

/* This code is designed to predict performance under frequency
   scaling when NUM_CORES == 1. For NUM_CORES > 1, this code can only
   be relied on to provide stats, not performance predictions. */

#include "perf_pred.h"
#include "debug/debug.param.h"
#include "debug/debug_macros.h"
#include "freq.h"
#include "globals/assert.h"
#include "globals/global_vars.h"
#include "math.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "ramulator.param.h"
#include "stat_mon.h"
#include "statistics.h"

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_PERF_PRED, ##args)

DEFINE_ENUM(Perf_Pred_Mech, PERF_PRED_MECH_LIST);
DEFINE_ENUM(Perf_Pred_Req_Latency_Mech, PERF_PRED_REQ_LATENCY_MECH_LIST);

/* Per core information for each DRAM bank */
typedef struct Bank_Info_struct {
  Counter length;  // length of the critical path through the request in this
                   // bank
  Counter num_critical_reqs;  // how many critical memory requests are
                              // outstanding for this bank?
  Counter last_updated;       // cycle this info was last updated
  Counter slack_in_this_slack_period;
  Counter slack_last_update_cycle;
  Counter core_service_cycles;  // cycles the corresponding core was serviced
  Counter bank_length;
  Flag    accessed;
  Flag    last_access_row_hit;
  Flag    bank_accessed_in_this_slack_period;
} Bank_Info;

/* Per core incormation */
typedef struct Proc_Info_struct {
  /* Leading loads */

  Mem_Req* current_leading_load;
  Counter  current_leading_load_start_cycle;

  Counter    mem_req_critical_path_length;
  Bank_Info* bank_infos;

  /* Chip utilization in memory request shadow */

  uns total_reqs;
  uns total_critical_reqs;

  /* Prefetch phase */

  uns     total_prefetch_reqs;
  uns     total_late_prefetch_reqs;
  Counter chip_busy_in_this_prefetch_phase;
  Counter chip_busy_under_critical_reqs_in_this_prefetch_phase;
  Counter mem_reqs_in_this_prefetch_phase;

  /* Slack period */

  Counter last_slack_period_start;
  Counter last_slack_period_start_in_memory_cycles;
  Counter global_slack_in_this_prefetch_phase;
  Counter mem_reqs_in_this_slack_period;

  /* Off-chip delay */
  uns total_off_chip_delays;

  /* Critical access plot file */

  FILE*   critical_access_plot_file;
  Counter last_plotted_cycle;
} Proc_Info;

enum {
  CRITICAL_REQUEST,
  CRITICAL_RETURN,
};

/* All performance prediction is done using the chip cycle count */
static Counter   chip_cycle_count;
static Stat_Mon* stat_mon;

static Proc_Info* proc_infos;

/* static function prototypes */
static void critical_access_plot(uns proc_id, Mem_Req_Type type, uns req_ret,
                                 uns num);
static Flag is_prefetch_type(Mem_Req_Type type);
static Flag is_critical_type(Mem_Req_Type type);
static Flag is_critical(Mem_Req_Type type, Flag offpath, Flag bw);
static Flag is_critical_req(Mem_Req* req);
static void process_slack_period(uns proc_id, Counter slack_period_end);
static void init_mlp_infos(void);

static void critical_access_plot(uns proc_id, Mem_Req_Type type, uns req_ret,
                                 uns num) {
  Proc_Info* proc = &proc_infos[proc_id];
  _TRACE(CRITICAL_ACCESS_PLOT_ENABLE, proc->critical_access_plot_file,
         "%lld\t%lld\t%s\t%d\t%d\n", proc->last_plotted_cycle, chip_cycle_count,
         Mem_Req_Type_str(type), req_ret, num);
  proc->last_plotted_cycle = chip_cycle_count;
}

static Flag is_prefetch_type(Mem_Req_Type type) {
  return type == MRT_DPRF || type == MRT_IPRF;
}

void init_perf_pred(void) {
  init_mlp_infos();

  if(!PERF_PRED_ENABLE)
    return;

  proc_infos = (Proc_Info*)malloc(sizeof(Proc_Info) * NUM_CORES);
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Proc_Info* proc  = &proc_infos[proc_id];
    proc->bank_infos = (Bank_Info*)malloc(sizeof(Bank_Info) * RAMULATOR_BANKS *
                                          RAMULATOR_CHANNELS);
    if(CRITICAL_ACCESS_PLOT_ENABLE) {
      char filename[MAX_STR_LENGTH + 1];
      sprintf(filename, "%s.%d.out", CRITICAL_ACCESS_PLOT_FILE, proc_id);
      proc->critical_access_plot_file = fopen(filename, "w");
      ASSERTM(0, proc->critical_access_plot_file, "Could not open %s",
              filename);
    }
  }
  reset_perf_pred();

  Stat_Enum monitored_stats[] = {
    PERF_PRED_CYCLE,
    NODE_CYCLE,
    MEM_REQ_CRITICAL_PATH_LENGTH,
    LEADING_LOAD_LATENCY,
    RET_BLOCKED_L1_MISS,
    TOTAL_MEMORY_SLACK,
    TOTAL_CHIP_UTILIZATION,
    CHIP_UTILIZATION_UNDER_CRITICAL_MEM_REQ,
    MEM_REQ_COMPLETE_MEM,
    DRAM_BUS_DIR_SWITCHES,
    RET_BLOCKED_MEM_STALL,
    DRAM_CYCLES,
  };
  stat_mon = stat_mon_create_from_array(monitored_stats,
                                        NUM_ELEMENTS(monitored_stats));
}

static Flag is_critical_type(Mem_Req_Type type) {
  if(PERF_PRED_COUNT_ALL)
    return TRUE;

  return (type == MRT_DFETCH) ||
         (PERF_PRED_COUNT_INST_MISSES && type == MRT_IFETCH) ||
         (PERF_PRED_COUNT_PREFETCHES && type == MRT_DPRF) ||
         (PERF_PRED_COUNT_INST_MISSES && PERF_PRED_COUNT_PREFETCHES &&
          type == MRT_IPRF);
}

static Flag is_critical(Mem_Req_Type type, Flag offpath, Flag bw) {
  return is_critical_type(type) && (PERF_PRED_COUNT_OFFPATH_REQS || !offpath) &&
         (PERF_PRED_COUNT_BW_REQS || !bw);
}

static Flag is_critical_req(Mem_Req* req) {
  if(PERF_PRED_UPDATE_MEM_REQ_TYPE) {
    return is_critical(req->type, req->off_path_confirmed,
                       req->bw_prefetch || req->bw_prefetchable);
  } else {
    return is_critical(req->perf_pred_type, req->perf_pred_off_path_confirmed,
                       req->bw_prefetch || req->bw_prefetchable);
  }
}

void perf_pred_mem_req_start(Mem_Req* req) {
  if(!PERF_PRED_ENABLE)
    return;

  DEBUG(0, "Mem req %d (%s) started (bank %d)\n", req->id,
        Mem_Req_Type_str(req->type), req->mem_flat_bank);

  Proc_Info* proc = &proc_infos[req->proc_id];

  // Update these first so that is_critical() works
  req->perf_pred_type               = req->type;
  req->perf_pred_off_path_confirmed = req->off_path_confirmed;

  // Leading load latency computation

  if(!proc->current_leading_load && is_critical_req(req)) {
    DEBUG(0, "Mem req %d is a leading load\n", req->id);
    proc->current_leading_load             = req;
    proc->current_leading_load_start_cycle = chip_cycle_count;
    STAT_EVENT(req->proc_id, LEADING_LOADS);
  }

  // Memory request critical path computation

  Bank_Info* info = &proc->bank_infos[req->mem_flat_bank];
  // only include the latency from last_updated to now in the bank's critical
  // path if there are critical requests outstanding to the bank
  Flag critical = is_critical_req(req);
  info->num_critical_reqs += critical;
  if(critical) {
    DEBUG(0, "Mem req %d is critical\n", req->id);
    critical_access_plot(req->proc_id, req->type, CRITICAL_REQUEST,
                         proc->total_critical_reqs);
  }
  info->last_updated                     = chip_cycle_count;
  req->mem_crit_path_at_entry            = proc->mem_req_critical_path_length;
  req->dram_core_service_cycles_at_start = info->core_service_cycles;

  if(is_prefetch_type(req->type)) {
    proc->total_prefetch_reqs += 1;
  }

  ASSERT(0, proc->total_reqs <
                (RAMULATOR_READQ_ENTRIES + RAMULATOR_WRITEQ_ENTRIES) ||
              PERF_PRED_REQS_FINISH_AT_FILL);
  proc->total_critical_reqs += critical;
  proc->total_reqs += 1;
}

void perf_pred_mem_req_done(Mem_Req* req) {
  if(!PERF_PRED_ENABLE)
    return;

  DEBUG(0, "Mem req %d done (bank %d)\n", req->id, req->mem_flat_bank);

  Proc_Info* proc = &proc_infos[req->proc_id];

  // Leading load latency computation

  if(proc->current_leading_load == req) {
    INC_STAT_EVENT(req->proc_id, LEADING_LOAD_LATENCY,
                   chip_cycle_count - proc->current_leading_load_start_cycle);
    proc->current_leading_load = NULL;
  }

  // Memory request critical path computation

  Bank_Info* info = &proc->bank_infos[req->mem_flat_bank];
  if(info->num_critical_reqs > 0) {
    INC_STAT_EVENT(req->proc_id, TOTAL_CRITICAL_BANK_LATENCY,
                   chip_cycle_count - info->last_updated);
  }
  Flag critical = is_critical_req(req);
  if(critical) {
    ASSERT(0, info->num_critical_reqs > 0);
    --info->num_critical_reqs;
    INC_STAT_EVENT(req->proc_id, TOTAL_CRITICAL_MEM_REQ_LATENCY,
                   chip_cycle_count - info->last_updated);
    critical_access_plot(req->proc_id, req->perf_pred_type, CRITICAL_RETURN,
                         proc->total_critical_reqs);
    ASSERT(0, proc->total_critical_reqs > 0);
    proc->total_critical_reqs -= 1;
  }
  if(critical ||
     PERF_PRED_REQ_LATENCY_MECH == PERF_PRED_REQ_LATENCY_MECH_DRAM_LATENCY) {
    Counter latency;
    switch(PERF_PRED_REQ_LATENCY_MECH) {
      case PERF_PRED_REQ_LATENCY_MECH_REQ_LATENCY:
        latency = chip_cycle_count - req->mem_queue_cycle;
        break;
      case PERF_PRED_REQ_LATENCY_MECH_DRAM_LATENCY:
        latency = req->dram_latency;
        break;
      case PERF_PRED_REQ_LATENCY_MECH_VIRTUAL_CLOCK:
        latency = info->core_service_cycles -
                  req->dram_core_service_cycles_at_start;
        break;
      default:
        FATAL_ERROR(req->proc_id, "Unknown request latency mechanism: %s\n",
                    Perf_Pred_Req_Latency_Mech_str(PERF_PRED_REQ_LATENCY_MECH));
    }
    DEBUG(0, "Mem req %d left as critical, latency: %llu\n", req->id, latency);
    Counter min_extra_req_crit_path_length =
      PERF_PRED_REQ_LATENCY_MECH == PERF_PRED_REQ_LATENCY_MECH_REQ_LATENCY ?
        0 :
        freq_convert(FREQ_DOMAIN_MEMORY, RAMULATOR_TBL, FREQ_DOMAIN_L1);
    Counter new_mem_req_critical_path_length = MAX2(
      latency + req->mem_crit_path_at_entry,
      proc->mem_req_critical_path_length + min_extra_req_crit_path_length);
    INC_STAT_EVENT(
      req->proc_id, MEM_REQ_CRITICAL_PATH_LENGTH,
      new_mem_req_critical_path_length - proc->mem_req_critical_path_length);
    proc->mem_req_critical_path_length = new_mem_req_critical_path_length;
    DEBUG(0, "Mem req critical path updated to %llu\n",
          new_mem_req_critical_path_length);
    if(PERF_PRED_REQ_LATENCY_MECH !=
       PERF_PRED_REQ_LATENCY_MECH_VIRTUAL_CLOCK) {  // HACK
      ASSERTM(req->proc_id,
              proc->mem_req_critical_path_length <= chip_cycle_count,
              "crit path: %llu\n", new_mem_req_critical_path_length);
    }
  }
  if(is_critical_type(req->type) && is_prefetch_type(req->perf_pred_type)) {
    ASSERT(0, proc->total_late_prefetch_reqs > 0);
    proc->total_late_prefetch_reqs -= 1;
  }

  if(is_prefetch_type(req->perf_pred_type)) {
    ASSERT(0, proc->total_prefetch_reqs > 0);
    proc->total_prefetch_reqs -= 1;
  }

  INC_STAT_EVENT(req->proc_id, TOTAL_MEM_REQ_LATENCY,
                 chip_cycle_count - info->last_updated);
  info->last_updated = chip_cycle_count;

  ASSERT(0, proc->total_reqs > 0);
  proc->total_reqs -= 1;
}

static Counter full_dram_latency(Flag prev_row_hit, Flag row_hit, Flag write) {
  if(row_hit)
    return (write ? RAMULATOR_TCWL : RAMULATOR_TCL) + RAMULATOR_TBL;
  if(prev_row_hit) {
    return RAMULATOR_TRP + RAMULATOR_TRCD +
           (write ? RAMULATOR_TCWL : RAMULATOR_TCL) + RAMULATOR_TBL;
  } else {
    // return RAMULATOR_TRC;
    return (RAMULATOR_TRAS + RAMULATOR_TRP);  // tRC
  }
}

static Counter overlapped_dram_latency(Flag prev_row_hit, Flag row_hit,
                                       Flag write) {
  if(row_hit)
    return (write ? RAMULATOR_TCWL : RAMULATOR_TCL);
  return prev_row_hit ? RAMULATOR_TCL : 0;
}

void perf_pred_dram_latency_start(Mem_Req* req) {
  if(!PERF_PRED_ENABLE)
    return;

  req->dram_access_cycle = chip_cycle_count;
  if(PERF_PRED_REQ_LATENCY_MECH == PERF_PRED_REQ_LATENCY_MECH_DRAM_LATENCY) {
    WARNINGU_ONCE(0, "This code may have stopped working when shadow row hit "
                     "detection was moved to ACTIVATE time\n");
    Flag row_hit = req->row_access_status == DRAM_REQ_ROW_HIT ||
                   req->shadow_row_hit;
    Flag       write = req->type == MRT_WB;
    Bank_Info* info  = &proc_infos[req->proc_id].bank_infos[req->mem_flat_bank];
    Counter    full_latency = freq_convert(
      FREQ_DOMAIN_MEMORY,
      full_dram_latency(info->last_access_row_hit, row_hit, write),
      FREQ_DOMAIN_L1);
    Counter overlapped_latency = freq_convert(
      FREQ_DOMAIN_MEMORY,
      info->accessed ?
        overlapped_dram_latency(info->last_access_row_hit, row_hit, write) :
        0,
      FREQ_DOMAIN_L1);
    ;
    ASSERT(req->proc_id, info->length >= overlapped_latency);
    Counter bank_path_length    = info->length - overlapped_latency;
    req->mem_crit_path_at_entry = MAX2(bank_path_length,
                                       req->mem_crit_path_at_entry);
    info->length                = req->mem_crit_path_at_entry + full_latency;
    req->dram_latency           = full_latency;
    info->accessed              = TRUE;
  }
}

void perf_pred_dram_latency_end(Mem_Req* req) {
  if(!PERF_PRED_ENABLE)
    return;

  WARNINGU_ONCE(0, "This code may have stopped working when shadow row hit "
                   "detection was moved to ACTIVATE time\n");
  Counter full_dram_cycles = (req->type == MRT_WB ? RAMULATOR_TCWL :
                                                    RAMULATOR_TCL) +
                             RAMULATOR_TBL +
                             (req->row_access_status == DRAM_REQ_ROW_HIT ||
                                  req->shadow_row_hit ?
                                0 :
                                RAMULATOR_TRP + RAMULATOR_TRCD);
  Counter pipelined_dram_cycles = req->row_access_status == DRAM_REQ_ROW_HIT ||
                                      req->shadow_row_hit ?
                                    RAMULATOR_TBL :
                                    full_dram_cycles;
  // freq_converts are HACKS, may not work with pairwise prime freqs
  Counter full_dram_latency = freq_convert(FREQ_DOMAIN_MEMORY, full_dram_cycles,
                                           FREQ_DOMAIN_L1);
  Counter pipelined_dram_latency = freq_convert(
    FREQ_DOMAIN_MEMORY, pipelined_dram_cycles, FREQ_DOMAIN_L1);
  Bank_Info* info = &proc_infos[req->proc_id].bank_infos[req->mem_flat_bank];
  info->core_service_cycles = MAX2(
    info->core_service_cycles + pipelined_dram_latency,
    req->dram_core_service_cycles_at_start + full_dram_latency);
}

void perf_pred_update_mem_req_type(Mem_Req* req, Mem_Req_Type old_type,
                                   Flag old_off_path_confirmed) {
  if(!PERF_PRED_ENABLE)
    return;

  Proc_Info* proc = &proc_infos[req->proc_id];

  //    ASSERT(0, is_critical_type(old_type) <= is_critical_type(req->type));
  //    // not necessarily true if prefetches are critical
  if(PERF_PRED_UPDATE_MEM_REQ_TYPE) {
    DEBUG(0, "Mem req %d updated, type: %s -> %s, offpath: %d -> %d\n", req->id,
          Mem_Req_Type_str(old_type), Mem_Req_Type_str(req->type),
          old_off_path_confirmed, req->off_path_confirmed);
    if(!is_critical(old_type, old_off_path_confirmed,
                    req->bw_prefetch || req->bw_prefetchable) &&
       is_critical_req(req)) {
      DEBUG(0, "Mem req %d is now critical\n", req->id);
      critical_access_plot(req->proc_id, old_type, CRITICAL_REQUEST,
                           proc->total_critical_reqs);
      proc->bank_infos[req->mem_flat_bank].num_critical_reqs += 1;
      proc->total_critical_reqs += 1;
    }
  }
  if(is_prefetch_type(old_type) && is_critical_type(req->type)) {
    proc->total_late_prefetch_reqs += 1;
  }
}

void reset_perf_pred() {
  if(!PERF_PRED_ENABLE)
    return;

  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Proc_Info* proc                    = &proc_infos[proc_id];
    proc->current_leading_load         = NULL;
    proc->mem_req_critical_path_length = 0;
    for(int bank_id = 0; bank_id < RAMULATOR_BANKS * RAMULATOR_CHANNELS;
        ++bank_id) {
      Bank_Info* bank                          = &proc->bank_infos[bank_id];
      bank->length                             = 0;
      bank->num_critical_reqs                  = 0;
      bank->last_updated                       = 0;
      bank->slack_in_this_slack_period         = 0;
      bank->slack_last_update_cycle            = 0;
      bank->core_service_cycles                = 0;
      bank->accessed                           = FALSE;
      bank->bank_accessed_in_this_slack_period = FALSE;
    }
    proc->total_reqs                                           = 0;
    proc->total_critical_reqs                                  = 0;
    proc->total_prefetch_reqs                                  = 0;
    proc->total_late_prefetch_reqs                             = 0;
    proc->chip_busy_in_this_prefetch_phase                     = 0;
    proc->chip_busy_under_critical_reqs_in_this_prefetch_phase = 0;
    proc->mem_reqs_in_this_prefetch_phase                      = 0;
    proc->total_off_chip_delays                                = 0;
    proc->last_slack_period_start                              = 0;
    proc->last_slack_period_start_in_memory_cycles             = 0;
    proc->mem_reqs_in_this_slack_period                        = 0;
    proc->last_plotted_cycle                                   = 0;
  }
}

void perf_pred_core_busy(uns proc_id, uns num_fus_busy) {
  if(!PERF_PRED_ENABLE)
    return;

  Proc_Info* proc = &proc_infos[proc_id];

  Flag busy = num_fus_busy > 0;
  INC_STAT_EVENT(proc_id, CHIP_UTILIZATION, busy);
  if(proc->total_reqs > 0) {
    STAT_EVENT(proc_id, CYCLES_UNDER_MEM_REQ);
    INC_STAT_EVENT(proc_id, CHIP_UTILIZATION_UNDER_MEM_REQ, busy);
    if(proc->total_critical_reqs > 0) {
      STAT_EVENT(proc_id, CYCLES_UNDER_CRITICAL_MEM_REQ);
      INC_STAT_EVENT(proc_id, CHIP_UTILIZATION_UNDER_CRITICAL_MEM_REQ, busy);
    } else {
      STAT_EVENT(proc_id, CYCLES_UNDER_NONCRITICAL_MEM_REQ);
      INC_STAT_EVENT(proc_id, CHIP_UTILIZATION_UNDER_NONCRITICAL_MEM_REQ, busy);
    }
  } else {
    STAT_EVENT(proc_id, CYCLES_UNDER_NO_MEM_REQ);
    INC_STAT_EVENT(proc_id, CHIP_UTILIZATION_UNDER_NO_MEM_REQ, busy);
  }
  proc->chip_busy_in_this_prefetch_phase += busy;
  INC_STAT_EVENT(proc_id, TOTAL_CHIP_UTILIZATION, busy);
  if(proc->total_critical_reqs > 0)
    INC_STAT_EVENT(proc_id, CHIP_BUSY_UNDER_CRITICAL_REQS, busy);

  if(proc->total_off_chip_delays == 0) {
    STAT_EVENT(proc_id, CYCLES_NOT_WAITING_FOR_OFF_CHIP);
    INC_STAT_EVENT(proc_id, FUS_BUSY_NOT_WAITING_FOR_OFF_CHIP, num_fus_busy);
  } else {
    STAT_EVENT(proc_id, CYCLES_WAITING_FOR_OFF_CHIP);
    INC_STAT_EVENT(proc_id, FUS_BUSY_WAITING_FOR_OFF_CHIP, num_fus_busy);
  }
}

void perf_pred_interval_done() {
  if(!PERF_PRED_ENABLE)
    return;

  process_slack_period(0, chip_cycle_count);
}

void perf_pred_done() {
  if(!PERF_PRED_ENABLE)
    return;

  perf_pred_interval_done();

  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Proc_Info* proc = &proc_infos[proc_id];

    if(CRITICAL_ACCESS_PLOT_ENABLE) {
      fclose(proc->critical_access_plot_file);
    }
    INC_STAT_EVENT(proc_id, TOTAL_OFF_CHIP_DELAYS_LEFT,
                   proc->total_off_chip_delays);
  }
}

void perf_pred_slack(Mem_Req* req, Counter old_constraint, Counter latency,
                     Flag final) {
  if(!PERF_PRED_ENABLE)
    return;

  Proc_Info* proc = &proc_infos[0];

  if(old_constraint < proc->last_slack_period_start_in_memory_cycles) {
    /* This check really guards against using an old_constraint
       from a previous DVFS interval (with different frequencies) */
    old_constraint = proc->last_slack_period_start_in_memory_cycles;
  }

  /* Convert to chip cycles - hopefully a temporary fix */
  old_constraint = proc->last_slack_period_start +
                   freq_convert(
                     FREQ_DOMAIN_MEMORY,
                     old_constraint -
                       proc->last_slack_period_start_in_memory_cycles,
                     FREQ_DOMAIN_L1);

  Bank_Info* info = &proc->bank_infos[req->mem_flat_bank];

  // HACK: guard against freq domain weirdness
  if(chip_cycle_count < old_constraint)
    old_constraint = chip_cycle_count;
  ASSERT(0, chip_cycle_count >= old_constraint);
  DEBUG(0,
        "Slack reported in bank %d during prefetch phase (%d): %lld cycles\n",
        req->mem_flat_bank, proc->total_prefetch_reqs,
        chip_cycle_count - old_constraint);

  info->slack_in_this_slack_period += chip_cycle_count -
                                      MAX2(old_constraint,
                                           proc->last_slack_period_start);
  info->bank_accessed_in_this_slack_period = TRUE;
  info->slack_last_update_cycle            = chip_cycle_count;
  proc->mem_reqs_in_this_slack_period += 1;
  if(proc->mem_reqs_in_this_slack_period == PERF_PRED_SLACK_PERIOD_SIZE) {
    process_slack_period(0, chip_cycle_count);
  }
}

static void process_slack_period(uns proc_id, Counter slack_period_end) {
  Proc_Info* proc = &proc_infos[proc_id];

  Counter least_slack     = slack_period_end - proc->last_slack_period_start;
  Counter min_bus_latency = PERF_PRED_SLACK_PERIOD_SIZE * BUS_WIDTH_IN_BYTES /
                            2 * freq_get_cycle_time(FREQ_DOMAIN_MEMORY) /
                            freq_get_cycle_time(FREQ_DOMAIN_L1);
  if(least_slack >= min_bus_latency)
    least_slack -= min_bus_latency;
  else
    least_slack = 0;
  for(int bank = 0; bank < RAMULATOR_BANKS * RAMULATOR_CHANNELS; ++bank) {
    Bank_Info* info  = &proc->bank_infos[bank];
    Counter    slack = (info->bank_accessed_in_this_slack_period ?
                       info->slack_in_this_slack_period +
                         (slack_period_end -
                          info->slack_last_update_cycle) :  // mixing
                       slack_period_end - proc->last_slack_period_start);
    if(slack > slack_period_end - proc->last_slack_period_start) {
      printf("%lld > %lld!\n", slack,
             slack_period_end - proc->last_slack_period_start);
    }
    ASSERT(0, slack <= slack_period_end - proc->last_slack_period_start);
    least_slack = MIN2(least_slack, slack);
    // reset for next slack period
    info->slack_in_this_slack_period         = 0;
    info->bank_accessed_in_this_slack_period = FALSE;
    info->slack_last_update_cycle            = slack_period_end;
  }
  // the following could happen at the end of prefetch phase
  DEBUG(0, "Added %lld of critical slack from a slack period of %lld cycles\n",
        least_slack, slack_period_end - proc->last_slack_period_start);
  INC_STAT_EVENT(proc_id, TOTAL_MEMORY_SLACK, least_slack);
  proc->last_slack_period_start                  = chip_cycle_count;
  proc->last_slack_period_start_in_memory_cycles = freq_cycle_count(
    FREQ_DOMAIN_MEMORY);
  proc->mem_reqs_in_this_slack_period = 0;
}

void perf_pred_off_chip_effect_start(Mem_Req* req) {
  if(!PERF_PRED_ENABLE)
    return;

  Proc_Info* proc = &proc_infos[req->proc_id];

  ASSERT(
    0, proc->total_off_chip_delays < 10000);  // sanity check with magic number
  if(proc->total_off_chip_delays == 0)
    DEBUG(0, "Entered off-chip effect phase (req %d)\n", req->id);
  proc->total_off_chip_delays += 1;
}

void perf_pred_off_chip_effect_end(Mem_Req* req) {
  if(!PERF_PRED_ENABLE)
    return;

  Proc_Info* proc = &proc_infos[req->proc_id];

  ASSERT(0, proc->total_off_chip_delays > 0);
  proc->total_off_chip_delays -= 1;
  if(proc->total_off_chip_delays == 0)
    DEBUG(0, "Exited off-chip effect phase (req %d)\n", req->id);
}

void perf_pred_reset_stats(void) {
  if(!PERF_PRED_ENABLE)
    return;

  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Proc_Info* proc = &proc_infos[proc_id];

    STAT_EVENT(proc_id, PERF_PRED_NUM_STAT_RESETS);
    GET_STAT_EVENT(proc_id,
                   PERF_PRED_RESET_STATS_CYCLE) = chip_cycle_count;  // HACK!

    for(int bank = 0; bank < RAMULATOR_BANKS * RAMULATOR_CHANNELS; ++bank) {
      Bank_Info* info                          = &proc->bank_infos[bank];
      info->length                             = 0;
      info->slack_in_this_slack_period         = 0;
      info->slack_last_update_cycle            = chip_cycle_count;
      info->bank_accessed_in_this_slack_period = FALSE;
    }
    proc->mem_req_critical_path_length                         = 0;
    proc->chip_busy_in_this_prefetch_phase                     = 0;
    proc->chip_busy_under_critical_reqs_in_this_prefetch_phase = 0;
    proc->last_slack_period_start                  = chip_cycle_count;
    proc->last_slack_period_start_in_memory_cycles = freq_cycle_count(
      FREQ_DOMAIN_MEMORY);
    proc->mem_reqs_in_this_slack_period       = 0;
    proc->global_slack_in_this_prefetch_phase = 0;
  }

  ASSERTM(0,
          (RAMULATOR_READQ_ENTRIES + RAMULATOR_WRITEQ_ENTRIES) ==
            MEM_REQ_BUFFER_ENTRIES,
          "MEM_REQ_BUFFER_ENTRIES needs to be set equal to "
          "(RAMULATOR_READQ_ENTRIES + RAMULATOR_WRITEQ_ENTRIES)\n");
  for(int i = 0; i < MEM_REQ_BUFFER_ENTRIES; ++i) {
    mem->req_buffer[i].mem_crit_path_at_entry = 0;
  }

  stat_mon_reset(stat_mon);
}

void perf_pred_cycle(void) {
  chip_cycle_count                   = freq_cycle_count(FREQ_DOMAIN_L1);
  GET_STAT_EVENT(0, PERF_PRED_CYCLE) = chip_cycle_count;
}

double perf_pred_slowdown(uns proc_id, Perf_Pred_Mech mech, uns chip_cycle_time,
                          uns memory_cycle_time) {
  Counter num_cycles = stat_mon_get_count(stat_mon, 0, PERF_PRED_CYCLE);
  ASSERT(proc_id, num_cycles > 0);

  double cp_frac = (double)stat_mon_get_count(stat_mon, proc_id,
                                              MEM_REQ_CRITICAL_PATH_LENGTH) /
                   (double)num_cycles;
  double ll_frac = (double)stat_mon_get_count(stat_mon, proc_id,
                                              LEADING_LOAD_LATENCY) /
                   (double)num_cycles;
  double stall_frac = (double)stat_mon_get_count(stat_mon, proc_id,
                                                 RET_BLOCKED_L1_MISS) /
                      (double)num_cycles;

  double memory_cycle_time_ratio = (double)memory_cycle_time /
                                   (double)freq_get_cycle_time(
                                     FREQ_DOMAIN_MEMORY);
  double chip_cycle_time_ratio = (double)chip_cycle_time /
                                 (double)freq_get_cycle_time(FREQ_DOMAIN_L1);

  if(mech != PERF_PRED_CP_PREF) {
    /* Simple prediction for the no prefetching linear model */
    double Tm;
    switch(mech) {
      case PERF_PRED_CP:
        Tm = cp_frac;
        break;
      case PERF_PRED_LEADING_LOADS:
        Tm = ll_frac;
        break;
      case PERF_PRED_STALL:
        Tm = stall_frac;
        break;
      default:
        ASSERTM(proc_id, 0, "Unsupported perf pred mechanism");
    }

    return Tm * memory_cycle_time_ratio + (1.0 - Tm) * chip_cycle_time_ratio;
  }

  /* Prefetching model (hockey stick) */
  ASSERT(proc_id, mech == PERF_PRED_CP_PREF);

  DEBUG(proc_id, "In perf_pred_slowdown, cp_frac: %lf\n", cp_frac);

  double mem_util;
  double chip_util;
  double chip_busy_crit;

  if(PERF_PRED_MEM_UTIL_VIA_BUS_BW) {
    uns rtw_bus_cost = (RAMULATOR_TCL + RAMULATOR_TCCD - RAMULATOR_TCWL + 2) +
                       RAMULATOR_TCWL - (RAMULATOR_TCL + RAMULATOR_TCCD);
    uns wtr_bus_cost = (RAMULATOR_TCWL + RAMULATOR_TBL + RAMULATOR_TWTR) +
                       RAMULATOR_TCL - (RAMULATOR_TCWL + RAMULATOR_TBL);
    double bus_dir_switch_cost = ((double)rtw_bus_cost + (double)wtr_bus_cost) /
                                 2.0;
    mem_util = (RAMULATOR_TBL * (double)stat_mon_get_count(
                                  stat_mon, proc_id, MEM_REQ_COMPLETE_MEM) +
                bus_dir_switch_cost *
                  (double)stat_mon_get_count(stat_mon, proc_id,
                                             DRAM_BUS_DIR_SWITCHES)) /
               stat_mon_get_count(stat_mon, proc_id, DRAM_CYCLES);
  } else {
    mem_util = 1.0 - (double)stat_mon_get_count(stat_mon, proc_id,
                                                TOTAL_MEMORY_SLACK) /
                       (double)num_cycles;
  }

  if(PERF_PRED_CHIP_UTIL_VIA_MEM_STALL) {
    chip_util = 1.0 -
                (double)stat_mon_get_count(stat_mon, proc_id,
                                           RET_BLOCKED_MEM_STALL) /
                  (double)stat_mon_get_count(stat_mon, proc_id, NODE_CYCLE);
    chip_busy_crit = 0;
  } else {
    chip_util = (double)stat_mon_get_count(stat_mon, proc_id,
                                           TOTAL_CHIP_UTILIZATION) /
                (double)num_cycles;
    /* Fraction of time chip was in prefetch phase, under a critical mem req,
     * and busy */
    chip_busy_crit = (double)stat_mon_get_count(
                       stat_mon, proc_id,
                       CHIP_UTILIZATION_UNDER_CRITICAL_MEM_REQ) /
                     (double)num_cycles;
  }

  Flag in_saturated_memory_region = FALSE;
  UNUSED(in_saturated_memory_region);

  /* FIXME: check whether this formula makes sense (look at denominators) */
  if(mem_util > chip_util - chip_busy_crit + cp_frac) {
    /* In prefetch phase, if memory utilization is greater than
     * the compute + demand critical path, assume the memory
     * bandwidth is saturated */
    mem_util                   = 1.0;
    in_saturated_memory_region = TRUE;
  } else {
    /* Assume we are compute bound: chip is fully utilized in
       prefetch phase except for demands */
    chip_util = 1.0 - cp_frac + chip_busy_crit;
  }

  double midpoint;
  if(chip_util <= chip_busy_crit) {
    midpoint = 1000000.0;  // crude approximation of infinity
  } else {
    midpoint = (mem_util - cp_frac) / (chip_util - chip_busy_crit) *
               (double)freq_get_cycle_time(FREQ_DOMAIN_L1) /
               (double)freq_get_cycle_time(FREQ_DOMAIN_MEMORY);
  }

  if((double)chip_cycle_time / (double)memory_cycle_time > midpoint) {
    /* Prefetching will not saturate memory bandwidth */
    double Tm = cp_frac;
    double Tc = chip_util - chip_busy_crit;
    return Tm * memory_cycle_time_ratio + Tc * chip_cycle_time_ratio;
  } else {
    /* memory bandwidth limited in prefetch phase */
    double Tm = mem_util;
    double Tc = 0.0;
    return Tm * memory_cycle_time_ratio + Tc * chip_cycle_time_ratio;
  }
}

typedef struct MLP_Info_struct {
  Counter window_start_opnum;
  Counter num_windows;
  uns     num_dcache_misses;
  uns     longest_chain;
} MLP_Info;

MLP_Info* mlp_infos;

static void init_mlp_infos(void);
static void reset_window_info(uns proc_id);
static void collect_mlp_info_stats(uns proc_id);

void perf_pred_l0_miss_start(struct Mem_Req_struct* req) {
  if(NUM_CORES > 1)
    return;  // temporary guard

  if(req->type == MRT_IFETCH || req->type == MRT_DFETCH) {
    uns       proc_id  = req->proc_id;
    MLP_Info* mlp_info = &mlp_infos[proc_id];

    if(mlp_info->num_dcache_misses >
         0 &&  // might be zero in the first call after warmup
       uop_count[proc_id] >= mlp_info->window_start_opnum + NODE_TABLE_SIZE) {
      collect_mlp_info_stats(proc_id);
      reset_window_info(proc_id);
      mlp_info->num_windows++;
      STAT_EVENT(proc_id, NUM_WINDOWS_WITH_DCACHE_MISS);
    }

    mlp_info->num_dcache_misses++;

    req->window_num    = mlp_info->num_windows;
    req->longest_chain = mlp_info->longest_chain + 1;
  }
}

void perf_pred_l0_miss_end(struct Mem_Req_struct* req) {
  if(NUM_CORES > 1)
    return;  // temporary guard

  if(req->type == MRT_IFETCH || req->type == MRT_DFETCH) {
    MLP_Info* mlp_info = &mlp_infos[req->proc_id];

    if(req->window_num == mlp_info->num_windows) {
      mlp_info->longest_chain = MAX2(mlp_info->longest_chain,
                                     req->longest_chain);
    }
  }
}

static void init_mlp_infos(void) {
  mlp_infos = (MLP_Info*)calloc(NUM_CORES, sizeof(MLP_Info));
}

static void reset_window_info(uns proc_id) {
  MLP_Info* mlp_info = &mlp_infos[proc_id];

  mlp_info->window_start_opnum = uop_count[proc_id];
  mlp_info->num_dcache_misses  = 0;
  mlp_info->longest_chain      = 0;
}

static void collect_mlp_info_stats(uns proc_id) {
  MLP_Info* mlp_info = &mlp_infos[proc_id];
  double    mlp      = 1.0 * mlp_info->num_dcache_misses /
               MAX2(1.0, mlp_info->longest_chain);
  int mlp_index = (int)(round(mlp * 2.0)) - 2;
  ASSERTM(proc_id, mlp_index >= 0,
          "mlp_index: %d, dcache_misses: %d, chain: %d", mlp_index,
          mlp_info->num_dcache_misses, mlp_info->longest_chain);

  STAT_EVENT(proc_id, NUM_DCACHE_MISSES_IN_WINDOW_1 +
                        MIN2(15, mlp_info->num_dcache_misses - 1));
  STAT_EVENT(proc_id, DCACHE_MLP_IN_WINDOW_1_0 + MIN2(15, mlp_index));
  INC_STAT_EVENT(proc_id, LONGEST_DCACHE_MISS_CHAIN,
                 MAX2(1.0, mlp_info->longest_chain));
}
