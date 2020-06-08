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
 * File         : l2markv_pref.c
 * Author       : HPS Research Group
 * Date         : 03/17/2004
 * Description  : markov prefetcher for L2 to L1
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
#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_L2MARKV, ##args)

/**************************************************************************************/
/* Global Variables */

extern Dcache_Stage* dc;

/***************************************************************************************/
/* Local Prototypes */

extern Memory* mem;
L2markv_Rec**  l2markv_table;
L1pref_Req*    l1pref_markv_req_queue;
L1pref_Req*    markv_l2send_req_queue;
static Counter l1pref_markv_send_no;
static Counter l1pref_markv_req_no;
static Counter markv_l2access_req_no;
static Counter markv_l2access_send_no;
static uns     last_set;
static uns     last_way;
static Addr    last_markv_addr;

/***************************************************************************************/

static inline uns cache_index_l(Cache* cache, Addr addr, Addr* tag,
                                Addr* line_addr) {
  *line_addr = addr & ~cache->offset_mask;
  *tag       = addr >> cache->shift_bits & cache->tag_mask;
  return addr >> cache->shift_bits & cache->set_mask;
}

void l2markv_init(void) {
  uns num_sets = L1_SIZE / L1_LINE_SIZE;
  uns assoc    = L1_ASSOC;

  uns ii;
  l2markv_table = (L2markv_Rec**)malloc(sizeof(L2markv_Rec*) * num_sets);

  for(ii = 0; ii < num_sets; ii++) {
    l2markv_table[ii] = (L2markv_Rec*)malloc(sizeof(L2markv_Rec) * assoc);
  }

  if(!L1MARKV_PREF_IMMEDIATE) {
    l1pref_markv_req_queue = (L1pref_Req*)malloc(sizeof(L1pref_Req) *
                                                 L1PREF_MARKV_REQ_QUEUE_SIZE);
    markv_l2send_req_queue = (L1pref_Req*)malloc(sizeof(L1pref_Req) *
                                                 L1PREF_MARKV_REQ_QUEUE_SIZE);
  }
}

void l2markv_pref(Mem_Req_Info* req, int* train_hit, int* pref_req,
                  Addr* req_addr) {
  *train_hit = l2markv_pref_train(req);
  *pref_req  = l2markv_pref_pred(req, req_addr);
  STAT_EVENT(0, L2MARKV_PREF_TRAIN);
}

int l2markv_pref_train(Mem_Req_Info* req) {
  Addr   tag;
  Addr   line_addr;
  Addr   addr  = req->addr;
  Cache* cache = &mem->uncores[req->proc_id].l1->cache;

  uns     set = cache_index_l(cache, addr, &tag, &line_addr);
  uns     ii;
  Counter time_diff;
  int     train_hit = FALSE;
#define INIT_WAY 99999
  uns current_way = INIT_WAY;
  for(ii = 0; ii < cache->assoc; ii++) {
    Cache_Entry* line = &cache->entries[set][ii];
    if(line->tag == tag && line->valid) {
      current_way = ii;
    }
  }
  if(current_way == INIT_WAY)
    return FALSE;
  // training for the next addr ( for future prefetching )

  if(l2markv_table[last_set][last_way].next_addr == addr) {
    if(l2markv_table[last_set][last_way].next_addr_counter < 3)
      l2markv_table[last_set][last_way].next_addr_counter++;
    STAT_EVENT(0, L2MARKV_NEXT_ADDR_HIT);
    train_hit = TRUE;
    DEBUG(0,
          "train_hit:va:0x%s last_addr:0x%s last_set:%d last_way:%d "
          "current_set:%d current_way:%d counter:%d\n",
          hexstr64(addr), hexstr64(last_markv_addr), last_set, last_way, set,
          current_way, l2markv_table[last_set][last_way].next_addr_counter);
  } else {
    DEBUG(0,
          "train_miss:va:0x%s last_addr:0x%s last_set:%d last_way:%d "
          "current_set:%d current_way:%d old_counter:%d\n",
          hexstr64(addr), hexstr64(last_markv_addr), last_set, last_way, set,
          current_way, l2markv_table[last_set][last_way].next_addr_counter);

    l2markv_table[last_set][last_way].next_addr         = addr;
    l2markv_table[last_set][last_way].next_addr_counter = 1;
    STAT_EVENT(0, L2MARKV_NEXT_ADDR_MISS);
  }

  // training for the last addr ( It will be used later. increase the order of
  // the markov preftecher )
  if(l2markv_table[set][current_way].last_addr == last_markv_addr) {
    l2markv_table[set][current_way].last_addr_counter++;
    STAT_EVENT(0, L2MARKV_LAST_ADDR_HIT);
  } else {
    l2markv_table[set][current_way].last_addr_counter = 0;
    l2markv_table[set][current_way].last_addr         = last_markv_addr;
    STAT_EVENT(0, L2MARKV_LAST_ADDR_MISS);
  }
  l2markv_table[set][current_way].last_access_time = cycle_count;
  time_diff = l2markv_table[last_set][last_way].time_diff =
    cycle_count - l2markv_table[last_set][last_way].last_access_time;

  if(time_diff < 10)
    STAT_EVENT(0, MARKV_L2_TIME_DIFF__0);
  else if(time_diff < 100)
    STAT_EVENT(0, MARKV_L2_TIME_DIFF__1);
  else if(time_diff < 1000)
    STAT_EVENT(0, MARKV_L2_TIME_DIFF__2);
  else if(time_diff < 10000)
    STAT_EVENT(0, MARKV_L2_TIME_DIFF__3);
  else if(time_diff < 100000)
    STAT_EVENT(0, MARKV_L2_TIME_DIFF__4);
  else if(time_diff < 1000000)
    STAT_EVENT(0, MARKV_L2_TIME_DIFF__5);
  else if(time_diff < 10000000)
    STAT_EVENT(0, MARKV_L2_TIME_DIFF__6);
  else if(time_diff < 100000000)
    STAT_EVENT(0, MARKV_L2_TIME_DIFF__7);
  else
    STAT_EVENT(0, MARKV_L2_TIME_DIFF__8);

  last_set        = set;
  last_way        = current_way;
  last_markv_addr = addr;
  return train_hit;
}

int l2markv_pref_pred(Mem_Req_Info* req, Addr* req_addr) {
  uns set         = last_set;
  uns current_way = last_way;  // last_way is set at l2markv_pref_train function
  int pref_req    = FALSE;

  if(l2markv_table[set][current_way].next_addr_counter > L1MARKV_REQ_TH) {
    Addr req_va = l2markv_table[set][current_way].next_addr;
    Addr line_addr, repl_line_addr;

    if(L2L1_IMMEDIATE_PREF_CACHE && DC_PREF_CACHE_ENABLE) {
      dc_pref_cache_insert(req_va);
      STAT_EVENT(0, L2MARKV_PREF_REQ);
    } else if(L1MARKV_PREF_IMMEDIATE) {
      Dcache_Data *data, *line;
      line = (Dcache_Data*)cache_access(&dc->dcache, req_va, &line_addr, FALSE);
      if(!line) {
        data = (Dcache_Data*)cache_insert(&dc->dcache, dc->proc_id, req_va,
                                          &line_addr, &repl_line_addr);
        if(data->dirty) {
          FATAL_ERROR(0,
                      "This writeback code is wrong. Writebacks may be lost.");
        }
        data->HW_prefetch = TRUE;
        STAT_EVENT(0, L2MARKV_PREF_REQ);
        STAT_EVENT(0, L2MARKV_PREF_HIT_DATA_REQ);
        pref_req = TRUE;
      } else
        STAT_EVENT(0, L2MARKV_PREF_HIT_DATA_IN_CACHE);
    } else {
      insert_l2markv_pref_req(req_va,
                              cycle_count + (Counter)L1MARKV_PREF_TIMER_DIS);
      pref_req = TRUE;
    }
    DEBUG(0,
          "pred_hit:va:0x%s pred_addr:0x%s current_set:%d current_way:%d "
          "counter:%d\n",
          hexstr64(req->addr), hexstr64(req_va), set, current_way,
          l2markv_table[set][current_way].next_addr_counter);

    *req_addr = req_va;
  } else {
    DEBUG(0, "pred_miss:va:0x%s current_set:%d current_way:%d counter:%d\n",
          hexstr64(req->addr), set, current_way,
          l2markv_table[set][current_way].next_addr_counter);
    STAT_EVENT(0, L2MARKV_PREF_MISS);
  }

  return pref_req;
}


void insert_l2markv_pref_req(Addr va, Counter time) {
  l1pref_markv_req_queue[l1pref_markv_req_no % L1PREF_MARKV_REQ_QUEUE_SIZE]
    .valid = TRUE;
  l1pref_markv_req_queue[l1pref_markv_req_no % L1PREF_MARKV_REQ_QUEUE_SIZE]
    .time = time;
  l1pref_markv_req_queue[l1pref_markv_req_no++ % L1PREF_MARKV_REQ_QUEUE_SIZE]
    .va = va;
  DEBUG(0, "[%s]insert va:%s time:%s req_no:%s send_no:%s \n",
        unsstr64(cycle_count), hexstr64(va), unsstr64(time),
        unsstr64(l1pref_markv_req_no), unsstr64(l1pref_markv_send_no));
}

void update_l2markv_pref_req_queue(void) {
  uns  ii;
  Flag dcache_read_port_full = FALSE;
  Flag buffer_empty          = FALSE;

  if(L2L1_IMMEDIATE_PREF_CACHE)
    return;

  while(!(dcache_read_port_full || buffer_empty)) {
    if(l1pref_markv_req_queue[l1pref_markv_send_no %
                              L1PREF_MARKV_REQ_QUEUE_SIZE]
         .valid) {  // req is valid

      if(l1pref_markv_req_queue[l1pref_markv_send_no %
                                L1PREF_MARKV_REQ_QUEUE_SIZE]
           .time <= cycle_count) {  // timer is expired

        Dcache_Data* line;
        Addr         line_addr;
        int  q_index = l1pref_markv_send_no % L1PREF_MARKV_REQ_QUEUE_SIZE;
        Addr req_va  = l1pref_markv_req_queue[q_index].va;
        uns  bank    = req_va >> dc->dcache.shift_bits &
                   N_BIT_MASK(LOG2(DCACHE_BANKS));

        if(get_read_port(&dc->ports[bank]) &&
           get_write_port(&dc->ports[bank])) {  // get ports
          // !!! we need to check whether the data is in the L1 cache (second
          // level cache or not!!!!)
          line = (Dcache_Data*)cache_access(&dc->dcache, req_va, &line_addr,
                                            FALSE);
          if(!line) {
            markv_l2send_req_queue[markv_l2access_req_no %
                                   MARKV_L2ACCESS_REQ_Q_SIZE]
              .va = req_va;
            markv_l2send_req_queue[markv_l2access_req_no %
                                   MARKV_L2ACCESS_REQ_Q_SIZE]
              .rdy_cycle = cycle_count + DCACHE_CYCLES;
            markv_l2send_req_queue[markv_l2access_req_no++ %
                                   MARKV_L2ACCESS_REQ_Q_SIZE]
              .valid = TRUE;
            DEBUG(0,
                  "dcache_check_dcache_miss:[%d]line_addr:0x%s "
                  "dcache_check_send_no:%lld dcache_check_req_no:%lld "
                  "l2check_send_no:%lld l2check_req_no:%lld \n",
                  q_index, hexstr64(req_va), l1pref_markv_send_no,
                  l1pref_markv_req_no, markv_l2access_send_no,
                  markv_l2access_req_no);
            STAT_EVENT(0, L2MARKV_PREF_HIT_DATA_REQ);
          } else {
            STAT_EVENT(0, L2MARKV_PREF_HIT_DATA_IN_CACHE);
            DEBUG(0,
                  "dcache_check_dcache_hit:[%d]line_addr:0x%s "
                  "dcache_check_send_no:%lld dcache_check_req_no:%lld "
                  "l2check_send_no:%lld l2check_req_no:%lld \n",
                  q_index, hexstr64(req_va), l1pref_markv_send_no,
                  l1pref_markv_req_no, markv_l2access_send_no,
                  markv_l2access_req_no);
          }

          l1pref_markv_req_queue[l1pref_markv_send_no++ %
                                 L1PREF_MARKV_REQ_QUEUE_SIZE]
            .valid = FALSE;
          STAT_EVENT(0, L2MARKV_L1INSERT_PORT_READY);
        } else {
          STAT_EVENT(0, L2MARKV_L1INSERT_PORT_FULL);
          dcache_read_port_full = TRUE;
          break;
        }
      } else
        buffer_empty = TRUE;  // if timer is set
    } else
      buffer_empty = TRUE;  // if valid
  }                         // end while

  for(ii = 0; ii < L1MARKV_PREF_SEND_QUEUE; ii++) {
    uns q_index = markv_l2access_send_no % MARKV_L2ACCESS_REQ_Q_SIZE;
    if(markv_l2send_req_queue[q_index].valid &&
       (cycle_count >= markv_l2send_req_queue[q_index].rdy_cycle)) {
      if((model->mem == MODEL_MEM &&
          // cmp FIXME
          new_mem_req(MRT_DPRF, 0, markv_l2send_req_queue[q_index].va,
                      L1_LINE_SIZE, 1, NULL, dcache_fill_line, unique_count,
                      0))) {
        STAT_EVENT(0, L2MARKV_PREF_REQ);
        DEBUG(0,
              "send to l2 : line_addr:%s q_no:%d req_no:%lld send_no:%lld \n",
              hexstr64(markv_l2send_req_queue[q_index].va), q_index,
              markv_l2access_send_no, markv_l2access_req_no);
        markv_l2send_req_queue[q_index].valid = FALSE;
        markv_l2access_send_no++;
      }
    } else
      break;
  }
}

/***************************************************************************************/
/* next line and previous line prefetcher */
/*                                                                                     */
/***************************************************************************************/


void l2next_pref(Mem_Req_Info* req) {
  uns ii;
  for(ii = 0; ii < 2; ii++) {
    Addr req_va;
    Addr line_addr;

    if(ii == 0)
      req_va = req->addr + 64;
    else
      req_va = req->addr - 64;

    if(L2L1_IMMEDIATE_PREF_CACHE && DC_PREF_CACHE_ENABLE) {
      dc_pref_cache_insert(req_va);
      STAT_EVENT(0, L2NEXT_PREF_REQ);
    } else {
      uns bank = req_va >> dc->dcache.shift_bits &
                 N_BIT_MASK(LOG2(DCACHE_BANKS));
      Cache*   l1_cache = &mem->uncores[req->proc_id].l1->cache;
      L1_Data* l1_data  = cache_access(l1_cache, req_va, &line_addr, FALSE);

      if(l1_data) {  // hit l1 cache
        if(get_read_port(&dc->ports[bank]) &&
           get_write_port(&dc->ports[bank])) {  // get ports
          // !!! we need to check whether the data is in the L1 cache (second
          // level cache or not!!!!)
          Addr         line_addr, repl_line_addr;
          Dcache_Data* line = (Dcache_Data*)cache_access(&dc->dcache, req_va,
                                                         &line_addr, FALSE);
          if(!line) {
            Dcache_Data* data = (Dcache_Data*)cache_insert(
              &dc->dcache, dc->proc_id, req_va, &line_addr, &repl_line_addr);
            if(data->dirty) {
              new_mem_req(MRT_WB, req->proc_id, repl_line_addr,
                          DCACHE_LINE_SIZE, 1, NULL, NULL, 0,
                          NULL);  // CMP FIXME
            }
            data->HW_prefetch = TRUE;
            STAT_EVENT(0, L2NEXT_PREF_REQ);
            STAT_EVENT(0, L2NEXT_PREF_HIT_DATA_REQ);
            DEBUG(0,
                  "[%s]miss_va:%s fetch va:%s miss_vline:%s fetch_vline:%s \n",
                  unsstr64(cycle_count), hexstr64(req->addr), hexstr64(req_va),
                  hexstr64((req->addr) >> 6), hexstr64(req_va >> 6));
          } else {
            STAT_EVENT(0, L2NEXT_PREF_HIT_DATA_IN_CACHE);
            DEBUG(0,
                  "[%s]miss_va:%s in_the_cache va:%s  miss_vline:%s "
                  "fetch_vline:%s\n",
                  unsstr64(cycle_count), hexstr64(req->addr), hexstr64(req_va),
                  hexstr64((req->addr) >> 6), hexstr64(req_va >> 6));
          }

          STAT_EVENT(0, L2NEXT_L1INSERT_PORT_READY);
        } else {
          STAT_EVENT(0, L2NEXT_L1INSERT_PORT_FULL);
          break;
        }
      } else {  // miss l1 cache  // no prefetching request
        STAT_EVENT(0, L2NEXT_PREF_MISS);
      }
    }
  }
}
