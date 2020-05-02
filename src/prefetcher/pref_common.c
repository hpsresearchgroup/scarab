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
 * File         : pref_common.c
 * Author       : HPS Research Group
 * Date         : 11/30/2004
 * Description  : Common framework for working with prefetchers - less stuff to
 **mess* with
 ***************************************************************************************/

#include <math.h>
#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"

#include "globals/assert.h"
#include "globals/utils.h"
#include "op.h"

#include "cmp_model.h"
#include "core.param.h"
#include "dcache_stage.h"
#include "debug/debug.param.h"
#include "general.param.h"
#include "libs/cache_lib.h"
#include "libs/hash_lib.h"
#include "libs/list_lib.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "prefetcher//pref_stream.h"
#include "prefetcher//pref_stride.h"
#include "prefetcher//pref_stridepc.h"
#include "prefetcher//stream.param.h"
#include "prefetcher/l2l1pref.h"
#include "prefetcher/pref.param.h"
#include "prefetcher/pref_2dc.h"
#include "prefetcher/pref_ghb.h"
#include "prefetcher/pref_markov.h"
#include "prefetcher/pref_phase.h"
#include "statistics.h"
/**************************************************************************************
 * Usage Notes
 *
 *
 **************************************************************************************/

/* Include table of available prefetchers */
#include "prefetcher/pref_table.def"

static int pref_table_size;

/**************************************************************************************/
/* Macros */
#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_PREF, ##args)

/**************************************************************************************/
/* Global Variables */

extern Memory*       mem;
extern Dcache_Stage* dc;

HWP_Common pref;

FILE* PREF_TRACE_OUT;

FILE* PREF_DEGFB_FILE;

static void pref_core_init(HWP_Core* pref_core);
static void pref_update_core(uns proc_id);
static void pref_polbv_update_on_evict(uns8 pref_proc_id, uns8 evicted_proc_id,
                                       Addr evicted_addr);
static void pref_polbv_lookup_on_miss(uns8 proc_id, Addr addr);
static void pref_polbv_update_on_repref(uns8 proc_id, Addr addr);
void        pref_feed_back_info_update(uns8 prefetcher_id);
/***************************************************************************************/
/* supporting functions */

int pref_compare_hwp_priority(const void* const a, const void* const b) {
  return (((HWP*)a)->hwp_info->priority - ((HWP*)b)->hwp_info->priority);
}

int pref_compare_prefloadhash(const void* const a, const void* const b) {
  Pref_LoadPCInfo** dataA = (Pref_LoadPCInfo**)a;
  Pref_LoadPCInfo** dataB = (Pref_LoadPCInfo**)b;

  return ((*dataB)->count - (*dataA)->count);
}

void pref_core_init(HWP_Core* pref_core) {
  // initialize queues
  pref_core->dl0req_queue   = (Pref_Mem_Req*)calloc(PREF_DL0REQ_QUEUE_SIZE,
                                                  sizeof(Pref_Mem_Req));
  pref_core->umlc_req_queue = (Pref_Mem_Req*)calloc(PREF_UMLC_REQ_QUEUE_SIZE,
                                                    sizeof(Pref_Mem_Req));
  pref_core->ul1req_queue   = (Pref_Mem_Req*)calloc(PREF_UL1REQ_QUEUE_SIZE,
                                                  sizeof(Pref_Mem_Req));

  pref_core->dl0req_queue_req_pos  = -1;
  pref_core->dl0req_queue_send_pos = 0;

  pref_core->umlc_req_queue_req_pos  = -1;
  pref_core->umlc_req_queue_send_pos = 0;

  pref_core->ul1req_queue_req_pos  = -1;
  pref_core->ul1req_queue_send_pos = 0;
}

void pref_init(void) {
  int          ii;
  static char* pref_trace_filename = "mem_trace";
  uns8         proc_id;

  if(!PREF_FRAMEWORK_ON)
    return;

  pref.cores_array = (HWP_Core*)calloc(NUM_CORES, sizeof(HWP_Core));
  pref.cores       = (HWP_Core**)calloc(NUM_CORES, sizeof(HWP_Core*));
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    pref_core_init(&pref.cores_array[proc_id]);
    pref.cores[proc_id] = &pref.cores_array[PREF_SHARED_QUEUES ? 0 : proc_id];
  }

  // initialize pref_table_size
  pref_table_size = 0;
  while(pref_table[pref_table_size].name != NULL) {
    pref_table_size++;
  }

  for(ii = 0; ii < pref_table_size; ii++) {
    uns8 proc_id;

    pref_table[ii].hwp_info     = (HWP_Info*)malloc(sizeof(HWP_Info));
    pref_table[ii].hwp_info->id = ii;

    pref_table[ii].hwp_info->useful_core = (Counter*)calloc(NUM_CORES,
                                                            sizeof(Counter));
    memset(pref_table[ii].hwp_info->useful_core, 0,
           sizeof(Counter) * NUM_CORES);
    pref_table[ii].hwp_info->sent_core = (Counter*)calloc(NUM_CORES,
                                                          sizeof(Counter));
    memset(pref_table[ii].hwp_info->sent_core, 0, sizeof(Counter) * NUM_CORES);
    pref_table[ii].hwp_info->late_core = (Counter*)calloc(NUM_CORES,
                                                          sizeof(Counter));
    memset(pref_table[ii].hwp_info->late_core, 0, sizeof(Counter) * NUM_CORES);

    pref_table[ii].hwp_info->curr_useful_core = (Counter*)calloc(
      NUM_CORES, sizeof(Counter));
    memset(pref_table[ii].hwp_info->curr_useful_core, 0,
           sizeof(Counter) * NUM_CORES);
    pref_table[ii].hwp_info->curr_sent_core = (Counter*)calloc(NUM_CORES,
                                                               sizeof(Counter));
    memset(pref_table[ii].hwp_info->curr_sent_core, 0,
           sizeof(Counter) * NUM_CORES);
    pref_table[ii].hwp_info->curr_late_core = (Counter*)calloc(NUM_CORES,
                                                               sizeof(Counter));
    memset(pref_table[ii].hwp_info->curr_late_core, 0,
           sizeof(Counter) * NUM_CORES);

    pref_table[ii].hwp_info->dyn_degree_core = (uns*)calloc(NUM_CORES,
                                                            sizeof(uns));
    for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      pref_table[ii].hwp_info->dyn_degree_core[proc_id] = 2;
    }

    pref_table[ii].hwp_info->priority = 0;
    pref_table[ii].hwp_info->enabled  = FALSE;

    if(pref_table[ii].init_func)
      pref_table[ii].init_func(&pref_table[ii]);
  }
  qsort(pref_table, pref_table_size, sizeof(HWP), pref_compare_hwp_priority);

  if(PREF_TRACE_ON)
    PREF_TRACE_OUT = file_tag_fopen(NULL, pref_trace_filename, "w");

  if(PREF_DEGFB_STATPHASEFILE) {
    PREF_DEGFB_FILE = file_tag_fopen(NULL, "prefdefbstats.out", "w");
  }

  // Feedback directed prefetching

  pref.num_ul1_evicted = 0;

  pref.num_ul1_misses      = 0;
  pref.curr_num_ul1_misses = 0;

  if(PREF_POLBV_ON) {
    for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      pref.cores[proc_id]->pref_polbv_info = (Pref_Polbv_Info*)calloc(
        PREF_POLBV_SIZE, sizeof(Pref_Polbv_Info));
      memset(pref.cores[proc_id]->pref_polbv_info, 0,
             sizeof(Pref_Polbv_Info) * PREF_POLBV_SIZE);
    }
  }
  ////////////////////////////////////////

  // Zhuang and Lee's hardware prefetching filter similar to gshare
  if(PREF_HFILTER_ON) {
    for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      pref.cores[proc_id]->pref_hfilter_pht = (uns8*)malloc(
        sizeof(uns8) * (0x1 << PREF_HFILTER_INDEX_BITS));
      memset(pref.cores[proc_id]->pref_hfilter_pht, 0,
             sizeof(uns8) * (0x1 << PREF_HFILTER_INDEX_BITS));
    }
  }

  pref.phase = 0;
}

void pref_per_core_done(uns proc_id) {
  for(uns ii = 0; ii < pref_table_size; ii++) {
    if(pref_table[ii].hwp_info->enabled && pref_table[ii].per_core_done_func) {
      pref_table[ii].per_core_done_func(proc_id);
    }
  }
}

void pref_done(void) {
  int ii;
  if(!PREF_FRAMEWORK_ON)
    return;
  if(PREF_ANALYZE_LOAD) {
    int    ii;
    void** new_array        = hash_table_flatten(mem->pref_loadPC_hash, NULL);
    FILE*  PREF_LOADPC_FILE = file_tag_fopen(NULL, "pref_loadpc", "w");

    qsort(new_array, mem->pref_loadPC_hash->count, sizeof(void*),
          pref_compare_prefloadhash);
    // Print To File

    for(ii = 0; ii < mem->pref_loadPC_hash->count; ii++) {
      fprintf(PREF_LOADPC_FILE, "%llx\t%d\n",
              ((Pref_LoadPCInfo*)new_array[ii])->loadPC,
              ((Pref_LoadPCInfo*)new_array[ii])->count);
    }
  }
  for(ii = 0; ii < pref_table_size; ii++) {
    if(pref_table[ii].hwp_info->enabled && pref_table[ii].done_func) {
      pref_table[ii].done_func();
    }
  }
}

// FIXME LATER
void pref_dl0_miss(Addr line_addr, Addr load_PC) {
  int ii;
  if(!PREF_FRAMEWORK_ON)
    return;
  if(PREF_DL0_MISS_ON) {
    for(ii = 0; ii < pref_table_size; ii++) {
      if(pref_table[ii].hwp_info->enabled && pref_table[ii].dl0_miss_func) {
        pref_table[ii].dl0_miss_func(line_addr, load_PC);
      }
    }
  }
}

// FIXME LATER
void pref_dl0_hit(Addr line_addr, Addr load_PC) {
  int ii;
  if(!PREF_FRAMEWORK_ON)
    return;
  if(PREF_DL0_HIT_ON) {
    for(ii = 0; ii < pref_table_size; ii++) {
      if(pref_table[ii].hwp_info->enabled && pref_table[ii].dl0_hit_func) {
        pref_table[ii].dl0_hit_func(line_addr, load_PC);
      }
    }
  }
}

// FIXME LATER
void pref_dl0_pref_hit(Addr line_addr, Addr load_PC, uns8 prefetcher_id) {
  int ii;
  if(!PREF_FRAMEWORK_ON)
    return;
  if(prefetcher_id == 0)
    return;

  if(PREF_DL0_HIT_ON) {
    for(ii = 0; ii < pref_table_size; ii++) {
      if(pref_table[ii].hwp_info->enabled && pref_table[ii].dl0_pref_hit) {
        pref_table[ii].dl0_pref_hit(line_addr, load_PC);
      }
    }
  }
}

void pref_umlc_miss(uns8 proc_id, Addr line_addr, Addr load_PC,
                    uns32 global_hist) {
  int ii;
  if(!PREF_FRAMEWORK_ON)
    return;

  // IGNORE BELOW
  if(PREF_TRACE_ON)
    fprintf(PREF_TRACE_OUT, "%s \t %s \t %s \t %s\n", hexstr64s(cycle_count),
            hexstr64s(load_PC), hexstr64s(line_addr), "UMLC_MISS");

  // UPDATE prefpolbv reset entry
  if(PREF_POLBV_ON) {
    pref_polbv_lookup_on_miss(proc_id, line_addr);
  }

  for(ii = 0; ii < pref_table_size; ii++) {
    if(pref_table[ii].hwp_info->enabled && pref_table[ii].umlc_miss_func) {
      pref_table[ii].umlc_miss_func(proc_id, line_addr, load_PC, global_hist);
    }
  }
}

void pref_umlc_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                   uns32 global_hist) {
  int ii;
  if(!PREF_FRAMEWORK_ON)
    return;
  if(PREF_TRACE_ON)
    fprintf(PREF_TRACE_OUT, "%s \t %s \t %s \t %s\n", hexstr64s(cycle_count),
            hexstr64s(0), hexstr64s(line_addr), "UMLC_HIT");

  for(ii = 0; ii < pref_table_size; ii++) {
    if(pref_table[ii].hwp_info->enabled && pref_table[ii].umlc_hit_func) {
      pref_table[ii].umlc_hit_func(proc_id, line_addr, load_PC, global_hist);
    }
  }
}

void pref_umlc_pref_hit_late(uns8 proc_id, Addr line_addr, Addr load_PC,
                             uns32 global_hist, uns8 prefetcher_id) {
  if(!PREF_FRAMEWORK_ON)
    return;
  if(prefetcher_id == 0)
    return;

  pref_table[prefetcher_id].hwp_info->curr_late_core[proc_id]++;
  pref_umlc_pref_hit(proc_id, line_addr, -1, load_PC, global_hist,
                     prefetcher_id);
}

void pref_umlc_pref_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                        uns32 global_hist, int lru_position,
                        uns8 prefetcher_id) {
  int ii;
  if(prefetcher_id == 0)
    return;

  if(!PREF_FRAMEWORK_ON)
    return;

  if(PREF_TRACE_ON)
    fprintf(PREF_TRACE_OUT, "%s \t %s \t %s \t %s\n", hexstr64s(cycle_count),
            hexstr64s(0), hexstr64s(line_addr), "UMLC_PREFHIT");

  pref_table[prefetcher_id].hwp_info->curr_useful_core[proc_id]++;

  for(ii = 0; ii < pref_table_size; ii++) {
    if(pref_table[ii].hwp_info->enabled && pref_table[ii].umlc_pref_hit) {
      pref_table[ii].umlc_pref_hit(proc_id, line_addr, load_PC, global_hist);
    }
  }
}

void pref_ul1_miss(uns8 proc_id, Addr line_addr, Addr load_PC,
                   uns32 global_hist) {
  int ii;
  if(!PREF_FRAMEWORK_ON)
    return;
  if(DUMB_CORE_ON && DUMB_CORE == proc_id)
    return;  // dumb core should not trigger prefetches

  pref.curr_num_ul1_misses++;
  pref.cores[proc_id]->curr_ul1_misses++;

  // IGNORE BELOW
  if(PREF_TRACE_ON)
    fprintf(PREF_TRACE_OUT, "%s \t %s \t %s \t %s\n", hexstr64s(cycle_count),
            hexstr64s(load_PC), hexstr64s(line_addr), "UL1_MISS");

  // UPDATE prefpolbv reset entry
  if(PREF_POLBV_ON) {
    pref_polbv_lookup_on_miss(proc_id, line_addr);
  }

  for(ii = 0; ii < pref_table_size; ii++) {
    if(pref_table[ii].hwp_info->enabled && pref_table[ii].ul1_miss_func) {
      pref_table[ii].ul1_miss_func(proc_id, line_addr, load_PC, global_hist);
    }
  }
}

void pref_ul1_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                  uns32 global_hist) {
  int ii;
  if(!PREF_FRAMEWORK_ON)
    return;
  if(DUMB_CORE_ON && DUMB_CORE == proc_id)
    return;  // dumb core should not trigger prefetches
  if(PREF_TRACE_ON)
    fprintf(PREF_TRACE_OUT, "%s \t %s \t %s \t %s\n", hexstr64s(cycle_count),
            hexstr64s(0), hexstr64s(line_addr), "UL1_HIT");

  for(ii = 0; ii < pref_table_size; ii++) {
    if(pref_table[ii].hwp_info->enabled && pref_table[ii].ul1_hit_func) {
      pref_table[ii].ul1_hit_func(proc_id, line_addr, load_PC, global_hist);
    }
  }
}

void pref_ul1_pref_hit_late(uns8 proc_id, Addr line_addr, Addr load_PC,
                            uns32 global_hist, uns8 prefetcher_id) {
  if(!PREF_FRAMEWORK_ON)
    return;
  if(prefetcher_id == 0)
    return;

  pref_table[prefetcher_id].hwp_info->curr_late_core[proc_id]++;
  pref_ul1_pref_hit(proc_id, line_addr, load_PC, global_hist, -1,
                    prefetcher_id);
  if(PREF_REPORT_PREF_MATCH_AS_MISS)
    pref_ul1_miss(proc_id, line_addr, load_PC, global_hist);
  if(PREF_REPORT_PREF_MATCH_AS_HIT)
    pref_ul1_hit(proc_id, line_addr, load_PC, global_hist);
}

void pref_ul1_pref_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                       uns32 global_hist, int lru_position,
                       uns8 prefetcher_id) {
  int ii;
  if(prefetcher_id == 0)
    return;

  if(!PREF_FRAMEWORK_ON)
    return;

  if(PREF_TRACE_ON)
    fprintf(PREF_TRACE_OUT, "%s \t %s \t %s \t %s\n", hexstr64s(cycle_count),
            hexstr64s(0), hexstr64s(line_addr), "UL1_PREFHIT");

  pref_table[prefetcher_id].hwp_info->curr_useful_core[proc_id]++;

  for(ii = 0; ii < pref_table_size; ii++) {
    if(pref_table[ii].hwp_info->enabled && pref_table[ii].ul1_pref_hit) {
      pref_table[ii].ul1_pref_hit(proc_id, line_addr, load_PC, global_hist);
    }
  }
}

Flag pref_dl0req_queue_filter(Addr line_addr) {
  if(!PREF_DL0REQ_QUEUE_FILTER_ON)
    return FALSE;
  uns           proc_id      = get_proc_id_from_cmp_addr(line_addr);
  Pref_Mem_Req* dl0req_queue = pref.cores[proc_id]->dl0req_queue;
  for(uns ii = 0; ii < PREF_DL0REQ_QUEUE_SIZE; ii++) {
    if(dl0req_queue[ii].valid &&
       (dl0req_queue[ii].line_addr >> LOG2(DCACHE_LINE_SIZE)) ==
         (line_addr >> LOG2(DCACHE_LINE_SIZE))) {
      dl0req_queue[ii].valid = FALSE;
      STAT_EVENT(0, PREF_DL0REQ_QUEUE_HIT_BY_DEMAND);
      return TRUE;
    }
  }
  return FALSE;
}

Flag pref_umlc_req_queue_filter(Addr line_addr) {
  if(!PREF_UMLC_REQ_QUEUE_FILTER_ON)
    return FALSE;
  uns           proc_id        = get_proc_id_from_cmp_addr(line_addr);
  Pref_Mem_Req* umlc_req_queue = pref.cores[proc_id]->umlc_req_queue;
  for(uns ii = 0; ii < PREF_UMLC_REQ_QUEUE_SIZE; ii++) {
    if(umlc_req_queue[ii].valid &&
       (umlc_req_queue[ii].line_addr >> LOG2(DCACHE_LINE_SIZE)) ==
         (line_addr >> LOG2(DCACHE_LINE_SIZE))) {
      umlc_req_queue[ii].valid = FALSE;
      STAT_EVENT(0, PREF_UMLC_REQ_QUEUE_HIT_BY_DEMAND);
      return TRUE;
    }
  }
  return FALSE;
}

Flag pref_ul1req_queue_filter(Addr line_addr) {
  if(!PREF_UL1REQ_QUEUE_FILTER_ON)
    return FALSE;
  uns           proc_id      = get_proc_id_from_cmp_addr(line_addr);
  Pref_Mem_Req* ul1req_queue = pref.cores[proc_id]->ul1req_queue;
  for(uns ii = 0; ii < PREF_UL1REQ_QUEUE_SIZE; ii++) {
    if(ul1req_queue[ii].valid &&
       (ul1req_queue[ii].line_addr >> LOG2(DCACHE_LINE_SIZE)) ==
         (line_addr >> LOG2(DCACHE_LINE_SIZE))) {
      ul1req_queue[ii].valid = FALSE;
      STAT_EVENT(0, PREF_UL1REQ_QUEUE_HIT_BY_DEMAND);
      return TRUE;
    }
  }
  return FALSE;
}

Flag pref_ul1req_queue_match(Addr line_addr) {
  uns           proc_id      = get_proc_id_from_cmp_addr(line_addr);
  Pref_Mem_Req* ul1req_queue = pref.cores[proc_id]->ul1req_queue;
  for(uns ii = 0; ii < PREF_UL1REQ_QUEUE_SIZE; ii++) {
    if(ul1req_queue[ii].valid &&
       (ul1req_queue[ii].line_addr >> LOG2(DCACHE_LINE_SIZE)) ==
         (line_addr >> LOG2(DCACHE_LINE_SIZE))) {
      return TRUE;
    }
  }
  return FALSE;
}

Flag pref_addto_dl0req_queue(uns8 proc_id, Addr line_index,
                             uns8 prefetcher_id) {
  int          ii;
  Pref_Mem_Req new_req = {0};
  if(!line_index)  // addr = 0
    return TRUE;
  Pref_Mem_Req* dl0req_queue = pref.cores[proc_id]->dl0req_queue;
  int* dl0req_queue_req_pos  = &pref.cores[proc_id]->dl0req_queue_req_pos;
  if(PREF_DL0REQ_ADD_FILTER_ON) {
    for(ii = 0; ii < PREF_DL0REQ_QUEUE_SIZE; ii++) {
      if(dl0req_queue[ii].line_index == line_index) {
        STAT_EVENT(0, PREF_DL0REQ_QUEUE_MATCHED_REQ);
        return TRUE;  // Hit another request
      }
    }
  }
  if(dl0req_queue[(*dl0req_queue_req_pos + 1) % PREF_DL0REQ_QUEUE_SIZE].valid) {
    STAT_EVENT_ALL(PREF_DL0REQ_QUEUE_FULL);
    if(!PREF_DL0REQ_QUEUE_OVERWRITE_ON_FULL) {
      return FALSE;  // Q full
    }
  }

  new_req.proc_id       = proc_id;
  new_req.line_addr     = line_index << LOG2(DCACHE_LINE_SIZE);
  new_req.line_index    = line_index;
  new_req.valid         = TRUE;
  new_req.prefetcher_id = prefetcher_id;

  *dl0req_queue_req_pos = (*dl0req_queue_req_pos + 1) % PREF_DL0REQ_QUEUE_SIZE;

  dl0req_queue[*dl0req_queue_req_pos] = new_req;
  return TRUE;
}

Flag pref_addto_umlc_req_queue(uns8 proc_id, Addr line_index,
                               uns8 prefetcher_id) {
  int          ii;
  Pref_Mem_Req new_req = {0};
  if(!line_index)  // addr = 0
    return TRUE;
  Pref_Mem_Req* umlc_req_queue = pref.cores[proc_id]->umlc_req_queue;
  int* umlc_req_queue_req_pos  = &pref.cores[proc_id]->umlc_req_queue_req_pos;
  if(PREF_UMLC_REQ_ADD_FILTER_ON) {
    for(ii = 0; ii < PREF_UMLC_REQ_QUEUE_SIZE; ii++) {
      if(umlc_req_queue[ii].line_index == line_index) {
        STAT_EVENT(0, PREF_UMLC_REQ_QUEUE_MATCHED_REQ);
        return TRUE;  // Hit another request
      }
    }
  }
  if(umlc_req_queue[(*umlc_req_queue_req_pos + 1) % PREF_UMLC_REQ_QUEUE_SIZE]
       .valid) {
    STAT_EVENT_ALL(PREF_UMLC_REQ_QUEUE_FULL);
    if(!PREF_UMLC_REQ_QUEUE_OVERWRITE_ON_FULL) {
      return FALSE;  // Q full
    }
  }

  new_req.proc_id       = proc_id;
  new_req.line_addr     = line_index << LOG2(DCACHE_LINE_SIZE);
  new_req.line_index    = line_index;
  new_req.valid         = TRUE;
  new_req.distance      = 0;      // Not used for MLC
  new_req.bw_limited    = FALSE;  // Not used for MLC
  new_req.prefetcher_id = prefetcher_id;

  *umlc_req_queue_req_pos = (*umlc_req_queue_req_pos + 1) %
                            PREF_UMLC_REQ_QUEUE_SIZE;

  umlc_req_queue[*umlc_req_queue_req_pos] = new_req;
  return TRUE;
}

Flag pref_addto_ul1req_queue(uns8 proc_id, Addr line_index,
                             uns8 prefetcher_id) {
  return pref_addto_ul1req_queue_set(proc_id, line_index, prefetcher_id, 0, 0,
                                     0, FALSE);
}

Flag pref_addto_ul1req_queue_set(uns8 proc_id, Addr line_index,
                                 uns8 prefetcher_id, uns distance, Addr loadPC,
                                 uns32 global_hist, Flag bw) {
  int          ii;
  Pref_Mem_Req new_req;
  Addr         line_addr;
  if(!line_index)  // addr = 0
    return TRUE;

  Pref_Mem_Req* ul1req_queue = pref.cores[proc_id]->ul1req_queue;
  int* ul1req_queue_req_pos  = &pref.cores[proc_id]->ul1req_queue_req_pos;

  line_addr = (line_index) << LOG2(DCACHE_LINE_SIZE);

  pref_feed_back_info_update(prefetcher_id);

  if(PREF_UL1REQ_ADD_FILTER_ON) {
    for(ii = 0; ii < PREF_UL1REQ_QUEUE_SIZE; ii++) {
      if(ul1req_queue[ii].line_index == line_index) {
        STAT_EVENT(0, PREF_UL1REQ_QUEUE_MATCHED_REQ);
        return TRUE;  // Hit another request
      }
    }
  }
  if(ul1req_queue[(*ul1req_queue_req_pos + 1) % PREF_UL1REQ_QUEUE_SIZE].valid) {
    STAT_EVENT_ALL(PREF_UL1REQ_QUEUE_FULL);
    if(!PREF_UL1REQ_QUEUE_OVERWRITE_ON_FULL) {
      return FALSE;  // Q full
    }
  }
  new_req.proc_id       = proc_id;
  new_req.line_addr     = line_addr;
  new_req.line_index    = line_index;
  new_req.valid         = TRUE;
  new_req.prefetcher_id = prefetcher_id;
  new_req.distance      = distance;
  new_req.loadPC        = loadPC;
  new_req.global_hist   = global_hist;
  new_req.bw_limited    = bw;
  new_req.rdy_cycle     = cycle_count;

  *ul1req_queue_req_pos = (*ul1req_queue_req_pos + 1) % PREF_UL1REQ_QUEUE_SIZE;

  ul1req_queue[*ul1req_queue_req_pos] = new_req;
  return TRUE;
}

void pref_update(void) {
  if(!PREF_FRAMEWORK_ON)
    return;

  if(PREF_HFILTER_ON && PREF_HFILTER_RESET_ENABLE &&
     cycle_count % PREF_HFILTER_RESET_INTERVAL == 0)
    pref_hfilter_pht_reset();

  if(PREF_SHARED_QUEUES) {
    pref_update_core(0);
  } else {
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      pref_update_core(proc_id);
    }
  }
}

void pref_update_core(uns proc_id) {
  // first check the dl0 req queue to see if they can be satisfied by the dl0.
  // otherwise send them to the ul1 by putting them in the ul1req queue

  // dcache access
  //  - 1. check to make sure ports are available
  //  - 2. if missing in the cache,  insert into the l2 req queue.

  // ul1 access
  //  - 1. create a new request and call new_mem_req

  Pref_Mem_Req* dl0req_queue   = pref.cores[proc_id]->dl0req_queue;
  int* dl0req_queue_send_pos   = &pref.cores[proc_id]->dl0req_queue_send_pos;
  Pref_Mem_Req* umlc_req_queue = pref.cores[proc_id]->umlc_req_queue;
  int* umlc_req_queue_send_pos = &pref.cores[proc_id]->umlc_req_queue_send_pos;
  Pref_Mem_Req* ul1req_queue   = pref.cores[proc_id]->ul1req_queue;
  int* ul1req_queue_send_pos   = &pref.cores[proc_id]->ul1req_queue_send_pos;

  set_dcache_stage(&cmp_model.dcache_stage[proc_id]);

  for(uns ii = 0; ii < PREF_DL0SCHEDULE_NUM; ii++) {
    int          q_index = *dl0req_queue_send_pos;
    uns          bank;
    Dcache_Data* dc_hit;
    Addr         dummy_line_addr;
    Flag         inc_send_pos = TRUE;

    if(dl0req_queue[q_index].valid) {
      set_dcache_stage(&cmp_model.dcache_stage[proc_id]);

      ASSERT(proc_id, proc_id == dl0req_queue[q_index].line_addr >> 58);

      bank = dl0req_queue[q_index].line_addr >> dc->dcache.shift_bits &
             N_BIT_MASK(LOG2(DCACHE_BANKS));

      // check on the availability of a read port for the given bank
      if(!get_read_port(&dc->ports[bank])) {
        // Port is not available.
        // TODO: Currently, we dont look at other requests. We may want to allow
        // looking at the next couple to see if they can go.
        continue;
      }
      // Now, access the cache

      dc_hit = (Dcache_Data*)cache_access(
        &dc->dcache, dl0req_queue[q_index].line_addr, &dummy_line_addr, FALSE);

      if(dc_hit) {
        // nothing for now
      } else {
        // put req. into the ul1req_queue
        if(!pref_addto_ul1req_queue(proc_id, dl0req_queue[q_index].line_index,
                                    dl0req_queue[q_index].prefetcher_id)) {
          inc_send_pos = FALSE;
        }
      }
    }
    // Done with the dl0
    if(inc_send_pos) {
      *dl0req_queue_send_pos = (*dl0req_queue_send_pos + 1) %
                               PREF_DL0REQ_QUEUE_SIZE;
    }
  }

  // Now work with the umlc
  for(uns ii = 0; ii < PREF_UMLC_SCHEDULE_NUM; ii++) {
    int  q_index      = *umlc_req_queue_send_pos;
    Flag inc_send_pos = TRUE;

    if(umlc_req_queue[q_index].valid) {
      proc_id = umlc_req_queue[q_index].proc_id;
      ASSERTM(proc_id, proc_id == umlc_req_queue[q_index].line_addr >> 58,
              "proc_id from addr: %llx\n", umlc_req_queue[q_index].line_addr);

      // now access the umlc
      Pref_Req_Info info;

      info.prefetcher_id = umlc_req_queue[q_index].prefetcher_id;
      info.distance      = umlc_req_queue[q_index].distance;
      info.loadPC        = umlc_req_queue[q_index].loadPC;
      info.global_hist   = umlc_req_queue[q_index].global_hist;
      info.bw_limited    = umlc_req_queue[q_index].bw_limited;
      info.dest          = DEST_MLC;

      ASSERT(proc_id, proc_id == umlc_req_queue[q_index].proc_id);
      ASSERT(proc_id, proc_id == umlc_req_queue[q_index].line_addr >> 58);
      // check if there is enough space in the mem req buffer
      if((model->mem == MODEL_MEM) &&
         ((MEM_REQ_BUFFER_ENTRIES - mem_get_req_count(proc_id)) <
          PREF_L1Q_DEMAND_RESERVE)) {  // really req buffer demand reserve
        STAT_EVENT(0, PREF_MLCQ_STALL);
        if(PREF_REQ_DROP &&
           MEM_REQ_BUFFER_ENTRIES == mem_get_req_count(proc_id)) {
          umlc_req_queue[q_index].valid = FALSE;
        } else {
          inc_send_pos = FALSE;
        }

        break;
      }
      if((model->mem == MODEL_MEM) &&
         new_mem_req(MRT_DPRF, proc_id, umlc_req_queue[q_index].line_addr,
                     MLC_LINE_SIZE, 1, NULL, NULL, unique_count,
                     &info)) {  // CMP maybe unique_count_per_core[proc_id]?
        DEBUG(0, "Sent req %llx to umlc Qpos:%d\n",
              umlc_req_queue[q_index].line_index, *umlc_req_queue_send_pos);
        STAT_EVENT(0, PREF_UMLC_REQ_QUEUE_SENTREQ);
        umlc_req_queue[q_index].valid = FALSE;
      } else {
        STAT_EVENT(0, PREF_UMLC_REQ_SEND_QUEUE_STALL);
        inc_send_pos = FALSE;
        break;  // buffer is full. wait!!
      }
    }
    if(inc_send_pos) {
      *umlc_req_queue_send_pos = (*umlc_req_queue_send_pos + 1) %
                                 PREF_UMLC_REQ_QUEUE_SIZE;
    }
  }

  // Now work with the ul1
  for(uns ii = 0; ii < PREF_UL1SCHEDULE_NUM; ii++) {
    int  q_index      = *ul1req_queue_send_pos;
    Flag inc_send_pos = TRUE;

    if(ul1req_queue[q_index].valid) {
      proc_id = ul1req_queue[q_index].proc_id;
      set_dcache_stage(&cmp_model.dcache_stage[proc_id]);
      ASSERTM(proc_id, proc_id == ul1req_queue[q_index].line_addr >> 58,
              "proc_id from addr: %llx\n", ul1req_queue[q_index].line_addr);

      // now access the ul1
      Pref_Req_Info info;

      info.prefetcher_id = ul1req_queue[q_index].prefetcher_id;
      info.distance      = ul1req_queue[q_index].distance;
      info.loadPC        = ul1req_queue[q_index].loadPC;
      info.global_hist   = ul1req_queue[q_index].global_hist;
      info.bw_limited    = ul1req_queue[q_index].bw_limited;
      info.dest          = DEST_L1;

      ASSERT(proc_id, proc_id == ul1req_queue[q_index].proc_id);
      ASSERT(proc_id, proc_id == ul1req_queue[q_index].line_addr >> 58);
      // check if there is enough space in the mem req buffer
      if((model->mem == MODEL_MEM) &&
         ((MEM_REQ_BUFFER_ENTRIES - mem_get_req_count(proc_id)) <
          PREF_L1Q_DEMAND_RESERVE)) {
        STAT_EVENT(0, PREF_L1Q_STALL);
        if(PREF_REQ_DROP &&
           MEM_REQ_BUFFER_ENTRIES == mem_get_req_count(proc_id)) {
          ul1req_queue[q_index].valid = FALSE;
        } else {
          inc_send_pos = FALSE;
        }
        break;
      }
      if((model->mem == MODEL_MEM) &&
         new_mem_req(
           MRT_DPRF, proc_id, ul1req_queue[q_index].line_addr, L1_LINE_SIZE, 1,
           NULL, STREAM_PREF_INTO_DCACHE ? dcache_fill_line : NULL,
           unique_count, &info)) {  // CMP maybe unique_count_per_core[proc_id]?
        DEBUG(0, "Sent req %llx to ul1 Qpos:%d\n",
              ul1req_queue[q_index].line_index, *ul1req_queue_send_pos);
        STAT_EVENT(0, PREF_UL1REQ_QUEUE_SENTREQ);
        ul1req_queue[q_index].valid = FALSE;
      } else {
        STAT_EVENT(0, PREF_UL1REQ_SEND_QUEUE_STALL);
        inc_send_pos = FALSE;
        break;  // buffer is full. wait!!
      }
    }
    if(inc_send_pos) {
      *ul1req_queue_send_pos = (*ul1req_queue_send_pos + 1) %
                               PREF_UL1REQ_QUEUE_SIZE;
    }
  }
}

void pref_ul1sent(uns8 proc_id, Addr addr, uns8 prefetcher_id) {
  if(!PREF_FRAMEWORK_ON)
    return;
  if(prefetcher_id == 0)
    return;


  if(PREF_POLBV_ON) {
    // UPDATE prefpolbv reset entry
    pref_polbv_update_on_repref(proc_id, addr);
  }

  // prefetch missed in the ul1 and went out on the bus
  pref_table[prefetcher_id].hwp_info->curr_sent_core[proc_id]++;

  STAT_EVENT_ALL(PREF_L1_TOTAL_SENT);
  STAT_EVENT(proc_id, CORE_PREF_L1_SENT);

  // IGNORE PREF_DHAL
  if(pref_table[prefetcher_id].hwp_info->curr_sent_core[proc_id] ==
       PREF_DHAL_SENTTHRESH &&
     PREF_DHAL) {
    if(pref_table[prefetcher_id].hwp_info->curr_useful_core[proc_id] >
       PREF_DHAL_USETHRESH_MAX) {
      // INC
      if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] <
         PREF_DHAL_MAXDEG)
        pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]++;
    } else if(pref_table[prefetcher_id].hwp_info->curr_useful_core[proc_id] <
              PREF_DHAL_USETHRESH_MIN2) {
      if(pref_table[prefetcher_id].hwp_info->curr_useful_core[proc_id] <
         PREF_DHAL_USETHRESH_MIN1) {
        // FAST DEC
        if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 8)
          pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] =
            pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] / 2;
        else
          pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] = 4;
      } else {
        // DEC
        if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 4)
          pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;
      }
    }
    // reset the counts.
    pref_table[prefetcher_id].hwp_info->curr_sent_core[proc_id]   = 0;
    pref_table[prefetcher_id].hwp_info->curr_useful_core[proc_id] = 0;
  }
}

#define COOK_HIST_BITS(hist, len, untouched) \
  ((uns32)(hist) >> (32 - (len) + (untouched)) << (untouched))
#define COOK_ADDR_BITS(addr, len, shift) \
  (((uns32)(addr) >> (shift)) & (N_BIT_MASK((len))))

inline void pref_evictline_used(uns8 proc_id, Addr addr, Addr loadPC,
                                uns32 global_hist) {
  if(!PREF_FRAMEWORK_ON)
    return;

  if(PREF_HFILTER_ON) {
    uns32 cooked_hist = COOK_HIST_BITS(global_hist, PREF_HFILTER_INDEX_BITS, 0);
    uns32 cooked_addr = PREF_HFILTER_USE_PC ?
                          COOK_ADDR_BITS(loadPC, PREF_HFILTER_INDEX_BITS, 0) :
                          COOK_ADDR_BITS(addr, PREF_HFILTER_INDEX_BITS,
                                         LOG2(L1_LINE_SIZE));
    uns32 pht_index = cooked_hist ^ cooked_addr;

    pref.cores[proc_id]->pref_hfilter_pht[pht_index] = SAT_DEC(
      pref.cores[proc_id]->pref_hfilter_pht[pht_index], 0);
  }
}

inline void pref_evictline_notused(uns8 proc_id, Addr addr, Addr loadPC,
                                   uns32 global_hist) {
  if(!PREF_FRAMEWORK_ON)
    return;
  STAT_EVENT(proc_id, PREF_UNUSED_EVICT);

  if(PREF_HFILTER_ON) {
    uns32 cooked_hist = COOK_HIST_BITS(global_hist, PREF_HFILTER_INDEX_BITS, 0);
    uns32 cooked_addr = PREF_HFILTER_USE_PC ?
                          COOK_ADDR_BITS(loadPC, PREF_HFILTER_INDEX_BITS, 0) :
                          COOK_ADDR_BITS(addr, PREF_HFILTER_INDEX_BITS,
                                         LOG2(L1_LINE_SIZE));
    uns32 pht_index = cooked_hist ^ cooked_addr;

    pref.cores[proc_id]->pref_hfilter_pht[pht_index] = SAT_INC(
      pref.cores[proc_id]->pref_hfilter_pht[pht_index], 3);
  }
}

inline Flag pref_hfilter_pred_useless(uns8 proc_id, Addr addr, Addr loadPC,
                                      uns32 global_hist) {
  if(!PREF_FRAMEWORK_ON)
    return FALSE;

  ASSERT(0, PREF_HFILTER_ON);

  Flag  useless;
  uns32 cooked_hist = COOK_HIST_BITS(global_hist, PREF_HFILTER_INDEX_BITS, 0);
  uns32 cooked_addr = PREF_HFILTER_USE_PC ?
                        COOK_ADDR_BITS(loadPC, PREF_HFILTER_INDEX_BITS, 0) :
                        COOK_ADDR_BITS(addr, PREF_HFILTER_INDEX_BITS,
                                       LOG2(L1_LINE_SIZE));
  uns32 pht_index = cooked_hist ^ cooked_addr;

  useless = (pref.cores[proc_id]->pref_hfilter_pht[pht_index] >=
             PREF_HFILTER_PRED_USELESS_THRES);

  return useless;
}

void pref_hfilter_pht_reset(void) {
  uns8 proc_id;

  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    memset(pref.cores[proc_id]->pref_hfilter_pht, 0,
           sizeof(uns8) * (0x1 << PREF_HFILTER_INDEX_BITS));
  }
}

inline void pref_ul1evict(uns8 proc_id, Addr addr) {
  if(!PREF_FRAMEWORK_ON)
    return;

  pref.num_ul1_evicted++;
}

inline void pref_ul1evictOnPF(uns8 pref_proc_id, uns8 evicted_proc_id,
                              Addr addr) {
  if(!PREF_FRAMEWORK_ON)
    return;

  if(PREF_POLBV_ON) {
    pref_polbv_update_on_evict(pref_proc_id, evicted_proc_id, addr);
  }
}


void pref_polbv_update_on_evict(uns8 pref_proc_id, uns8 evicted_proc_id,
                                Addr evicted_addr) {
  Addr line_index;
  uns  index;
  ASSERT(pref_proc_id, PREF_POLBV_ON);

  line_index = (evicted_addr >> LOG2(DCACHE_LINE_SIZE));
  index      = ((line_index >> LOG2(PREF_POLBV_SIZE)) ^ line_index) &
          LOG2(PREF_POLBV_SIZE);

  pref.cores[pref_proc_id]->pref_polbv_info[index].proc_id   = evicted_proc_id;
  pref.cores[pref_proc_id]->pref_polbv_info[index].pollution = TRUE;
}

void pref_polbv_lookup_on_miss(uns8 proc_id, Addr addr) {
  Addr line_index;
  uns  index;
  uns8 proc_id_tmp;

  ASSERT(proc_id, PREF_POLBV_ON);
  line_index = (addr >> LOG2(DCACHE_LINE_SIZE));
  index      = ((line_index >> LOG2(PREF_POLBV_SIZE)) ^ line_index) &
          LOG2(PREF_POLBV_SIZE);

  for(proc_id_tmp = 0; proc_id_tmp < NUM_CORES; proc_id_tmp++) {
    if(pref.cores[proc_id_tmp]->pref_polbv_info[index].proc_id == proc_id) {
      if(pref.cores[proc_id_tmp]->pref_polbv_info[index].pollution == TRUE) {
        pref.cores[proc_id_tmp]->curr_pfpol++;
        pref.cores[proc_id_tmp]->pref_polbv_info[index].pollution = FALSE;
        STAT_EVENT(proc_id_tmp, PREF_PFPOL);
      }
    }
  }
}

void pref_polbv_update_on_repref(uns8 proc_id, Addr addr) {
  Addr line_index;
  uns  index;
  uns8 proc_id_tmp;

  ASSERT(proc_id, PREF_POLBV_ON);
  line_index = (addr >> LOG2(DCACHE_LINE_SIZE));
  index      = ((line_index >> LOG2(PREF_POLBV_SIZE)) ^ line_index) &
          LOG2(PREF_POLBV_SIZE);

  for(proc_id_tmp = 0; proc_id_tmp < NUM_CORES; proc_id_tmp++) {
    if(pref.cores[proc_id_tmp]->pref_polbv_info[index].proc_id == proc_id) {
      pref.cores[proc_id_tmp]->pref_polbv_info[index].pollution = FALSE;
    }
  }
}


void pref_feed_back_info_update(uns8 prefetcher_id) {
  static Counter prev_num_ul1_evicted = 0;

  if(PREF_UPDATE_INTERVAL != 0 &&
     (pref.num_ul1_evicted - prev_num_ul1_evicted >= PREF_UPDATE_INTERVAL)) {
    float acc, timely, pol;
    uns8  proc_id;

    prev_num_ul1_evicted = pref.num_ul1_evicted;

    for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      if(pref_table[prefetcher_id].hwp_info->curr_sent_core[proc_id]) {
        pref_table[prefetcher_id].hwp_info->useful_core[proc_id] =
          (0.5 * pref_table[prefetcher_id].hwp_info->useful_core[proc_id]) +
          (0.5 * pref_table[prefetcher_id].hwp_info->curr_useful_core[proc_id]);
        pref_table[prefetcher_id].hwp_info->curr_useful_core[proc_id] = 0;

        pref_table[prefetcher_id].hwp_info->sent_core[proc_id] =
          (0.5 * pref_table[prefetcher_id].hwp_info->sent_core[proc_id]) +
          (0.5 * pref_table[prefetcher_id].hwp_info->curr_sent_core[proc_id]);
        pref_table[prefetcher_id].hwp_info->curr_sent_core[proc_id] = 0;

        pref.cores[proc_id]->pfpol = (0.5 * pref.cores[proc_id]->pfpol) +
                                     (0.5 * pref.cores[proc_id]->curr_pfpol);
        pref.cores[proc_id]->curr_pfpol = 0;

        pref_table[prefetcher_id].hwp_info->late_core[proc_id] =
          (0.5 * pref_table[prefetcher_id].hwp_info->late_core[proc_id]) +
          (0.5 * pref_table[prefetcher_id].hwp_info->curr_late_core[proc_id]);
        pref_table[prefetcher_id].hwp_info->curr_late_core[proc_id] = 0;

        pref.cores[proc_id]->ul1_misses =
          (0.5 * pref.cores[proc_id]->ul1_misses) +
          (0.5 * pref.cores[proc_id]->curr_ul1_misses);
        pref.cores[proc_id]->curr_ul1_misses = 0;

        pref.num_ul1_misses = pref.curr_num_ul1_misses;

        pref.cores[proc_id]->update_acc = TRUE;


        acc    = pref_get_accuracy(proc_id, prefetcher_id);
        pol    = pref_get_ul1pollution(proc_id);
        timely = pref_get_timeliness(proc_id, prefetcher_id);

        if(acc > 0.9) {
          STAT_EVENT(proc_id, PREF_ACC_1);
        } else if(acc > 0.8) {
          STAT_EVENT(proc_id, PREF_ACC_2);
        } else if(acc > 0.7) {
          STAT_EVENT(proc_id, PREF_ACC_3);
        } else if(acc > 0.6) {
          STAT_EVENT(proc_id, PREF_ACC_4);
        } else if(acc > 0.5) {
          STAT_EVENT(proc_id, PREF_ACC_5);
        } else if(acc > 0.4) {
          STAT_EVENT(proc_id, PREF_ACC_6);
        } else if(acc > 0.3) {
          STAT_EVENT(proc_id, PREF_ACC_7);
        } else if(acc > 0.2) {
          STAT_EVENT(proc_id, PREF_ACC_8);
        } else if(acc > 0.1) {
          STAT_EVENT(proc_id, PREF_ACC_9);
        } else {
          STAT_EVENT(proc_id, PREF_ACC_10);
        }

        if(timely > 0.9) {
          STAT_EVENT(proc_id, PREF_TIMELY_1);
        } else if(timely > 0.8) {
          STAT_EVENT(proc_id, PREF_TIMELY_2);
        } else if(timely > 0.7) {
          STAT_EVENT(proc_id, PREF_TIMELY_3);
        } else if(timely > 0.6) {
          STAT_EVENT(proc_id, PREF_TIMELY_4);
        } else if(timely > 0.5) {
          STAT_EVENT(proc_id, PREF_TIMELY_5);
        } else if(timely > 0.4) {
          STAT_EVENT(proc_id, PREF_TIMELY_6);
        } else if(timely > 0.3) {
          STAT_EVENT(proc_id, PREF_TIMELY_7);
        } else if(timely > 0.2) {
          STAT_EVENT(proc_id, PREF_TIMELY_8);
        } else if(timely > 0.1) {
          STAT_EVENT(proc_id, PREF_TIMELY_9);
        } else {
          STAT_EVENT(proc_id, PREF_TIMELY_10);
        }


        if(pol > 0.5) {
          STAT_EVENT(proc_id, PREF_POL_1);
        } else if(pol > 0.40) {
          STAT_EVENT(proc_id, PREF_POL_2);
        } else if(pol > 0.25) {
          STAT_EVENT(proc_id, PREF_POL_3);
        } else if(pol > 0.10) {
          STAT_EVENT(proc_id, PREF_POL_4);
        } else if(pol > 0.05) {
          STAT_EVENT(proc_id, PREF_POL_5);
        } else if(pol > 0.01) {
          STAT_EVENT(proc_id, PREF_POL_6);
        } else if(pol > 0.0075) {
          STAT_EVENT(proc_id, PREF_POL_7);
        } else if(pol > 0.005) {
          STAT_EVENT(proc_id, PREF_POL_8);
        } else if(pol > 0.001) {
          STAT_EVENT(proc_id, PREF_POL_9);
        } else {
          STAT_EVENT(proc_id, PREF_POL_10);
        }
      }
    }
    pref.curr_num_ul1_misses = 0;
  }
}


// This function says whether you want to increase/decrease the degree.
// Use only with UPDATE.
HWP_DynAggr pref_get_degfb(uns8 proc_id, uns8 prefetcher_id) {
  HWP_DynAggr ret = AGGR_STAY;
  if(pref.cores[proc_id]->update_acc) {
    pref.cores[proc_id]->update_acc = FALSE;
    float acc                       = pref_get_accuracy(proc_id, prefetcher_id);
    float timely = pref_get_timeliness(proc_id, prefetcher_id);
    float pol    = pref_get_ul1pollution(proc_id);


    STAT_EVENT(proc_id, PREF_UPDATE_COUNT);

    if(PREF_DEGFB_USEONLYLATE) {
      if(timely > PREF_TIMELY_THRESH) {  // NOT TIMELY
        ret = AGGR_INC;
        STAT_EVENT(proc_id, PREF_ACC1_HT_LP);
        if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] <
           PREF_MAX_DEGFB)
          pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]++;
      } else if(timely < PREF_TIMELY_THRESH_2) {  // TOO TIMELY... go down
        ret = AGGR_DEC;
        if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 0)
          pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;
      }
    } else if(PREF_DEGFB_USEONLYPOL) {
      if(pol > PREF_POL_THRESH_1) {
        ret = AGGR_DEC;
        if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 0)
          pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;
      } else if(pol < PREF_POL_THRESH_2) {
        ret = AGGR_INC;
        STAT_EVENT(proc_id, PREF_ACC1_HT_LP);
        if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] <
           PREF_MAX_DEGFB)
          pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]++;
      }
    } else if(acc > PREF_ACC_THRESH_1) {
      if(PREF_DEGFB_USEONLYACC) {
        ret = AGGR_INC;
        STAT_EVENT(proc_id, PREF_ACC1_HT_LP);
        if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] <
           PREF_MAX_DEGFB)
          pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]++;
      } else if(timely < PREF_TIMELY_THRESH) {
        if(pol > PREF_POLPF_THRESH) {  // TIMELY WITH HIGH POL
          STAT_EVENT(proc_id, PREF_ACC1_HT_HP);
          ret = AGGR_DEC;
          if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 0)
            pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;
        } else {  // TIMELY WITH LOW POL
          STAT_EVENT(proc_id, PREF_ACC1_HT_LP);
          ret = AGGR_STAY;
        }
      } else {
        if(pol > PREF_POLPF_THRESH) {  // NOT TIMELY WITH HIGH POL
          STAT_EVENT(proc_id, PREF_ACC1_LT_HP);
          ret = AGGR_INC;
          if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] <
             PREF_MAX_DEGFB)
            pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]++;
        } else {  // NOT TIMELY WITH LOW POL
          STAT_EVENT(proc_id, PREF_ACC1_LT_LP);
          ret = AGGR_INC;
          if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] <
             PREF_MAX_DEGFB)
            pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]++;
        }
      }
    } else if(acc > PREF_ACC_THRESH_2) {
      if(PREF_DEGFB_USEONLYACC) {
        ret = AGGR_STAY;
        STAT_EVENT(proc_id, PREF_ACC2_HT_LP);
      } else if(timely < PREF_TIMELY_THRESH) {
        if(pol > PREF_POLPF_THRESH) {  // TIMELY WITH HIGH POL
          STAT_EVENT(proc_id, PREF_ACC2_HT_HP);
          ret = AGGR_DEC;
          if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 0)
            pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;
        } else {  // TIMELY WITH LOW POL
          STAT_EVENT(proc_id, PREF_ACC2_HT_LP);
          ret = AGGR_STAY;
        }
      } else {
        if(pol > PREF_POLPF_THRESH) {  // NOT TIMELY WITH HIGH POL
          STAT_EVENT(proc_id, PREF_ACC2_LT_HP);
          ret = AGGR_DEC;
          if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 0)
            pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;
        } else {  // NOT TIMELY WITH LOW POL
          STAT_EVENT(proc_id, PREF_ACC2_LT_LP);
          ret = AGGR_INC;
          if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] <
             PREF_MAX_DEGFB)
            pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]++;
        }
      }
    } else if(acc > PREF_ACC_THRESH_3) {
      if(PREF_DEGFB_USEONLYACC) {
        STAT_EVENT(proc_id, PREF_ACC3_HT_LP);
        ret = AGGR_DEC;
        if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 0)
          pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;
      } else if(timely < PREF_TIMELY_THRESH) {
        if(pol > PREF_POLPF_THRESH) {  // TIMELY WITH HIGH POL
          STAT_EVENT(proc_id, PREF_ACC3_HT_HP);
          ret = AGGR_DEC;
          if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 0)
            pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;
        } else {  // TIMELY WITH LOW POL
          STAT_EVENT(proc_id, PREF_ACC3_HT_LP);
          ret = AGGR_DEC;
          if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 0)
            pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;

          // ret = AGGR_STAY; // MAYBE DEC for B/W
        }
      } else {
        if(pol > PREF_POLPF_THRESH) {  // NOT TIMELY WITH HIGH POL
          STAT_EVENT(proc_id, PREF_ACC3_LT_HP);
          ret = AGGR_DEC;
          if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 0)
            pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;
        } else {  // NOT TIMELY WITH LOW POL
          STAT_EVENT(proc_id, PREF_ACC3_LT_LP);
          ret = AGGR_STAY;
        }
      }
    } else {
      if(PREF_DEGFB_USEONLYACC) {
        STAT_EVENT(proc_id, PREF_ACC4_HT_LP);
        ret = AGGR_DEC;
        if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 0)
          pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;
      } else if(timely < PREF_TIMELY_THRESH) {
        if(pol > PREF_POLPF_THRESH) {  // TIMELY WITH HIGH POL
          STAT_EVENT(proc_id, PREF_ACC4_HT_HP);
          ret = AGGR_DEC;
          if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 0)
            pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;
        } else {  // TIMELY WITH LOW POL
          STAT_EVENT(proc_id, PREF_ACC4_HT_LP);
          //		    ret = AGGR_STAY; // MAYBE DEC FOR BW
          ret = AGGR_STAY;
        }
      } else {
        if(pol > PREF_POLPF_THRESH) {  // NOT TIMELY WITH HIGH POL
          STAT_EVENT(proc_id, PREF_ACC4_LT_HP);
          ret = AGGR_DEC;
          if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 0)
            pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;
        } else {  // NOT TIMELY WITH LOW POL
          STAT_EVENT(proc_id, PREF_ACC4_LT_LP);
          ret = AGGR_DEC;
          if(pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id] > 0)
            pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]--;
        }
      }
    }

    STAT_EVENT(proc_id,
               PREF_DISTANCE_1 +
                 pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id]);

    pref.phase++;
    if(PREF_DEGFB_STATPHASEFILE) {
      fprintf(PREF_DEGFB_FILE, "%d   %d\n",
              pref_table[prefetcher_id].hwp_info->dyn_degree_core[proc_id],
              pref.phase);
    }
  }

  return ret;
}

float pref_get_accuracy(uns8 proc_id, uns8 prefetcher_id) {
  float acc;
  if(PREF_UPDATE_INTERVAL != 0) {
    acc = (pref_table[prefetcher_id].hwp_info->sent_core[proc_id] > 20) ?
            ((float)pref_table[prefetcher_id].hwp_info->useful_core[proc_id] /
             (float)pref_table[prefetcher_id].hwp_info->sent_core[proc_id]) :
            1.0;
  } else {
    acc =
      (pref_table[prefetcher_id].hwp_info->curr_sent_core[proc_id] > 100) ?
        ((float)pref_table[prefetcher_id].hwp_info->curr_useful_core[proc_id] /
         (float)pref_table[prefetcher_id].hwp_info->curr_sent_core[proc_id]) :
        1.0;
  }
  return acc;
}

float pref_get_timeliness(uns8 proc_id, uns8 prefetcher_id) {
  float timely = 0.0;
  if(PREF_UPDATE_INTERVAL != 0) {
    timely =
      (pref_table[prefetcher_id].hwp_info->useful_core[proc_id] > 100) ?
        ((float)pref_table[prefetcher_id].hwp_info->late_core[proc_id] /
         (float)pref_table[prefetcher_id].hwp_info->useful_core[proc_id]) :
        1.0;
  } else {
    timely =
      (pref_table[prefetcher_id].hwp_info->curr_useful_core[proc_id] > 100) ?
        ((float)pref_table[prefetcher_id].hwp_info->curr_late_core[proc_id] /
         (float)pref_table[prefetcher_id].hwp_info->curr_useful_core[proc_id]) :
        1.0;
  }
  return timely;
}

float pref_get_ul1pollution(uns8 proc_id) {
  float pol;
  if(PREF_UPDATE_INTERVAL != 0) {
    // CMP I changed this one with unified total num of misses
    pol = (float)pref.cores[proc_id]->pfpol / (float)(pref.num_ul1_misses);
  } else {
    pol = (((pref.cores[proc_id]->curr_ul1_misses) > 1000) ?
             ((float)pref.cores[proc_id]->curr_pfpol /
              (float)(pref.cores[proc_id]->curr_ul1_misses)) :
             0.0);
  }

  return pol;
}

// BE

void pref_req_drop_process(uns8 proc_id, uns8 prefetcher_id) {
  ASSERT(0, PREF_FRAMEWORK_ON);
  ASSERT(0, prefetcher_id);

  if(pref_table[prefetcher_id].hwp_info->curr_sent_core != 0)
    pref_table[prefetcher_id].hwp_info->curr_sent_core[proc_id]--;
}
