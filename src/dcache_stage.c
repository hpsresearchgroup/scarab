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
 * File         : dcache_stage.c
 * Author       : HPS Research Group
 * Date         : 3/8/1999
 * Description  :
 ***************************************************************************************/

#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"

#include "bp/bp.h"
#include "dcache_stage.h"
#include "map.h"
#include "model.h"

#include "core.param.h"
#include "debug/debug.param.h"
#include "memory/memory.param.h"
#include "prefetcher//stream.param.h"
#include "prefetcher/pref.param.h"
#include "prefetcher/pref_common.h"
#include "prefetcher/stream_pref.h"
#include "statistics.h"

#include "cmp_model.h"
#include "prefetcher/l2l1pref.h"

/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_DCACHE_STAGE, ##args)
#define STAGE_MAX_OP_COUNT NUM_FUS


/**************************************************************************************/
/* Global Variables */

Dcache_Stage* dc = NULL;

/**************************************************************************************/
/* set_dcache_stage: */

void set_dcache_stage(Dcache_Stage* new_dc) {
  dc = new_dc;
}


/**************************************************************************************/
/* init_dcache_stage: */

void init_dcache_stage(uns8 proc_id, const char* name) {
  uns ii;

  ASSERT(0, dc);
  DEBUG(proc_id, "Initializing %s stage\n", name);

  memset(dc, 0, sizeof(Dcache_Stage));

  dc->proc_id = proc_id;

  dc->sd.name = (char*)strdup(name);

  dc->sd.max_op_count = STAGE_MAX_OP_COUNT;
  dc->sd.ops          = (Op**)malloc(sizeof(Op*) * STAGE_MAX_OP_COUNT);

  /* initialize the cache structure */
  init_cache(&dc->dcache, "DCACHE", DCACHE_SIZE, DCACHE_ASSOC, DCACHE_LINE_SIZE,
             sizeof(Dcache_Data), DCACHE_REPL);

  reset_dcache_stage();

  dc->ports = (Ports*)malloc(sizeof(Ports) * DCACHE_BANKS);
  for(ii = 0; ii < DCACHE_BANKS; ii++) {
    char name[MAX_STR_LENGTH];
    snprintf(name, MAX_STR_LENGTH, "DCACHE BANK %d PORTS", ii);
    init_ports(&dc->ports[ii], name, DCACHE_READ_PORTS, DCACHE_WRITE_PORTS,
               FALSE);
  }

  dc->dcache.repl_pref_thresh = DCACHE_REPL_PREF_THRESH;

  if(DC_PREF_CACHE_ENABLE)
    init_cache(&dc->pref_dcache, "DC_PREF_CACHE", DC_PREF_CACHE_SIZE,
               DC_PREF_CACHE_ASSOC, DCACHE_LINE_SIZE, sizeof(Dcache_Data),
               DCACHE_REPL);

  memset(dc->rand_wb_state, 0, NUM_ELEMENTS(dc->rand_wb_state));
}


/**************************************************************************************/
/* reset_dcache_stage: */

void reset_dcache_stage(void) {
  uns ii;
  for(ii = 0; ii < STAGE_MAX_OP_COUNT; ii++)
    dc->sd.ops[ii] = NULL;
  dc->sd.op_count = 0;
  dc->idle_cycle  = 0;
}


/**************************************************************************************/
/* recover_dcache_stage: */

void recover_dcache_stage() {
  uns ii;
  for(ii = 0; ii < NUM_FUS; ii++) {
    Op* op = dc->sd.ops[ii];
    if(op && op->op_num > bp_recovery_info->recovery_op_num) {
      dc->sd.ops[ii] = NULL;
      dc->sd.op_count--;
    }
  }
  dc->idle_cycle = cycle_count + 1;
}


/**************************************************************************************/
/* debug_dcache_stage: */

void debug_dcache_stage() {
  DPRINTF("# %-10s  op_count:%d  busy: %d\n", dc->sd.name, dc->sd.op_count,
          dc->idle_cycle > cycle_count);
  print_op_array(GLOBAL_DEBUG_STREAM, dc->sd.ops, STAGE_MAX_OP_COUNT,
                 STAGE_MAX_OP_COUNT);
}

/**************************************************************************************/
/* update_dcache_stage: */
void update_dcache_stage(Stage_Data* src_sd) {
  Dcache_Data* line;
  Counter      oldest_op_num, last_oldest_op_num;
  uns          oldest_index;
  int          start_op_count;
  Addr         line_addr;
  uns          ii, jj;

  // {{{ phase 1 - move ops into the dcache stage
  ASSERT(dc->proc_id, src_sd->max_op_count == dc->sd.max_op_count);
  for(ii = 0; ii < src_sd->max_op_count; ii++) {
    Op* op    = src_sd->ops[ii];
    Op* dc_op = dc->sd.ops[ii];

    Flag stall_dc_op = dc_op &&
                       (dc_op->state == OS_WAIT_DCACHE ||
                        (STALL_ON_WAIT_MEM && dc_op->state == OS_WAIT_MEM));
    if(dc_op && !stall_dc_op) {
      // unless the op stalled getting a dcache port, it's gone
      dc->sd.ops[ii] = NULL;
      dc->sd.op_count--;
      ASSERT(dc->proc_id, dc->sd.op_count >= 0);
    }

    if(op && cycle_count < op->rdy_cycle) {
      ASSERTM(dc->proc_id, op->replay, "o:%s  rdy:%s", unsstr64(op->op_num),
              unsstr64(op->rdy_cycle));
      // op just got told to replay this cycle (clobber it)
      src_sd->ops[ii] = NULL;
      src_sd->op_count--;
      ASSERT(dc->proc_id, src_sd->op_count >= 0);
      op = NULL;
    }

    if(dc->sd.ops[ii])
      op = dc->sd.ops[ii];
    else if(!op)
      continue;
    else if(cycle_count < op->exec_cycle &&
            !(DCACHE_CYCLES == 0 && cycle_count + 1 == op->exec_cycle))
      /* this is a little screwy --- if the addr gen time is
             more than one cycle, then the op won't get cleared out
             of the exec stage, thus making it block the functional
             unit (not for the henry mem system, which handles agen
             itself) */
      /* the DCACHE_CYCLES == 0 check is to make a address + 0
         cycle cache.  This stage will grab the op out of exec a
         cycle before normal, so the wake up happens in the same
         cycle as execute */
      continue;
    else if(op->table_info->mem_type == NOT_MEM) {
      /* just squish non-memory ops */
      src_sd->ops[ii] = NULL;
      src_sd->op_count--;
      ASSERT(dc->proc_id, src_sd->op_count >= 0);
      continue;
    } else if(op->table_info->mem_type == MEM_PF && !ENABLE_SWPRF) {
      op->done_cycle  = cycle_count + DCACHE_CYCLES;
      op->state       = OS_SCHEDULED;
      src_sd->ops[ii] = NULL;
      src_sd->op_count--;
      continue;
    } else {
      /* if the op is valid, move it into the dcache stage */
      dc->sd.ops[ii] = op;
      dc->sd.op_count++;
      ASSERT(dc->proc_id, dc->sd.op_count <= dc->sd.max_op_count);
      src_sd->ops[ii] = NULL;
      src_sd->op_count--;
      ASSERT(dc->proc_id, src_sd->op_count >= 0);
    }
    ASSERTM(dc->proc_id, cycle_count >= op->exec_cycle, "o:%s  %s\n",
            unsstr64(op->op_num), Op_State_str(op->state));
  }
  // }}}

  // {{{ phase 2 - update in program order (make things easier)
  start_op_count     = dc->sd.op_count;
  last_oldest_op_num = 0;
  for(ii = 0; ii < start_op_count; ii++) {
    Op*  op;
    uns  bank;
    Flag wrongpath_dcmiss = FALSE;

    oldest_index  = 0;
    oldest_op_num = MAX_CTR;
    for(jj = 0; jj < dc->sd.max_op_count; jj++)
      if(dc->sd.ops[jj] && dc->sd.ops[jj]->op_num > last_oldest_op_num &&
         dc->sd.ops[jj]->op_num < oldest_op_num) {
        oldest_op_num = dc->sd.ops[jj]->op_num;
        oldest_index  = jj;
      }
    last_oldest_op_num = oldest_op_num;

    ASSERT(dc->proc_id, oldest_op_num < MAX_CTR);

    op = dc->sd.ops[oldest_index];

    if(op->replay && op->exec_cycle == MAX_CTR) {
      // the op is replaying, squish it
      dc->sd.ops[oldest_index] = NULL;
      dc->sd.op_count--;
      ASSERT(dc->proc_id, dc->sd.op_count >= 0);
      continue;
    }

    /* compute the bank---the bank bits are the lowest order cache index bits */
    bank = op->oracle_info.va >> dc->dcache.shift_bits &
           N_BIT_MASK(LOG2(DCACHE_BANKS));
    /* check on the availability of a read port for the given bank */
    DEBUG(dc->proc_id,
          "check_read and write port availiabilty mem_type:%s bank:%d \n",
          (op->table_info->mem_type == MEM_ST) ? "ST" : "LD", bank);
    if(!PERFECT_DCACHE && ((op->table_info->mem_type == MEM_ST &&
                            !get_write_port(&dc->ports[bank])) ||
                           (op->table_info->mem_type != MEM_ST &&
                            !get_read_port(&dc->ports[bank])))) {
      op->state = OS_WAIT_DCACHE;
      continue;
    } else {
      op->state = OS_SCHEDULED;  // memory ops are marked as scheduled so that
                                 // they can be removed from the node->rdy_list
    }

    // ideal l2 l1 prefetcher bring l1 data immediately
    if(IDEAL_L2_L1_PREFETCHER)
      ideal_l2l1_prefetcher(op);

    /* now access the dcache with it */

    line = (Dcache_Data*)cache_access(&dc->dcache, op->oracle_info.va,
                                      &line_addr, TRUE);
    op->dcache_cycle = cycle_count;
    dc->idle_cycle   = MAX2(dc->idle_cycle, cycle_count + DCACHE_CYCLES);

    if(op->table_info->mem_type == MEM_ST)
      STAT_EVENT(op->proc_id, POWER_DCACHE_WRITE_ACCESS);
    else
      STAT_EVENT(op->proc_id, POWER_DCACHE_READ_ACCESS);

    if(DC_PREF_CACHE_ENABLE && !line) {
      line = dc_pref_cache_access(op);  // if the data hits dc_pref_cache then
                                        // insert to the dcache immediately
    }

    op->oracle_info.dcmiss = FALSE;
    wrongpath_dcmiss       = FALSE;
    if(PERFECT_DCACHE) {
      if(!op->off_path) {
        STAT_EVENT(op->proc_id, DCACHE_HIT);
        STAT_EVENT(op->proc_id, DCACHE_HIT_ONPATH);
      } else
        STAT_EVENT(op->proc_id, DCACHE_HIT_OFFPATH);

      op->done_cycle = cycle_count + DCACHE_CYCLES +
                       op->inst_info->extra_ld_latency;
      if(op->table_info->mem_type != MEM_ST) {
        op->wake_cycle = op->done_cycle;
        wake_up_ops(op, REG_DATA_DEP, model->wake_hook);
      }
    } else if(line) {  // data cache hit

      if(PREF_FRAMEWORK_ON &&  // if framework is on use new prefetcher.
                               // otherwise old one
         (PREF_UPDATE_ON_WRONGPATH || !op->off_path)) {
        if(line->HW_prefetch) {
          pref_dl0_pref_hit(line_addr, op->inst_info->addr, 0);  // CHANGEME
          line->HW_prefetch = FALSE;
        } else {
          pref_dl0_hit(line_addr, op->inst_info->addr);
        }
      } else if((STREAM_TRAIN_ON_WRONGPATH ||
                 !op->off_path) &&  // old prefetcher code
                line->HW_prefetch) {
        STAT_EVENT(op->proc_id, DCACHE_PREF_HIT);
        STAT_EVENT(op->proc_id, STREAM_DCACHE_PREF_HIT);
        line->HW_prefetch = FALSE;  // not anymore prefetched data
        if(L2L1PREF_ON)
          l2l1pref_dcache(line_addr, op);
        if(STREAM_PREFETCH_ON && STREAM_PREF_INTO_DCACHE) {
          stream_dl0_hit_train(line_addr);
        }
      }

      if(L2L1PREF_ON && L2L1_DC_HIT_TRAIN) {
        l2l1pref_dcache(line_addr, op);
      }

      wp_process_dcache_hit(line, op);

      line->misc_state = (line->misc_state & 2) | op->off_path;
      if(!op->off_path) {
        STAT_EVENT(op->proc_id, DCACHE_HIT);
        STAT_EVENT(op->proc_id, DCACHE_HIT_ONPATH);
      } else
        STAT_EVENT(op->proc_id, DCACHE_HIT_OFFPATH);

      op->done_cycle = cycle_count + DCACHE_CYCLES +
                       op->inst_info->extra_ld_latency;

      if(!op->off_path) {
        line->dirty |= op->table_info->mem_type == MEM_ST;
      }
      line->read_count[op->off_path] = line->read_count[op->off_path] +
                                       (op->table_info->mem_type == MEM_LD);
      line->write_count[op->off_path] = line->write_count[op->off_path] +
                                        (op->table_info->mem_type == MEM_ST);

      if(op->table_info->mem_type != MEM_ST) {
        op->wake_cycle = op->done_cycle;
        wake_up_ops(op, REG_DATA_DEP, model->wake_hook);
      }
    } else {  // data cache miss
      if(op->table_info->mem_type == MEM_ST)
        STAT_EVENT(op->proc_id, POWER_DCACHE_WRITE_MISS);
      else
        STAT_EVENT(op->proc_id, POWER_DCACHE_READ_MISS);

      if(CACHE_STAT_ENABLE)
        dc_miss_stat(op);

      if(op->table_info->mem_type == MEM_LD) {  // load request
        if(((model->mem == MODEL_MEM) &&
            scan_stores(
              op->oracle_info.va,
              op->oracle_info.mem_size))) {  // scan the store forwarding buffer
          if(!op->off_path) {
            STAT_EVENT(op->proc_id, DCACHE_ST_BUFFER_HIT);
            STAT_EVENT(op->proc_id, DCACHE_ST_BUFFER_HIT_ONPATH);
          } else
            STAT_EVENT(op->proc_id, DCACHE_ST_BUFFER_HIT_OFFPATH);
          op->done_cycle = cycle_count + DCACHE_CYCLES +
                           op->inst_info->extra_ld_latency;
          op->wake_cycle = cycle_count + DCACHE_CYCLES +
                           op->inst_info->extra_ld_latency;
          wake_up_ops(op, REG_DATA_DEP, model->wake_hook);
        } else if(((model->mem == MODEL_MEM) &&
                   new_mem_req(
                     MRT_DFETCH, dc->proc_id, line_addr, DCACHE_LINE_SIZE,
                     DCACHE_CYCLES - 1 + op->inst_info->extra_ld_latency, op,
                     dcache_fill_line, op->unique_num, 0))) {
          if(PREF_UPDATE_ON_WRONGPATH || !op->off_path) {
            pref_dl0_miss(line_addr, op->inst_info->addr);
          }

          if(ONE_MORE_CACHE_LINE_ENABLE) {
            Addr         one_more_addr;
            Addr         extra_line_addr;
            Dcache_Data* extra_line;

            one_more_addr = ((line_addr >> LOG2(DCACHE_LINE_SIZE)) & 1) ?
                              ((line_addr >> LOG2(DCACHE_LINE_SIZE)) - 1)
                                << LOG2(DCACHE_LINE_SIZE) :
                              ((line_addr >> LOG2(DCACHE_LINE_SIZE)) + 1)
                                << LOG2(DCACHE_LINE_SIZE);

            extra_line = (Dcache_Data*)cache_access(&dc->dcache, one_more_addr,
                                                    &extra_line_addr, FALSE);
            ASSERT(dc->proc_id, one_more_addr == extra_line_addr);
            if(!extra_line) {
              if(new_mem_req(
                   MRT_DFETCH, dc->proc_id, extra_line_addr, DCACHE_LINE_SIZE,
                   DCACHE_CYCLES - 1 + op->inst_info->extra_ld_latency, NULL,
                   NULL, op->unique_num, 0))
                STAT_EVENT_ALL(ONE_MORE_SUCESS);
              else
                STAT_EVENT_ALL(ONE_MORE_DISCARDED_MEM_REQ_FULL);
            } else
              STAT_EVENT_ALL(ONE_MORE_DISCARDED_L0CACHE);
          }

          if(!op->off_path) {
            STAT_EVENT(op->proc_id, DCACHE_MISS);
            STAT_EVENT(op->proc_id, DCACHE_MISS_ONPATH);
            STAT_EVENT(op->proc_id, DCACHE_MISS_LD_ONPATH);
            op->oracle_info.dcmiss = TRUE;
            STAT_EVENT(op->proc_id, DCACHE_MISS_LD);
          } else {
            wrongpath_dcmiss = TRUE;
            STAT_EVENT(op->proc_id, DCACHE_MISS_OFFPATH);
            STAT_EVENT(op->proc_id, DCACHE_MISS_LD_OFFPATH);
          }
          op->state              = OS_MISS;
          op->engine_info.dcmiss = TRUE;
        } else {
          op->state = OS_WAIT_MEM;  // go into this state if no miss buffer is
                                    // available
          cmp_model.node_stage[dc->proc_id].mem_blocked = TRUE;
          mem->uncores[dc->proc_id].mem_block_start     = freq_cycle_count(
            FREQ_DOMAIN_L1);
          STAT_EVENT(op->proc_id, DCACHE_MISS_WAITMEM);
        }
      } else if(op->table_info->mem_type == MEM_PF ||
                op->table_info->mem_type == MEM_WH) {
        // prefetches don't scan the store buffer

        if(((model->mem == MODEL_MEM) &&
            new_mem_req(MRT_DPRF, dc->proc_id, line_addr, DCACHE_LINE_SIZE,
                        DCACHE_CYCLES - 1 + op->inst_info->extra_ld_latency, op,
                        dcache_fill_line, op->unique_num, 0))) {
          if(ONE_MORE_CACHE_LINE_ENABLE) {
            Addr         one_more_addr;
            Addr         extra_line_addr;
            Dcache_Data* extra_line;

            one_more_addr = ((line_addr >> LOG2(DCACHE_LINE_SIZE)) & 1) ?
                              ((line_addr >> LOG2(DCACHE_LINE_SIZE)) - 1)
                                << LOG2(DCACHE_LINE_SIZE) :
                              ((line_addr >> LOG2(DCACHE_LINE_SIZE)) + 1)
                                << LOG2(DCACHE_LINE_SIZE);

            extra_line = (Dcache_Data*)cache_access(&dc->dcache, one_more_addr,
                                                    &extra_line_addr, FALSE);
            ASSERT(dc->proc_id, one_more_addr == extra_line_addr);
            if(!extra_line) {
              if(new_mem_req(
                   MRT_DPRF, dc->proc_id, extra_line_addr, DCACHE_LINE_SIZE,
                   DCACHE_CYCLES - 1 + op->inst_info->extra_ld_latency, NULL,
                   NULL, op->unique_num, 0))
                STAT_EVENT_ALL(ONE_MORE_SUCESS);
              else
                STAT_EVENT_ALL(ONE_MORE_DISCARDED_MEM_REQ_FULL);
            } else
              STAT_EVENT_ALL(ONE_MORE_DISCARDED_L0CACHE);
          }

          if(!op->off_path) {
            STAT_EVENT(op->proc_id, DCACHE_MISS);
            STAT_EVENT(op->proc_id, DCACHE_MISS_ONPATH);
            STAT_EVENT(op->proc_id, DCACHE_MISS_LD_ONPATH);
            op->oracle_info.dcmiss = TRUE;
            STAT_EVENT(op->proc_id, DCACHE_MISS_LD);
          } else {
            wrongpath_dcmiss = TRUE;
            STAT_EVENT(op->proc_id, DCACHE_MISS_OFFPATH);
            STAT_EVENT(op->proc_id, DCACHE_MISS_LD_OFFPATH);
          }
          op->state = OS_MISS;
          if(PREFS_DO_NOT_BLOCK_WINDOW || op->table_info->mem_type == MEM_PF) {
            op->done_cycle = cycle_count + DCACHE_CYCLES +
                             op->inst_info->extra_ld_latency;
            op->state = OS_SCHEDULED;
          }
        } else {
          op->state = OS_WAIT_MEM;  // go into this state if no miss buffer is
                                    // available
          cmp_model.node_stage[dc->proc_id].mem_blocked = TRUE;
          mem->uncores[dc->proc_id].mem_block_start     = freq_cycle_count(
            FREQ_DOMAIN_L1);
          STAT_EVENT(op->proc_id, DCACHE_MISS_WAITMEM);
        }
      } else {  // store request
        ASSERT(dc->proc_id, op->table_info->mem_type == MEM_ST);

        if(((model->mem == MODEL_MEM) &&
            new_mem_req(MRT_DSTORE, dc->proc_id, line_addr, DCACHE_LINE_SIZE,
                        DCACHE_CYCLES - 1 + op->inst_info->extra_ld_latency, op,
                        dcache_fill_line, op->unique_num, 0))) {
          if(ONE_MORE_CACHE_LINE_ENABLE) {
            Addr         one_more_addr;
            Addr         extra_line_addr;
            Dcache_Data* extra_line;

            one_more_addr = ((line_addr >> LOG2(DCACHE_LINE_SIZE)) & 1) ?
                              ((line_addr >> LOG2(DCACHE_LINE_SIZE)) - 1)
                                << LOG2(DCACHE_LINE_SIZE) :
                              ((line_addr >> LOG2(DCACHE_LINE_SIZE)) + 1)
                                << LOG2(DCACHE_LINE_SIZE);

            extra_line = (Dcache_Data*)cache_access(&dc->dcache, one_more_addr,
                                                    &extra_line_addr, FALSE);
            ASSERT(dc->proc_id, one_more_addr == extra_line_addr);
            if(!extra_line) {
              if(new_mem_req(
                   MRT_DFETCH, dc->proc_id, extra_line_addr, DCACHE_LINE_SIZE,
                   DCACHE_CYCLES - 1 + op->inst_info->extra_ld_latency, NULL,
                   NULL, op->unique_num, 0))
                STAT_EVENT_ALL(ONE_MORE_SUCESS);
              else
                STAT_EVENT_ALL(ONE_MORE_DISCARDED_MEM_REQ_FULL);
            } else
              STAT_EVENT_ALL(ONE_MORE_DISCARDED_L0CACHE);
          }

          if(!op->off_path) {
            STAT_EVENT(op->proc_id, DCACHE_MISS);
            STAT_EVENT(op->proc_id, DCACHE_MISS_ONPATH);
            STAT_EVENT(op->proc_id, DCACHE_MISS_ST_ONPATH);
            op->oracle_info.dcmiss = TRUE;
            STAT_EVENT(op->proc_id, DCACHE_MISS_ST);
          } else {
            wrongpath_dcmiss = TRUE;
            STAT_EVENT(op->proc_id, DCACHE_MISS_OFFPATH);
            STAT_EVENT(op->proc_id, DCACHE_MISS_ST_OFFPATH);
          }
          op->state = OS_MISS;
          if(STORES_DO_NOT_BLOCK_WINDOW) {
            op->done_cycle = cycle_count + DCACHE_CYCLES +
                             op->inst_info->extra_ld_latency;
            op->state = OS_SCHEDULED;
          }
        } else {
          op->state                                     = OS_WAIT_MEM;
          cmp_model.node_stage[dc->proc_id].mem_blocked = TRUE;
          mem->uncores[dc->proc_id].mem_block_start     = freq_cycle_count(
            FREQ_DOMAIN_L1);
          STAT_EVENT(op->proc_id, DCACHE_MISS_WAITMEM);
        }
      }
    }

    if(STREAM_PREFETCH_ON &&
       ((op->oracle_info.dcmiss == TRUE) ||
        (STREAM_TRAIN_ON_WRONGPATH && (wrongpath_dcmiss == TRUE)))) {
      _DEBUG(dc->proc_id, DEBUG_STREAM_MEM,
             "dl0 miss : line_addr :%d op_count %lld  type :%d\n",
             (int)line_addr, op->op_num, (int)op->table_info->mem_type);
      stream_dl0_miss(line_addr);
    }
  }
  // }}}
  /* prefetcher update */
  if(STREAM_PREFETCH_ON)
    update_pref_queue();
  if(L2WAY_PREF && !L1PREF_IMMEDIATE)
    update_l2way_pref_req_queue();
  if(L2MARKV_PREF_ON && !L1MARKV_PREF_IMMEDIATE)
    update_l2markv_pref_req_queue();
}


/**************************************************************************************/
/* dcache_fill_line: */

Flag dcache_fill_line(Mem_Req* req) {
  uns bank = req->addr >> dc->dcache.shift_bits &
             N_BIT_MASK(LOG2(DCACHE_BANKS));
  Dcache_Data* data;
  Addr         line_addr, repl_line_addr;
  Op*          op;
  Op**         op_p  = (Op**)list_start_head_traversal(&req->op_ptrs);
  Counter* op_unique = (Counter*)list_start_head_traversal(&req->op_uniques);

  set_dcache_stage(&cmp_model.dcache_stage[req->proc_id]);
  Counter old_cycle_count = cycle_count;  // FIXME HACK!
  cycle_count             = freq_cycle_count(FREQ_DOMAIN_CORES[req->proc_id]);

  ASSERT(dc->proc_id, dc->proc_id == req->proc_id);
  ASSERT(dc->proc_id, req->op_count == req->op_ptrs.count);
  ASSERT(dc->proc_id, req->op_count == req->op_uniques.count);

  /* if it can't get a write port, fail */
  if(!get_write_port(&dc->ports[bank])) {
    cycle_count = old_cycle_count;
    return FAILURE;
  }

  /* get new line in the cache */
  if(DC_PREF_CACHE_ENABLE &&
     ((USE_CONFIRMED_OFF ? req->off_path_confirmed : req->off_path) ||
      (req->type == MRT_DPRF))) {  // Add prefetches here
    DEBUG(dc->proc_id,
          "Filling pref_dcache off_path:%d addr:0x%s  :%7d index:%7d "
          "op_count:%d oldest:%lld\n",
          req->off_path, hexstr64s(req->addr), (int)req->addr,
          (int)(req->addr >> LOG2(DCACHE_LINE_SIZE)), req->op_count,
          (req->op_count ? req->oldest_op_unique_num : -1));

    data = (Dcache_Data*)cache_insert(&dc->pref_dcache, dc->proc_id, req->addr,
                                      &line_addr, &repl_line_addr);
    // mark the data as HW_prefetch if prefetch mark it as
    // fetched_by_offpath if off_path this is done downstairs
  } else {
    /* Do not insert the line yet, just check which line we
       need to replace. If that line is dirty, it's possible
       that we won't be able to insert the writeback into the
       memory system. */
    Flag repl_line_valid;
    data = (Dcache_Data*)get_next_repl_line(&dc->dcache, dc->proc_id, req->addr,
                                            &repl_line_addr, &repl_line_valid);
    if(repl_line_valid && data->dirty) {
      /* need to do a write-back */
      uns repl_proc_id = get_proc_id_from_cmp_addr(repl_line_addr);
      DEBUG(dc->proc_id, "Scheduling writeback of addr:0x%s\n",
            hexstr64s(repl_line_addr));
      ASSERT(dc->proc_id, data->read_count[0] || data->read_count[1] ||
                            data->write_count[0] || data->write_count[1]);

      ASSERT(dc->proc_id,
             repl_line_addr || data->fetched_by_offpath || data->HW_prefetched);
      if(!new_mem_dc_wb_req(MRT_WB, repl_proc_id, repl_line_addr,
                            DCACHE_LINE_SIZE, 1, NULL, NULL, unique_count,
                            TRUE)) {
        // This is a hack to get around a deadlock issue. It doesn't completely
        // eliminate the deadlock, but makes it less likely...The deadlock
        // occurs when all the mem_req buffers are used, and all pending
        // mem_reqs need to fill the dcache, but the highest priority dcache
        // fill ends up evicting a dirty line from the dcache, which then needs
        // to be written back to L1/MLC. This dcache fill will aquire a write
        // port via get_write_port(), but then fail here, because there are no
        // more mem_req buffers available for dc wb req, and new_mem_dc_wb_req()
        // will return FALSE. If we don't release the write port, then all other
        // mem_reqs, which still need to fill the dcache, will fail, and we end
        // up in a deadlock. So instead, we release the write port below.
        // HOWEVER, a deadlock is still possible if all pending mem_reqs fill
        // the dcache and all end up evicting a dirty line
        ASSERT(dc->proc_id, 0 < dc->ports[bank].write_ports_in_use);
        dc->ports[bank].write_ports_in_use--;
        ASSERT(dc->proc_id,
               dc->ports[bank].write_ports_in_use < dc->ports->num_write_ports);

        cycle_count = old_cycle_count;
        return FAILURE;
      }
      STAT_EVENT(dc->proc_id, DCACHE_WB_REQ_DIRTY);
      STAT_EVENT(dc->proc_id, DCACHE_WB_REQ);
    }

    data = (Dcache_Data*)cache_insert(&dc->dcache, dc->proc_id, req->addr,
                                      &line_addr, &repl_line_addr);
    DEBUG(dc->proc_id,
          "Filling dcache  off_path:%d addr:0x%s  :%7d index:%7d op_count:%d "
          "oldest:%lld\n",
          req->off_path, hexstr64s(req->addr), (int)req->addr,
          (int)(req->addr >> LOG2(DCACHE_LINE_SIZE)), req->op_count,
          (req->op_count ? req->oldest_op_unique_num : -1));
    STAT_EVENT(dc->proc_id, DCACHE_FILL);
  }

  /* set up dcache line fields */
  data->dirty              = req->dirty_l0 ? TRUE : FALSE;
  data->prefetch           = TRUE;
  data->read_count[0]      = 0;
  data->read_count[1]      = 0;
  data->write_count[0]     = 0;
  data->write_count[1]     = 0;
  data->misc_state         = req->off_path | req->off_path << 1;
  data->fetched_by_offpath = USE_CONFIRMED_OFF ? req->off_path_confirmed :
                                                 req->off_path;
  data->offpath_op_addr   = req->oldest_op_addr;
  data->offpath_op_unique = req->oldest_op_unique_num;
  data->fetch_cycle       = cycle_count;
  data->onpath_use_cycle  = (req->type == MRT_DPRF || req->off_path) ?
                             0 :
                             cycle_count;

  wp_process_dcache_fill(data, req);

  if(req->type == MRT_DPRF) {  // cmp FIXME
    data->HW_prefetch   = TRUE;
    data->HW_prefetched = TRUE;
  } else {
    data->HW_prefetch   = FALSE;
    data->HW_prefetched = FALSE;
  }

  for(; op_p; op_p = (Op**)list_next_element(&req->op_ptrs)) {
    ASSERT(dc->proc_id, op_unique);
    op = *op_p;
    ASSERT(dc->proc_id, op);


    if(op->unique_num == *op_unique && op->op_pool_valid) {
      ASSERT(dc->proc_id, dc->proc_id == op->proc_id);
      ASSERT(dc->proc_id, op->proc_id == req->proc_id);
      if(!op->off_path && op->table_info->mem_type == MEM_ST)
        ASSERT(dc->proc_id, data->dirty);
      data->prefetch &= op->table_info->mem_type == MEM_PF ||
                        op->table_info->mem_type == MEM_WH;
      data->read_count[op->off_path] = data->read_count[op->off_path] +
                                       (op->table_info->mem_type == MEM_LD);
      data->write_count[op->off_path] = data->write_count[op->off_path] +
                                        (op->table_info->mem_type == MEM_ST);
      DEBUG(dc->proc_id, "%s: %s line addr:0x%s: %7d\n", unsstr64(op->op_num),
            disasm_op(op, FALSE), hexstr64s(req->addr),
            (int)(req->addr >> LOG2(DCACHE_LINE_SIZE)));
    }

    if(op->unique_num == *op_unique && op->op_pool_valid) {
      DEBUG(dc->proc_id, "Awakening op_num:%lld %d %d\n", op->op_num,
            op->engine_info.l1_miss_satisfied, op->in_rdy_list);
      ASSERT(dc->proc_id, !op->in_rdy_list);

      op->done_cycle = cycle_count + 1;
      op->state      = OS_SCHEDULED;

      if(op->table_info->mem_type != MEM_ST) {
        op->wake_cycle = op->done_cycle;
        wake_up_ops(op, REG_DATA_DEP,
                    model->wake_hook); /* wake up dependent ops */
      }
    }

    op_unique = (Counter*)list_next_element(&req->op_uniques);
  }

  /* This write_count is missing all the stores that retired before
     this fill happened. Still, we know at least one on-path write
     must have occurred if the line is dirty. */
  if(data->dirty && data->write_count[0] == 0)
    data->write_count[0] = 1;

  ASSERT(dc->proc_id, data->read_count[0] || data->read_count[1] ||
                        data->write_count[0] || data->write_count[1] ||
                        req->off_path || data->prefetch || data->HW_prefetch);

  cycle_count = old_cycle_count;
  return SUCCESS;
}


/**************************************************************************************/
/* do_oracle_dcache_access: */

Flag do_oracle_dcache_access(Op* op, Addr* line_addr) {
  Dcache_Data* hit;
  hit = (Dcache_Data*)cache_access(&dc->dcache, op->oracle_info.va, line_addr,
                                   FALSE);

  if(hit)
    return TRUE;
  else
    return FALSE;
}

/**************************************************************************************/
/* wp_process_dcache_hit: */

void wp_process_dcache_hit(Dcache_Data* line, Op* op) {
  L1_Data* l1_line;

  if(!line) {
    ASSERT(dc->proc_id, PERFECT_DCACHE);
    return;
  }

  if(!WP_COLLECT_STATS)
    return;

  if(!op->off_path) {
    if(line->fetched_by_offpath) {
      STAT_EVENT(dc->proc_id, DCACHE_HIT_ONPATH_SAT_BY_OFFPATH);
      STAT_EVENT(dc->proc_id, DCACHE_USE_OFFPATH);
      STAT_EVENT(dc->proc_id, DIST_DCACHE_FILL_OFFPATH_USED);
      STAT_EVENT(dc->proc_id, DIST_REQBUF_OFFPATH_USED);
      STAT_EVENT(dc->proc_id, DIST2_REQBUF_OFFPATH_USED_FULL);

      l1_line = do_l1_access(op);
      if(l1_line) {
        if(l1_line->fetched_by_offpath) {
          STAT_EVENT(dc->proc_id, L1_USE_OFFPATH);
          STAT_EVENT(dc->proc_id, DIST_L1_FILL_OFFPATH_USED);
          STAT_EVENT(dc->proc_id, L1_USE_OFFPATH_DATA);
          l1_line->fetched_by_offpath             = FALSE;
          l1_line->l0_modified_fetched_by_offpath = TRUE;
        }
      }

      DEBUG(0,
            "Dcache hit: On path hits off path. va:%s op:%s op:0x%s wp_op:0x%s "
            "opu:%s wpu:%s dist:%s%s\n",
            hexstr64s(op->oracle_info.va), disasm_op(op, TRUE),
            hexstr64s(op->inst_info->addr), hexstr64s(line->offpath_op_addr),
            unsstr64(op->unique_num), unsstr64(line->offpath_op_unique),
            op->unique_num > line->offpath_op_unique ? " " : "-",
            op->unique_num > line->offpath_op_unique ?
              unsstr64(op->unique_num - line->offpath_op_unique) :
              unsstr64(line->offpath_op_unique - op->unique_num));
    } else {
      STAT_EVENT(dc->proc_id, DCACHE_HIT_ONPATH_SAT_BY_ONPATH);
      STAT_EVENT(dc->proc_id, DCACHE_USE_ONPATH);
    }
  } else {
    if(line->fetched_by_offpath) {
      STAT_EVENT(dc->proc_id, DCACHE_HIT_OFFPATH_SAT_BY_OFFPATH);
    } else {
      STAT_EVENT(dc->proc_id, DCACHE_HIT_OFFPATH_SAT_BY_ONPATH);
    }
  }

  if(!op->off_path)
    line->fetched_by_offpath = FALSE;
}

/**************************************************************************************/
/* wp_process_dcache_fill: */

void wp_process_dcache_fill(Dcache_Data* line, Mem_Req* req) {
  if(!WP_COLLECT_STATS)
    return;

  if((req->type == MRT_WB) || (req->type == MRT_WB_NODIRTY) ||
     (req->type == MRT_DPRF)) /* for now we don't consider prefetches */
    return;

  if(req->off_path) {
    switch(req->type) {
      case MRT_DFETCH:
      case MRT_DSTORE:
        STAT_EVENT(dc->proc_id, DCACHE_FILL_OFFPATH);
        STAT_EVENT(dc->proc_id, DIST_DCACHE_FILL);
        break;
      default:
        break;
    }
  } else {
    switch(req->type) {
      case MRT_DFETCH:
      case MRT_DSTORE:
        STAT_EVENT(dc->proc_id, DCACHE_FILL_ONPATH);
        STAT_EVENT(dc->proc_id, DIST_DCACHE_FILL);
        if(req->onpath_match_offpath)
          STAT_EVENT(dc->proc_id, DIST_DCACHE_FILL_ONPATH_PARTIAL);
        else
          STAT_EVENT(dc->proc_id, DIST_DCACHE_FILL_ONPATH);
        break;
      default:
        break;
    }
  }
}
