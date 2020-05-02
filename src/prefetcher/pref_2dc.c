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
 * File         : pref_2dc.c
 * Author       : HPS Research Group
 * Date         : 1/19/2006
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
#include "prefetcher/pref_2dc.h"
#include "prefetcher/pref_2dc.param.h"
#include "prefetcher/pref_common.h"
#include "statistics.h"

/*
   2dc_prefetcher : 2 delta-correlation prefetcher

   O.k... So far 2-delta correlation prefetchers have just gone for
   the basic approach - 2-d table. So we are implementing a cache like
   table which can achieve most of the benefits from a much smaller
   structure.

   Implementation -> Take the deltas and the PC, and the address and
   come up with a hash function that works. Use this to access the
   cache.
*/

/**************************************************************************************/
/* Macros */
#define DEBUG(args...) _DEBUG(DEBUG_PREF_2DC, ##args)

Pref_2DC* tdc_hwp;

void pref_2dc_init(HWP* hwp) {
  if(!PREF_2DC_ON)
    return;

  tdc_hwp                    = (Pref_2DC*)malloc(sizeof(Pref_2DC));
  tdc_hwp->hwp_info          = hwp->hwp_info;
  tdc_hwp->hwp_info->enabled = TRUE;

  tdc_hwp->regions = (Pref_2DC_Region*)calloc(PREF_2DC_NUM_REGIONS,
                                              sizeof(Pref_2DC_Region));

  tdc_hwp->last_access = 0;
  tdc_hwp->last_loadPC = 0;

  init_cache(&tdc_hwp->cache, "PREF_2DC_CACHE", PREF_2DC_CACHE_SIZE,
             PREF_2DC_CACHE_ASSOC, PREF_2DC_CACHE_LINE_SIZE,
             sizeof(Pref_2DC_Cache_Data), REPL_TRUE_LRU);

  tdc_hwp->cache_index_bits = LOG2(PREF_2DC_CACHE_SIZE / 4);
  tdc_hwp->hash_func        = PREF_2DC_HASH_FUNC_DEFAULT;
  tdc_hwp->pref_degree      = PREF_2DC_DEGREE;
}

void pref_2dc_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                          uns32 global_hist) {
  pref_2dc_ul1_train(lineAddr, loadPC, TRUE);  // FIXME
}

void pref_2dc_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                       uns32 global_histC) {
  pref_2dc_ul1_train(lineAddr, loadPC, FALSE);  // FIXME
}

void pref_2dc_ul1_train(Addr lineAddr, Addr loadPC, Flag ul1_hit) {
  int              delta;
  Addr             hash;
  Addr             lineIndex = lineAddr >> LOG2(DCACHE_LINE_SIZE);
  Addr             dummy_lineaddr;
  Pref_2DC_Region* region =
    &tdc_hwp
       ->regions[(lineIndex >> PREF_2DC_ZONE_SHIFT) % PREF_2DC_REGION_HASH];

  if(tdc_hwp->last_access != 0) {
    delta = lineIndex - tdc_hwp->last_access;
    if(delta == 0) {  // no point updating if we have the same address twice
      return;
    }
    // update state of the cache for deltaA, deltaB
    // no point inserting same deltas in. so if deltaA == deltaB and deltaB ==
    // delta then dont insert.
    if(region->deltaA != 0 && region->deltaB != 0 &&
       (!(region->deltaA == region->deltaB && region->deltaB == delta))) {
      hash = pref_2dc_hash(tdc_hwp->last_access, tdc_hwp->last_loadPC,
                           region->deltaA, region->deltaB);
      Pref_2DC_Cache_Data* data = cache_access(&tdc_hwp->cache, hash,
                                               &dummy_lineaddr, TRUE);
      if(!data) {
        Addr repl_addr;
        if(!ul1_hit) {  // insert only on miss
          data = cache_insert(&tdc_hwp->cache, 0, hash, &dummy_lineaddr,
                              &repl_addr);
        } else {
          return;
        }
      }
      data->delta = delta;
    }
    region->deltaC = region->deltaB;
    region->deltaB = region->deltaA;
    region->deltaA = delta;
  }
  tdc_hwp->last_access = lineIndex;
  tdc_hwp->last_loadPC = loadPC;

  if(region->deltaA == 0 || region->deltaB == 0) {
    return;
  }  // No useful deltas yet

  {
    // Send out prefetches
    Pref_2DC_Cache_Data* data;
    uns                  num_pref_sent = 0;
    int                  delta1        = region->deltaB;
    int                  delta2        = region->deltaA;

    if(region->deltaA == region->deltaB && region->deltaB == region->deltaC) {
      // Now just assume that this is a strided access and send out the next
      // few.
      for(; num_pref_sent < tdc_hwp->pref_degree; num_pref_sent++) {
        lineIndex += region->deltaA;
        pref_addto_ul1req_queue_set(0, lineIndex, tdc_hwp->hwp_info->id, 0,
                                    loadPC, 0, FALSE);  // FIXME
      }
    }
    while(num_pref_sent < tdc_hwp->pref_degree) {
      hash = pref_2dc_hash(lineIndex, loadPC, delta1, delta2);
      data = cache_access(&tdc_hwp->cache, hash, &dummy_lineaddr, TRUE);
      if(!data) {  // no hit for this hash
        return;
      }
      lineIndex += data->delta;

      delta1 = delta2;
      delta2 = data->delta;

      pref_addto_ul1req_queue_set(0, lineIndex, tdc_hwp->hwp_info->id, 0,
                                  loadPC, 0, FALSE);  // FIXME
      num_pref_sent++;
    }
  }
}

void pref_2dc_throttle(void) {
  int dyn_shift = 0;

  float acc = pref_get_accuracy(0, tdc_hwp->hwp_info->id);

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
    tdc_hwp->pref_degree = 64;
  } else {
    if(dyn_shift >= 2) {
      tdc_hwp->pref_degree = 64;
      STAT_EVENT(0, PREF_DISTANCE_5);
    } else if(dyn_shift == 1) {
      tdc_hwp->pref_degree = 32;
      STAT_EVENT(0, PREF_DISTANCE_4);
    } else if(dyn_shift == 0) {
      tdc_hwp->pref_degree = 16;
      STAT_EVENT(0, PREF_DISTANCE_3);
    } else if(dyn_shift == -1) {
      tdc_hwp->pref_degree = 8;
      STAT_EVENT(0, PREF_DISTANCE_2);
    } else if(dyn_shift <= -2) {
      tdc_hwp->pref_degree = 2;
      STAT_EVENT(0, PREF_DISTANCE_1);
    }
  }
}

Addr pref_2dc_hash(Addr lineIndex, Addr loadPC, int deltaA, int deltaB) {
  Addr res = 0;
  uns  cache_indexbitsA;
  uns  cache_indexbitsB;
  uns  tagbits;

  switch(tdc_hwp->hash_func) {
    case PREF_2DC_HASH_FUNC_DEFAULT:
      // In this function, we just use the lower bits from each delta
      // to form the hash.
      cache_indexbitsA = tdc_hwp->cache_index_bits >> 1;
      cache_indexbitsB = tdc_hwp->cache_index_bits - cache_indexbitsA;

      tagbits = (((deltaA >> cache_indexbitsA) ^ (deltaB >> cache_indexbitsB) ^
                  (lineIndex >> PREF_2DC_ZONE_SHIFT)) &
                 N_BIT_MASK(PREF_2DC_TAG_SIZE));

      res = (((deltaA & N_BIT_MASK(cache_indexbitsA)) |
              ((deltaB & N_BIT_MASK(cache_indexbitsB)) << cache_indexbitsA)) |
             (tagbits << tdc_hwp->cache_index_bits));
      break;
    default:
      ASSERT(0, 0);
      break;
  }
  return res;
}
