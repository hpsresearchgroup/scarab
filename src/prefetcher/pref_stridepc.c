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
 * File         : pref_stride.c
 * Author       : HPS Research Group
 * Date         : 1/23/2005
 * Description  : Stride Prefetcher - Based on load's PC address
 ***************************************************************************************/

#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"

#include "globals/assert.h"
#include "globals/utils.h"
#include "op.h"

#include "core.param.h"
#include "debug/debug.param.h"
#include "general.param.h"
#include "libs/hash_lib.h"
#include "libs/list_lib.h"
#include "memory/memory.param.h"
#include "prefetcher//pref_stridepc.h"
#include "prefetcher//pref_stridepc.param.h"
#include "prefetcher/pref_common.h"
#include "statistics.h"

/*
   stride prefetcher : Stride prefetcher based on the original stride work -
   Essentially use the load's PC to index into a table of prefetch entries
*/

/**************************************************************************************/
/* Macros */
#define DEBUG(args...) _DEBUG(DEBUG_PREF_STRIDEPC, ##args)

Pref_StridePC* stridepc_hwp;
Pref_StridePC* stridepc_hwp_core;

void set_pref_stridepc(Pref_StridePC* new_stridepc) {
  stridepc_hwp = new_stridepc;
}


void pref_stridepc_init(HWP* hwp) {
  uns8 proc_id;

  if(!PREF_STRIDEPC_ON)
    return;
  hwp->hwp_info->enabled = TRUE;

  stridepc_hwp_core = (Pref_StridePC*)malloc(sizeof(Pref_StridePC) * NUM_CORES);

  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    stridepc_hwp_core[proc_id].hwp_info     = hwp->hwp_info;
    stridepc_hwp_core[proc_id].stride_table = (StridePC_Table_Entry*)calloc(
      PREF_STRIDEPC_TABLE_N, sizeof(StridePC_Table_Entry));
  }
}

void pref_stridepc_ul1_hit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                           uns32 global_hist) {
  set_pref_stridepc(&stridepc_hwp_core[proc_id]);
  pref_stridepc_ul1_train(proc_id, lineAddr, loadPC, TRUE);
}

void pref_stridepc_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                            uns32 global_hist) {
  set_pref_stridepc(&stridepc_hwp_core[proc_id]);
  pref_stridepc_ul1_train(proc_id, lineAddr, loadPC, FALSE);
}

void pref_stridepc_ul1_train(uns8 proc_id, Addr lineAddr, Addr loadPC,
                             Flag ul1_hit) {
  int ii;
  int idx = -1;

  Addr                  lineIndex = lineAddr >> LOG2(DCACHE_LINE_SIZE);
  StridePC_Table_Entry* entry     = NULL;

  int stride;

  if(loadPC == 0) {
    return;  // no point hashing on a null address
  }
  for(ii = 0; ii < PREF_STRIDEPC_TABLE_N; ii++) {
    if(stridepc_hwp->stride_table[ii].load_addr == loadPC &&
       stridepc_hwp->stride_table[ii].valid) {
      idx = ii;
      break;
    }
  }
  if(idx == -1) {
    if(ul1_hit) {  // ONLY TRAIN on hit
      return;
    }
    for(ii = 0; ii < PREF_STRIDEPC_TABLE_N; ii++) {
      if(!stridepc_hwp->stride_table[ii].valid) {
        idx = ii;
        break;
      }
      if(idx == -1 || (stridepc_hwp->stride_table[idx].last_access <
                       stridepc_hwp->stride_table[ii].last_access)) {
        idx = ii;
      }
    }
    stridepc_hwp->stride_table[idx].trained     = FALSE;
    stridepc_hwp->stride_table[idx].valid       = TRUE;
    stridepc_hwp->stride_table[idx].stride      = 0;
    stridepc_hwp->stride_table[idx].train_num   = 0;
    stridepc_hwp->stride_table[idx].pref_sent   = 0;
    stridepc_hwp->stride_table[idx].last_addr   = (PREF_STRIDEPC_USELOADADDR ?
                                                   lineAddr :
                                                   lineIndex);
    stridepc_hwp->stride_table[idx].load_addr   = loadPC;
    stridepc_hwp->stride_table[idx].last_access = cycle_count;
    return;
  }

  entry              = &stridepc_hwp->stride_table[idx];
  entry->last_access = cycle_count;
  stride = (PREF_STRIDEPC_USELOADADDR ? (lineAddr - entry->last_addr) :
                                        (lineIndex - entry->last_addr));

  if(!entry->trained) {
    // Now let's train
    if(stride == 0)
      return;
    if(entry->stride != stride) {
      entry->stride    = stride;
      entry->train_num = 1;
    } else {
      entry->train_num++;
    }
    if(entry->train_num == PREF_STRIDEPC_TRAINNUM) {
      entry->trained     = TRUE;
      entry->start_index = (PREF_STRIDEPC_USELOADADDR ? lineAddr : lineIndex);
      entry->pref_last_index = entry->start_index +
                               (PREF_STRIDEPC_STARTDIS * entry->stride);
      entry->pref_sent = 0;
    }
  } else {
    Addr pref_index;
    Addr curr_idx = (PREF_STRIDEPC_USELOADADDR ? lineAddr : lineIndex);

    if(entry->pref_sent)
      entry->pref_sent--;

    if((stride % entry->stride == 0) &&
       (((stride > 0) && (curr_idx >= entry->start_index) &&
         (curr_idx <= entry->pref_last_index)) ||
        ((stride < 0) && (curr_idx <= entry->start_index) &&
         (curr_idx >= entry->pref_last_index)))) {
      // all good. continue sending out prefetches
      for(ii = 0; (ii < PREF_STRIDEPC_DEGREE &&
                   entry->pref_sent < PREF_STRIDEPC_DISTANCE);
          ii++, entry->pref_sent++) {
        pref_index = entry->pref_last_index + entry->stride;

        ASSERT(proc_id,
               proc_id == (pref_index >> (58 - LOG2(DCACHE_LINE_SIZE))));

        if(!pref_addto_ul1req_queue(proc_id,
                                    (PREF_STRIDEPC_USELOADADDR ?
                                       (pref_index >> LOG2(DCACHE_LINE_SIZE)) :
                                       pref_index),
                                    stridepc_hwp->hwp_info->id))  // FIXME
          break;                                                  // q is full
        entry->pref_last_index = pref_index;
      }
    } else {
      // stride has changed...
      // lets retrain
      entry->trained   = FALSE;
      entry->train_num = 1;
    }
  }
  entry->last_addr = (PREF_STRIDEPC_USELOADADDR ? lineAddr : lineIndex);
}
