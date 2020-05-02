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
 * Description  : Stride Prefetcher - Based on RPT Prefetcher ( ICS'04 )
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
#include "prefetcher//pref_stride.h"
#include "prefetcher//pref_stride.param.h"
#include "prefetcher/pref.param.h"
#include "prefetcher/pref_common.h"
#include "statistics.h"

/*
   stride prefetcher : Stride prefetcher based on Abraham's ICS'04 paper
   - "Effective Stream-Based and Execution-Based Data Prefetching"

   Divides memory in regions and then does multi-stride prefetching
*/

/**************************************************************************************/
/* Macros */
#define DEBUG(args...) _DEBUG(DEBUG_PREF_STRIDE, ##args)

Pref_Stride* stride_hwp;

void pref_stride_init(HWP* hwp) {
  if(!PREF_STRIDE_ON)
    return;

  ASSERTM(0, PREF_REPORT_PREF_MATCH_AS_HIT || PREF_REPORT_PREF_MATCH_AS_MISS,
          "Stride prefetcher must train on demands matching prefetch request "
          "buffers\n");

  stride_hwp               = (Pref_Stride*)malloc(sizeof(Pref_Stride));
  stride_hwp->hwp_info     = hwp->hwp_info;
  hwp->hwp_info->enabled   = TRUE;
  stride_hwp->region_table = (Stride_Region_Table_Entry*)calloc(
    PREF_STRIDE_TABLE_N, sizeof(Stride_Region_Table_Entry));
  stride_hwp->index_table = (Stride_Index_Table_Entry*)calloc(
    PREF_STRIDE_TABLE_N, sizeof(Stride_Index_Table_Entry));
}

void pref_stride_ul1_hit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                         uns32 global_hist) {
  pref_stride_ul1_train(lineAddr, loadPC, TRUE);
}

void pref_stride_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                          uns32 global_hist) {
  pref_stride_ul1_train(lineAddr, loadPC, FALSE);
}

void pref_stride_ul1_train(Addr lineAddr, Addr loadPC, Flag ul1_hit) {
  int ii;
  int region_idx = -1;

  Addr                      lineIndex = lineAddr >> LOG2(DCACHE_LINE_SIZE);
  Addr                      index_tag = STRIDE_REGION(lineAddr);
  Stride_Index_Table_Entry* entry     = NULL;

  int stride;

  for(ii = 0; ii < PREF_STRIDE_TABLE_N; ii++) {
    if(index_tag == stride_hwp->region_table[ii].tag &&
       stride_hwp->region_table[ii].valid) {
      // got a hit in the region table
      region_idx = ii;
      break;
    }
  }
  if(region_idx == -1) {
    if(ul1_hit) {  // DONT CREATE ON HIT
      return;
    }
    // Not present in region table.
    // Make new region
    // First look if any entry is unused
    for(ii = 0; ii < PREF_STRIDE_TABLE_N; ii++) {
      if(!stride_hwp->region_table[ii].valid) {
        region_idx = ii;
        break;
      }
      if(region_idx == -1 || (stride_hwp->region_table[region_idx].last_access <
                              stride_hwp->region_table[ii].last_access)) {
        region_idx = ii;
      }
    }
    pref_stride_create_newentry(region_idx, lineAddr, index_tag);
    return;
  }

  entry  = &stride_hwp->index_table[region_idx];
  stride = lineIndex - stride_hwp->index_table[region_idx].last_index;
  stride_hwp->index_table[region_idx].last_index   = lineIndex;
  stride_hwp->region_table[region_idx].last_access = cycle_count;

  if(!entry->trained) {
    // Now let's train

    if(!entry->train_count_mode) {
      if(entry->stride[entry->curr_state] == 0) {
        entry->stride[entry->curr_state] = stride;
        entry->s_cnt[entry->curr_state]  = 1;
      } else if(entry->stride[entry->curr_state] == stride) {
        entry->stride[entry->curr_state] = stride;
        entry->s_cnt[entry->curr_state]++;
      } else {
        if(PREF_STRIDE_SINGLE_STRIDE_MODE) {
          entry->stride[entry->curr_state] = stride;
          entry->s_cnt[entry->curr_state]  = 1;  // CORRECT ---
        } else {
          // in mult-stride mode
          // new stride -> Maybe it is a transition:
          entry->strans[entry->curr_state] = stride;
          if(entry->num_states == 1) {
            entry->num_states = 2;
          }
          entry->curr_state = (1 -
                               entry->curr_state);  // change 0 to 1 or 1 to 0
          if(entry->curr_state == 0) {
            entry->train_count_mode = TRUE;  // move into a checking mode
            entry->count            = 0;
            entry->recnt            = 0;
          }
        }
      }
    } else {
      // in train_count_mode
      if(stride == entry->stride[entry->curr_state] &&
         entry->count < entry->s_cnt[entry->curr_state]) {
        entry->recnt++;
        entry->count++;
      } else if(stride == entry->strans[entry->curr_state] &&
                entry->count == entry->s_cnt[entry->curr_state]) {
        entry->recnt++;
        entry->count      = 0;
        entry->curr_state = (1 - entry->curr_state);
      } else {
        // does not match... lets reset.
        pref_stride_create_newentry(region_idx, lineAddr, index_tag);
      }
    }
    if((entry->s_cnt[entry->curr_state] >= PREF_STRIDE_SINGLE_THRESH)) {
      // single stride stream
      entry->trained         = TRUE;
      entry->num_states      = 1;
      entry->curr_state      = 0;
      entry->stride[0]       = entry->stride[entry->curr_state];
      entry->pref_last_index = entry->last_index +
                               (entry->stride[0] * PREF_STRIDE_STARTDISTANCE);
    }
    if(entry->recnt >= PREF_STRIDE_MULTI_THRESH) {
      Addr pref_index;
      entry->trained         = TRUE;
      entry->pref_count      = entry->count;
      entry->pref_curr_state = entry->curr_state;
      entry->pref_last_index = entry->last_index;
      for(ii = 0; (ii < PREF_STRIDE_STARTDISTANCE); ii++) {
        if(entry->pref_count == entry->s_cnt[entry->pref_curr_state]) {
          pref_index = entry->pref_last_index +
                       entry->strans[entry->pref_curr_state];
          entry->pref_count      = 0;
          entry->pref_curr_state = (1 - entry->pref_curr_state);
        } else {
          pref_index = entry->pref_last_index +
                       entry->stride[entry->pref_curr_state];
          entry->pref_count++;
        }
        entry->pref_last_index = pref_index;
      }
    }
  } else {
    Addr pref_index;
    // entry is trained
    if(entry->pref_sent)
      entry->pref_sent--;
    if(entry->num_states == 1 && stride == entry->stride[0]) {
      // single stride case
      for(ii = 0;
          (ii < PREF_STRIDE_DEGREE && entry->pref_sent < PREF_STRIDE_DISTANCE);
          ii++, entry->pref_sent++) {
        pref_index = entry->pref_last_index + entry->stride[0];
        if(!pref_addto_ul1req_queue(0, pref_index,
                                    stride_hwp->hwp_info->id))  // FIXME
          break;                                                // q is full
        entry->pref_last_index = pref_index;
      }
    } else if((stride == entry->stride[entry->curr_state] &&
               entry->count < entry->s_cnt[entry->curr_state]) ||
              (stride == entry->strans[entry->curr_state] &&
               entry->count == entry->s_cnt[entry->curr_state])) {
      // first update verification info.
      if(entry->count == entry->s_cnt[entry->curr_state]) {
        entry->count      = 0;
        entry->curr_state = (1 - entry->curr_state);
      } else {
        entry->count++;
      }
      // now send out prefetches
      for(ii = 0;
          (ii < PREF_STRIDE_DEGREE && entry->pref_sent < PREF_STRIDE_DISTANCE);
          ii++, entry->pref_sent++) {
        if(entry->pref_count == entry->s_cnt[entry->pref_curr_state]) {
          pref_index = entry->pref_last_index +
                       entry->strans[entry->pref_curr_state];
          if(!pref_addto_ul1req_queue(0, pref_index,
                                      stride_hwp->hwp_info->id))  // FIXME
            break;                                                // q is full
          entry->pref_count      = 0;
          entry->pref_curr_state = (1 - entry->pref_curr_state);
        } else {
          pref_index = entry->pref_last_index +
                       entry->stride[entry->pref_curr_state];
          if(!pref_addto_ul1req_queue(0, pref_index,
                                      stride_hwp->hwp_info->id))  // FIXME
            break;                                                // q is full
          entry->pref_count++;
        }
        entry->pref_last_index = pref_index;
      }
    } else {
      // does not match...
      entry->trained          = FALSE;
      entry->train_count_mode = FALSE;
      entry->num_states       = 1;
      entry->curr_state       = 0;
      entry->stride[0]        = 0;
      entry->stride[1]        = 0;
      entry->s_cnt[0]         = 0;
      entry->s_cnt[1]         = 0;
      entry->strans[0]        = 0;
      entry->strans[1]        = 0;

      entry->count     = 0;
      entry->recnt     = 0;
      entry->pref_sent = 0;
    }
  }
}

void pref_stride_create_newentry(int idx, Addr line_addr, Addr region_tag) {
  stride_hwp->region_table[idx].tag         = region_tag;
  stride_hwp->region_table[idx].valid       = TRUE;
  stride_hwp->region_table[idx].last_access = cycle_count;

  stride_hwp->index_table[idx].trained    = FALSE;
  stride_hwp->index_table[idx].num_states = 1;
  stride_hwp->index_table[idx].curr_state = 0;  // 0 or 1
  stride_hwp->index_table[idx].last_index = line_addr >> LOG2(DCACHE_LINE_SIZE);
  stride_hwp->index_table[idx].stride[0]  = 0;
  stride_hwp->index_table[idx].s_cnt[0]   = 0;
  stride_hwp->index_table[idx].stride[1]  = 0;
  stride_hwp->index_table[idx].s_cnt[1]   = 0;

  stride_hwp->index_table[idx].strans[0] = 0;
  stride_hwp->index_table[idx].strans[1] = 0;

  stride_hwp->index_table[idx].recnt            = 0;
  stride_hwp->index_table[idx].count            = 0;
  stride_hwp->index_table[idx].train_count_mode = FALSE;
  stride_hwp->index_table[idx].pref_sent        = 0;
}
