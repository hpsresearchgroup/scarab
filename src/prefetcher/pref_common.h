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
 * File         : pref_common.h
 * Author       : HPS Research Group
 * Date         : 11/30/2004
 * Description  : Common framework for working with prefetchers - less stuff to
 *mess with
 ***************************************************************************************/
#ifndef __PREF_COMMON_H__
#define __PREF_COMMON_H__

#include "memory/mem_req.h"

#define PREF_TRACKERS_NUM 16

// typedef in globals/global_types.h
struct Pref_Mem_Req_struct {
  uns8 proc_id;
  Addr line_addr;
  Addr line_index;

  Addr    loadPC;
  uns32   global_hist;  // Used for perf hfilter
  uns8    prefetcher_id;
  uns     distance;
  Flag    valid;
  Flag    bw_limited;
  Counter rdy_cycle;  // Move this out
};

typedef struct Pref_Polbv_Info_struct {
  uns8 proc_id;
  Flag pollution;
} Pref_Polbv_Info;

// typedef in globals/global_types.h
struct HWP_Info_struct {
  uns8 id;        // This prefetcher's id
  Flag enabled;   // Is the prefetcher enabled
  int  priority;  // priority this prefetcher gets in the pecking order

  // Feedback direted prefetching
  Counter* useful_core;  // num of useful prefetches per core
  Counter* sent_core;    // num of sent prefetches per core
  Counter* late_core;    // num of late prefetches per core

  // These are the counts for the current time slice
  Counter* curr_useful_core;  // num of Useful prefetches in currect per core
  Counter* curr_sent_core;
  Counter* curr_late_core;

  uns* dyn_degree_core;
};

typedef enum HWP_Type_enum {
  PREF_TO_UL1,
  PREF_TO_UMLC,
  PREF_TO_DL0,
} HWP_Type;

typedef enum HWP_DynAggr_enum {
  AGGR_DEC,
  AGGR_STAY,
  AGGR_INC,
} HWP_DynAggr;

// typedef in globals/global_types.h
struct HWP_struct {
  char const* const name;
  HWP_Type          hwp_type;

  HWP_Info* hwp_info;

  void (*init_func)(struct HWP_struct* hwp);  // initialize the hw prefetcher
                                              // and set enable if turned on

  void (*done_func)(void);                  // called before exiting
  void (*per_core_done_func)(uns proc_id);  // cores may dump stats at different
                                            // times, hence this function

  void (*dl0_miss_func)(Addr lineAddr,
                        Addr loadPC);                // always check loadPC != 0
  void (*dl0_hit_func)(Addr lineAddr, Addr loadPC);  //
  void (*dl0_pref_hit)(Addr lineAddr, Addr loadPC);

  void (*umlc_miss_func)(uns8 proc_id, Addr lineAddr, Addr loadPC,
                         uns32 global_hist);  // called when a umlc access
                                              // misses
  void (*umlc_hit_func)(uns8 proc_id, Addr lineAddr, Addr loadPC,
                        uns32 global_hist);  // called when a umlc access hits
  void (*umlc_pref_hit)(uns8 proc_id, Addr lineAddr, Addr loadPC,
                        uns32 global_hist);  // called when a umlc access hits a
                                             // prefetched line for the first
                                             // time

  void (*ul1_miss_func)(uns8 proc_id, Addr lineAddr, Addr loadPC,
                        uns32 global_hist);  // called when a ul1 access misses
  void (*ul1_hit_func)(uns8 proc_id, Addr lineAddr, Addr loadPC,
                       uns32 global_hist);  // called when a ul1 access hits
  void (*ul1_pref_hit)(uns8 proc_id, Addr lineAddr, Addr loadPC,
                       uns32 global_hist);  // called when a ul1 access hits a
                                            // prefetched line for the first
                                            // time
};

/* Per core prefetching data */
typedef struct HWP_Core_struct {
  Pref_Mem_Req* dl0req_queue;    // L1 req queue
  Pref_Mem_Req* umlc_req_queue;  // MLC req queue
  Pref_Mem_Req* ul1req_queue;    // L2 req queue

  int dl0req_queue_req_pos;
  int dl0req_queue_send_pos;

  int umlc_req_queue_req_pos;
  int umlc_req_queue_send_pos;

  int ul1req_queue_req_pos;
  int ul1req_queue_send_pos;

  Counter ul1_misses;
  Counter curr_ul1_misses;

  Counter          pfpol;
  Counter          curr_pfpol;
  Pref_Polbv_Info* pref_polbv_info;  // This is the pollution bitvector per core

  Flag update_acc;  // Do we need to recompute acc? (used in "update" driven
                    // approach)

  // Zhuang and Lee's hardware prefetching filter similar to gshare
  uns8* pref_hfilter_pht;
} HWP_Core;

/* System-wide prefetching data */
typedef struct HWP_Common_struct {
  HWP_Core*  cores_array;  // actual data structures
  HWP_Core** cores;        // array of pointers (this indirection makes
                           // PREF_SHARED_QUEUES easier to implement)

  // Feedback direted prefetching
  // For pollution metric
  Counter num_ul1_evicted;

  Counter num_ul1_misses;
  Counter curr_num_ul1_misses;

  uns phase;
} HWP_Common;


/**************************************************************/
/* Framework interface */

void pref_init(void);
void pref_done(void);
void pref_per_core_done(uns proc_id);

void pref_dl0_miss(Addr line_addr, Addr load_PC);
void pref_dl0_hit(Addr line_addr, Addr load_PC);
void pref_dl0_pref_hit(Addr line_addr, Addr load_PC, uns8 prefetcher_id);

void pref_umlc_miss(uns8 proc_id, Addr line_addr, Addr load_PC,
                    uns32 global_hist);
void pref_umlc_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                   uns32 global_hist);

void pref_umlc_pref_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                        uns32 global_hist, int lru_position,
                        uns8 prefetcher_id);
void pref_umlc_pref_hit_late(uns8 proc_id, Addr line_addr, Addr load_PC,
                             uns32 global_hist, uns8 prefetcher_id);

void pref_ul1_miss(uns8 proc_id, Addr line_addr, Addr load_PC,
                   uns32 global_hist);
void pref_ul1_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                  uns32 global_hist);

void pref_ul1_pref_hit(uns8 proc_id, Addr line_addr, Addr load_PC,
                       uns32 global_hist, int lru_position, uns8 prefetcher_id);
void pref_ul1_pref_hit_late(uns8 proc_id, Addr line_addr, Addr load_PC,
                            uns32 global_hist, uns8 prefetcher_id);

void pref_update(void);

// returns true if req hits in the req queue. It also invalidates the request in
// the pref queue.
Flag pref_dl0req_queue_filter(Addr line_addr);
Flag pref_umlc_req_queue_filter(Addr line_addr);
Flag pref_ul1req_queue_filter(Addr line_addr);
Flag pref_ul1req_queue_match(Addr line_addr);  // doesn't invalidate

// returns true if the req was added/matched an existing req.
//         false if queue was full
Flag pref_addto_dl0req_queue(uns8 proc_id, Addr line_index, uns8 prefetcher_id);
Flag pref_addto_umlc_req_queue(uns8 proc_id, Addr line_index,
                               uns8 prefetcher_id);
Flag pref_addto_ul1req_queue(uns8 proc_id, Addr line_index, uns8 prefetcher_id);
Flag pref_addto_ul1req_queue_set(uns8 proc_id, Addr line_index,
                                 uns8 prefetcher_id, uns distance,
                                 Addr loadAddr, uns32 global_hist, Flag bw);

// prefetch missed in the ul1 and went out on the bus
void pref_ul1sent(uns8 proc_id, Addr addr, uns8 prefetcher_id);

/*************************************************************/
/* Misc functions */
int         pref_compare_hwp_priority(const void* const a, const void* const b);
float       pref_get_accuracy(uns8 proc_id, uns8 prefetcher_id);
float       pref_get_timeliness(uns8 proc_id, uns8 prefetcher_id);
HWP_DynAggr pref_get_degfb(uns8 proc_id, uns8 prefetcher_id);


float pref_get_overallaccuracy(HWP_Type);
float pref_get_ul1pollution(uns8 proc_id);

float pref_get_replaccuracy(uns8 prefetcher_id);

int pref_compare_prefloadhash(const void* const a, const void* const b);

void pref_evictline_notused(uns8 proc_id, Addr addr, Addr loadPC,
                            uns32 global_hist);
void pref_evictline_used(uns8 proc_id, Addr addr, Addr loadPC,
                         uns32 global_hist);
Flag pref_hfilter_pred_useless(uns8 proc_id, Addr addr, Addr loadPC,
                               uns32 global_hist);
void pref_hfilter_pht_reset(void);

void pref_ul1evict(uns8 proc_id, Addr addr);
void pref_ul1evictOnPF(uns8 pref_proc_id, uns8 evicted_proc_id, Addr addr);

float pref_get_regionbased_acc(void);

void pref_req_drop_process(uns8 proc_id, uns8 prefetcher_id);

#endif /*  __PREF_COMMON_H__*/
