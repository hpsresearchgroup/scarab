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
 * File         : sim.c
 * Author       : HPS Research Group
 * Date         : 11/10/1997
 * Description  :
 ***************************************************************************************/

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"
#include "op_pool.h"
#include "statistics.h"

#include "freq.h"
#include "frontend/frontend.h"
#include "frontend/frontend_intf.h"
#include "sim.h"
#include "thread.h"

#include "cmp_model.h"
#include "debug/memview.h"
#include "debug/pipeview.h"
#include "dumb_model.h"
#include "frontend/pin_trace_fe.h"
#include "model.h"
#include "optimizer2.h"
#include "power/power_intf.h"
#include "stat_trace.h"
#include "trigger.h"

#include "bp/bp.param.h"
#include "core.param.h"
#include "debug/debug.param.h"
#include "general.param.h"

#include "ramulator.h"

/**************************************************************************************/
/* Macros */

#define HEARTBEAT_PRINT_CPS FALSE /* default FALSE */

/**************************************************************************************/
/* Global Variables */

Trigger* sim_limit;
Trigger* clear_stats;
Counter* inst_limit;

// Current version does not support more than 8 cores!
Counter  unique_count;          /* the global unique op counter */
Counter* unique_count_per_core; /* the unique op count per core */
Counter* op_count;              /* the global op counter per core*/
Counter* inst_count; /* the global instruction counter - retired per core */
Counter* uop_count;  /* the global uop counter - retired per core*/
Counter  cycle_count = 0; /* the global cycle counter */
Counter  sim_time    = 0; /* the global time counter */
Counter* pret_inst_count; /* the global pseudo-retired instruction counter */
Flag*    trace_read_done;
Flag*    reached_exit;
Flag*    retired_exit;
Flag*    sim_done;
Counter* last_forward_progress;
Counter* last_uop_count;
Counter* sim_done_last_inst_count;
Counter* sim_done_last_uop_count;
Counter* sim_done_last_cycle_count;
uns*     sim_count;
uns      operating_mode = SIMULATION_MODE;

time_t sim_start_time; /* the time that the simulator was started */

FILE* mystdout; /* default output (can be redirected via --stdout) */
FILE* mystderr; /* default error (can be redirected via --stderr) */
FILE* mystatus; /* default status (can be redirected via --status_file) */
int   mystatus_fd = 0; /* file descriptor pointing to the mystatus file */

#include "model_table.def"

Model* model; /* pointer to the simulator model being used
       (points to an entry in the model_table array) */

Thread_Data
             single_td; /* cmp Only For single processor: backward compatibility issue*/
Thread_Data* td = &single_td; /* array of tds for muti-core, all state
                                 associated with the simulated thread */

/**************************************************************************************/
/* Prototypes */

static void init_global_counter(void);
static void init_model(uns mode);
static void init_output_streams(void);
static void process_params(void);
static void reset_uop_mode_counters(void);

static inline void    check_heartbeat(uns8 proc_id, Flag final);
static inline Counter check_forward_progress(uns8 proc_id);
static inline double  sim_progress(void);
static inline void    set_last_sim_param(uns8 proc_id);
static inline void    print_bogus_sim_param(uns8 proc_id);

/**************************************************************************************/
/* handle_SIGINT: this handler is for exiting smoothly when a SIGINT is caught
 */

void handle_SIGINT(int signum) {
  uns8 proc_id;

  ASSERTU(0, signum == SIGINT);

  fprintf(mystdout, "** Handler:  Caught SIGINT.  Exiting...\n");

  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    retired_exit[proc_id] = TRUE;
  }
}


/**************************************************************************************/
/* check_heartbeat: Determine if the heartbeat needs to happen and do it if
 * needed. */
/* Instruction Count Based on Core 0*/
static inline void check_heartbeat(uns8 proc_id, Flag final) {
  ASSERT(proc_id, proc_id == 0 || final);
#define ROUND 6
  static int     last_heartbeat_idx           = -1;
  static time_t  heartbeat_last_time          = 0;
  static Counter heartbeat_last_cycle_count   = 0;
  static Counter heartbeat_last_inst_count    = 0;
  static Counter heartbeat_checked_inst_count = 0;
  static Flag    first_call                   = TRUE;
  UNUSED(heartbeat_last_cycle_count);

  static uns last_operating_mode;

  /* Bookkeeping so that heartbeat can be called from multiple modes several
   * times */
  if(first_call == FALSE) {
    if(operating_mode != last_operating_mode) {
      last_heartbeat_idx           = -1;
      heartbeat_last_time          = 0;
      heartbeat_last_cycle_count   = 0;
      heartbeat_last_inst_count    = 0;
      heartbeat_checked_inst_count = 0;
    }
  }
  first_call          = FALSE;
  last_operating_mode = operating_mode;
  /* End Bookkeeping */

  Counter inst_diff = inst_count[proc_id] -
                      heartbeat_checked_inst_count;  // FIXME cmp
  Counter rounded_interval = HEARTBEAT_INTERVAL;

  /* print heartbeat message if necessary */
  if((HEARTBEAT_INTERVAL && inst_diff >= rounded_interval) || final) {
    heartbeat_checked_inst_count = inst_count[proc_id];
    double progress_frac         = 0.0;
    if(!final) {
      ASSERT(0, operating_mode == SIMULATION_MODE);
      progress_frac = sim_progress();  // sim_progress() only works in
                                       // SIMULATION_MODE
      int heartbeat_idx = (int)(progress_frac * NUM_HEARTBEATS);
      if(NUM_HEARTBEATS && heartbeat_idx <= last_heartbeat_idx)
        return;
      last_heartbeat_idx = heartbeat_idx;
    }
    time_t  cur_time         = time(NULL);
    double  cum_ipc          = (double)inst_count[proc_id] / cycle_count;
    Counter total_inst_count = 0;
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++)
      total_inst_count += inst_count[proc_id];
#if HEARTBEAT_PRINT_CPS
    double int_khz = (double)HEARTBEAT_INTERVAL /
                     (cur_time - heartbeat_last_time) / 1000;
    double cum_khz = (double)cycle_count / (cur_time - sim_start_time) / 1000;
#else
    double int_khz = (double)(total_inst_count - heartbeat_last_inst_count) /
                     (cur_time - heartbeat_last_time) / 1000;
    double cum_khz = (double)total_inst_count / (cur_time - sim_start_time) /
                     1000;
#endif

    if(final) {
      // if using optimizer2, only one process should dump final information
      if(opt2_in_use() && !opt2_is_leader())
        return;

      switch(operating_mode) {
        case WARMUP_MODE:
          fprintf(mystdout,
                  "** WARMUP End:   insts:%-10s  cycles:%-10s  time:%-18s  -- "
                  "%.2f IPC (%.2f IPC) --  N/A  KIPS (%.2f KIPS)\n",
                  unsstr64(inst_count[proc_id]), unsstr64(cycle_count),
                  unsstr64(sim_time), cum_ipc, cum_ipc, cum_khz);
          fflush(mystdout);
          break;

        case SIMULATION_MODE:
          fprintf(mystdout,
                  "** Core %u Finished:    insts:%-10s  cycles:%-10s  "
                  "time:%-18s  -- %.2f IPC (%.2f IPC) --  N/A  KIPS (%.2f "
                  "KIPS)\n",
                  proc_id, unsstr64(inst_count[proc_id]), unsstr64(cycle_count),
                  unsstr64(sim_time), cum_ipc, cum_ipc, cum_khz);
          break;

        default:
          ASSERT(0, 0);
      }
    } else if(!opt2_in_use() || opt2_is_leader()) {
      fprintf(mystdout, "** Heartbeat: %3d%% -- { ",
              (int)(progress_frac * 100.0));
      for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++)
        fprintf(mystdout, "%lld ", inst_count[proc_id]);
      fprintf(mystdout, "} -- %.2f KIPS (%.2f KIPS)\n", int_khz, cum_khz);
      fflush(mystdout);
      heartbeat_last_time        = cur_time;
      heartbeat_last_cycle_count = cycle_count;
      heartbeat_last_inst_count  = total_inst_count;
    }
  }
#undef ROUND
}

/**************************************************************************************/
/* check_forward_progress: */

static inline Counter check_forward_progress(uns8 proc_id) {
  if(uop_count[proc_id] > last_uop_count[proc_id]) {
    last_forward_progress[proc_id] = cycle_count;
    last_uop_count[proc_id]        = uop_count[proc_id];
  }

  if(!(cycle_count - last_forward_progress[proc_id] <=
       (Counter)FORWARD_PROGRESS_LIMIT)) {
    uns8 proc_id2;
    for(proc_id2 = 0; proc_id2 < NUM_CORES; proc_id2++) {
      if(!sim_done[proc_id2])
        dump_stats(proc_id2, TRUE, global_stat_array[proc_id2],
                   NUM_GLOBAL_STATS);
    }

    if(cmp_model.node_stage[proc_id].node_head) {
      printf("What op prevents proceeding? unique: %llu, valid: %u, va: %llx, "
             "opstate: %u, op_type: %u, mem_type: %u, req: %p proc: %u, addr: "
             "%llu, state: %u\n",
             cmp_model.node_stage[proc_id].node_head->unique_num,
             cmp_model.node_stage[proc_id].node_head->op_pool_valid,
             cmp_model.node_stage[proc_id].node_head->oracle_info.va,
             cmp_model.node_stage[proc_id].node_head->state,
             cmp_model.node_stage[proc_id].node_head->table_info->op_type,
             cmp_model.node_stage[proc_id].node_head->table_info->mem_type,
             cmp_model.node_stage[proc_id].node_head->req ?
               cmp_model.node_stage[proc_id].node_head->req :
               0,
             cmp_model.node_stage[proc_id].node_head->req ?
               cmp_model.node_stage[proc_id].node_head->req->proc_id :
               0,
             cmp_model.node_stage[proc_id].node_head->req ?
               cmp_model.node_stage[proc_id].node_head->req->addr :
               0,
             cmp_model.node_stage[proc_id].node_head->req ?
               cmp_model.node_stage[proc_id].node_head->req->state :
               0);
    } else {
      printf("What prevents proceeding? Node stage is empty!\n");
    }
  }

  ASSERTM(0,
          cycle_count - last_forward_progress[proc_id] <=
            (Counter)FORWARD_PROGRESS_LIMIT,
          "last_forward_progress:%llu\n", last_forward_progress[proc_id]);

  return last_forward_progress[proc_id] + FORWARD_PROGRESS_LIMIT;
}

/**************************************************************************************/
/* sim_progress: */

static inline double sim_progress(void) {
  double sim_limit_progress = SIM_MODE == FULL_SIM_MODE ?
                                trigger_progress(sim_limit) :
                                0.0;

  if(!INST_LIMIT)
    return sim_limit_progress;

  double inst_limit_progress = 1.0;
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    inst_limit_progress = MIN2(
      inst_limit_progress,
      (double)inst_count[proc_id] / (double)inst_limit[proc_id]);
  }

  return MAX2(sim_limit_progress, inst_limit_progress);
}

/**************************************************************************************/
/* init_output_streams: */
///////////////////////// FIXME for cmp support: proc_id??
static void init_output_streams() {
  if(STDERR_FILE) {
    mystderr = file_tag_fopen(OUTPUT_DIR, STDERR_FILE, "w");

    // can't use assert since my* hasn't been set up yet
    if(!mystderr) {
      fprintf(stderr, "\n");
      fprintf(stderr, "%s:%d: ASSERT FAILED (O=%s  I=%s  C=%s):  ", __FILE__,
              __LINE__, intstr64(op_count[0]), intstr64(inst_count[0]),
              intstr64(cycle_count));
      fprintf(stderr, "%s '%s'\n", "mystderr", STDERR_FILE);
      breakpoint(__FILE__, __LINE__);
      exit(15);
    }
  }

  if(STDOUT_FILE) {
    mystdout = file_tag_fopen(OUTPUT_DIR, STDOUT_FILE, "w");

    // can't use assert since my* hasn't been set up yet
    if(!mystdout) {
      fprintf(stderr, "\n");
      fprintf(stderr, "%s:%d: ASSERT FAILED (O=%s  I=%s  C=%s):  ", __FILE__,
              __LINE__, intstr64(op_count[0]), intstr64(inst_count[0]),
              intstr64(cycle_count));
      fprintf(stderr, "%s '%s'\n", "mystdout", STDOUT_FILE);
      breakpoint(__FILE__, __LINE__);
      exit(15);
    }
  }

  if(STATUS_FILE) {
    mystatus = fopen(STATUS_FILE, "a");

    // can't use assert since my* hasn't been set up yet
    if(!mystatus) {
      fprintf(stderr, "\n");
      fprintf(stderr, "%s:%d: ASSERT FAILED (O=%s  I=%s  C=%s):  ", __FILE__,
              __LINE__, intstr64(op_count[0]), intstr64(inst_count[0]),
              intstr64(cycle_count));
      fprintf(stderr, "%s '%s'\n", "mystatus", STATUS_FILE);
      breakpoint(__FILE__, __LINE__);
      exit(15);
    }
  }
}

/**************************************************************************************/
/* close_output_streams */

void close_output_streams(void) {
  if(STDOUT_FILE)
    fclose(mystdout);
  if(STDERR_FILE)
    fclose(mystderr);
  if(STATUS_FILE)
    fclose(mystatus);
}

/**************************************************************************************/
/* init_global_counter */

void init_global_counter() {
  inst_limit = (Counter*)malloc(sizeof(Counter) * NUM_CORES);
  memset(inst_limit, 0, sizeof(Counter) * NUM_CORES);
  unique_count_per_core = (Counter*)malloc(sizeof(Counter) * NUM_CORES);
  memset(unique_count_per_core, 0, sizeof(Counter) * NUM_CORES);
  op_count = (Counter*)malloc(sizeof(Counter) * NUM_CORES);
  ;
  memset(op_count, 0, sizeof(Counter) * NUM_CORES);
  inst_count = (Counter*)malloc(sizeof(Counter) * NUM_CORES);
  memset(inst_count, 0, sizeof(Counter) * NUM_CORES);
  uop_count = (Counter*)malloc(sizeof(Counter) * NUM_CORES);
  ;
  memset(uop_count, 0, sizeof(Counter) * NUM_CORES);
  pret_inst_count = (Counter*)malloc(sizeof(Counter) * NUM_CORES);
  memset(pret_inst_count, 0, sizeof(Counter) * NUM_CORES);
  trace_read_done = (Flag*)malloc(NUM_CORES * sizeof(Flag));
  memset(trace_read_done, 0, NUM_CORES * sizeof(Flag));
  reached_exit = (Flag*)malloc(sizeof(Flag) * NUM_CORES);
  memset(reached_exit, 0, sizeof(Flag) * NUM_CORES);
  retired_exit = (Flag*)malloc(sizeof(Flag) * NUM_CORES);
  memset(retired_exit, 0, sizeof(Flag) * NUM_CORES);
  sim_done = (Flag*)malloc(sizeof(Flag) * NUM_CORES);
  memset(sim_done, 0, sizeof(Flag) * NUM_CORES);
  last_forward_progress = (Counter*)malloc(sizeof(Counter) * NUM_CORES);
  memset(last_forward_progress, 0, sizeof(Counter) * NUM_CORES);
  last_uop_count = (Counter*)malloc(sizeof(Counter) * NUM_CORES);
  memset(last_uop_count, 0, sizeof(Counter) * NUM_CORES);
  sim_done_last_inst_count = (Counter*)malloc(sizeof(Counter) * NUM_CORES);
  memset(sim_done_last_inst_count, 0, sizeof(Counter) * NUM_CORES);
  sim_done_last_uop_count = (Counter*)malloc(sizeof(Counter) * NUM_CORES);
  memset(sim_done_last_uop_count, 0, sizeof(Counter) * NUM_CORES);
  sim_done_last_cycle_count = (Counter*)malloc(sizeof(Counter) * NUM_CORES);
  memset(sim_done_last_cycle_count, 0, sizeof(Counter) * NUM_CORES);
  sim_count = (uns*)malloc(sizeof(uns) * NUM_CORES);
  memset(sim_count, 0, sizeof(uns) * NUM_CORES);
}

/**************************************************************************************/
/* Reset counters used by the uop mode */

void reset_uop_mode_counters(void) {
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    op_count[proc_id]   = 1;  // FIXME: find out why zero breaks an ASSERT
    inst_count[proc_id] = 0;
  }
}

/**************************************************************************************/
/* process_params: pre-process some simulation parameters */

void process_params(void) {
  if(INST_LIMIT) {
    int cores_specified = parse_uns64_array(inst_limit, INST_LIMIT, NUM_CORES);
    if(cores_specified == 1) {
      for(uns ii = 1; ii < NUM_CORES; ii++)
        inst_limit[ii] = inst_limit[0];
    } else {
      ASSERTM(0, cores_specified == NUM_CORES,
              "Invalid INST_LIMIT syntax: %s\n", INST_LIMIT);
    }
  }
}

/**************************************************************************************/
/* init_all: Calls all the initialization functions that are used by all
   simulation modes.  Mode-specific initialization is done at the beginning of
   the mode simulation function.  */

void init_global(char* argv[], char* envp[]) {
  uns proc_id;
  init_global_counter();
  init_output_streams();
  init_global_stats_array();
  for(proc_id = 0; proc_id < NUM_CORES; proc_id++)
    init_global_stats(proc_id);
  process_params();
  stat_trace_init();
  if(SIM_MODEL != DUMB_MODEL)
    frontend_init();
  power_intf_init();
  init_thread(td, argv, envp);  // Remove later may be? This is here for
                                // execution driven version
  sim_start_time = time(NULL);
}


/**************************************************************************************/
/* init_model: Set up the model pointer */

static void init_model(uns mode) {
  model = &model_table[SIM_MODEL];
  model->init_func(mode);
  if(SIM_MODEL != DUMB_MODEL && DUMB_CORE_ON) {
    ASSERT(0, DUMB_CORE < NUM_CORES);
    model_table[DUMB_MODEL].init_func(mode);
  }
}

/**************************************************************************************/
/* set_last_sim_param: for bogus run stats*/
static inline void set_last_sim_param(uns8 proc_id) {
  sim_done_last_cycle_count[proc_id] = cycle_count;
  sim_done_last_inst_count[proc_id]  = inst_count[proc_id];
  sim_done_last_uop_count[proc_id]   = uop_count[proc_id];
  sim_count[proc_id]++;
}

/**************************************************************************************/
/* print_bogus_sim_param: for bogus run stats*/
static inline void print_bogus_sim_param(uns8 proc_id) {
  double ipc = (double)(inst_count[proc_id] -
                        sim_done_last_inst_count[proc_id]) /
               (cycle_count - sim_done_last_cycle_count[proc_id]);
  fprintf(mystdout,
          " --Core: %-2u %u run finished:    insts:%-10llu  uops:%-10llu  "
          "cycles:%-10llu -- %.2f IPC\n",
          proc_id, sim_count[proc_id] + 1,
          inst_count[proc_id] - sim_done_last_inst_count[proc_id],
          uop_count[proc_id] - sim_done_last_uop_count[proc_id],
          cycle_count - sim_done_last_cycle_count[proc_id], ipc);
}

/**************************************************************************************/
/* uop_sim: This is the main loop for running in uop level simulation mode.*/

void uop_sim() {
  ASSERTM(0, operating_mode != SIMULATION_MODE || !strcmp(SIM_LIMIT, "none"),
          "SIM_LIMIT does not work in uop simulation mode\n");
  ASSERTM(0, operating_mode != WARMUP_MODE || model->warmup_func,
          "Model %s does not have a warmup function\n", model->name);
  ASSERTM(0, NUM_CORES == 1 || !FAST_FORWARD_UNTIL_ADDR,
          "FAST_FORWARD_UNTIL_ADDR works only for single core\n");

  Op         op;
  Table_Info table_info;
  Inst_Info  inst_info;
  op.table_info = &table_info;
  op.inst_info  = &inst_info;
  op.mbp7_info  = NULL;

  Flag uop_sim_done = FALSE;

  while(!uop_sim_done) {
    if(operating_mode == SIMULATION_MODE)
      uop_sim_done = TRUE;
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      if(DUMB_CORE_ON && DUMB_CORE == proc_id)
        continue;
      if(!retired_exit[proc_id]) {
        do {
          frontend_fetch_op(proc_id, &op);

          if(op.table_info->mem_type != NOT_MEM && op.oracle_info.va == 0) {
            FATAL_ERROR(proc_id, "Access to 0x0\n");
          }

          if(DUMP_TRACE && DEBUG_RANGE_COND(proc_id))
            print_func_op(&op);

          op_count[proc_id]++;
          if(op.eom)
            inst_count[proc_id]++;
          if(op.exit)
            retired_exit[proc_id] = TRUE;
          // fprintf(stderr, "op mode is %x, opexit is %d\n", operating_mode,
          // op.exit);
          ASSERTM(proc_id, !op.exit || operating_mode == SIMULATION_MODE,
                  "Program ended before start of simulation\n");

          switch(operating_mode) {
            case WARMUP_MODE:
              model->warmup_func(&op);
              break;
            case SIMULATION_MODE:
              if(!sim_done[proc_id]) {
                if(retired_exit[proc_id] ||
                   (INST_LIMIT && inst_count[proc_id] == inst_limit[proc_id])) {
                  sim_done[proc_id] = TRUE;
                  dump_stats(proc_id, TRUE, global_stat_array[proc_id],
                             NUM_GLOBAL_STATS);
                  check_heartbeat(proc_id, TRUE);
                } else {
                  uop_sim_done = FALSE;
                  if(proc_id == 0)
                    check_heartbeat(0, FALSE);
                }
              }
              break;
            default:
              FATAL_ERROR(0, "Unknown simulation mode\n");
          }
          if(op.eom) {
            frontend_retire(op.proc_id, op.inst_uid);
          }
        } while(!uop_sim_done && !op.eom);
      }
    }
    switch(operating_mode) {
      case WARMUP_MODE:
        if(inst_count[0] == WARMUP || retired_exit[0]) {
          uop_sim_done = TRUE;
          check_heartbeat(0, TRUE);
        }
        // HACK that ensures that cache replacement works in warmup
        do {
          freq_advance_time();
        } while(!freq_is_ready(FREQ_DOMAIN_L1));
        sim_time = freq_time();
        break;
      default:
        ASSERT(0, operating_mode == SIMULATION_MODE);
        break;
    }
  }
}

/**************************************************************************************/
/* full_sim: This is the main loop for running in full simulation mode.*/

void full_sim() {
  uns8 proc_id;
  Flag all_sim_done = FALSE;

  /* perform initialization  */
  init_model(WARMUP_MODE);  // make sure this happens before init_op_pool

  if(WARMUP) {
    operating_mode = WARMUP_MODE;
    uop_sim();
    reset_uop_mode_counters();
    reset_stats(FALSE);  // ignore stats accumulated during warmup
    /* The call below resets the cycle counts of all frequency
       domains but maintains the execution time value. This allows us to:

       * maintain cache replacement information remaining from
         warmup mode (the replacement state is stored using access
         timestamps and thus requires time never to be reset), and

       * keep the simulation mode initialization code happy (this
         code is not memory-aware and assumes that the first
         simulation cycle is cycle zero). */
    freq_reset_cycle_counts();
  }

  operating_mode = SIMULATION_MODE;
  init_model(operating_mode);

  if(PIPEVIEW)
    pipeview_init();
  if(MEMVIEW)
    memview_init();

  init_op_pool();
  unique_count = 1;

  sim_limit   = trigger_create("SIM_LIMIT", SIM_LIMIT, TRIGGER_ONCE);
  clear_stats = trigger_create("CLEAR_STATS", CLEAR_STATS, TRIGGER_ONCE);

  /* main loop */
  while(!trigger_fired(sim_limit) && !all_sim_done) {
    freq_advance_time();
    sim_time = freq_time();
    model->cycle_func();
    if(SIM_MODEL != DUMB_MODEL && DUMB_CORE_ON)
      model_table[DUMB_MODEL].cycle_func();


    if(DEBUG_MODEL && DEBUG_RANGE_COND(0) && ENABLE_GLOBAL_DEBUG_PRINT)
      model->debug_func();

    /* Avoid confusing any old global mechanisms (like check
       forward progress) by using only core 0 cycles */
    cycle_count = freq_cycle_count(FREQ_DOMAIN_CORES[0]);

    // check_dump_stats();  This is not being used in general
    check_heartbeat(0, FALSE);

    stat_trace_cycle();
    if(trigger_fired(clear_stats)) {
      reset_stats(TRUE);
    }

    all_sim_done = TRUE;
    for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      Flag reachedInstLimit = (INST_LIMIT &&
                               inst_count[proc_id] >= inst_limit[proc_id]);
      if(SIM_MODEL != DUMB_MODEL && DUMB_CORE_ON && DUMB_CORE == proc_id)
        continue;
      if(!sim_done[proc_id] && (retired_exit[proc_id] || reachedInstLimit)) {
        if(model->per_core_done_func)
          model->per_core_done_func(proc_id);
        dump_stats(proc_id, TRUE, global_stat_array[proc_id], NUM_GLOBAL_STATS);
        sim_done[proc_id] = TRUE;
        check_heartbeat(proc_id, TRUE);

        if(retired_exit[proc_id] && FRONTEND == FE_TRACE) {
          set_last_sim_param(proc_id);
          // rerun the corresponding benchmark again.
          // (reset retired_exit and reached_exit)
          cmp_init_bogus_sim(proc_id);
        }
      } else if(sim_done[proc_id] && retired_exit[proc_id]) {
        ASSERTM(
          proc_id, FRONTEND == FE_TRACE,
          "Unhandled case: benchmark finished in execution-driven mode\n");
        // rerun the corresponding benchmark again.
        if(FRONTEND == FE_TRACE) {
          print_bogus_sim_param(proc_id);
          set_last_sim_param(proc_id);
          cmp_init_bogus_sim(proc_id);
        }
      }

      all_sim_done &= sim_done[proc_id];
    }

    if(cycle_count % FORWARD_PROGRESS_INTERVAL ==
       0) {  // for simulator performance check every 10000000 cycles.
      for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
        check_forward_progress(proc_id);
      }
    }
  }

  if(model->done_func)
    model->done_func();
  if(SIM_MODEL != DUMB_MODEL && DUMB_CORE_ON)
    model_table[DUMB_MODEL].done_func();

  stat_trace_done();
  if(PIPEVIEW)
    pipeview_done();
  memview_done();
  power_intf_done();
  frontend_done(retired_exit);
  ramulator_finish();

  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    if(!sim_done[proc_id]) {
      dump_stats(proc_id, TRUE, global_stat_array[proc_id], NUM_GLOBAL_STATS);
      check_heartbeat(proc_id, TRUE);
    }
  }

  if(FRONTEND == FE_TRACE)
    trace_done();

  trigger_free(sim_limit);
  trigger_free(clear_stats);
}


/**************************************************************************************/
