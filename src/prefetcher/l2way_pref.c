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
 * File         : l2way_pref.c
 * Author       : HPS Research Group
 * Date         : 03/10/2004
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
#include "debug/debug.param.h"
#include "general.param.h"
#include "libs/cache_lib.h"
#include "libs/hash_lib.h"
#include "libs/list_lib.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "prefetcher/l2l1pref.h"
#include "statistics.h"

/**************************************************************************************/
/* Macros */
#define DEBUG(args...) _DEBUG(DEBUG_WAY, ##args)

/**************************************************************************************/
/* Global Variables */

extern Memory*       mem;
extern Dcache_Stage* dc;

/***************************************************************************************/
/* Local Prototypes */

L2way_Rec**    l2way_table;
L1pref_Req*    l1pref_req_queue;
static Counter l1pref_send_no;
static Counter l1pref_req_no;
static Cache*  l1_cache;

/**************************************************************************************/

static inline uns cache_index_l(Cache* cache, Addr addr, Addr* tag,
                                Addr* line_addr) {
  *line_addr = addr & ~cache->offset_mask;
  *tag       = addr >> cache->shift_bits & cache->tag_mask;
  return addr >> cache->shift_bits & cache->set_mask;
}

void l2way_init(void) {
  uns num_sets = L1_SIZE / L1_LINE_SIZE;
  uns assoc    = L1_ASSOC;

  uns ii;
  l2way_table = (L2way_Rec**)malloc(sizeof(L2way_Rec*) * num_sets);

  for(ii = 0; ii < num_sets; ii++) {
    l2way_table[ii] = (L2way_Rec*)malloc(sizeof(L2way_Rec) * assoc);
  }
  if(!L1PREF_IMMEDIATE)
    l1pref_req_queue = (L1pref_Req*)malloc(sizeof(L1pref_Req) *
                                           L1PREF_REQ_QUEUE_SIZE);

  if(model->mem == MODEL_MEM) {
    ASSERTM(0, !PRIVATE_L1, "L2 Way Prefetcher assumes shared L1\n");
    l1_cache = &mem->uncores[0].l1->cache;
  }
}

void l2way_pref(Mem_Req_Info* req) {
  if(req->type == MRT_WB)
    return;  // Don't train for the write back
  l2way_pref_train(req);
  l2way_pref_pred(req);
}


void l2way_pref_train(Mem_Req_Info* req) {
  Addr   tag;
  Addr   line_addr;
  Addr   addr     = req->addr;
  Cache* cache    = l1_cache;
  uns    set      = cache_index_l(cache, addr, &tag, &line_addr);
  uns    prev_way = l2way_table[set][0].last_way;
#define INIT_WAY 99999
  uns     current_way = INIT_WAY;
  uns     ii;
  Counter l2_access_interval;
  for(ii = 0; ii < cache->assoc; ii++) {
    Cache_Entry* line = &cache->entries[set][ii];

    if(line->tag == tag && line->valid) {
      current_way = ii;
    }
  }
  if(current_way == INIT_WAY)
    return;
  if(l2way_table[set][prev_way].pred_way == current_way) {
    if(l2way_table[set][prev_way].counter < 3)
      l2way_table[set][prev_way].counter++;
    STAT_EVENT(0, L2WAY_WAY_HIT);
  } else {
    l2way_table[set][prev_way].pred_way = current_way;
    l2way_table[set][prev_way].counter  = 0;
    STAT_EVENT(0, L2WAY_WAY_MISS);
  }
  /* record the order of the way */ /* A->B */
  l2_access_interval = cycle_count - l2way_table[set][0].last_access_time;
  if(l2_access_interval) {
    if(l2_access_interval < 10)
      STAT_EVENT(0, L2_ACCESS_INTERVAL__0);
    else if(l2_access_interval < 100)
      STAT_EVENT(0, L2_ACCESS_INTERVAL__1);
    else if(l2_access_interval < 1000)
      STAT_EVENT(0, L2_ACCESS_INTERVAL__2);
    else if(l2_access_interval < 10000)
      STAT_EVENT(0, L2_ACCESS_INTERVAL__3);
    else if(l2_access_interval < 100000)
      STAT_EVENT(0, L2_ACCESS_INTERVAL__4);
    else if(l2_access_interval < 1000000)
      STAT_EVENT(0, L2_ACCESS_INTERVAL__5);
    else if(l2_access_interval < 10000000)
      STAT_EVENT(0, L2_ACCESS_INTERVAL__6);
    else if(l2_access_interval < 100000000)
      STAT_EVENT(0, L2_ACCESS_INTERVAL__7);
    else
      STAT_EVENT(0, L2_ACCESS_INTERVAL__8);
  }
  l2way_table[set][0].last_access_time = cycle_count;
  l2way_table[set][0].last_way         = current_way;
}

void l2way_pref_pred(Mem_Req_Info* req) {
  Addr   tag;
  Addr   line_addr;
  Addr   addr  = req->addr;
  Cache* cache = l1_cache;
  uns    set   = cache_index_l(cache, addr, &tag, &line_addr);
  uns    ii;
  uns    current_way = INIT_WAY;

  for(ii = 0; ii < cache->assoc; ii++) {
    Cache_Entry* line = &cache->entries[set][ii];

    if(line->tag == tag && line->valid) {
      current_way = ii;
    }
  }
  if(current_way == INIT_WAY)
    return;
  if(req->type == MRT_WB)
    return;  // Don't prefetch for the write back

  if(l2way_table[set][current_way].counter > 2) {
    uns  fetch_way = l2way_table[set][current_way].pred_way;
    Addr tag       = l1_cache->entries[set][fetch_way].tag;
    Addr va        = line_addr | (tag << cache->shift_bits);
    Addr line_addr, repl_line_addr;

    if(L2L1_IMMEDIATE_PREF_CACHE && DC_PREF_CACHE_ENABLE) {
      dc_pref_cache_insert(va);
      STAT_EVENT(0, L2WAY_PREF_REQ);
    } else if(L1PREF_IMMEDIATE) {
      Dcache_Data *data, *line;
      line = (Dcache_Data*)cache_access(&dc->dcache, va, &line_addr, FALSE);
      if(!line) {
        data = (Dcache_Data*)cache_insert(&dc->dcache, dc->proc_id, va,
                                          &line_addr, &repl_line_addr);
        if(data->dirty) {
          FATAL_ERROR(0,
                      "This writeback code is wrong. Writebacks may be lost.");
        }
        data->HW_prefetch = TRUE;
        STAT_EVENT(0, L2WAY_PREF_REQ);
        STAT_EVENT(0, L2WAY_PREF_HIT_DATA_REQ);
      } else
        STAT_EVENT(0, L2WAY_PREF_HIT_DATA_IN_CACHE);
    } else {
      insert_l2way_pref_req(va, cycle_count + (Counter)L1WAY_PREF_TIMER_DIS);
    }
    STAT_EVENT(0, L2WAY_TRAIN_HIT);
  } else
    STAT_EVENT(0, L2WAY_TRAIN_MISS);
}


void insert_l2way_pref_req(Addr va, Counter time) {
  l1pref_req_queue[l1pref_req_no % L1PREF_REQ_QUEUE_SIZE].valid = TRUE;
  l1pref_req_queue[l1pref_req_no % L1PREF_REQ_QUEUE_SIZE].time  = time;
  l1pref_req_queue[l1pref_req_no++ % L1PREF_REQ_QUEUE_SIZE].va  = va;
  DEBUG(0, "[%s]insert va:%s time:%s req_no:%s send_no:%s \n",
        unsstr64(cycle_count), hexstr64(va), unsstr64(time),
        unsstr64(l1pref_req_no), unsstr64(l1pref_send_no));
}

void update_l2way_pref_req_queue(void) {
  uns ii;
  if(L2L1_IMMEDIATE_PREF_CACHE)
    return;

  for(ii = 0; ii < L1WAY_PREF_SEND_QUEUE; ii++) {
    if(l1pref_req_queue[l1pref_send_no % L1PREF_REQ_QUEUE_SIZE]
         .valid) {  // req is valid

      if(l1pref_req_queue[l1pref_send_no % L1PREF_REQ_QUEUE_SIZE].time <=
         cycle_count) {  // timer is expired

        Dcache_Data *data, *line;
        Addr         line_addr, repl_line_addr;
        Addr         req_va =
          l1pref_req_queue[l1pref_send_no % L1PREF_REQ_QUEUE_SIZE].va;
        uns bank = req_va >> dc->dcache.shift_bits &
                   N_BIT_MASK(LOG2(DCACHE_BANKS));

        if(get_read_port(&dc->ports[bank]) &&
           get_write_port(&dc->ports[bank])) {  // get ports

          line = (Dcache_Data*)cache_access(&dc->dcache, req_va, &line_addr,
                                            FALSE);
          if(!line) {
            data = (Dcache_Data*)cache_insert(&dc->dcache, dc->proc_id, req_va,
                                              &line_addr, &repl_line_addr);
            if(data->dirty) {
              FATAL_ERROR(
                0, "This writeback code is wrong. Writebacks may be lost.");
            }
            data->HW_prefetch = TRUE;
            STAT_EVENT(0, L2WAY_PREF_REQ);
            STAT_EVENT(0, L2WAY_PREF_HIT_DATA_REQ);
          } else {
            STAT_EVENT(0, L2WAY_PREF_HIT_DATA_IN_CACHE);
          }

          l1pref_req_queue[l1pref_send_no++ % L1PREF_REQ_QUEUE_SIZE].valid =
            FALSE;
          STAT_EVENT(0, L2WAY_L1INSERT_PORT_READY);
        } else {
          STAT_EVENT(0, L2WAY_L1INSERT_PORT_FULL);
          break;
        }

      }  // if timer is setted
    }    // if valid
  }      // end for
}
