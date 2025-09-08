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
 * File         : l2l1pref.c
 * Author       : HPS Research Group
 * Date         : 03/24/2004
 * Description  : L2 to L1 prefetcher support functions
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
#include "prefetcher//stream.param.h"
#include "prefetcher/l2l1pref.h"
#include "prefetcher/stream_pref.h"
#include "statistics.h"

/**************************************************************************************/
/* Macros */
#define DEBUG(args...) _DEBUG(DEBUG_L2L1PREF, ##args)

/**************************************************************************************/
/* Global Variables */

extern Memory*       mem;
extern Dcache_Stage* dc;
static Cache*        l1_cache;

/***************************************************************************************/
/* Local Prototypes */
FILE* f_l1_hit;

static const char* const mem_req_type_info_names[] = {"IFTCH", "DFTCH", "DSTOR",
                                                      "IPRF",  "DPRF",  "WB"};


Hash_Table ip_table;  // L1 HIT IP addresses
Counter    last_dc_miss;
Counter    last_hps_miss;
Counter    last_hps_hit;

/***************************************************************************************/

void init_prefetch(void) {
  if(model->mem == MODEL_MEM) {
    ASSERTM(0, !PRIVATE_L1, "L2L1 Prefetcher assumes shared L1\n");
    l1_cache = &mem->uncores[0].l1->cache;
  }

  if(L2L1PREF_ON)
    l2l1_init();
}

void l2l1_init(void) {
  if(L2WAY_PREF)
    l2way_init();
  if(L2MARKV_PREF_ON)
    l2markv_init();

  if(L1_HIT_DUMP_FILE_ON) {
    f_l1_hit = fopen(L1_HIT_DUMPFILE, "w");
  }
  if(L2L1_L2_HIT_STAT)
    init_hash_table(&ip_table, "L1 HIT IP TABLE", 10003,
                    sizeof(L2_hit_ip_stat_entry));
}


void l2l1pref_mem(Mem_Req* req) {
  Mem_Req_Info mem_req_info;
  Op**         op_p = (Op**)list_start_head_traversal(&req->op_ptrs);
  Op*          op   = op_p ? *op_p : NULL;
  mem_req_info.addr = req->addr;
  mem_req_info.type = (Mem_Req_Type)req->type;  // FIXME !!
  mem_req_info.oldest_op_unique_num = req->oldest_op_unique_num;
  mem_req_info.oldest_op_inst_addr  = (op ? op->inst_info->addr : 1);
  l2l1pref_mem_process(&mem_req_info);
}


void l2l1pref_mem_process(Mem_Req_Info* req) {
  int  train_hit = 0;
  int  pref_req  = 0;
  Addr req_addr  = 0;

  if(L2HIT_STREAM_PREF_ON && STREAM_PREFETCH_ON) {
    if(req->type == MRT_DFETCH)
      l2_hit_stream_pref(req->addr, FALSE);  // only demanding l2hit can send to
                                             // prefetcher module
  }

  if(L2WAY_PREF)
    l2way_pref(req);
  if(L2MARKV_PREF_ON && req->type == MRT_DFETCH)
    l2markv_pref(req, &train_hit, &pref_req, &req_addr);
  if(L2NEXT_PREF_ON)
    l2next_pref(req);

  if(L1_HIT_DUMP_FILE_ON && (req->type == MRT_DFETCH)) {
    int l1_set = req->addr >> (l1_cache)->shift_bits & (l1_cache)->set_mask;
    int dc_set = req->addr >> (&dc->dcache)->shift_bits &
                 (&dc->dcache)->set_mask;
    if(!L1_HIT_DUMP_WO_TXT)
      fprintf(f_l1_hit,
              "op_uniq_no:%8s l *0x%10s va:0x%s li:%4s l1_set:%4d dc_set:%4d "
              "%5s co:%8s t_hit:%d p_req:%d req_addr:0x%8s \n",
              unsstr64(req->oldest_op_unique_num),
              hexstr64(req->oldest_op_inst_addr), hexstr64(req->addr),
              hexstr64(req->addr >> 6), dc_set, l1_set,
              mem_req_type_info_names[MIN2(req->type, 6)],
              unsstr64(cycle_count), train_hit, pref_req, hexstr64(req_addr));

    else
      fprintf(f_l1_hit, "%s %s %s %s %d %d %s %d %d %s \n",
              unsstr64(req->oldest_op_unique_num),
              hexstr64(req->oldest_op_inst_addr), unsstr64(req->addr),
              unsstr64(req->addr >> 6), dc_set, l1_set, unsstr64(cycle_count),
              train_hit, pref_req, unsstr64(req_addr));
  }

  if(L2L1_L2_HIT_STAT && (req->type == MRT_DFETCH)) {
    L2_hit_ip_stat_entry *ip_hash_entry, *ip_created_hash_entry;
    Flag                  new_entry;
    if(req->oldest_op_inst_addr != 1) {
      ip_hash_entry = hash_table_access(&ip_table,
                                        (int64)(req->oldest_op_inst_addr));
      if(!ip_hash_entry) {
        ip_created_hash_entry = hash_table_access_create(
          &ip_table, (int64)(req->oldest_op_inst_addr), &new_entry);
        ip_created_hash_entry->hit_count  = 1;
        ip_created_hash_entry->last_cycle = cycle_count;
      } else {
        Counter delta = cycle_count - ip_hash_entry->last_cycle;
        ip_hash_entry->hit_count++;
        if(delta < 50) {
          ip_hash_entry->delta[0] = ip_hash_entry->delta[0] + 1;
          STAT_EVENT(0, L2HIT_SAME_IP_DELTA__0);
        } else if(delta < 500) {
          ip_hash_entry->delta[1] = ip_hash_entry->delta[1] + 1;
          STAT_EVENT(0, L2HIT_SAME_IP_DELTA__1);
        } else if(delta < 5000) {
          ip_hash_entry->delta[2] = ip_hash_entry->delta[2] + 1;
          STAT_EVENT(0, L2HIT_SAME_IP_DELTA__2);
        } else if(delta < 50000) {
          ip_hash_entry->delta[3] = ip_hash_entry->delta[3] + 1;
          STAT_EVENT(0, L2HIT_SAME_IP_DELTA__3);
        } else if(delta < 500000) {
          ip_hash_entry->delta[4] = ip_hash_entry->delta[4] + 1;
          STAT_EVENT(0, L2HIT_SAME_IP_DELTA__4);
        } else if(delta < 5000000) {
          ip_hash_entry->delta[5] = ip_hash_entry->delta[5] + 1;
          STAT_EVENT(0, L2HIT_SAME_IP_DELTA__5);
        } else if(delta < 50000000) {
          ip_hash_entry->delta[6] = ip_hash_entry->delta[6] + 1;
          STAT_EVENT(0, L2HIT_SAME_IP_DELTA__6);
        } else if(delta < 500000000) {
          ip_hash_entry->delta[7] = ip_hash_entry->delta[7] + 1;
          STAT_EVENT(0, L2HIT_SAME_IP_DELTA__7);
        } else {
          ip_hash_entry->delta[8] = ip_hash_entry->delta[8] + 1;
          STAT_EVENT(0, L2HIT_SAME_IP_DELTA__8);
        }
      }
    }
  }
}

void l2l1pref_dcache(Addr line_addr, Op* op) {
  Mem_Req_Info tmp_req;
  tmp_req.addr = line_addr;

  if((HW_PREF_HIT_TRAIN_STREAM || L2L1_HIT_TRAIN) && STREAM_PREFETCH_ON &&
     L2HIT_STREAM_PREF_ON)
    l2_hit_stream_pref(line_addr, TRUE);

  if(L2L1_HIT_TRAIN && L2WAY_PREF)
    l2way_pref(&tmp_req);

  if(L2L1_HIT_TRAIN && L2MARKV_PREF_ON) {
    int  train_hit = 0;
    int  pref_req  = 0;
    Addr req_addr  = 0;
    int  l1_set    = line_addr >> (l1_cache)->shift_bits & (l1_cache)->set_mask;
    int  dc_set    = line_addr >> (&dc->dcache)->shift_bits &
                 (&dc->dcache)->set_mask;

    l2markv_pref(&tmp_req, &train_hit, &pref_req, &req_addr);
    if(L1_HIT_DUMP_FILE_ON) {
      fprintf(f_l1_hit,
              "op_uniq_no:%8s l *0x%10s va:0x%s li:%4s l1_set:%4d dc_set:%4d "
              "%5s co:%8s t_hit:%d p_req:%d req_addr:0x%8s \n",
              unsstr64(op->unique_num), hexstr64(op->inst_info->addr),
              hexstr64(line_addr), hexstr64(line_addr >> 6), l1_set, dc_set,
              "DCACHE", unsstr64(cycle_count), train_hit, pref_req,
              hexstr64(req_addr));
    }
  }
  if(L2L1_HIT_TRAIN && L2NEXT_PREF_ON)
    l2next_pref(&tmp_req);
}

Dcache_Data* dc_pref_cache_access(Op* op) {
  Addr         pref_line_addr, repl_line_addr;
  Flag         pref_cache_hit = FALSE;
  Flag         data_hit       = FALSE;
  Dcache_Data* old_data       = NULL;

  Dcache_Data* data = (Dcache_Data*)cache_access(
    &dc->pref_dcache, op->oracle_info.va, &pref_line_addr, FALSE);

  if(data && (!PREF_CACHE_USE_RDY_CYCLE || (data->rdy_cycle <= cycle_count)))
    data_hit = TRUE;

  if(data) {
    if(data->rdy_cycle > cycle_count)
      STAT_EVENT(0, DCACHE_PREF_NOT_RDY);
    else {
      int time_diff = cycle_count - data->rdy_cycle;
      if(time_diff < 10)
        STAT_EVENT(0, DCACHE_PREF_FETCH_10);
      else if(time_diff < 100)
        STAT_EVENT(0, DCACHE_PREF_FETCH_100);
      else if(time_diff < 1000)
        STAT_EVENT(0, DCACHE_PREF_FETCH_1000);
      else if(time_diff < 10000)
        STAT_EVENT(0, DCACHE_PREF_FETCH_10000);
      else if(time_diff < 100000)
        STAT_EVENT(0, DCACHE_PREF_FETCH_100000);
      else if(time_diff < 1000000)
        STAT_EVENT(0, DCACHE_PREF_FETCH_1000000);
      else if(time_diff < 10000000)
        STAT_EVENT(0, DCACHE_PREF_FETCH_10000000);
      else if(time_diff < 100000000)
        STAT_EVENT(0, DCACHE_PREF_FETCH_100000000);
      else
        STAT_EVENT(0, DCACHE_PREF_FETCH_MORE);
    }
  }

  if(DC_PREF_ONLY_L1HIT) {
    Addr     line_addr;
    L1_Data* l1_data = (L1_Data*)cache_access(l1_cache, op->oracle_info.va,
                                              &line_addr, FALSE);
    if(!l1_data) {
      pref_cache_hit = FALSE;
      if(data_hit)
        STAT_EVENT(0, DC_PREF_HIT_L1_MISS);
    } else if(data_hit && l1_data)
      pref_cache_hit = TRUE;
  } else if(data_hit)
    pref_cache_hit = TRUE;

  if(op->off_path && !PREFCACHE_MOVE_OFFPATH) {
    if(pref_cache_hit) {
      STAT_EVENT(0, DC_PREF_CACHE_HIT_OFFPATH);
      STAT_EVENT(0, DC_PREF_CACHE_HIT_PER_OFFPATH);
      return data;
    } else
      return NULL;
  }

  // return if this access is DPRF, there should be no prefetches accessing
  // this stuff...

  if(PREF_INSERT_DCACHE_IMM && pref_cache_hit) {
    Addr dcache_line_addr;
    old_data = (Dcache_Data*)cache_insert(&dc->dcache, dc->proc_id,
                                          op->oracle_info.va, &dcache_line_addr,
                                          &repl_line_addr);

    STAT_EVENT(0, DC_PREF_MOVE_DC);
    DEBUG(dc->proc_id, "pref_dcache fill dcache  addr:0x%s  :%7s index:%7s\n",
          hexstr64s(op->oracle_info.va), unsstr64(op->oracle_info.va),
          unsstr64((op->oracle_info.va) >> LOG2(DCACHE_LINE_SIZE)));

    if(old_data->dirty) {
      DEBUG(dc->proc_id, "Scheduling writeback of addr:0x%s\n",
            hexstr64s(repl_line_addr));
      ASSERT(0, old_data->read_count[0] || old_data->read_count[1] ||
                  old_data->write_count[0] || old_data->write_count[1]);
      FATAL_ERROR(0, "This writeback code is wrong. Writebacks may be lost.");
      //             if(model->mem == MODEL_MEM) new_mem_wb_req(MRT_WB,
      //             WB_DCACHE, op->proc_id, repl_line_addr); // cmp FIXME
      //             prefetchers
    }
    old_data->dirty          = FALSE;
    old_data->prefetch       = data->HW_prefetch;  // what about this flag?
    old_data->read_count[0]  = 0;  // only true for off_path stuff
    old_data->write_count[0] = 0;
    old_data->read_count[1]  = data->fetched_by_offpath;
    old_data->write_count[1] = 0;
    // old_data->HW_prefetch = TRUE;  // do not mark it as prefetch - train the
    // prefetcher if old_HW_prefetch is true
    old_data->HW_prefetch = data->HW_prefetch;
    /* line is invalidate */
    cache_invalidate(&dc->pref_dcache, op->oracle_info.va, &pref_line_addr);

    if(PREF_DCACHE_HIT_FILL_L1) {
      if(model->mem == MODEL_MEM) {
        Addr     line_addr;
        L1_Data* l1_data = (L1_Data*)cache_access(l1_cache, op->oracle_info.va,
                                                  &line_addr, TRUE);
        if(!l1_data) {
          Mem_Req tmp_req;
          tmp_req.addr     = op->oracle_info.va;
          tmp_req.op_count = 0;
          tmp_req.off_path = FALSE;
          DEBUG(dc->proc_id,
                "pref_dcache request fill l1cache  addr:0x%s  :%7s index:%7s\n",
                hexstr64s(op->oracle_info.va), unsstr64(op->oracle_info.va),
                unsstr64((op->oracle_info.va) >> LOG2(DCACHE_LINE_SIZE)));

          FATAL_ERROR(0, "This fill code is wrong. Writebacks may be lost.");
          l1_fill_line(&tmp_req);
          STAT_EVENT(0, DC_PREF_MOVE_L1);
        }
      }
    }
  }
  if(pref_cache_hit) {
    DEBUG(dc->proc_id, "pref_dcache hit addr:0x%s \n",
          hexstr64s(op->oracle_info.va));
    STAT_EVENT(0, DC_PREF_CACHE_HIT_PER + op->off_path);
    STAT_EVENT(0, DC_PREF_CACHE_HIT + op->off_path);
  }

  if(pref_cache_hit) {
    if(old_data)
      return old_data;
    else
      return data;
  } else
    return NULL;
}

// cmp FIXME
Flag dc_pref_cache_fill_line(Mem_Req* req) {
  Addr         addr = req->addr;
  Dcache_Data* old_data;
  Addr         line_addr, repl_line_addr;
  old_data = (Dcache_Data*)cache_insert(&dc->pref_dcache, dc->proc_id, addr,
                                        &line_addr, &repl_line_addr);
  old_data->rdy_cycle = cycle_count + DC_PREF_CACHE_CYCLE;
  DEBUG(dc->proc_id, "Filling pref_cache addr:0x%s :%8s index:%7s \n",
        hexstr64s(addr), unsstr64(addr),
        unsstr64(addr >> LOG2(DCACHE_LINE_SIZE)));
  STAT_EVENT(0, DC_PREF_CACHE_FILL);
  return SUCCESS;
}

void dc_pref_cache_insert(Addr addr) {
  Addr line_addr, repl_line_addr;

  Dcache_Data* data = (Dcache_Data*)cache_access(&dc->pref_dcache, addr,
                                                 &line_addr, FALSE);

  Dcache_Data* dc_data = (Dcache_Data*)cache_access(&dc->dcache, addr,
                                                    &line_addr, FALSE);

  L1_Data* l1_data = (L1_Data*)cache_access(l1_cache, addr, &line_addr, FALSE);

  if(dc_data)
    STAT_EVENT(0, DC_PREF_REQ_DCACHE_HIT);
  else if(data)
    STAT_EVENT(0, DC_PREF_REQ_PREF_CACHE_HIT);
  else if(!l1_data)
    STAT_EVENT(0, DC_PREF_REQ_L1_MISS);
  else
    STAT_EVENT(0, DC_PREF_REQ_CORR);

  STAT_EVENT(0, DC_PREF_CACHE_INSERT_REQ);

  if(!data && l1_data) {
    Dcache_Data* new_data = (Dcache_Data*)cache_insert(
      &dc->pref_dcache, dc->proc_id, addr, &line_addr, &repl_line_addr);
    DEBUG(dc->proc_id, "Filling pref_cache addr:0x%s :%8s index:%7s \n",
          hexstr64s(addr), unsstr64(addr),
          unsstr64(addr >> LOG2(DCACHE_LINE_SIZE)));
    STAT_EVENT(0, DC_PREF_CACHE_INSERT);
    new_data->read_count[0] = 1;
    new_data->rdy_cycle     = cycle_count + DC_PREF_CACHE_CYCLE;
  }
}


void ideal_l2l1_prefetcher(Op* op) {
  Addr         line_addr;
  Dcache_Data* line = (Dcache_Data*)cache_access(
    &dc->dcache, op->oracle_info.va, &line_addr, FALSE);

  if(!line) {  // dcache miss
    L1_Data* data = (L1_Data*)cache_access(
      l1_cache, op->oracle_info.va, &line_addr,
      TRUE);  // update the replacement policy

    if(data) {  // l1 hit
      Addr         repl_line_addr;
      Dcache_Data* dcache_data;
      dcache_data = (Dcache_Data*)cache_insert(&dc->dcache, dc->proc_id,
                                               op->oracle_info.va, &line_addr,
                                               &repl_line_addr);
      STAT_EVENT(0, L2_IDEAL_FILL_L1);
      // need to do a write-back
      if(dcache_data->dirty) {
        ASSERT(0, dcache_data->read_count[0] || dcache_data->read_count[1] ||
                    dcache_data->write_count[0] || dcache_data->write_count[1]);
        FATAL_ERROR(0, "This writeback code is wrong. Writebacks may be lost.");
        // if(model->mem == MODEL_MEM) new_mem_wb_req(MRT_WB, WB_DCACHE,
        // op->proc_id, repl_line_addr); // cmp FIXME prefetcher
      }
      dcache_data->dirty          = FALSE;
      dcache_data->read_count[0]  = 0;
      dcache_data->read_count[1]  = 0;
      dcache_data->write_count[0] = 0;
      dcache_data->write_count[1] = 0;
    } else {
      // l1 miss
      STAT_EVENT(0, L2_IDEAL_MISS_L2);
    }
  }
}

void l2l1_done(void) {
  if(L2L1_L2_HIT_STAT) {
    L2_hit_ip_stat_entry** ip_data_array = NULL;
    int                    ii;

    ip_data_array = (L2_hit_ip_stat_entry**)hash_table_flatten(&ip_table, NULL);

    for(ii = 0; ii < ip_table.count; ii++) {
      if(ip_data_array[ii]->hit_count < 10)
        STAT_EVENT(0, L2HIT_IP_HIT_COUNT__1);
      else if(ip_data_array[ii]->hit_count < 100)
        STAT_EVENT(0, L2HIT_IP_HIT_COUNT__2);
      else if(ip_data_array[ii]->hit_count < 1000)
        STAT_EVENT(0, L2HIT_IP_HIT_COUNT__3);
      else if(ip_data_array[ii]->hit_count < 10000)
        STAT_EVENT(0, L2HIT_IP_HIT_COUNT__4);
      else if(ip_data_array[ii]->hit_count < 100000)
        STAT_EVENT(0, L2HIT_IP_HIT_COUNT__5);
      else if(ip_data_array[ii]->hit_count < 1000000)
        STAT_EVENT(0, L2HIT_IP_HIT_COUNT__6);
      else if(ip_data_array[ii]->hit_count < 10000000)
        STAT_EVENT(0, L2HIT_IP_HIT_COUNT__7);
      else if(ip_data_array[ii]->hit_count < 100000000)
        STAT_EVENT(0, L2HIT_IP_HIT_COUNT__8);
      else
        STAT_EVENT(0, L2HIT_IP_HIT_COUNT__9);
    }
  }
}


void dc_miss_stat(Op* op) {
  Counter cycle_delta = cycle_count - last_dc_miss;
  STAT_EVENT(0, DC_MISS_DELTA__0 + MIN2(LOG10(cycle_delta), 9));
  last_dc_miss = cycle_count;
}

void hps_hit_stat(Mem_Req* req) {
  Counter cycle_delta = cycle_count - last_hps_hit;
  STAT_EVENT(0, HPS_HIT_DELTA__0 + MIN2(LOG10(cycle_delta), 9));
  last_hps_hit = cycle_count;
}

void hps_miss_stat(Mem_Req* req) {
  Counter cycle_delta = cycle_count - last_hps_miss;
  STAT_EVENT(0, HPS_MISS_DELTA__0 + MIN2(LOG10(cycle_delta), 9));
  last_hps_miss = cycle_count;
}
