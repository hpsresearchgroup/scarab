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
 * File         : exec_stage.c
 * Author       : HPS Research Group
 * Date         : 1/27/1999
 * Description  : CMP support
 ***************************************************************************************/

#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "debug/memview.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"

#include "bp/bp.h"
#include "exec_ports.h"
#include "exec_stage.h"
#include "map.h"

#include "bp/bp.param.h"
#include "cmp_model.h"
#include "core.param.h"
#include "debug/debug.param.h"
#include "dvfs/perf_pred.h"
#include "general.param.h"
#include "memory/memory.param.h"
#include "statistics.h"


/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_EXEC_STAGE, ##args)


/**************************************************************************************/
/* Global Variables */

Exec_Stage* exec = NULL;
int         op_type_delays[NUM_OP_TYPES];

/**************************************************************************************/
/* Prototypes */

static void init_op_type_delays();
void        exec_stage_inc_power_stats(Op* op);

/**************************************************************************************/
/* set_exec_stage: */

void set_exec_stage(Exec_Stage* new_exec) {
  exec = new_exec;
}

/**************************************************************************************/
// init_op_type_delays

static void init_op_type_delays() {
  uns ii;

  if(UNIFORM_OP_DELAY) {
    for(ii = 0; ii < NUM_OP_TYPES; ii++)
      op_type_delays[ii] = UNIFORM_OP_DELAY;
    return;
  }

#define OP_TYPE_DELAY_INIT(op_type) \
  op_type_delays[OP_##op_type] = OP_##op_type##_DELAY;
  OP_TYPE_LIST(OP_TYPE_DELAY_INIT);
#undef OP_TYPE_DELAY_INIT

  /* make sure all op_type_delays were set */
  for(ii = 0; ii < NUM_OP_TYPES; ii++)
    ASSERT(td->proc_id, op_type_delays[ii] != 0);
}

/**************************************************************************************/
/* init_exec_stage: */

void init_exec_stage(uns8 proc_id, const char* name) {
  ASSERT(proc_id, exec);
  DEBUG(proc_id, "Initializing %s stage\n", name);

  memset(exec, 0, sizeof(Exec_Stage));

  exec->proc_id = proc_id;

  exec->sd.name         = (char*)strdup(name);
  exec->sd.max_op_count = NUM_FUS;
  exec->sd.ops          = (Op**)malloc(sizeof(Op*) * NUM_FUS);
  exec->fus_busy        = 0;

  reset_exec_stage();

  init_op_type_delays();
}


/**************************************************************************************/
/* reset_exec_stage: */

void reset_exec_stage() {
  uns ii;
  exec->sd.op_count = 0;
  for(ii = 0; ii < NUM_FUS; ii++) {
    exec->sd.ops[ii] = NULL;
  }
}


/**************************************************************************************/
/* recover_exec_stage: */

void recover_exec_stage() {
  uns ii;
  for(ii = 0; ii < NUM_FUS; ii++) {
    Func_Unit* fu = &exec->fus[ii];
    Op*        op = exec->sd.ops[ii];
    if(op && op->op_num > bp_recovery_info->recovery_op_num) {
      exec->sd.ops[ii] = NULL;
      exec->sd.op_count--;
      fu->avail_cycle = cycle_count + 1;
      fu->idle_cycle  = cycle_count + 1;
    }
  }
}


/**************************************************************************************/
/* debug_exec_stage: */

void debug_exec_stage() {
  int ii;
  DPRINTF("# %-10s  op_count:%d  busy:", exec->sd.name, exec->sd.op_count);
  for(ii = 0; ii < NUM_FUS; ii++) {
    Func_Unit* fu = &exec->fus[ii];
    if(ii % 4 == 0)
      DPRINTF(" ");
    DPRINTF("%d", fu->idle_cycle > cycle_count);
  }
  DPRINTF("  mem_stalls:");
  for(ii = 0; ii < NUM_FUS; ii++) {
    Func_Unit* fu = &exec->fus[ii];
    if(ii % 4 == 0)
      DPRINTF(" ");
    DPRINTF("%d", fu->held_by_mem);
  }
  DPRINTF("\n");
  print_op_array(GLOBAL_DEBUG_STREAM, exec->sd.ops, NUM_FUS, NUM_FUS);
}


/**************************************************************************************/
/* exec_cycle: */

void update_exec_stage(Stage_Data* src_sd) {
  uns ii;
  ASSERT(exec->proc_id, exec->sd.op_count <= exec->sd.max_op_count);

  // {{{ phase 1 - success/failure of latching and wake up of dependent ops
  for(ii = 0; ii < src_sd->max_op_count; ii++) {
    Func_Unit* fu  = &exec->fus[ii];
    Op*        op  = src_sd->ops[ii];
    Op*        fop = exec->sd.ops[ii];
    int        latency;
    Counter    exec_cycle;

    // {{{ rejection/failure to latch cases
    if(cycle_count < fu->avail_cycle) {
      // fu not available, so nullify node stage entry to make
      //   instruction get scheduled again
      if(op != NULL) {
        op->delay_bit   = 1;
        src_sd->ops[ii] = NULL;
        src_sd->op_count--;
      }
      continue;
    }
    if(fop && fop->table_info->mem_type) {
      if(fop->replay && fop->replay_cycle == cycle_count) {
        // it's a simultaneous replay...need to kill it
        exec->sd.ops[ii] = NULL;
        exec->sd.op_count--;
        ASSERT(exec->proc_id, exec->sd.op_count >= 0);
      } else {
        // memory stall
        if(op != NULL) {
          op->delay_bit   = 1;
          src_sd->ops[ii] = NULL;
          src_sd->op_count--;
        }
        continue;
      }
    } else {
      // remove op currently in the fu
      exec->sd.op_count -= exec->sd.ops[ii] != NULL;
      ASSERT(exec->proc_id, exec->sd.op_count >= 0);
      exec->sd.ops[ii] = NULL;
    }
    if(!op)
      continue;
    // }}}

    // {{{ dependent instruction wakeup

    // if we get to here, then it means the op is going into the
    // functional unit.  We need to perform wake-ups of all the
    // dependent ops.  This is done before the actual latching of
    // the ops into the execute stage in order to make sure ops
    // flushed or replayed during the current cycle do not sneak
    // into the execute stage because they happened to be
    // processed before the op causing the recovery or replay.

    latency = op->inst_info->latency;
    ASSERTM(exec->proc_id, OP_SRCS_RDY(op), "op_num:%s\n",
            unsstr64(op->op_num));
    ASSERT(
      exec->proc_id,
      get_fu_type(op->table_info->op_type, op->table_info->is_simd) & fu->type);
    exec_cycle      = cycle_count + MAX2(latency, -latency);
    op->sched_cycle = cycle_count;

    DEBUG(exec->proc_id, "op_num:%s fu_num:%d sched_cycle:%s off_path:%d\n",
          unsstr64(op->op_num), op->fu_num, unsstr64(op->sched_cycle),
          op->off_path);
    if(op->table_info->mem_type == NOT_MEM) {
      // non-memory ops will always distribute their results
      // after the op's latency
      op->wake_cycle = exec_cycle;
      wake_up_ops(op, REG_DATA_DEP, model->wake_hook);
    } else if(op->table_info->mem_type == MEM_ST) {
      // stores have their addresses computed in this cycle and
      // also write their data into the store buffer
      if(op->exec_count == 0) {
        // only wake up if this is the first time this op executes
        op->wake_cycle = exec_cycle;
        wake_up_ops(op, MEM_ADDR_DEP, model->wake_hook);
        wake_up_ops(op, MEM_DATA_DEP, model->wake_hook);
      }
    }

    exec_stage_inc_power_stats(op);

    // all other ops (loads) will be handled by the memory system
    // }}}
  }
  // }}}

  // {{{ phase 2 - actual latching of instructions and setting of state
  for(ii = 0; ii < src_sd->max_op_count; ii++) {
    Func_Unit* fu  = &exec->fus[ii];
    Op*        op  = src_sd->ops[ii];
    Op*        fop = exec->sd.ops[ii];
    int        latency;
    Counter    exec_cycle;
    Flag       is_replay = FALSE;

    UNUSED(exec_cycle);

    if(fop) {
      // if there is still an op in the fu, the fu is still busy and there is
      // nothing to latch
      ASSERT(exec->proc_id, !op);
      STAT_EVENT(exec->proc_id, FU_BUSY_0 + ii);
      STAT_EVENT(exec->proc_id, FUS_BUSY_ON_PATH + fop->off_path);
      if(fop->table_info->mem_type) {
        fu->held_by_mem = TRUE;
        STAT_EVENT(exec->proc_id, FU_BUSY_MEM_STALL);
      }
      continue;
    }

    fu->held_by_mem = FALSE;

    if(!op) {
      STAT_EVENT(exec->proc_id, FUS_EMPTY);
      continue;  // there is nothing to latch from the previous stage
    }

    STAT_EVENT(exec->proc_id, FU_BUSY_0 + ii);
    STAT_EVENT(exec->proc_id, FUS_BUSY_ON_PATH + op->off_path);

    // remove the op from the "schedule" list
    src_sd->ops[ii] = NULL;
    src_sd->op_count--;
    ASSERT(exec->proc_id, src_sd->op_count >= 0);

    // busy the functional unit
    latency = op->inst_info->latency;
    ASSERT(0, latency);  // otherwise ready list management breaks
    exec_cycle       = cycle_count + MAX2(latency, -latency);
    exec->sd.ops[ii] = op;
    exec->sd.op_count++;
    ASSERT(exec->proc_id, exec->sd.op_count <= exec->sd.max_op_count);
    // if the op is not pipelined, then busy up the functional unit
    fu->avail_cycle = cycle_count + (latency < 0 ? -latency : 1);
    fu->idle_cycle  = cycle_count + (latency < 0 ? -latency : latency);

    // set the op's state to reflect it's execution
    if(op->table_info->mem_type == NOT_MEM || STALL_ON_WAIT_MEM) {
      op->state = OS_SCHEDULED;
    } else {
      op->state = OS_TENTATIVE;  // mem op may fail if it misses and can't get a
                                 // mem req buffer
    }
    op->exec_cycle = cycle_count + MAX2(latency, -latency);
    op->exec_count++;

    if(op->table_info->mem_type == NOT_MEM)
      op->done_cycle = op->exec_cycle;

    STAT_EVENT(op->proc_id, EXEC_ON_PATH_INST + op->off_path);
    STAT_EVENT(op->proc_id, EXEC_ON_PATH_INST_MEM +
                              (op->table_info->mem_type == NOT_MEM) +
                              2 * op->off_path);
    STAT_EVENT(op->proc_id, EXEC_ALL_INST);
    exec->fus_busy++;

    DEBUG(exec->proc_id,
          "op_num:%s fu_num:%d exec_cycle:%s done_cycle:%s off_path:%d\n",
          unsstr64(op->op_num), op->fu_num, unsstr64(op->exec_cycle),
          unsstr64(op->done_cycle), op->off_path);

    // {{{ branch recovery/resolution code
    if(op->table_info->cf_type && !is_replay) {
      // branch recovery currently does not like to be done more
      // than 1 time.  since we don't have any way to know if an
      // op is going to be replayed, we have to go with the
      // first recovery (even though it is improper) for the
      // time being

      if(!BP_UPDATE_AT_RETIRE) {
        // this code updates the branch prediction structures
        if(op->table_info->cf_type >= CF_IBR)
          bp_target_known_op(g_bp_data, op);

        bp_resolve_op(g_bp_data, op);
      }

      if(op->oracle_info.mispred || op->oracle_info.misfetch) {
        bp_sched_recovery(bp_recovery_info, op, op->exec_cycle,
                          /*late_bp_recovery=*/FALSE, /*force_offpath=*/FALSE);
        if(!op->off_path)
          op->recovery_scheduled = TRUE;
      } else if(op->table_info->cf_type >= CF_IBR &&
                op->oracle_info.no_target) {
        bp_sched_redirect(bp_recovery_info, op, op->exec_cycle);
      }
    }
    // }}}

    /* value prediction recovery/resolution code  */  // if we know the value at
                                                      // this point if not ?
                                                      // then we need to wait.
  }
  // }}}

  exec->fus_busy = 0;
  for(ii = 0; ii < src_sd->max_op_count; ii++) {
    Func_Unit* fu = &exec->fus[ii];
    // a functional unit is busy if there's an op in any stage
    // of its pipeline unless it's stalled by memory
    if(fu->idle_cycle > cycle_count && !fu->held_by_mem) {
      exec->fus_busy++;
    }
  }

  memview_fus_busy(exec->proc_id, exec->fus_busy);
}

void exec_stage_inc_power_stats(Op* op) {
  STAT_EVENT(op->proc_id, POWER_ROB_READ);
  STAT_EVENT(op->proc_id, POWER_ROB_WRITE);

  STAT_EVENT(op->proc_id, POWER_OP);

  if(op->table_info->op_type > OP_NOP && op->table_info->op_type < OP_FMEM) {
    STAT_EVENT(op->proc_id, POWER_INT_OP);
  } else if(op->table_info->op_type >= OP_FMEM) {
    STAT_EVENT(op->proc_id, POWER_FP_OP);
  }

  if(op->table_info->mem_type == MEM_LD || op->table_info->mem_type == MEM_PF) {
    STAT_EVENT(op->proc_id, POWER_LD_OP);
  } else if(op->table_info->mem_type == MEM_ST) {
    STAT_EVENT(op->proc_id, POWER_ST_OP);
  }


  if(!op->off_path) {
    STAT_EVENT(op->proc_id, POWER_COMMITTED_OP);

    if(op->table_info->op_type > OP_NOP && op->table_info->op_type < OP_FMEM) {
      STAT_EVENT(op->proc_id, POWER_COMMITTED_INT_OP);
    } else {
      STAT_EVENT(op->proc_id, POWER_COMMITTED_FP_OP);
    }
  }

  if(op->table_info->cf_type == CF_CALL ||
     op->table_info->cf_type == CF_ICALL) {
    STAT_EVENT(op->proc_id, POWER_FUNCTION_CALL);
  }

  if(op->table_info->cf_type > NOT_CF) {
    STAT_EVENT(op->proc_id, POWER_BRANCH_OP);
  }

  if(power_get_fu_type(op->table_info->op_type, op->table_info->is_simd) !=
     POWER_FU_FPU) {
    /*Integer instructions*/
    INC_STAT_EVENT(op->proc_id, POWER_RENAME_READ, 2);
    STAT_EVENT(op->proc_id, POWER_RENAME_WRITE);

    STAT_EVENT(op->proc_id, POWER_INST_WINDOW_READ);
    STAT_EVENT(op->proc_id, POWER_INST_WINDOW_WRITE);
    STAT_EVENT(op->proc_id, POWER_INST_WINDOW_WAKEUP_ACCESS);

    INC_STAT_EVENT(op->proc_id, POWER_INT_REGFILE_READ,
                   op->table_info->num_src_regs);
    INC_STAT_EVENT(op->proc_id, POWER_INT_REGFILE_WRITE,
                   op->table_info->num_dest_regs);

    if(power_get_fu_type(op->table_info->op_type, op->table_info->is_simd) ==
       POWER_FU_MUL_DIV) {
      INC_STAT_EVENT(op->proc_id, POWER_MUL_ACCESS,
                     abs(op_type_delays[op->table_info->type]));
      STAT_EVENT(op->proc_id, POWER_CDB_MUL_ACCESS);
    } else {
      INC_STAT_EVENT(op->proc_id, POWER_IALU_ACCESS,
                     abs(op_type_delays[op->table_info->type]));
      STAT_EVENT(op->proc_id, POWER_CDB_IALU_ACCESS);
    }
  } else {
    /*Floating Point instructions*/
    STAT_EVENT(op->proc_id, POWER_FP_RENAME_WRITE);
    INC_STAT_EVENT(op->proc_id, POWER_FP_RENAME_READ, 2);

    STAT_EVENT(op->proc_id, POWER_FP_INST_WINDOW_READ);
    STAT_EVENT(op->proc_id, POWER_FP_INST_WINDOW_WRITE);
    STAT_EVENT(op->proc_id, POWER_FP_INST_WINDOW_WAKEUP_ACCESS);

    INC_STAT_EVENT(op->proc_id, POWER_FP_REGFILE_READ,
                   op->table_info->num_src_regs);
    INC_STAT_EVENT(op->proc_id, POWER_FP_REGFILE_WRITE,
                   op->table_info->num_dest_regs);

    INC_STAT_EVENT(op->proc_id, POWER_FPU_ACCESS,
                   abs(op_type_delays[op->table_info->type]));
    STAT_EVENT(op->proc_id, POWER_CDB_FPU_ACCESS);
  }


  if(op->table_info->mem_type == MEM_ST) {
    STAT_EVENT(op->proc_id, POWER_DTLB_ACCESS);
  }
}
