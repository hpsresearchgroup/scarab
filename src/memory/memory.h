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
 * File         : memory/memory.h
 * Author       : HPS Research Group
 * Date         : 3/30/2004
 * Description  : An improved memory model with filtering, promotion, and bus
 *modeling
 ***************************************************************************************/

#ifndef __MEMORY_H__
#define __MEMORY_H__

#include "freq.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "libs/cache_lib.h"
#include "libs/hash_lib.h"
#include "libs/list_lib.h"
#include "libs/port_lib.h"
#include "memory/mem_req.h"
#include "op_info.h"
//#include "dram.h"

/**************************************************************************************/
/* Defines */

//#define MAX_OPS_PER_REQ    2048


/**************************************************************************************/
/* Types */

typedef struct L1_Data_struct {
  uns8  proc_id;       /* processor id that generated this miss */
  Flag  dirty;         /* is the line dirty? */
  Flag  prefetch;      /* was the line prefetched? */
  Flag  seen_prefetch; /* have we counted this prefetch earlier */
  uns   pref_distance;
  Addr  pref_loadPC;
  uns32 global_hist;   /* used for prefetch hfilter */
  uns8  prefetcher_id; /* which Prefetcher sent this prefetch */
  Flag  dcache_touch;  /* does dcache touch? for measuring useless prefetch */
  Flag  fetched_by_offpath;             /* fetched by an off_path op? */
  Flag  l0_modified_fetched_by_offpath; /* fetched by an off_path op? */
  Addr  offpath_op_addr; /* PC of the off path op that fetched this line */
  Counter
    offpath_op_unique; /* unique of the off path op that fetched this line */

  Counter mlc_miss_latency; /* memory latency the request for this line
                               experienced */
  Counter l1miss_latency;   /* memory latency the request for this line
                               experienced */
  Counter fetch_cycle;
  Counter onpath_use_cycle;
} L1_Data;

typedef L1_Data MLC_Data; /* Use the same data structure for simplicity */

typedef enum Mem_Queue_Type_enum {
  QUEUE_L1        = 1 << 0,
  QUEUE_BUS_OUT   = 1 << 1,
  QUEUE_MEM       = 1 << 2,
  QUEUE_L1FILL    = 1 << 3,
  QUEUE_MLC       = 1 << 4,
  QUEUE_MLC_FILL  = 1 << 5,
  QUEUE_CORE_FILL = 1 << 6,
} Mem_Queue_Type;

typedef struct Mem_Queue_Entry_struct {
  int     reqbuf;   /* request buffer num */
  Counter priority; /* priority of the miss */
  Counter rdy_cycle;
} Mem_Queue_Entry;

typedef struct Mem_Queue_struct {
  Mem_Queue_Entry* base;
  int              entry_count;
  int              reserved_entry_count; /* for HIER_MSHR_ON */
  uns              size;
  char             name[20];
  Mem_Queue_Type   type;
} Mem_Queue;

typedef struct Mem_Bank_Queue_Entry_struct {
  uns8         proc_id;
  uns          index;
  Counter      priority;
  Counter      bank_priority;
  Addr         addr;
  Mem_Req_Type type;
} Mem_Bank_Queue_Entry;

typedef struct Mem_Bank_Queue_struct {
  Mem_Bank_Queue_Entry* base;
  uns                   entry_count;
  uns                   demand_count;
} Mem_Bank_Queue;

typedef struct Ported_Cache_struct {
  struct Cache_struct  cache;
  struct Ports_struct* ports;
  uns                  num_banks;
} Ported_Cache;

typedef struct Uncore_struct {
  Ported_Cache* mlc;
  Ported_Cache* l1;
  uns           num_outstanding_l1_accesses;
  uns           num_outstanding_l1_misses;
  Counter       mem_block_start;
} Uncore;

typedef struct Memory_struct {
  /* miss buffer */
  Mem_Req* req_buffer;
  List     req_buffer_free_list;
  List*    l1_in_buffer_core;
  uns      total_mem_req_buffers;
  uns*     num_req_buffers_per_core;

  int req_count;

  /* uncore (includes MLC and L1) */
  Uncore* uncores;

  /* prfetcher cache */
  Cache pref_l1_cache;

  /* various queues (arrays) */
  Mem_Queue  mlc_queue;
  Mem_Queue  mlc_fill_queue;
  Mem_Queue  l1_queue;
  Mem_Queue  bus_out_queue;
  Mem_Queue  l1fill_queue;
  Mem_Queue* core_fill_queues;

  Counter last_mem_queue_cycle;

  /* PREF_ANALYZE_LOAD study */
  Hash_Table*       pref_loadPC_hash;
  Cache_Insert_Repl pref_replpos;

  // BE
  double* l1_ave_num_ways_per_core;

  // CP
  Cache*   umon_cache_core;
  double** umon_cache_hit_count_core;

  uns*  bus_out_queue_entry_count_core;
  int*  bus_out_queue_index_core;  // bus_out_queue to mem_queue scheduling
  Flag* bus_out_queue_seen_oldest_core;  // FIFO for bus_out_queue
  uns8  bus_out_queue_round_robin_next_proc_id;
  uns   bus_out_queue_one_core_first_num_sent;
} Memory;

typedef struct Pref_LoadPCInfo_Struct {
  Addr loadPC;
  int  count;
} Pref_LoadPCInfo;

typedef enum Exclusive_WB_Policy_Number_enum {
  WB_ALL,                /* 0 */
  WB_ONLY_ON_PATH_TOUCH, /* 1 */
  WB_RANDOM,             /* 2 */
  WB_RANDOM_UNUSED,      /* 3 */
  WB_USED_REQBUF_UNUSED, /* 4 */
} Exclusive_WB_Policy_Number;

typedef struct Pref_Req_Info_Struct {
  Counter     prefetcher_id;
  Addr        loadPC;
  uns32       global_hist;  // Used for perf hfilter
  uns         distance;
  Flag        bw_limited;
  Destination dest;  // Only MLC/L2 values matter
} Pref_Req_Info;

typedef enum L1_Dyn_Partition_Policy_enum {
  PREF_ACC_ONLY,
  UMON_DSS,
  MARGINAL_UTIL,
  NUM_POLICY,
} L1_Dyn_Partition_Policy;

typedef struct Umon_Cache_Data_struct {
  Addr addr;
  Flag prefetch; /* was the line prefetched? */
} Umon_Cache_Data;

/**************************************************************************************/
/* Prototypes */
int mem_compare_priority(const void* a, const void* b);

void set_memory(Memory*);
void init_memory(void);
void reset_memory(void);
void recover_memory(void);
void debug_memory(void);
void update_memory(void);

Flag     scan_stores(Addr, uns);
void     op_nuke_mem_req(Op*);
Flag     mem_req_younger_than_uniquenum(int, Counter);
Flag     mem_req_older_than_uniquenum(int, Counter);
L1_Data* do_l1_access(Op* op);
L1_Data* do_l1_access_addr(Addr);
L1_Data* do_mlc_access(Op* op);
L1_Data* do_mlc_access_addr(Addr);

Flag new_mem_req(Mem_Req_Type type, uns8 proc_id, Addr addr, uns size,
                 uns delay, Op* op, Flag done_func(Mem_Req*),
                 Counter unique_num, Pref_Req_Info*);
void mem_free_reqbuf(Mem_Req* req);
void mem_complete_bus_in_access(Mem_Req* req, Counter priority);
void print_mem_queue(Mem_Queue_Type queue_type);
Flag new_mem_dc_wb_req(Mem_Req_Type type, uns8 proc_id, Addr addr, uns size,
                       uns delay, Op* op, Flag done_func(Mem_Req*),
                       Counter unique_num, Flag used_onpath);
Flag mlc_fill_line(Mem_Req* req);
Flag l1_fill_line(Mem_Req* req);

void mark_ops_as_l1_miss_satisfied(Mem_Req* req);
int  mem_get_req_count(uns proc_id);
Flag mem_can_allocate_req_buffer(uns proc_id, Mem_Req_Type type);

void open_mem_stat_interval_file(void);
void close_mem_stat_interval_file(void);
void collect_mem_stat_interval(Flag final);
void stats_per_core_collect(uns8 proc_id);
void finalize_memory(void);
void l1_cache_collect_stats(void);

void wp_process_l1_hit(L1_Data* line, Mem_Req* req);
void wp_process_l1_fill(L1_Data* line, Mem_Req* req);
void wp_process_reqbuf_match(Mem_Req* req, Op* op);

// batch scheduler
uns num_chip_demands(void);
uns num_offchip_stall_reqs(uns proc_id);

/**************************************************************************************/
/* Externs */

extern Counter        Mem_Req_Priority[];
extern Counter        Mem_Req_Priority_Offset[];
extern Memory*        mem;
extern Freq_Domain_Id FREQ_DOMAIN_CHIP;
extern Freq_Domain_Id FREQ_DOMAIN_MEMORY;
extern Counter        mem_seq_num;

#endif /* #ifndef __MEMORY_H__*/
