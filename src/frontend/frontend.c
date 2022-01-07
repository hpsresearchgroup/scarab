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
 * File         : frontend.c
 * Author       : HPS Research Group
 * Date         : 10/26/2011
 * Description  : Interface for an external frontend.
 ***************************************************************************************/

#include "frontend.h"
#include "bp/bp.h"
#include "core.param.h"
#include "frontend_intf.h"
#include "general.param.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_vars.h"
#include "icache_stage.h"
#include "op.h"
#include "pin_exec_driven_fe.h"
#include "pin_trace_fe.h"
#include "sim.h"
#include "statistics.h"
#include "thread.h"

#ifdef ENABLE_MEMTRACE
#include "frontend/memtrace/memtrace_fe.h"
#endif

/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_FRONTEND, ##args)


static void collect_op_stats(Op* op);

extern int op_type_delays[];

void frontend_init() {
  ASSERT(0, ST_OP_INV + NUM_OP_TYPES == ST_NOT_CF);
  frontend_intf_init();

  switch(FRONTEND) {
    case FE_PIN_EXEC_DRIVEN: {
      pin_exec_driven_init(NUM_CORES);
      break;
    }
    case FE_TRACE: {
      trace_init();
      break;
    }
#ifdef ENABLE_MEMTRACE
    case FE_MEMTRACE: {
      memtrace_init();
      break;
    }
#endif
    default:
      ASSERT(0, 0);
      break;
  }
}

void frontend_done(Flag* retired_exit) {
  switch(FRONTEND) {
    case FE_PIN_EXEC_DRIVEN: {
      pin_exec_driven_done(retired_exit);
      break;
    }
    case FE_TRACE: {
      trace_done();
      break;
    }
#ifdef ENABLE_MEMTRACE
    case FE_MEMTRACE: {
      memtrace_done();
      break;
    }
#endif
    default:
      ASSERT(0, 0);
      break;
  }
}

Addr frontend_next_fetch_addr(uns proc_id) {
  return frontend->next_fetch_addr(proc_id);
}

Flag frontend_can_fetch_op(uns proc_id) {
  return frontend->can_fetch_op(proc_id);
}

void frontend_fetch_op(uns proc_id, Op* op) {
  frontend->fetch_op(proc_id, op);
  collect_op_stats(op);
}

void frontend_redirect(uns proc_id, uns64 inst_uid, Addr fetch_addr) {
  DEBUG(proc_id, "Redirect after op_num %lld to 0x%08llx\n",
        op_count[proc_id] - 1, fetch_addr);
  frontend->redirect(proc_id, inst_uid, fetch_addr);
}

void frontend_recover(uns proc_id, uns64 inst_uid) {
  DEBUG(proc_id, "Recover after inst_uid %lld\n", inst_uid);

  /* Recover to correct path */
  frontend->recover(proc_id, inst_uid);
}

void frontend_retire(uns proc_id, uns64 inst_uid) {
  DEBUG(proc_id, "Retiring inst_uid %lld\n", inst_uid);

  /* Recover to correct path */
  frontend->retire(proc_id, inst_uid);
  DEBUG(proc_id, "Retiring inst_uid %lld end\n", inst_uid);
}

static void collect_op_stats(Op* op) {
  if(!ic || !ic->off_path) {
    STAT_EVENT(op->proc_id, ST_OP_ONPATH);
    if(op->eom)
      STAT_EVENT(op->proc_id, ST_INST_ONPATH);
    STAT_EVENT(op->proc_id, ST_OP_INV + op->table_info->op_type);
    STAT_EVENT(op->proc_id, ST_NOT_CF + op->table_info->cf_type);
    STAT_EVENT(op->proc_id, ST_BAR_NONE + op->table_info->bar_type);
    STAT_EVENT(op->proc_id, ST_NOT_MEM + op->table_info->mem_type);
  } else {
    STAT_EVENT(op->proc_id, ST_OP_OFFPATH);
    STAT_EVENT(op->proc_id,
               ST_FAKE_REASON_NOT_FAKE + op->inst_info->fake_inst_reason);
    if(op->inst_info->fake_inst) {
      STAT_EVENT(op->proc_id, ST_FAKE_OP_OFFPATH);
    } else {
      STAT_EVENT(op->proc_id, ST_NOT_FAKE_OP_OFFPATH);
    }
    if(op->eom)
      STAT_EVENT(op->proc_id, ST_INST_OFFPATH);
    STAT_EVENT(op->proc_id, ST_NOT_MEM_OFFPATH + op->table_info->mem_type);
  }
}
/*************************************************************/
