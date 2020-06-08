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
* File         : main.c
* Author       : HPS Research Group
* Date         : 9/30/1997
* Description  :

\mainpage

Scarab is a detailed x86 multicore microarchitectural simulator. See
the [tutorial](../tutorial/tutorial.pdf) for general information on
how to check it out from version control, run experiments, summarize
the results, etc. This documentation deals with the source code of
Scarab.

Scarab's source code is organized as follows:

+ Infrastructure

  + Execution starting point
    + main.c

  + General helpful macros and functions
    + globals/assert.h
    + debug/debug_macros.h
    + globals/enum.h
    + globals/global_defs.h
    + globals/global_types.h
    + globals/utils.h

  + Data structure libraries
    + libs/hash_lib.h
    + libs/list_lib.h
    + libs/malloc_lib.h

  + Parameters and statistics
    + param_globals/enum.headers.h
    + param_parser.h
    + statistics.h

  + Misc
    + version.h

+ Functional model

  + General frontend/frontend.code
    + frontend/frontend.h
    + frontend/frontend_intf.h
    + isa/isa.h
    + isa/isa_macros.h

  + Multi2Sim
    + multi2sim.h
    + multi2sim directory

  + Pin trace
    + ctype_pin_inst.h
    + pin_inst.h
    + frontend/pin_trace_read.h
    + frontend/pin_trace_fe.h
    + gen_trace directory (trace generation)

  + Dependence maintenance
    + map.h

  + Thread
    + thread.h

+ Timing model

  + Top level architectural models
    + sim.h
    + model.h
    + cmp_model.h
    + cmp_model_support.h
    + dumb_model.h

  + Cores

    + Microinstruction data structures
      + op.h
      + op_info.h
      + op_pool.h
      + inst_info.h
      + table_info.h

    + Pipeline
      + stage_data.h
      + icache_stage.h
      + packet_build.h
      + decode_stage.h
      + dcache_stage.h
      + map_stage.h
      + node_stage.h
      + exec_stage.h
      + dcache_stage.h

    + Branch prediction
      + bp/bp.h
      + bp/bp_conf.h
      + bp/gshare.h
      + bp/hybridgp.h
      + bp/tagescl.h
      + bp/bp_targ_mech.h
      + path_id.h

  + %Memory system

    + %Memory request data structures
      + memory/mem_req.h

    + Cache hierarchy
      + memory/memory.h

    + DRAM
      + dram.h
      + Schedulers
        + dram_sched.h
        + atlas.h
        + batch_sched.h
        + bw_sched.h
        + dram_batch.h
        + dram_bw_part.h
        + equal_use_sched.h
        + fair_queuing.h
        + pri_dram_sched.h
        + prob_pri_sched.h
        + tcm_sched.h

    + Prefetching

      + Common prefetching interface
        + prefetcher/pref_common.h
        + prefetcher/pref_2dc.h
        + prefetcher/pref_ghb.h
        + prefetcher/pref_markov.h
        + prefetcher/pref_phase.h
        + prefetcher//pref_stream.h
        + prefetcher//pref_stride.h
        + prefetcher//pref_stridepc.h
        + prefetcher/pref_type.h

      + Others
        + prefetcher/l2l1pref.h
        + prefetcher/l2markv_pref.h
        + prefetcher/l2way_pref.h
        + prefetcher/stream_pref.h

  + Global architecture code
    + addr_trans.h
    + freq.h
    + globals/global_vars.h
    + stat_mon.h
    + trigger.h

  + Studied features
    + dvfs/dvfs.h
    + dvfs/perf_pred.h
    + dvfs/power_pred.h

  + Visualization and debug output
    + debug/debug_print.h
    + debug/memview.h
    + debug/pipeview.h
    + stat_trace.h

  + Power models

    + Wattch
      + cacti_conflict.h
      + power_cache.h
      + power_event.h
      + power_modules.h

    + McPAT
      + power_intf.h

  + Architectural libraries
    + bus_lib.h
    + libs/cache_lib.h
    + libs/port_lib.h

***************************************************************************************/

#include <signal.h>
#include <unistd.h>
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"

#include "optimizer2.h"
#include "param_parser.h"
#include "sim.h"
#include "statistics.h"
#include "version.h"

#include "general.param.h"

/**************************************************************************************/

int main(int argc, char* argv[], char* envp[]) {
  char** simulated_argv;
  time_t cur_time;

  /* initialize some of my output streams to the standards */
  mystdout = stdout;
  mystderr = stderr;
  mystatus = NULL;

  /* print banner with revision info */
  fprintf(mystdout, "Scarab gitrev: %s\n", version());

  /* make sure all the variable sizes are what we expect */
  ASSERTU(0, sizeof(uns8) == 1);
  ASSERTU(0, sizeof(uns16) == 2);
  ASSERTU(0, sizeof(uns32) == 4);
  ASSERTU(0, sizeof(uns64) == 8);
  ASSERTU(0, sizeof(int8) == 1);
  ASSERTU(0, sizeof(int16) == 2);
  ASSERTU(0, sizeof(int32) == 4);
  ASSERTU(0, sizeof(int64) == 8);

  /* read parameters from PARAMS.in and the command line */
  simulated_argv = get_params(argc, argv);

  /* perform global initialization */
  init_global(simulated_argv, envp);

  /* print PID (sometimes useful for debugging) */
  if(PRINT_PID) {
    fprintf(stderr, "PID: %d\n", getpid());
    sleep(10);
  }

  /* set up signal handler for SIGINT */
  signal(SIGINT, handle_SIGINT);

  /* print startup messages */
  time(&cur_time);
  fprintf(mystdout, "Scarab started at %s\n", ctime(&cur_time));
  WRITE_STATUS("PID %d", getpid());
  WRITE_STATUS("STARTED");

  /* call the function for the type of simulation  */
  switch(SIM_MODE) {
    case UOP_SIM_MODE:
      uop_sim();
      break;
    case FULL_SIM_MODE:
      full_sim();
      break;
    default:
      FATAL_ERROR(0, "Unknown simulation mode.");
      break;
  }

  /* all done --- print finish messages */
  time(&cur_time);
  fprintf(mystdout, "Scarab finished at %s\n", ctime(&cur_time));
  WRITE_STATUS("FINISHED");

  close_output_streams();

  if(opt2_in_use())
    opt2_sim_complete();

  return 0;
}
