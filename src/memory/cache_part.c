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
 * File         : cache_part.c
 * Author       : HPS Research Group
 * Date         : 2/19/2014
 * Description  : Shared cache partitioning mechanisms
 ***************************************************************************************/

#include <math.h>

#include "core.param.h"
#include "debug/debug_macros.h"
#include "globals/assert.h"
#include "globals/global_types.h"
#include "libs/cache_lib.h"
#include "memory/cache_part.h"
#include "memory/mem_req.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "stat_mon.h"
#include "statistics.h"
#include "trigger.h"

/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_CACHE_PART, ##args)

/**************************************************************************************/
/* Types */

typedef struct Proc_Info_struct {
  Cache   shadow_cache;
  double* miss_rates;  // indexed by number of ways - 1
} Proc_Info;

typedef struct Shadow_Cache_Data_struct {
  Flag prefetched;
} Shadow_Cache_Data;

typedef double (*Metric_Func)(uns*);
typedef void (*Search_Func)(void);

/**************************************************************************************/
/* Global variables */

Proc_Info*  proc_infos;
Trigger*    l1_part_start;
Trigger*    l1_part_trigger;
Stat_Mon*   stat_mon;
Metric_Func metric_func;
Search_Func search_func;
uns*        current_partition;  // actual enforced partition
uns*        new_partition;      // pre-allocated structure for new partition
uns* temp_partition;  // pre-allocated structure for partition exploration
uns  tie_breaker_proc_id;

/**************************************************************************************/
/* Enums */

DEFINE_ENUM(Cache_Part_Metric, CACHE_PART_METRIC_LIST);
DEFINE_ENUM(Cache_Part_Search, CACHE_PART_SEARCH_LIST);

/**************************************************************************************/
/* Local Prototypes */

static Flag   in_shadow_cache(Addr addr);
static double get_global_miss_rate(uns* partition);
static double get_miss_rate_sum(uns* partition);
static double get_gmean_perf(uns* partition);
static double get_best_marginal_utility(uns* partition, uns proc_id,
                                        uns balance, uns* extra_ways);
static void   measure_miss_curves(void);
static void   search_lookahead(void);
static void   search_bruteforce(void);
static void   set_partition(void);
static void   debug_cache_part(uns* old_partition, uns* new_partition);

/**************************************************************************************/
/* cache_part_init: */

void cache_part_init(void) {
  if(!L1_PART_ON)
    return;

  ASSERTM(0, !PRIVATE_L1, "Cache partitioning works only on shared cache.\n");
  ASSERT(0, L1_CACHE_REPL_POLICY == REPL_PARTITION);
  ASSERT(0, L1_ASSOC <= 16);

  proc_infos = calloc(NUM_CORES, sizeof(Proc_Info));
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Proc_Info* proc_info = &proc_infos[proc_id];
    char       buf[MAX_STR_LENGTH];
    sprintf(buf, "SHADOW L1[%d]", proc_id);
    init_cache(&proc_info->shadow_cache, buf, L1_SIZE, L1_ASSOC, L1_LINE_SIZE,
               sizeof(Shadow_Cache_Data), REPL_TRUE_LRU);
    proc_info->miss_rates = calloc(L1_ASSOC, sizeof(double));
  }

  l1_part_trigger = trigger_create("L1 PART TRIGGER", L1_PART_TRIGGER,
                                   TRIGGER_REPEAT);
  l1_part_start = trigger_create("L1 PART START", L1_PART_START, TRIGGER_ONCE);
  Stat_Enum monitored_stats[] = {
    NODE_CYCLE,
    RET_BLOCKED_L1_MISS,
    CORE_MEM_BLOCKED,
    L1_SHADOW_ACCESS_STALLING,
    L1_SHADOW_ACCESS_DEMAND,
    L1_SHADOW_STALLING_HIT_POS0,
    L1_SHADOW_STALLING_HIT_POS1,
    L1_SHADOW_STALLING_HIT_POS2,
    L1_SHADOW_STALLING_HIT_POS3,
    L1_SHADOW_STALLING_HIT_POS4,
    L1_SHADOW_STALLING_HIT_POS5,
    L1_SHADOW_STALLING_HIT_POS6,
    L1_SHADOW_STALLING_HIT_POS7,
    L1_SHADOW_STALLING_HIT_POS8,
    L1_SHADOW_STALLING_HIT_POS9,
    L1_SHADOW_STALLING_HIT_POS10,
    L1_SHADOW_STALLING_HIT_POS11,
    L1_SHADOW_STALLING_HIT_POS12,
    L1_SHADOW_STALLING_HIT_POS13,
    L1_SHADOW_STALLING_HIT_POS14,
    L1_SHADOW_STALLING_HIT_POS15,
    L1_SHADOW_DEMAND_HIT_POS0,
    L1_SHADOW_DEMAND_HIT_POS1,
    L1_SHADOW_DEMAND_HIT_POS2,
    L1_SHADOW_DEMAND_HIT_POS3,
    L1_SHADOW_DEMAND_HIT_POS4,
    L1_SHADOW_DEMAND_HIT_POS5,
    L1_SHADOW_DEMAND_HIT_POS6,
    L1_SHADOW_DEMAND_HIT_POS7,
    L1_SHADOW_DEMAND_HIT_POS8,
    L1_SHADOW_DEMAND_HIT_POS9,
    L1_SHADOW_DEMAND_HIT_POS10,
    L1_SHADOW_DEMAND_HIT_POS11,
    L1_SHADOW_DEMAND_HIT_POS12,
    L1_SHADOW_DEMAND_HIT_POS13,
    L1_SHADOW_DEMAND_HIT_POS14,
    L1_SHADOW_DEMAND_HIT_POS15,
  };
  stat_mon = stat_mon_create_from_array(monitored_stats,
                                        NUM_ELEMENTS(monitored_stats));

  switch(L1_PART_METRIC) {
    case CACHE_PART_METRIC_GLOBAL_MISS_RATE:
      metric_func = &get_global_miss_rate;
      break;
    case CACHE_PART_METRIC_MISS_RATE_SUM:
      metric_func = &get_miss_rate_sum;
      break;
    case CACHE_PART_METRIC_GMEAN_PERF:
      metric_func = &get_gmean_perf;
      break;
    default:
      FATAL_ERROR(0, "Unknown metric %s\n",
                  Cache_Part_Metric_str(L1_PART_METRIC));
      break;
  }

  switch(L1_PART_SEARCH) {
    case CACHE_PART_SEARCH_LOOKAHEAD:
      search_func = &search_lookahead;
      break;
    case CACHE_PART_SEARCH_BRUTE_FORCE:
      search_func = &search_bruteforce;
      break;
    default:
      FATAL_ERROR(0, "Unknown search algorithm %s\n",
                  Cache_Part_Search_str(L1_PART_METRIC));
      break;
  }

  current_partition = calloc(NUM_CORES, sizeof(uns));
  ASSERT(0, L1_ASSOC % NUM_CORES == 0);
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    current_partition[proc_id] = L1_ASSOC / NUM_CORES;
    set_partition_allocate(&mem->uncores[0].l1->cache, proc_id,
                           current_partition[proc_id]);
    GET_STAT_EVENT(proc_id, NORESET_L1_PARTITION) = current_partition[proc_id];
  }
  new_partition       = calloc(NUM_CORES, sizeof(uns));
  temp_partition      = calloc(NUM_CORES, sizeof(uns));
  tie_breaker_proc_id = 0;
}

/**************************************************************************************/
/* cache_part_l1_access: */

void cache_part_l1_access(Mem_Req* req) {
  if(!L1_PART_ON)
    return;
  if(!in_shadow_cache(req->addr))
    return;

  Proc_Info* proc_info = &proc_infos[req->proc_id];
  Addr       dummy_line_addr;
  int  pos = cache_find_pos_in_lru_stack(&proc_info->shadow_cache, req->proc_id,
                                        req->addr, &dummy_line_addr);
  Flag miss         = (pos == -1);
  Flag untimely_hit = FALSE;
  Flag stalling     = mem_req_type_is_stalling(req->type);
  Flag demand       = mem_req_type_is_demand(req->type);
  if(!miss && L1_PART_FILL_DELAY) {
    L1_Data* data = (L1_Data*)cache_access(&proc_info->shadow_cache, req->addr,
                                           &dummy_line_addr, FALSE);
    ASSERT(req->proc_id, data);
    untimely_hit = data->fetch_cycle > freq_cycle_count(FREQ_DOMAIN_L1);
  }
  STAT_EVENT(req->proc_id, L1_SHADOW_ACCESS);
  if(stalling)
    STAT_EVENT(req->proc_id, L1_SHADOW_ACCESS_STALLING);
  if(demand)
    STAT_EVENT(req->proc_id, L1_SHADOW_ACCESS_DEMAND);
  if(!miss && !untimely_hit) {
    STAT_EVENT(req->proc_id, L1_SHADOW_HIT_POS0 + MIN2(pos, 15));
    if(stalling)
      STAT_EVENT(req->proc_id, L1_SHADOW_STALLING_HIT_POS0 + MIN2(pos, 15));
    if(demand)
      STAT_EVENT(req->proc_id, L1_SHADOW_DEMAND_HIT_POS0 + MIN2(pos, 15));
  }
  INC_STAT_EVENT(req->proc_id, L1_SHADOW_HIT, !miss);
  INC_STAT_EVENT(req->proc_id, L1_SHADOW_HIT_STALLING, stalling && !miss);
  INC_STAT_EVENT(req->proc_id, L1_SHADOW_HIT_DEMAND, demand && !miss);
  INC_STAT_EVENT(req->proc_id, L1_SHADOW_UNTIMELY_HIT, untimely_hit);
  INC_STAT_EVENT(req->proc_id, L1_SHADOW_UNTIMELY_HIT_STALLING,
                 stalling && untimely_hit);
  INC_STAT_EVENT(req->proc_id, L1_SHADOW_UNTIMELY_HIT_DEMAND,
                 demand && untimely_hit);
  if(miss) {
    L1_Data* data     = cache_insert(&proc_info->shadow_cache, req->proc_id,
                                 req->addr, &dummy_line_addr, &dummy_line_addr);
    data->fetch_cycle = freq_cycle_count(FREQ_DOMAIN_L1) +
                        (stalling || req->type == MRT_WB ? 0 :
                                                           L1_PART_FILL_DELAY);
  } else {
    cache_access(&proc_info->shadow_cache, req->addr, &dummy_line_addr, TRUE);
  }
}

/**************************************************************************************/
/* cache_part_l1_warmup: */

void cache_part_l1_warmup(uns proc_id, Addr addr) {
  Proc_Info* proc_info = &proc_infos[proc_id];
  Addr       dummy_line_addr;
  L1_Data*   data = (L1_Data*)cache_access(&proc_info->shadow_cache, addr,
                                         &dummy_line_addr, TRUE);
  if(!data) {
    L1_Data* data     = cache_insert(&proc_info->shadow_cache, proc_id, addr,
                                 &dummy_line_addr, &dummy_line_addr);
    data->fetch_cycle = 0;
  }
}

/**************************************************************************************/
/* cache_part_update: */

void cache_part_update(void) {
  if(!L1_PART_ON)
    return;

  if(trigger_fired(l1_part_start)) {
    ASSERT(0, mem->uncores[0].l1->cache.repl_policy == REPL_TRUE_LRU);
    mem->uncores[0].l1->cache.repl_policy = REPL_PARTITION;
  }
  if(!trigger_fired(l1_part_trigger))
    return;

  DEBUG(0, "Cache partition triggered\n");
  if(trigger_on(l1_part_start)) {
    measure_miss_curves();
    set_partition();
  }
  stat_mon_reset(stat_mon);
}

/**************************************************************************************/
/* Is the line with specified addr tracked in the shadow cache? */

Flag in_shadow_cache(Addr addr) {
  Addr dummy_addr;
  uns  set = ext_cache_index(&proc_infos[0].shadow_cache, addr, &dummy_addr,
                            &dummy_addr);
  return set % L1_SHADOW_TAGS_MODULO == 0;
}

/**************************************************************************************/
/* Measure miss curves from monitored statistics */

void measure_miss_curves(void) {
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Proc_Info* proc_info   = &proc_infos[proc_id];
    uns        access_stat = L1_PART_USE_STALLING ? L1_SHADOW_ACCESS_STALLING :
                                             L1_SHADOW_ACCESS_DEMAND;
    uns pos0_hit_stat = L1_PART_USE_STALLING ? L1_SHADOW_STALLING_HIT_POS0 :
                                               L1_SHADOW_DEMAND_HIT_POS0;
    Counter shadow_accesses   = stat_mon_get_count(stat_mon, proc_id,
                                                 access_stat);
    Counter shadow_misses_sum = shadow_accesses;
    for(uns ii = 0; ii < L1_ASSOC - 1; ii++) {
      Counter way_hits = stat_mon_get_count(stat_mon, proc_id,
                                            pos0_hit_stat + ii);
      shadow_misses_sum -= way_hits;
      proc_info->miss_rates[ii] = (double)shadow_misses_sum /
                                  (double)shadow_accesses;
    }
  }
}

/**************************************************************************************/
/* Find best marginal utility using lookahead method (Algorithm 2) of Moin's
 * paper.
 * Qureshi, Moinuddin K., and Yale N. Patt. "Utility-based cache partitioning:
 * A low-overhead, high-performance, runtime mechanism to partition shared
 * caches." 2006 39th Annual IEEE/ACM International Symposium on
 * Microarchitecture (MICRO'06). IEEE, 2006.  */

double get_best_marginal_utility(uns* partition, uns proc_id, uns balance,
                                 uns* extra_ways) {
  uns old_ways = partition[proc_id];
  uns max_ways = old_ways + balance;
  ASSERT(0, max_ways <= L1_ASSOC);
  double cur_metric = metric_func(partition);
  double best_mu    = 0.0;
  uns    best_ways  = old_ways;
  for(uns ways = old_ways + 1; ways <= max_ways; ways++) {
    partition[proc_id] = ways;
    double new_metric  = metric_func(partition);
    double mu          = (new_metric - cur_metric) / (double)(ways - old_ways);
    if(mu < best_mu) {
      best_mu   = mu;
      best_ways = ways;
    }
  }
  partition[proc_id] = old_ways;
  *extra_ways        = best_ways - old_ways;
  return best_mu;
}

/**************************************************************************************/
/* Use brute force method to estimate best partition */

void search_bruteforce(void) {
  uns* partition = new_partition;
  Flag done      = FALSE;
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    partition[proc_id] = 1;
  }
  if(NUM_CORES == L1_ASSOC)
    return;
  double best_metric    = 1.0e99;
  uns*   best_partition = temp_partition;
  while(!done) {
    /* make sure partition is correct */
    uns sum = 0;
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      sum += partition[proc_id];
    }
    ASSERT(0, sum <= L1_ASSOC);
    partition[NUM_CORES - 1] += L1_ASSOC - sum;

    /* check the metric for the partition */
    double metric = metric_func(partition);
    if(ENABLE_GLOBAL_DEBUG_PRINT && DEBUG_RANGE_COND(0)) {
      char  buf[MAX_STR_LENGTH];
      char* ptr = buf;
      ptr += sprintf(buf, "{");
      for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
        ptr += sprintf(ptr, " %d", partition[proc_id]);
      }
      ptr += sprintf(ptr, "}: %.4f\n", metric);
      DPRINTF("%s", buf);
    }
    if(metric < best_metric) {
      best_metric = metric;
      memcpy(best_partition, partition, NUM_CORES * sizeof(uns));
    }

    /* generate next partition */
    uns proc_id;
    for(proc_id = NUM_CORES - 1; proc_id > 0; proc_id--) {
      if(partition[proc_id] != 1)
        break;
    }
    if(proc_id == 0) {
      done = TRUE;
    } else {
      partition[proc_id] = 1;
      partition[proc_id - 1]++;
    }
  }
  memcpy(partition, best_partition, NUM_CORES * sizeof(uns));
  ASSERT(0, best_metric != 1.0e99);
}

/**************************************************************************************/
/* Use lookahead method to estimate best partition */

void search_lookahead(void) {
  uns* partition            = new_partition;
  uns  total_ways_allocated = 0;
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    partition[proc_id] = 1;
    total_ways_allocated++;
  }

  while(total_ways_allocated < L1_ASSOC) {
    uns    balance         = L1_ASSOC - total_ways_allocated;
    double best_mu         = 1.0e99;
    uns    best_proc_id    = NUM_CORES;
    uns    best_extra_ways = 0;
    DEBUG(0, "Balance %d\n", balance);
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      uns    extra_ways;
      double mu = get_best_marginal_utility(partition, proc_id, balance,
                                            &extra_ways);
      DEBUG(0, "Marginal util of core %d: %.4f (%d ways)\n", proc_id, mu,
            extra_ways);
      if(mu < best_mu) {
        best_proc_id    = proc_id;
        best_mu         = mu;
        best_extra_ways = extra_ways;
      }
    }
    ASSERT(0, best_mu != 1.0e99);
    if(best_extra_ways == 0) {
      best_proc_id        = tie_breaker_proc_id;
      tie_breaker_proc_id = (tie_breaker_proc_id + 1) % NUM_CORES;
      best_extra_ways     = 1;
    }
    ASSERT(0, best_proc_id != NUM_CORES);
    partition[best_proc_id] += best_extra_ways;
    total_ways_allocated += best_extra_ways;
    DEBUG(0, "Gave %d ways to core %d, marginal util: %.4f\n", best_extra_ways,
          best_proc_id, best_mu);
  }
}

/**************************************************************************************/
/* Set target partition */

void set_partition(void) {
  search_func();

  if(ENABLE_GLOBAL_DEBUG_PRINT && DEBUG_RANGE_COND(0)) {
    debug_cache_part(current_partition, new_partition);
  }

  /* set up the estimated best partition */
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    set_partition_allocate(&mem->uncores[0].l1->cache, proc_id,
                           new_partition[proc_id]);
    current_partition[proc_id]                    = new_partition[proc_id];
    GET_STAT_EVENT(proc_id, NORESET_L1_PARTITION) = new_partition[proc_id];
  }
  STAT_EVENT_ALL(L1_PARTITION_INTERVALS);
}

void debug_cache_part(uns* old_partition, uns* new_partition) {
  char  buf[MAX_STR_LENGTH];
  char* ptr = buf;
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    ptr += sprintf(ptr, "%d,", new_partition[proc_id]);
    DPRINTF("Miss curve[%d]:", proc_id);
    for(uns ii = 0; ii < L1_ASSOC - 1; ii++) {
      DPRINTF(" %.4f", proc_infos[proc_id].miss_rates[ii]);
    }
    DPRINTF("\n");
  }
  DPRINTF("New partition {%s}, metric %.4f -> %.4f\n", buf,
          metric_func(old_partition), metric_func(new_partition));
}

/**************************************************************************************/
/* get global miss rate */

double get_global_miss_rate(uns* partition) {
  double sum = 0.0;
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Proc_Info* proc_info = &proc_infos[proc_id];
    Counter    accesses  = stat_mon_get_count(stat_mon, proc_id,
                                          L1_PART_USE_STALLING ?
                                            L1_SHADOW_ACCESS_STALLING :
                                            L1_SHADOW_ACCESS_DEMAND);
    sum += proc_info->miss_rates[partition[proc_id]] * (double)accesses;
  }
  return sum;
}

/**************************************************************************************/
/* get miss rate sum */

double get_miss_rate_sum(uns* partition) {
  double sum = 0.0;
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Proc_Info* proc_info = &proc_infos[proc_id];
    sum += proc_info->miss_rates[partition[proc_id]];
  }
  return sum;
}

/**************************************************************************************/
/* get negative gmean of core performance (negative because we minimize the
 * metric) */

double get_gmean_perf(uns* partition) {
  double product = 1.0;
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    /* Assuming constant stall time per miss and constant compute time per
       access:

          stall time    misses      compute time       time
          ---------- x --------  +  ------------  =  --------
            misses     accesses       accesses       accesses

          stall time   miss rate    compute time       time
           per miss                   per miss      per access

           CONSTANT    VARIABLE       CONSTANT       VARIABLE

       From this model, we can derive that normalized performance
       given a new vs old miss rate is the *reciprocal* of:

               / new miss rate     \
           1 + | ------------- - 1 | x stall frac
               \ old miss rate     /
    */
    Proc_Info* proc_info  = &proc_infos[proc_id];
    double     stall_frac = (double)stat_mon_get_count(stat_mon, proc_id,
                                                   RET_BLOCKED_L1_MISS) /
                        (double)stat_mon_get_count(stat_mon, proc_id,
                                                   NODE_CYCLE);
    double miss_rate0 = proc_info->miss_rates[current_partition[proc_id]];
    double miss_rate  = proc_info->miss_rates[partition[proc_id]];
    double pred_perf;
    if(miss_rate0 == 0.0 || stall_frac == 0.0) {
      // in case of zero misses or stall time make the smallest
      // partition most attractive
      if(partition[proc_id] == 1) {
        pred_perf = 1.0;
      } else {
        pred_perf = 0.0;
      }
    } else {
      pred_perf = 1.0 / (1.0 + (miss_rate / miss_rate0 - 1) * stall_frac);
    }
    product *= pred_perf;
  }
  return -product;
}
