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
 * File         : pref_ghb.c
 * Author       : HPS Research Group
 * Date         : 11/16/2004
 * Description  :
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
#include "dcache_stage.h"
#include "debug/debug.param.h"
#include "general.param.h"
#include "libs/cache_lib.h"
#include "libs/hash_lib.h"
#include "libs/list_lib.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "prefetcher/l2l1pref.h"
#include "prefetcher/pref.param.h"
#include "prefetcher/pref_common.h"
#include "prefetcher/pref_ghb.h"
#include "prefetcher/pref_ghb.param.h"
#include "statistics.h"

/* ghb_prefetcher : Global History Buffer prefetcher
 * Based on the C/DC prefetcher described in the AC/DC paper

 * Divides memory into "regions" - static partition of the address space The
 * index table is indexed by the region id and gives a pointer to the last
 * access in that region in the GHB */

/**************************************************************************************/
/* Macros */
#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_PREF_GHB, ##args)

Pref_GHB* ghb_hwp_core;
Pref_GHB* ghb_hwp;

void set_pref_ghb(Pref_GHB* new_ghb_hwp) {
  ghb_hwp = new_ghb_hwp;
}


void pref_ghb_init(HWP* hwp) {
  int  ii;
  uns8 proc_id;

  if(!PREF_GHB_ON)
    return;
  ghb_hwp_core = (Pref_GHB*)malloc(sizeof(Pref_GHB) * NUM_CORES);

  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    ghb_hwp_core[proc_id].hwp_info          = hwp->hwp_info;
    ghb_hwp_core[proc_id].hwp_info->enabled = TRUE;
    ghb_hwp_core[proc_id].index_table       = (GHB_Index_Table_Entry*)malloc(
      sizeof(GHB_Index_Table_Entry) * PREF_GHB_INDEX_N);
    ghb_hwp_core[proc_id].ghb_buffer = (GHB_Entry*)malloc(sizeof(GHB_Entry) *
                                                          PREF_GHB_BUFFER_N);

    ghb_hwp_core[proc_id].ghb_head    = -1;
    ghb_hwp_core[proc_id].ghb_tail    = -1;
    ghb_hwp_core[proc_id].deltab_size = PREF_GHB_MAX_DEGREE + 2;

    ghb_hwp_core[proc_id].delta_buffer = (int*)calloc(
      ghb_hwp_core[proc_id].deltab_size, sizeof(int));
    ghb_hwp_core[proc_id].pref_degree = PREF_GHB_DEGREE;

    for(ii = 0; ii < PREF_GHB_INDEX_N; ii++) {
      ghb_hwp_core[proc_id].index_table[ii].valid       = FALSE;
      ghb_hwp_core[proc_id].index_table[ii].last_access = 0;
    }
    for(ii = 0; ii < PREF_GHB_BUFFER_N; ii++) {
      ghb_hwp_core[proc_id].ghb_buffer[ii].ghb_ptr         = -1;
      ghb_hwp_core[proc_id].ghb_buffer[ii].ghb_reverse_ptr = -1;
      ghb_hwp_core[proc_id].ghb_buffer[ii].idx_reverse_ptr = -1;
    }

    ghb_hwp_core[proc_id].pref_degree_vals[0] = 2;
    ghb_hwp_core[proc_id].pref_degree_vals[1] = 4;
    ghb_hwp_core[proc_id].pref_degree_vals[2] = 8;
    ghb_hwp_core[proc_id].pref_degree_vals[3] = 12;
    ghb_hwp_core[proc_id].pref_degree_vals[4] = 16;
  }
}

void pref_ghb_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                          uns32 global_hist) {
  set_pref_ghb(&ghb_hwp_core[proc_id]);
  pref_ghb_ul1_train(proc_id, lineAddr, loadPC, TRUE);
}

void pref_ghb_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                       uns32 global_hist) {
  set_pref_ghb(&ghb_hwp_core[proc_id]);
  pref_ghb_ul1_train(proc_id, lineAddr, loadPC, FALSE);
}

void pref_ghb_ul1_train(uns8 proc_id, Addr lineAddr, Addr loadPC,
                        Flag ul1_hit) {
  // 1. adds address to ghb
  // 2. sends upto "degree" prefetches to the prefQ
  int ii;
  int czone_idx = -1;
  int old_ptr   = -1;

  int ghb_idx = -1;
  int delta1  = 0;
  int delta2  = 0;

  int num_pref_sent    = 0;
  int deltab_head      = -1;
  int curr_deltab_size = 0;

  Addr lineIndex     = lineAddr >> LOG2(DCACHE_LINE_SIZE);
  Addr currLineIndex = lineIndex;
  Addr index_tag     = CZONE_TAG(lineAddr);

  for(ii = 0; ii < PREF_GHB_INDEX_N; ii++) {
    if(index_tag == ghb_hwp->index_table[ii].czone_tag &&
       ghb_hwp->index_table[ii].valid) {
      // got a hit in the index table
      czone_idx = ii;
      old_ptr   = ghb_hwp->index_table[ii].ghb_ptr;
      break;
    }
  }
  if(czone_idx == -1) {
    if(ul1_hit) {  // ONLY TRAIN on ul1_hit
      return;
    }

    // Not present in index table.
    // Make new czone
    // First look if any entry is unused
    for(ii = 0; ii < PREF_GHB_INDEX_N; ii++) {
      if(!ghb_hwp->index_table[ii].valid) {
        czone_idx = ii;
        break;
      }
      if(czone_idx == -1 || (ghb_hwp->index_table[czone_idx].last_access <
                             ghb_hwp->index_table[ii].last_access)) {
        czone_idx = ii;
      }
    }
  }
  if(old_ptr != -1 && ghb_hwp->ghb_buffer[old_ptr].miss_index == lineIndex) {
    return;
  }
  if(PREF_THROTTLE_ON) {
    pref_ghb_throttle();
  }
  if(PREF_THROTTLEFB_ON) {
    pref_ghb_throttle_fb();
  }

  pref_ghb_create_newentry(czone_idx, lineAddr, index_tag, old_ptr);

  for(ii = 0; ii < ghb_hwp->deltab_size; ii++)
    ghb_hwp->delta_buffer[ii] = 0;

  // Now ghb_tail points to the new entry. Work backwards to find a 2 delta
  // match...
  ghb_idx = ghb_hwp->ghb_buffer[ghb_hwp->ghb_tail].ghb_ptr;
  DEBUG(0, "ul1hit:%d lineidx:%llx loadPC:%llx\n", ul1_hit, lineIndex, loadPC);
  while(ghb_idx != -1 && num_pref_sent < ghb_hwp->pref_degree) {
    int delta = currLineIndex - ghb_hwp->ghb_buffer[ghb_idx].miss_index;
    if(delta > 100 || delta < -100)
      break;

    // insert into delta buffer
    deltab_head = (deltab_head + 1) % ghb_hwp->deltab_size;
    ghb_hwp->delta_buffer[deltab_head] = delta;
    curr_deltab_size++;
    if(delta1 == 0) {
      delta1 = delta;
    } else if(delta2 == 0) {
      delta2 = delta;
    } else {
      DEBUG(0, "delta1:%d, delta2:%d", delta1, delta2);
      // Catch strides quickly
      if(delta1 == delta2) {
        for(; num_pref_sent < ghb_hwp->pref_degree; num_pref_sent++) {
          lineIndex += delta1;
          ASSERT(proc_id,
                 proc_id == (lineIndex >> (58 - LOG2(DCACHE_LINE_SIZE))));
          pref_addto_ul1req_queue_set(proc_id, lineIndex, ghb_hwp->hwp_info->id,
                                      0, loadPC, 0, FALSE);  // FIXME
        }
      } else {
        if(delta1 ==
             ghb_hwp->delta_buffer[(deltab_head - 1) % ghb_hwp->deltab_size] &&
           delta2 == ghb_hwp->delta_buffer[deltab_head]) {
          // found a match
          // lets go for a walk
          int deltab_idx       = (deltab_head - 2) % ghb_hwp->deltab_size;
          int deltab_start_idx = deltab_idx;
          for(; num_pref_sent < ghb_hwp->pref_degree; num_pref_sent++) {
            lineIndex += ghb_hwp->delta_buffer[deltab_idx];
            ASSERT(proc_id,
                   proc_id == (lineIndex >> (58 - LOG2(DCACHE_LINE_SIZE))));
            pref_addto_ul1req_queue_set(proc_id, lineIndex,
                                        ghb_hwp->hwp_info->id, 0, loadPC, 0,
                                        FALSE);  // FIXME
            DEBUG(0, "Sent %llx\n", lineIndex);
            deltab_idx = CIRC_DEC(deltab_idx, ghb_hwp->deltab_size);
            if(deltab_idx > curr_deltab_size) {
              deltab_idx = deltab_start_idx;
            }
          }
          break;
        }
      }
    }
    currLineIndex = ghb_hwp->ghb_buffer[ghb_idx].miss_index;
    ghb_idx       = ghb_hwp->ghb_buffer[ghb_idx].ghb_ptr;
  }
  if(num_pref_sent) {
    DEBUG(0, "Num sent %d\n", num_pref_sent);
  }
}

void pref_ghb_create_newentry(int idx, Addr line_addr, Addr czone_tag,
                              int old_ptr) {
  int rev_ptr;
  int rev_idx_ptr;

  ghb_hwp->index_table[idx].valid       = TRUE;
  ghb_hwp->index_table[idx].czone_tag   = czone_tag;
  ghb_hwp->index_table[idx].last_access = cycle_count;

  // Now make entry in ghb
  ghb_hwp->ghb_tail = (ghb_hwp->ghb_tail + 1) % PREF_GHB_BUFFER_N;
  if(ghb_hwp->ghb_tail == old_ptr) {  // takes care of some bad corner cases
    old_ptr = -1;
  }
  if(ghb_hwp->ghb_head == -1) {
    ghb_hwp->ghb_head = 0;
  } else if(ghb_hwp->ghb_tail == ghb_hwp->ghb_head) {
    // wrap-around
    ghb_hwp->ghb_head = (ghb_hwp->ghb_head + 1) % PREF_GHB_BUFFER_N;
  }

  rev_ptr     = ghb_hwp->ghb_buffer[ghb_hwp->ghb_tail].ghb_reverse_ptr;
  rev_idx_ptr = ghb_hwp->ghb_buffer[ghb_hwp->ghb_tail].idx_reverse_ptr;
  if(rev_ptr != -1) {
    ghb_hwp->ghb_buffer[rev_ptr].ghb_ptr = -1;
  }

  if(rev_idx_ptr != -1 &&
     ghb_hwp->index_table[rev_idx_ptr].ghb_ptr == ghb_hwp->ghb_tail &&
     rev_idx_ptr != idx) {
    ghb_hwp->index_table[rev_idx_ptr].ghb_ptr = -1;
    ghb_hwp->index_table[rev_idx_ptr].valid   = FALSE;
  }

  ghb_hwp->ghb_buffer[ghb_hwp->ghb_tail].miss_index = line_addr >>
                                                      LOG2(DCACHE_LINE_SIZE);
  ghb_hwp->ghb_buffer[ghb_hwp->ghb_tail].ghb_ptr         = old_ptr;
  ghb_hwp->ghb_buffer[ghb_hwp->ghb_tail].ghb_reverse_ptr = -1;
  ghb_hwp->ghb_buffer[ghb_hwp->ghb_tail].idx_reverse_ptr = idx;
  if(old_ptr != -1)
    ghb_hwp->ghb_buffer[old_ptr].ghb_reverse_ptr = ghb_hwp->ghb_tail;

  ghb_hwp->index_table[idx].ghb_ptr = ghb_hwp->ghb_tail;
}

void pref_ghb_throttle(void) {
  int dyn_shift = 0;

  float acc = pref_get_accuracy(0, ghb_hwp->hwp_info->id);  // FIXME

  if(acc != 1.0) {
    if(acc > PREF_ACC_THRESH_1) {
      dyn_shift += 2;
    } else if(acc > PREF_ACC_THRESH_2) {
      dyn_shift += 1;
    } else if(acc > PREF_ACC_THRESH_3) {
      dyn_shift = 0;
    } else if(acc > PREF_ACC_THRESH_4) {
      dyn_shift = dyn_shift - 1;
    } else {
      dyn_shift = dyn_shift - 2;
    }
  }
  // COLLECT STATS
  if(acc > 0.9) {
    STAT_EVENT(0, PREF_ACC_1);
  } else if(acc > 0.8) {
    STAT_EVENT(0, PREF_ACC_2);
  } else if(acc > 0.7) {
    STAT_EVENT(0, PREF_ACC_3);
  } else if(acc > 0.6) {
    STAT_EVENT(0, PREF_ACC_4);
  } else if(acc > 0.5) {
    STAT_EVENT(0, PREF_ACC_5);
  } else if(acc > 0.4) {
    STAT_EVENT(0, PREF_ACC_6);
  } else if(acc > 0.3) {
    STAT_EVENT(0, PREF_ACC_7);
  } else if(acc > 0.2) {
    STAT_EVENT(0, PREF_ACC_8);
  } else if(acc > 0.1) {
    STAT_EVENT(0, PREF_ACC_9);
  } else {
    STAT_EVENT(0, PREF_ACC_10);
  }

  if(acc == 1.0) {
    ghb_hwp->pref_degree = 64;
  } else {
    if(dyn_shift >= 2) {
      ghb_hwp->pref_degree = 64;
      STAT_EVENT(0, PREF_DISTANCE_5);
    } else if(dyn_shift == 1) {
      ghb_hwp->pref_degree = 32;
      STAT_EVENT(0, PREF_DISTANCE_4);
    } else if(dyn_shift == 0) {
      ghb_hwp->pref_degree = 16;
      STAT_EVENT(0, PREF_DISTANCE_3);
    } else if(dyn_shift == -1) {
      ghb_hwp->pref_degree = 8;
      STAT_EVENT(0, PREF_DISTANCE_2);
    } else if(dyn_shift <= -2) {
      ghb_hwp->pref_degree = 2;
      STAT_EVENT(0, PREF_DISTANCE_1);
    }
  }
}

void pref_ghb_throttle_fb(void) {
  pref_get_degfb(0, ghb_hwp->hwp_info->id);  // FIXME
  ASSERT(0, ghb_hwp->hwp_info->dyn_degree_core[0] >= 0 &&
              ghb_hwp->hwp_info->dyn_degree_core[0] <= 4);  // FIXME
  ghb_hwp->pref_degree =
    ghb_hwp->pref_degree_vals[ghb_hwp->hwp_info->dyn_degree_core[0]];  // FIXME
}
