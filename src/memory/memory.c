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
 * File         : memory.c
 * Author       : HPS Research Group
 * Date         : 3/30/2004
 * Description  : CMP version of memory.c
 ***************************************************************************************/
/*** CMP  remov all the wrong path stuff  ***/

#include <limits.h>
#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "debug/memview.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"

#include "addr_trans.h"
#include "bp/bp.h"
#include "cache_part.h"
#include "mem_req.h"
#include "memory.h"
#include "op.h"
#include "prefetcher//pref_stream.h"

#include "cmp_model.h"
#include "core.param.h"
#include "debug/debug.param.h"
#include "dvfs/perf_pred.h"
#include "icache_stage.h"
#include "memory.param.h"
#include "prefetcher//stream.param.h"
#include "prefetcher/l2l1pref.h"
#include "prefetcher/pref.param.h"
#include "prefetcher/pref_common.h"
#include "prefetcher/stream_pref.h"
#include "statistics.h"
//#include "dram.h"
//#include "dram.param.h"
#include "ramulator.h"
#include "ramulator.param.h"

/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_MEMORY, ##args)
#define BANK_HASH(a, num, int, shift) \
  ((a) >> (LOG2(int) + LOG2(num) + shift) & N_BIT_MASK(LOG2(num)))

// Bringing one more cache line based on one more biger cache
#define CACHE_SIZE_ADDR(size, addr) ((addr) & ~N_BIT_MASK(size))

#define MLC(proc_id) (mem->uncores[proc_id].mlc)
#define L1(proc_id) (mem->uncores[proc_id].l1)

/**************************************************************************************/
/* Global Variables */

FILE*           l1_plot_file;
FILE*           mem_plot_file;
static Counter  order_num        = 1;
Counter         mem_seq_num      = 1;
static Counter  l1_seq_num       = 1;
static Counter  mlc_seq_num      = 1;
static Counter  bus_out_seq_num  = 1;
static Counter  l1fill_seq_num   = 1;
static Counter  mlc_fill_seq_num = 1;
static Counter* core_fill_seq_num;
static uns      mem_req_demand_entries = 0;
static uns      mem_req_pref_entries   = 0;
static uns      mem_req_wb_entries     = 0;

Memory*              mem = NULL;
extern Icache_Stage* ic;

Counter Mem_Req_Priority[MRT_NUM_ELEMS];
Counter Mem_Req_Priority_Offset[MRT_NUM_ELEMS];

/**************************************************************************************/
/* Local Prototypes */

static void init_mem_req_type_priorities(void);
static void init_uncores(void);
static void update_memory_queues(void);
static void update_on_chip_memory_stats(void);

static void mark_ops_as_l1_miss(Mem_Req* req);
static void mark_l1_miss_deps(Op* op);
static void unmark_l1_miss_deps(Op* op);
static void update_mem_req_occupancy_counter(Mem_Req_Type type, int delta);

int         mem_compare_priority(const void* a, const void* b);
void        mem_start_mlc_access(Mem_Req* req);
static void mem_process_core_fill_reqs(uns proc_id);
Flag mem_process_mlc_hit_access(Mem_Req* req, Mem_Queue_Entry* mlc_queue_entry,
                                Addr* line_addr, MLC_Data* data,
                                int lruu_position);
static void mem_process_mlc_fill_reqs(void);
void        mem_start_l1_access(Mem_Req* req);
Flag mem_process_l1_hit_access(Mem_Req* req, Mem_Queue_Entry* l1_queue_entry,
                               Addr* line_addr, L1_Data* data,
                               int lruu_position);

static void mem_process_l1_fill_reqs(void);
static void mem_process_bus_out_reqs(void);

static Flag mem_process_mlc_miss_access(Mem_Req*         req,
                                        Mem_Queue_Entry* mlc_queue_entry,
                                        Addr* line_addr, MLC_Data* data);
static Flag mem_complete_mlc_access(Mem_Req*         req,
                                    Mem_Queue_Entry* mlc_queue_entry,
                                    int*             l1_queue_insertion_count,
                                    int*             reserved_entry_count);
static Flag mem_process_l1_miss_access(Mem_Req*         req,
                                       Mem_Queue_Entry* l1_queue_entry,
                                       Addr* line_addr, L1_Data* data);
static Flag mem_complete_l1_access(Mem_Req*         req,
                                   Mem_Queue_Entry* l1_queue_entry,
                                   int* bus_out_queue_insertion_count,
                                   int* reserved_entry_count);

static inline Mem_Queue_Entry* mem_insert_req_into_queue(Mem_Req*   new_req,
                                                         Mem_Queue* queue,
                                                         Counter    priority);
static inline Flag insert_new_req_into_l1_queue(uns proc_id, Mem_Req* new_req);
static inline Flag insert_new_req_into_mlc_queue(uns proc_id, Mem_Req* new_req);

static void mem_process_mlc_reqs(void);
static void mem_process_l1_reqs(void);

static inline Mem_Req* mem_search_queue(Mem_Queue* queue, uns8 proc_id,
                                        Addr addr, Mem_Req_Type type, uns size,
                                        Flag*             demand_hit_prefetch,
                                        Flag*             demand_hit_writeback,
                                        Mem_Queue_Entry** queue_entry,
                                        Flag              collect_stats);

static inline Mem_Req* mem_search_reqbuf(uns8 proc_id, Addr addr,
                                         Mem_Req_Type type, uns size,
                                         Flag*             demand_hit_prefetch,
                                         Flag*             demand_hit_writeback,
                                         uns               queues_to_search,
                                         Mem_Queue_Entry** queue_entry);

static Flag mem_adjust_matching_request(
  Mem_Req* req, Mem_Req_Type type, Addr addr, uns size, Destination destination,
  uns delay, Op* op, Flag done_func(Mem_Req*), Counter unique_num,
  Flag demand_hit_prefetch, Flag demand_hit_writeback,
  Mem_Queue_Entry** queue_entry, Counter new_priority);
static inline Mem_Req* mem_allocate_req_buffer(uns proc_id, Mem_Req_Type type);
static Mem_Req* mem_kick_out_prefetch_from_queue(uns mem_bank, Mem_Queue* queue,
                                                 Counter new_priority);
static Mem_Req* mem_kick_out_prefetch_from_queues(uns     mem_bank,
                                                  Counter new_priority,
                                                  uns     queues_to_search);
static Mem_Req* mem_kick_out_oldest_first_prefetch_from_queues(
  uns mem_bank, Counter new_priority, uns queues_to_search);

static void mem_init_new_req(Mem_Req* new_req, Mem_Req_Type type,
                             Mem_Queue_Type queue_type, uns8 proc_id, Addr addr,
                             uns size, uns delay, Op* op,
                             Flag done_func(Mem_Req*), Counter unique_num,
                             Flag kicked_out, Counter new_priority);

static inline void init_mem_queue(Mem_Queue* queue, char* name, uns size,
                                  Mem_Queue_Type type);

static void print_mem_queue_generic(Mem_Queue* queue);

static inline void queue_sanity_check(int location);

void mem_insert_req_round_robin(void);

static Flag new_mem_mlc_wb_req(Mem_Req_Type type, uns8 proc_id, Addr addr,
                               uns size, uns delay, Op* op,
                               Flag done_func(Mem_Req*), Counter unique_num);
static Flag new_mem_l1_wb_req(Mem_Req_Type type, uns8 proc_id, Addr addr,
                              uns size, uns delay, Op* op,
                              Flag done_func(Mem_Req*), Counter unique_num);

static inline void set_off_path_confirmed_status(Mem_Req* req);
static void        mem_clear_reqbuf(Mem_Req* req);
static L1_Data*    l1_pref_cache_access(Mem_Req* req);

static inline Flag queue_full(Mem_Queue* queue);
static inline uns  queue_num_free(Mem_Queue* queue);

Flag is_final_state(Mem_Req_State state);

/**************************************************************************************/
/* set_memory: */

void set_memory(Memory* new_mem) {
  mem = new_mem;
}


/**************************************************************************************/
/* init_mem_queue: */

static inline void init_mem_queue(Mem_Queue* queue, char* name, uns size,
                                  Mem_Queue_Type type) {
  ASSERTM(
    0, !(queue->type & QUEUE_MEM),
    "Ramulator does not use QUEUE_MEM. QUEUE_MEM should not be initialized!\n");

  queue->base = (Mem_Queue_Entry*)malloc(sizeof(Mem_Queue_Entry) * (size + 1));
  queue->size = size;
  queue->entry_count          = 0;
  queue->reserved_entry_count = 0;
  queue->type                 = type;
  strcpy(queue->name, name);
}

/**************************************************************************************/
/* init_mem_req_type_priorities: */

void init_mem_req_type_priorities() {
  for(uns type = 0; type < MRT_NUM_ELEMS; ++type) {
    uns priority = 0;
    /* Least number is the highest priority. Priority should be
       defined in the upper bits because an op number may be added to the
       priority to establish a program order on memory requests of the same
       type priority. */
    const uns num_type_priority_bits = 4;
    const uns least_priority         = (1 << num_type_priority_bits) -
                               2;  // leave one for MIN_PRIORITY
    switch(type) {
      case MRT_IFETCH:
        priority = MEM_PRIORITY_IFETCH;
        break;
      case MRT_DFETCH:
        priority = MEM_PRIORITY_DFETCH;
        break;
      case MRT_DSTORE:
        priority = MEM_PRIORITY_DSTORE;
        break;
      case MRT_IPRF:
        priority = MEM_PRIORITY_IPRF;
        break;
      case MRT_DPRF:
        priority = MEM_PRIORITY_DPRF;
        break;
      case MRT_WB:
        priority = MEM_PRIORITY_WB;
        break;
      case MRT_WB_NODIRTY:
        priority = MEM_PRIORITY_WB_NODIRTY;
        break;
      case MRT_MIN_PRIORITY:
        priority = least_priority + 1;
        break;
      default:
        FATAL_ERROR(0, "Priority for mem req type %s not specified\n",
                    Mem_Req_Type_str(type));
        break;
    }
    ASSERTM(0, priority <= least_priority || type == MRT_MIN_PRIORITY,
            "Specified priority %d of mem req type %s is outside of the "
            "allowed range [0:%d]\n",
            priority, Mem_Req_Type_str(type), least_priority);
    Mem_Req_Priority[type]        = priority;
    Mem_Req_Priority_Offset[type] = (Counter)priority
                                    << (sizeof(Counter) * CHAR_BIT -
                                        num_type_priority_bits);
  }
}

/**************************************************************************************/
/* init_memory: */

void init_memory() {
  int  ii;
  char name[20];
  uns8 proc_id;

  ASSERT(0, mem);
  ASSERT(0, L1_LINE_SIZE <= L1_INTERLEAVE_FACTOR);
  ASSERT(0, L1_LINE_SIZE <= MLC_INTERLEAVE_FACTOR);
  ASSERT(0, L1_LINE_SIZE <= VA_PAGE_SIZE_BYTES);
  ASSERT(0, NUM_ADDR_NON_SIGN_EXTEND_BITS <= 58);
  ASSERT(0, LOG2(VA_PAGE_SIZE_BYTES) <= NUM_ADDR_NON_SIGN_EXTEND_BITS);
  memset(mem, 0, sizeof(Memory));

  init_mem_req_type_priorities();

  /* Initialize request buffers */
  mem->total_mem_req_buffers = MEM_REQ_BUFFER_ENTRIES *
                               (PRIVATE_MSHR_ON ? NUM_CORES : 1);
  mem->req_buffer = (Mem_Req*)malloc(sizeof(Mem_Req) *
                                     mem->total_mem_req_buffers);
  for(ii = 0; ii < mem->total_mem_req_buffers; ii++) {
    mem->req_buffer[ii].state = MRS_INV;
  }
  mem->num_req_buffers_per_core = calloc(NUM_CORES, sizeof(uns));
  init_list(&mem->req_buffer_free_list, "REQ BUF FREE LIST", sizeof(int), TRUE);

  if(ROUND_ROBIN_TO_L1) {
    mem->l1_in_buffer_core = (List*)malloc(sizeof(List) * NUM_CORES);
    for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      init_list(&mem->l1_in_buffer_core[proc_id], "L1 IN BUFFER",
                sizeof(Mem_Req*), TRUE);
    }
  }

  for(ii = 0; ii < mem->total_mem_req_buffers; ii++) {
    mem->req_buffer[ii].id = ii;
    sprintf(name, "%d OPP_L", ii);
    init_list(&mem->req_buffer[ii].op_ptrs, name, sizeof(Op*), TRUE);
    sprintf(name, "%d OPU_L", ii);
    init_list(&mem->req_buffer[ii].op_uniques, name, sizeof(Counter), TRUE);
  }

  /* Initialize l1 and bus access queues which hold id's of request buffers */
  init_mem_queue(
    &mem->mlc_queue, "MLC_QUEUE",
    QUEUE_MLC_SIZE == 0 ? mem->total_mem_req_buffers : QUEUE_MLC_SIZE,
    QUEUE_MLC);
  init_mem_queue(&mem->mlc_fill_queue, "MLC_FILL_QUEUE",
                 mem->total_mem_req_buffers, QUEUE_MLC_FILL);
  init_mem_queue(
    &mem->l1_queue, "L1_QUEUE",
    QUEUE_L1_SIZE == 0 ? mem->total_mem_req_buffers : QUEUE_L1_SIZE, QUEUE_L1);
  init_mem_queue(
    &mem->bus_out_queue, "BUS_OUT_QUEUE",
    QUEUE_BUS_OUT_SIZE == 0 ? mem->total_mem_req_buffers : QUEUE_BUS_OUT_SIZE,
    QUEUE_BUS_OUT);
  init_mem_queue(&mem->l1fill_queue, "L1FILL_QUEUE", mem->total_mem_req_buffers,
                 QUEUE_L1FILL);

  mem->core_fill_queues = (Mem_Queue*)malloc(sizeof(Mem_Queue) * NUM_CORES);
  core_fill_seq_num     = (Counter*)malloc(sizeof(Counter) * NUM_CORES);
  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    char buf[MAX_STR_LENGTH + 1];
    sprintf(buf, "CORE_%d_FILL_QUEUE", proc_id);
    init_mem_queue(&mem->core_fill_queues[proc_id], buf,
                   QUEUE_CORE_FILL_SIZE == 0 ? mem->total_mem_req_buffers :
                                               QUEUE_CORE_FILL_SIZE,
                   QUEUE_CORE_FILL);
    core_fill_seq_num[proc_id] = 1;
  }

  init_uncores();

  init_cache(&mem->pref_l1_cache, "L1_PREF_CACHE", L1_PREF_CACHE_SIZE,
             L1_PREF_CACHE_ASSOC, L1_LINE_SIZE, sizeof(L1_Data),
             L1_CACHE_REPL_POLICY);

  if(STREAM_PREFETCH_ON)
    init_stream_HWP();
  pref_init();
  mem->pref_replpos = INSERT_REPL_MRU;
  if(PREF_ANALYZE_LOAD) {
    mem->pref_loadPC_hash = (Hash_Table*)malloc(sizeof(Hash_Table));
    init_hash_table(mem->pref_loadPC_hash, "Pref_loadPC_hash", 100000,
                    sizeof(Pref_LoadPCInfo));
  }

  // BW
  mem->l1_ave_num_ways_per_core = (double*)malloc(sizeof(double) * NUM_CORES);

  // init_dram ();
  ramulator_init();

  reset_memory();

  init_perf_pred();
}

void init_uncores(void) {
  mem->uncores = (Uncore*)malloc(sizeof(Uncore) * NUM_CORES);

  /* Initialize MLC cache (shared only for now) */
  Ported_Cache* mlc = (Ported_Cache*)malloc(sizeof(Ported_Cache));
  init_cache(&mlc->cache, "MLC_CACHE", MLC_SIZE, MLC_ASSOC, MLC_LINE_SIZE,
             sizeof(MLC_Data), MLC_CACHE_REPL_POLICY);
  mlc->num_banks = MLC_BANKS;
  mlc->ports     = (Ports*)malloc(sizeof(Ports) * mlc->num_banks);
  for(uns ii = 0; ii < mlc->num_banks; ii++) {
    char name[MAX_STR_LENGTH + 1];
    snprintf(name, MAX_STR_LENGTH, "MLC BANK %d PORTS", ii);
    init_ports(&mlc->ports[ii], name, MLC_READ_PORTS, MLC_WRITE_PORTS, FALSE);
  }
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    MLC(proc_id) = mlc;
  }

  /* Initialize L2 cache */
  if(PRIVATE_L1) {
    ASSERTM(
      0, L1_SIZE % NUM_CORES == 0,
      "Total L1_SIZE must be a multiple of NUM_CORES if PRIVATE_L1 is on\n");
    ASSERTM(
      0, L1_BANKS % NUM_CORES == 0,
      "Total L1_BANKS must be a multiple of NUM_CORES if PRIVATE_L1 is on\n");
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      Ported_Cache* l1 = (Ported_Cache*)malloc(sizeof(Ported_Cache));

      char buf[MAX_STR_LENGTH + 1];
      sprintf(buf, "L1[%d]", proc_id);
      init_cache(&l1->cache, buf, L1_SIZE / NUM_CORES, L1_ASSOC, L1_LINE_SIZE,
                 sizeof(L1_Data), L1_CACHE_REPL_POLICY);

      l1->num_banks = L1_BANKS / NUM_CORES;
      l1->ports     = (Ports*)malloc(sizeof(Ports) * l1->num_banks);
      for(uns ii = 0; ii < l1->num_banks; ii++) {
        char name[MAX_STR_LENGTH + 1];
        snprintf(name, MAX_STR_LENGTH, "L1[%d] BANK %d PORTS", proc_id, ii);
        init_ports(&l1->ports[ii], name, L1_READ_PORTS, L1_WRITE_PORTS, FALSE);
      }
      L1(proc_id) = l1;
    }
  } else {  // shared L2
    Ported_Cache* l1 = (Ported_Cache*)malloc(sizeof(Ported_Cache));
    init_cache(&l1->cache, "L1_CACHE", L1_SIZE, L1_ASSOC, L1_LINE_SIZE,
               sizeof(L1_Data), L1_CACHE_REPL_POLICY);
    l1->num_banks = L1_BANKS;
    l1->ports     = (Ports*)malloc(sizeof(Ports) * l1->num_banks);
    for(uns ii = 0; ii < l1->num_banks; ii++) {
      char name[MAX_STR_LENGTH + 1];
      snprintf(name, MAX_STR_LENGTH, "L1 BANK %d PORTS", ii);
      init_ports(&l1->ports[ii], name, L1_READ_PORTS, L1_WRITE_PORTS, FALSE);
    }
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      L1(proc_id) = l1;
    }
    if(L1_CACHE_REPL_POLICY == REPL_PARTITION && !L1_PART_WARMUP) {
      l1->cache.repl_policy = REPL_TRUE_LRU;
    }
  }

  if(L1_CACHE_REPL_POLICY == REPL_PARTITION) {
    // initially equally partition
    uns num_ways = L1_ASSOC / NUM_CORES;
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      set_partition_allocate(&L1(proc_id)->cache, proc_id, num_ways);
    }
    if(L1_STATIC_PARTITION_ENABLE) {
      ASSERT(0, !L1_DYNAMIC_PARTITION_ENABLE);
      ASSERTM(0, L1_STATIC_PARTITION, "Please specify L1_STATIC_PARTITION\n");
      int ways_per_core[MAX_NUM_PROCS];
      int num_tokens = parse_int_array(ways_per_core, L1_STATIC_PARTITION,
                                       MAX_NUM_PROCS);
      ASSERT(0, num_tokens == NUM_CORES);
      for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
        set_partition_allocate(&L1(proc_id)->cache, proc_id,
                               ways_per_core[proc_id]);
      }
    }

    if(L1_DYNAMIC_PARTITION_ENABLE && L1_DYNAMIC_PARTITION_POLICY == UMON_DSS) {
      mem->umon_cache_core = (Cache*)malloc(sizeof(Cache) * NUM_CORES);
      mem->umon_cache_hit_count_core = (double**)malloc(sizeof(double*) *
                                                        NUM_CORES);
      int ii;

      for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
        init_cache(&mem->umon_cache_core[proc_id], "UMON_CACHE",
                   L1_SIZE / 32 / L1_LINE_SIZE, L1_ASSOC, 1,
                   sizeof(Umon_Cache_Data), REPL_TRUE_LRU);

        mem->umon_cache_hit_count_core[proc_id] = (double*)malloc(
          sizeof(double) * L1_ASSOC);
        for(ii = 0; ii < L1_ASSOC; ii++) {
          mem->umon_cache_hit_count_core[proc_id][ii] = 0;
        }
      }
    }
  }

  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    mem->uncores[proc_id].num_outstanding_l1_accesses = 0;
    mem->uncores[proc_id].num_outstanding_l1_misses   = 0;
    mem->uncores[proc_id].mem_block_start             = 0;
  }
}

/**************************************************************************************/
/* reset_memory: */

void reset_memory() {
  uns ii;

  clear_list(&mem->req_buffer_free_list);

  mem->l1_queue.entry_count       = 0;
  mem->mlc_queue.entry_count      = 0;
  mem->bus_out_queue.entry_count  = 0;
  mem->l1fill_queue.entry_count   = 0;
  mem->mlc_fill_queue.entry_count = 0;

  for(ii = 0; ii < mem->total_mem_req_buffers; ii++) {
    int* free_list_entry      = sl_list_add_tail(&mem->req_buffer_free_list);
    *free_list_entry          = ii;
    mem->req_buffer[ii].state = MRS_INV;
  }

  mem->req_count = 0;

  uns8 proc_id;
  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    mem->l1_ave_num_ways_per_core[proc_id] = 0;
  }
}

static void mem_clear_reqbuf(Mem_Req* req) {
  clear_list(&req->op_ptrs);
  clear_list(&req->op_uniques);
}


void mem_free_reqbuf(Mem_Req* req) {
  int* reqbuf_num_ptr;

  DEBUG(
    req->proc_id,
    "Freeing mem buffer entry  index:%d queue:%s rcount:%d l1:%d bo:%d lf:%d\n",
    req->id, (NULL == req->queue) ? "NULL" : req->queue->name, mem->req_count,
    mem->l1_queue.entry_count, mem->bus_out_queue.entry_count,
    mem->l1fill_queue.entry_count);


  if(req->state == MRS_MEM_DONE) {
    ASSERT(req->proc_id, req->type == MRT_WB);
    ASSERT(req->proc_id, !req->off_path);
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_WB);
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE);
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_MEM);
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_MEM_WB);
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_ONPATH);
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_ONPATH_WB);
  } else if(req->state == MRS_FILL_DONE) {
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_IFETCH + MIN2(req->type, 7));
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE);
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_MEM);
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_MEM_IFETCH + MIN2(req->type, 7));
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_ONPATH + req->off_path);
    if(req->off_path)
      STAT_EVENT(req->proc_id,
                 MEM_REQ_COMPLETE_OFFPATH_IFETCH + MIN2(req->type, 7));
    else
      STAT_EVENT(req->proc_id,
                 MEM_REQ_COMPLETE_ONPATH_IFETCH + MIN2(req->type, 7));
    if(mem_req_type_is_demand(req->type)) {
      if(!req->demand_match_prefetch && req->bw_prefetchable)
        STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_MEM_BW_PREFETCHABLE);
      if(req->demand_match_prefetch && req->bw_prefetch)
        STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_MEM_MATCHED_BW_PREF);
    }

    if(req->type == MRT_WB)
      STAT_EVENT(req->proc_id, WB_COMING_BACK_FROM_MEM);
  } else if(req->state == MRS_L1_HIT_DONE) {
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_IFETCH + MIN2(req->type, 7));
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE);
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_L1);
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_L1_IFETCH + MIN2(req->type, 7));
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_ONPATH + req->off_path);
    if(req->off_path)
      STAT_EVENT(req->proc_id,
                 MEM_REQ_COMPLETE_OFFPATH_IFETCH + MIN2(req->type, 7));
    else
      STAT_EVENT(req->proc_id,
                 MEM_REQ_COMPLETE_ONPATH_IFETCH + MIN2(req->type, 7));

    if(req->wb_requested_back) {
      ASSERT(req->proc_id,
             (req->type == MRT_WB) || (req->type == MRT_WB_NODIRTY));
      STAT_EVENT(req->proc_id, WB_COMING_BACK_FROM_L1);
    }
  } else if(req->state == MRS_MLC_HIT_DONE) {
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_IFETCH + MIN2(req->type, 7));
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE);
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_MLC);
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_MLC_IFETCH + MIN2(req->type, 7));
    STAT_EVENT(req->proc_id, MEM_REQ_COMPLETE_ONPATH + req->off_path);
    if(req->off_path)
      STAT_EVENT(req->proc_id,
                 MEM_REQ_COMPLETE_OFFPATH_IFETCH + MIN2(req->type, 7));
    else
      STAT_EVENT(req->proc_id,
                 MEM_REQ_COMPLETE_ONPATH_IFETCH + MIN2(req->type, 7));

    if(req->wb_requested_back) {
      ASSERT(req->proc_id,
             (req->type == MRT_WB) || (req->type == MRT_WB_NODIRTY));
      STAT_EVENT(req->proc_id, WB_COMING_BACK_FROM_MLC);
    }
  } else { /* killed */
    STAT_EVENT(req->proc_id, MEM_REQ_KILLED_IFETCH + MIN2(req->type, 7));
    STAT_EVENT(req->proc_id, MEM_REQ_KILLED);
  }

  perf_pred_l0_miss_end(req);

  ASSERT(req->proc_id, mem->num_req_buffers_per_core[req->proc_id] > 0);
  mem->num_req_buffers_per_core[req->proc_id] -= 1;
  update_mem_req_occupancy_counter(req->type, -1);

  ASSERT(req->proc_id, req->reserved_entry_count == 0);

  req->state = MRS_INV;
  mem->req_count--;
  ASSERT(req->proc_id, mem->req_count >= 0);
  clear_list(&req->op_ptrs);
  clear_list(&req->op_uniques);

  reqbuf_num_ptr = sl_list_add_tail(&mem->req_buffer_free_list);

  ASSERT(req->proc_id, reqbuf_num_ptr);
  *reqbuf_num_ptr = req->id;

  ASSERT(req->proc_id,
         mem->req_buffer_free_list.count <= mem->total_mem_req_buffers);
  ASSERT(req->proc_id, (mem->req_count + mem->req_buffer_free_list.count) ==
                         mem->total_mem_req_buffers);
}


/**************************************************************************************/
/* queue_full: */

static inline Flag queue_full(Mem_Queue* queue) {
  if(queue->entry_count == (queue->size - queue->reserved_entry_count))
    return TRUE;

  ASSERT(0, queue->entry_count < (queue->size - queue->reserved_entry_count));
  return FALSE;
}
/**************************************************************************************/
/* queue_num_free: */

static inline uns queue_num_free(Mem_Queue* queue) {
  return (queue->size - queue->reserved_entry_count) - queue->entry_count;
}


/**************************************************************************************/
/* print_mem_queue: */

static void print_mem_queue_generic(Mem_Queue* queue) {
  int      ii;
  Mem_Req* req = NULL;

  fprintf(stdout, "%s --- entries: %d  cycle: %s\n", queue->name,
          queue->entry_count, unsstr64(cycle_count));
  fprintf(stdout, "------------------------------------------------------\n");

  for(ii = 0; ii < queue->entry_count; ii++) {
    req = &(mem->req_buffer[queue->base[ii].reqbuf]);
    fprintf(stdout,
            "%d: q:%s reqbuf:%d index:%d pri:%s st:%s type:%s pri:%s beg:%s "
            "rdy:%s addr:%s size:%d age:%s mbank:%d oc:%d oo:%s off:%d\n",
            ii, (req->queue ? req->queue->name : "ramulator"),
            queue->base[ii].reqbuf, req->id, unsstr64(queue->base[ii].priority),
            mem_req_state_names[req->state], Mem_Req_Type_str(req->type),
            unsstr64(req->priority), unsstr64(req->start_cycle),
            unsstr64(req->rdy_cycle), hexstr64s(req->addr), req->size,
            unsstr64(cycle_count - req->start_cycle), req->mem_flat_bank,
            req->op_count, unsstr64(req->oldest_op_unique_num), req->off_path);
  }

  fprintf(stdout, "------------------------------------------------------\n");
}

/**************************************************************************************/
/* print_mem_queue: */

void print_mem_queue(Mem_Queue_Type queue_type) {
  fprintf(stdout, "\n");
  if(queue_type & QUEUE_L1)
    print_mem_queue_generic(&(mem->l1_queue));

  if(queue_type & QUEUE_MLC)
    print_mem_queue_generic(&(mem->mlc_queue));

  if(queue_type & QUEUE_BUS_OUT)
    print_mem_queue_generic(&(mem->bus_out_queue));

  cycle_count = freq_cycle_count(FREQ_DOMAIN_L1);

  if(queue_type & QUEUE_L1FILL)
    print_mem_queue_generic(&(mem->l1fill_queue));

  if(queue_type & QUEUE_MLC_FILL)
    print_mem_queue_generic(&(mem->mlc_fill_queue));
}


/**************************************************************************************/
/* new_mem_req_younger_than_uniquenum: */

static inline Flag new_mem_req_younger_than_uniquenum(Mem_Req* req,
                                                      Counter  unique_num) {
  if(req->oldest_op_unique_num == 0) {
    if(req->type == MRT_IFETCH) {
      if(req->unique_num > unique_num)
        return TRUE;
      else
        return FALSE;
    } else
      return req->off_path;
  } else {
    if(req->oldest_op_unique_num > unique_num)
      return TRUE;
    else
      return FALSE;
  }
}

/**************************************************************************************/
/* Set_off_path_confirmed_status: */

static inline void set_off_path_confirmed_status(Mem_Req* req) {
  if((req->type != MRT_WB) &&
     (req->type != MRT_WB_NODIRTY)) { /* writebacks cannot be off_path */
    if(new_mem_req_younger_than_uniquenum(
         req, bp_recovery_info->recovery_unique_num))
      req->off_path_confirmed = TRUE;
    STAT_EVENT(req->proc_id, OFF_PATH_CONFIRMED);
  }
}


/**************************************************************************************/
/* recover_memory: */

void recover_memory() {
  if(SET_OFF_PATH_CONFIRMED) {
    for(uns ii = 0; ii < mem->total_mem_req_buffers;
        ii++) {  // FIXME: inefficient
      Mem_Req* req = &(mem->req_buffer[ii]);
      if(req->state != MRS_INV && req->proc_id == bp_recovery_info->proc_id) {
        set_off_path_confirmed_status(req);
      }
    }
  }

  /* If we are supposed to do really nothing for requests that are known to be
   * off-path, then return */
  return;
}


/**************************************************************************************/
/* debug_memory: */

void debug_memory() {
  DPRINTF("# MEMORY\n");
  DPRINTF("reqbuf_used_count:    %d\n", mem->req_count);
  DPRINTF("reqbuf_free_count:    %d\n", mem->req_buffer_free_list.count);
  DPRINTF("mlc_queue_count:      %d\n", mem->mlc_queue.entry_count);
  DPRINTF("l1_queue_count:       %d\n", mem->l1_queue.entry_count);
  DPRINTF("bus_out_queue_count:  %d\n", mem->bus_out_queue.entry_count);
  DPRINTF("mlc_fill_queue_count: %d\n", mem->mlc_fill_queue.entry_count);
  DPRINTF("l1fill_queue_count:   %d\n", mem->l1fill_queue.entry_count);

  print_mem_queue(QUEUE_L1 | QUEUE_BUS_OUT | QUEUE_L1FILL | QUEUE_MLC |
                  QUEUE_MLC_FILL);
}

/**************************************************************************************/
/* update_memory: */

static inline void queue_sanity_check(int location) {
  int queue_count = mem->l1_queue.entry_count + mem->bus_out_queue.entry_count +
                    mem->l1fill_queue.entry_count + mem->mlc_queue.entry_count +
                    mem->mlc_fill_queue.entry_count;

  ASSERTM(0, mem->req_count == queue_count, "rc:%d l1:%d bo:%d lf:%d loc:%d\n",
          mem->req_count, mem->l1_queue.entry_count,
          mem->bus_out_queue.entry_count, mem->l1fill_queue.entry_count,
          location);

  ASSERTM(0,
          (mem->req_count + mem->req_buffer_free_list.count) ==
            mem->total_mem_req_buffers,
          "rc:%d rf:%d l1:%d bo:%d lf:%d loc:%d\n", mem->req_count,
          mem->req_buffer_free_list.count, mem->l1_queue.entry_count,
          mem->bus_out_queue.entry_count, mem->l1fill_queue.entry_count,
          location);
}

int cycle_l1q_insert_count     = 0;
int cycle_mlcq_insert_count    = 0;
int cycle_busoutq_insert_count = 0;
int l1_in_buf_count            = 0;

void update_memory_queues() {
  // here!!! mix the requests
  if(ROUND_ROBIN_TO_L1 && l1_in_buf_count > 0) {
    mem_insert_req_round_robin();
  }

  if(!ALL_FIFO_QUEUES && (cycle_l1q_insert_count > 0)) {
    qsort(mem->l1_queue.base, mem->l1_queue.entry_count,
          sizeof(Mem_Queue_Entry), mem_compare_priority);
    cycle_l1q_insert_count = 0;
  }

  if(!ALL_FIFO_QUEUES && (cycle_mlcq_insert_count > 0)) {
    qsort(mem->mlc_queue.base, mem->mlc_queue.entry_count,
          sizeof(Mem_Queue_Entry), mem_compare_priority);
    cycle_mlcq_insert_count = 0;
  }

  if(!ALL_FIFO_QUEUES && (cycle_busoutq_insert_count > 0)) {
    qsort(mem->bus_out_queue.base, mem->bus_out_queue.entry_count,
          sizeof(Mem_Queue_Entry), mem_compare_priority);
    cycle_busoutq_insert_count = 0;
  }
}

void update_on_chip_memory_stats() {
  STAT_EVENT_ALL(L1_CYCLE);
  STAT_EVENT(0, MIN2(MEM_REQ_DEMANDS__0 + mem_req_demand_entries / 4,
                     MEM_REQ_DEMANDS_64));
  STAT_EVENT(
    0, MIN2(MEM_REQ_PREFS__0 + mem_req_pref_entries / 4, MEM_REQ_PREFS_64));
  STAT_EVENT(0, MIN2(MEM_REQ_WRITEBACKS__0 + mem_req_wb_entries / 4,
                     MEM_REQ_WRITEBACKS_64));
  INC_STAT_EVENT(0, MEM_REQ_DEMAND_CYCLES, mem_req_demand_entries);
  INC_STAT_EVENT(0, MEM_REQ_PREF_CYCLES, mem_req_pref_entries);
  INC_STAT_EVENT(0, MEM_REQ_WB_CYCLES, mem_req_wb_entries);
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    STAT_EVENT(
      proc_id,
      CORE_MLP_0 + MIN2(mem->uncores[proc_id].num_outstanding_l1_misses, 32));
    INC_STAT_EVENT(proc_id, CORE_MLP,
                   mem->uncores[proc_id].num_outstanding_l1_misses);
    Counter l1_lines = GET_TOTAL_STAT_EVENT(proc_id, NORESET_L1_FILL) -
                       GET_TOTAL_STAT_EVENT(proc_id, NORESET_L1_EVICT);
    INC_STAT_EVENT(proc_id, L1_LINES, l1_lines);
  }
}

void update_memory() {
  if(freq_is_ready(FREQ_DOMAIN_L1)) {
    cycle_count = freq_cycle_count(FREQ_DOMAIN_L1);

    perf_pred_cycle();

    pref_update();
    update_memory_queues();
    update_on_chip_memory_stats();

    mem_process_mlc_fill_reqs();
    mem_process_l1_fill_reqs();
  }

  if(freq_is_ready(FREQ_DOMAIN_MEMORY)) {
    cycle_count = freq_cycle_count(FREQ_DOMAIN_MEMORY);

    // dram_process_main_memory_reqs();
    ramulator_tick();
  }

  if(freq_is_ready(FREQ_DOMAIN_L1)) {
    cycle_count = freq_cycle_count(FREQ_DOMAIN_L1);

    mem_process_bus_out_reqs();
    mem_process_l1_reqs();
    mem_process_mlc_reqs();
  }

  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    if(freq_is_ready(FREQ_DOMAIN_CORES[proc_id])) {
      cycle_count = freq_cycle_count(FREQ_DOMAIN_CORES[proc_id]);
      mem_process_core_fill_reqs(proc_id);
    }
  }
}

/**************************************************************************************/
/* mem_compare_priority: */

int mem_compare_priority(const void* a, const void* b) {
  Mem_Queue_Entry* e0        = (Mem_Queue_Entry*)a;
  Mem_Queue_Entry* e1        = (Mem_Queue_Entry*)b;
  Counter          priority0 = e0->priority;
  Counter          priority1 = e1->priority;

  if(priority0 < priority1)
    return -1;
  else if(priority1 < priority0)
    return 1;
  else
    return 0;
}

/**************************************************************************************/
/* mem_start_mlc_access: */

void mem_start_mlc_access(Mem_Req* req) {
  Flag avail = FALSE;

  /* FIXME: Only WB reqs try to get a write port? How about stores? */
  Flag need_wp = ((req->type == MRT_WB) || (req->type == MRT_WB_NODIRTY));
  Flag need_rp = !need_wp;
  if((need_wp && get_write_port(&MLC(req->proc_id)->ports[req->mlc_bank])) ||
     (need_rp && get_read_port(&MLC(req->proc_id)->ports[req->mlc_bank]))) {
    DEBUG(req->proc_id,
          "Mem request accessing MLC  index:%ld  type:%s  addr:0x%s  "
          "mem_bank:%d  size:%d  state: %s\n",
          (long int)(req - mem->req_buffer), Mem_Req_Type_str(req->type),
          hexstr64s(req->addr), req->mem_flat_bank, req->size,
          mem_req_state_names[req->state]);

    avail          = TRUE;
    req->state     = MRS_MLC_WAIT;
    req->rdy_cycle = cycle_count + MLC_CYCLES;
  }

  if(need_wp)
    STAT_EVENT(req->proc_id, MLC_ST_BANK_BLOCK + avail);
  else
    STAT_EVENT(req->proc_id, MLC_LD_BANK_BLOCK + avail);
}

/**************************************************************************************/
/* mem_start_l1_access: */

void mem_start_l1_access(Mem_Req* req) {
  Flag avail = FALSE;

  /* FIXME: Only WB reqs try to get a write port? How about stores? */
  Flag need_wp = ((req->type == MRT_WB) || (req->type == MRT_WB_NODIRTY));
  Flag need_rp = !need_wp;
  if((need_wp && get_write_port(&L1(req->proc_id)->ports[req->l1_bank])) ||
     (need_rp && get_read_port(&L1(req->proc_id)->ports[req->l1_bank]))) {
    DEBUG(req->proc_id,
          "Mem request accessing L1  index:%ld  type:%s  addr:0x%s  "
          "mem_bank:%d  size:%d  state: %s\n",
          (long int)(req - mem->req_buffer), Mem_Req_Type_str(req->type),
          hexstr64s(req->addr), req->mem_flat_bank, req->size,
          mem_req_state_names[req->state]);

    avail      = TRUE;
    req->state = MRS_L1_WAIT;
    if(L1_USE_CORE_FREQ) {
      // model cache as being in the requesting core's frequency domain
      // useful for modeling per-core DVFS with private LLCs
      Freq_Domain_Id core_domain      = FREQ_DOMAIN_CORES[req->proc_id];
      Counter        core_cycle_count = freq_cycle_count(core_domain);
      req->rdy_cycle                  = freq_convert_future_cycle(
        core_domain, core_cycle_count + L1_CYCLES, FREQ_DOMAIN_L1);
    } else {
      req->rdy_cycle = cycle_count + L1_CYCLES;
    }

    mem->uncores[req->proc_id].num_outstanding_l1_accesses++;
    memview_l1(req);
  }

  if(need_wp)
    STAT_EVENT(req->proc_id, L1_ST_BANK_BLOCK + avail);
  else
    STAT_EVENT(req->proc_id, L1_LD_BANK_BLOCK + avail);
}

/**************************************************************************************/
/* mem_process_l1_hit_access: */
/* Returns TRUE if l1 access is complete and needs to be removed from l1_queue
 */

Flag mem_process_l1_hit_access(Mem_Req* req, Mem_Queue_Entry* l1_queue_entry,
                               Addr* line_addr, L1_Data* data,
                               int lru_position) {
  Flag fill_mlc = MLC_PRESENT && req->destination != DEST_L1 &&
                  (req->type != MRT_WB && req->type != MRT_WB_NODIRTY);

  if(data) { /* not perfect l1 */
    if((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
       (req->type == MRT_IFETCH)) {
      if(L1_CACHE_HIT_POSITION_COLLECT) {
        ASSERT(data->proc_id, lru_position != -1);
        if(data->prefetch && !data->seen_prefetch)  // prefetch hit
          STAT_EVENT(data->proc_id, CORE_L1_PREF_USED_POS0 + lru_position);
        else  // demand hit
          STAT_EVENT(data->proc_id, CORE_L1_DEMAND_USED_POS0 + lru_position);
      }

      if(L1_DYNAMIC_PARTITION_ENABLE &&
         L1_DYNAMIC_PARTITION_POLICY == MARGINAL_UTIL) {
        ASSERT(data->proc_id, lru_position != -1);
      }

      if(data->prefetch) {  // prefetch hit
        DEBUG(req->proc_id, "%7lld l1 prefetch hit %d\n", cycle_count,
              (int)(req->addr));
        STAT_EVENT(req->proc_id, L1_PREF_HIT);
        if(!data->seen_prefetch) {
          data->seen_prefetch = TRUE;
          // pref_ul1_pref_hit(req->proc_id, req->addr, req->loadPC,
          // lru_position, data->prefetcher_id);   // this is the last version.
          // the new change affects only 2dc prefetcher sine req->loadPC is used
          // only there in the pref_ul1_pref_hit function
          pref_ul1_pref_hit(
            req->proc_id, req->addr, data->pref_loadPC, data->global_hist,
            lru_position, data->prefetcher_id);  // FIXME: lru position FOR CMP

          STAT_EVENT(req->proc_id, L1_PREF_UNIQUE_HIT);
          STAT_EVENT(req->proc_id, PREF_L1_TOTAL_USED);
          STAT_EVENT(req->proc_id, CORE_PREF_L1_USED);
          STAT_EVENT(req->proc_id, CORE_L1_PREF_FILL_USED);
          STAT_EVENT(req->proc_id, NORESET_L1_PREF_USED);
        }
      }
    }

    if(req->type == MRT_DPRF || req->type == MRT_IPRF ||
       req->demand_match_prefetch) {
      STAT_EVENT(req->proc_id, L1_PREF_REQ_HIT);
      STAT_EVENT(req->proc_id, CORE_L1_PREF_REQ_HIT);
    } else if((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
              (req->type == MRT_IFETCH)) {
      STAT_EVENT(req->proc_id, L1_DEMAND_HIT);
      STAT_EVENT(req->proc_id, CORE_L1_DEMAND_HIT);
    } else {  // CMP Watch out RA
      STAT_EVENT(req->proc_id, L1_WB_HIT);
      STAT_EVENT(req->proc_id, CORE_L1_WB_HIT);
    }
    data->dirty |= (req->type == MRT_WB);
  }

  DEBUG(req->proc_id,
        "Mem request hit in the L1  index:%ld  type:%s  addr:0x%s  l1_bank:%d  "
        "size:%d\n",
        (long int)(req - mem->req_buffer), Mem_Req_Type_str(req->type),
        hexstr64s(req->addr), req->l1_bank, req->size);

  if((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
     (req->type == MRT_IFETCH)) {
    STAT_EVENT(req->proc_id, L1_HIT);
    STAT_EVENT(req->proc_id, CORE_L1_HIT);
    STAT_EVENT(req->proc_id, L1_HIT_ONPATH + req->off_path);
    if(0 && DEBUG_EXC_INSERTS) {
      printf("addr:%s hit in L1 type:%s\n", hexstr64s(req->addr),
             Mem_Req_Type_str(req->type));
    }
  }

  STAT_EVENT_ALL(L1_HIT_ALL);
  STAT_EVENT_ALL(L1_HIT_ALL_ONPATH + req->off_path);

  // cmp IGNORE
  if(req->off_path)
    STAT_EVENT(req->proc_id, L1_HIT_OFFPATH_IFETCH + MIN2(req->type, 6));
  else
    STAT_EVENT(req->proc_id, L1_HIT_ONPATH_IFETCH + MIN2(req->type, 6));

  if(!req->demand_match_prefetch &&
     (req->type == MRT_DFETCH || req->type == MRT_DSTORE ||
      req->type == MRT_IFETCH)) {
    DEBUG(req->proc_id, "Req index:%d no longer a chip demand\n", req->id);
  }

  // this is just a stat collection
  wp_process_l1_hit(data, req);

  if(L1_WRITE_THROUGH && (req->type == MRT_WB)) {
    req->state     = MRS_BUS_NEW;
    req->rdy_cycle = cycle_count + L1Q_TO_FSB_TRANSFER_LATENCY;
  } else if(fill_mlc) {
    req->state     = MRS_FILL_MLC;
    req->rdy_cycle = cycle_count + 1;
    // insert into mlc queue
    req->queue = &(mem->mlc_fill_queue);
    if(!ORDER_BEYOND_BUS)
      mem_insert_req_into_queue(
        req, req->queue,
        ALL_FIFO_QUEUES ? mlc_fill_seq_num : l1_queue_entry->priority);
    else
      mem_insert_req_into_queue(req, req->queue,
                                ALL_FIFO_QUEUES ? mlc_fill_seq_num : 0);
    mlc_fill_seq_num++;
  } else if(!req->done_func) {
    req->state = MRS_L1_HIT_DONE;
    // Free the request buffer
    mem_free_reqbuf(req);
  } else {
    req->state     = MRS_L1_HIT_DONE;
    req->rdy_cycle = freq_cycle_count(
      FREQ_DOMAIN_CORES[req->proc_id]);  // no +1 to match old performance
    // insert into core fill queue
    req->queue = &(mem->core_fill_queues[req->proc_id]);
    if(!ORDER_BEYOND_BUS)
      mem_insert_req_into_queue(req, req->queue,
                                ALL_FIFO_QUEUES ?
                                  core_fill_seq_num[req->proc_id] :
                                  l1_queue_entry->priority);
    else
      mem_insert_req_into_queue(
        req, req->queue, ALL_FIFO_QUEUES ? core_fill_seq_num[req->proc_id] : 0);
    core_fill_seq_num[req->proc_id]++;
  }

  /* Set the priority so that this entry will be removed from the l1_queue */
  l1_queue_entry->priority = Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];


  if(L2L1PREF_ON)
    l2l1pref_mem(req);

  return TRUE;
}

/**************************************************************************************/
/* mem_process_mlc_hit_access: */
/* Returns TRUE if mlc access is complete and needs to be removed from mlc_queue
 */

Flag mem_process_mlc_hit_access(Mem_Req* req, Mem_Queue_Entry* mlc_queue_entry,
                                Addr* line_addr, MLC_Data* data,
                                int lru_position) {
  if(!req->done_func ||
     req->done_func(req)) { /* If done_func is not complete we will keep
                               accessing MLC until done_func returns TRUE */

    if(data) { /* not perfect mlc */
      if((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
         (req->type == MRT_IFETCH)) {
        if(data->prefetch) {  // prefetch hit
          DEBUG(req->proc_id, "%7lld mlc prefetch hit %d\n", cycle_count,
                (int)(req->addr));
          STAT_EVENT(req->proc_id, MLC_PREF_HIT);
          if(!data->seen_prefetch) {
            data->seen_prefetch = TRUE;

            STAT_EVENT(req->proc_id, MLC_PREF_UNIQUE_HIT);
            STAT_EVENT(req->proc_id, PREF_MLC_TOTAL_USED);
            STAT_EVENT(req->proc_id, CORE_PREF_MLC_USED);
            STAT_EVENT(req->proc_id, CORE_MLC_PREF_FILL_USED);
          }
        }
      }

      if(req->type == MRT_DPRF || req->type == MRT_IPRF ||
         req->demand_match_prefetch) {
        STAT_EVENT(req->proc_id, MLC_PREF_REQ_HIT);
        STAT_EVENT(req->proc_id, CORE_MLC_PREF_REQ_HIT);
      } else if((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
                (req->type == MRT_IFETCH)) {
        STAT_EVENT(req->proc_id, MLC_DEMAND_HIT);
        STAT_EVENT(req->proc_id, CORE_MLC_DEMAND_HIT);
      } else {  // CMP Watch out RA
        STAT_EVENT(req->proc_id, MLC_WB_HIT);
        STAT_EVENT(req->proc_id, CORE_MLC_WB_HIT);
      }
      data->dirty |= (req->type == MRT_WB);
    }

    if((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
       (req->type == MRT_IFETCH)) {
      STAT_EVENT(req->proc_id, MLC_HIT);
      STAT_EVENT(req->proc_id, CORE_MLC_HIT);
      STAT_EVENT(req->proc_id, MLC_HIT_ONPATH + req->off_path);
      if(0 && DEBUG_EXC_INSERTS) {
        printf("addr:%s hit in MLC type:%s\n", hexstr64s(req->addr),
               Mem_Req_Type_str(req->type));
      }
    }

    STAT_EVENT_ALL(MLC_HIT_ALL);
    STAT_EVENT_ALL(MLC_HIT_ALL_ONPATH + req->off_path);

    // cmp IGNORE
    if(req->off_path)
      STAT_EVENT(req->proc_id, MLC_HIT_OFFPATH_IFETCH + MIN2(req->type, 6));
    else
      STAT_EVENT(req->proc_id, MLC_HIT_ONPATH_IFETCH + MIN2(req->type, 6));

    if(MLC_WRITE_THROUGH && (req->type == MRT_WB)) {
      req->state     = MRS_L1_NEW;
      req->rdy_cycle = cycle_count + MLCQ_TO_L1Q_TRANSFER_LATENCY;
    } else {  // writeback done
      /* Remove the entry from request buffer */
      req->state = MRS_MLC_HIT_DONE;
      mem_free_reqbuf(req);
    }

    /* Set the priority so that this entry will be removed from the mlc_queue */
    mlc_queue_entry->priority = Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];

    return TRUE;
  } else {
    return FALSE;
  }
}

/**************************************************************************************/
/* mem_process_l1_miss_access: */

static Flag mem_process_l1_miss_access(Mem_Req*         req,
                                       Mem_Queue_Entry* l1_queue_entry,
                                       Addr* line_addr, L1_Data* data) {
  DEBUG(req->proc_id,
        "Mem request missed in the L1  index:%ld  type:%s  addr:0x%s  "
        "l1_bank:%d  size:%d  state: %s\n",
        (long int)(req - mem->req_buffer), Mem_Req_Type_str(req->type),
        hexstr64s(req->addr), req->l1_bank, req->size,
        mem_req_state_names[req->state]);

  if(!req->l1_miss) {  // have we collected these statistics already?
    if(req->type == MRT_DFETCH || req->type == MRT_DSTORE ||
       req->type == MRT_IFETCH) {
      perf_pred_off_chip_effect_start(req);
      if(!req->demand_match_prefetch) {
        DEBUG(req->proc_id, "Req index:%d no longer a chip demand\n", req->id);
      }
    }

    if(req->type == MRT_DPRF || req->type == MRT_IPRF ||
       req->demand_match_prefetch) {
      STAT_EVENT(req->proc_id, L1_PREF_REQ_MISS);
      STAT_EVENT(req->proc_id, CORE_L1_PREF_REQ_MISS);
    } else if((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
              (req->type == MRT_IFETCH)) {
      STAT_EVENT(req->proc_id, L1_DEMAND_MISS);
      STAT_EVENT(req->proc_id, CORE_L1_DEMAND_MISS);
    } else {  // CMP Watch out RA
      STAT_EVENT(req->proc_id, L1_WB_MISS);
      STAT_EVENT(req->proc_id, CORE_L1_WB_MISS);
    }

    if((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
       (req->type == MRT_IFETCH)) {
      STAT_EVENT(req->proc_id, L1_MISS);
      STAT_EVENT(req->proc_id, CORE_L1_MISS);
      STAT_EVENT(req->proc_id, L1_MISS_ONPATH + req->off_path);
      STAT_EVENT(req->proc_id, PER1K_L1_DEMAND_MISS_ONPATH + req->off_path);
    }
    STAT_EVENT_ALL(L1_MISS_ALL);
    STAT_EVENT_ALL(L1_MISS_ALL_ONPATH + req->off_path);

    if(req->type == MRT_WB || req->type == MRT_WB_NODIRTY) {
      STAT_EVENT(req->proc_id, POWER_LLC_WRITE_MISS);
    } else {
      STAT_EVENT(req->proc_id, POWER_LLC_READ_MISS);
    }

    td->td_info.last_l1_miss_time = cycle_count;

    if(req->off_path)
      STAT_EVENT(req->proc_id, L1_MISS_OFFPATH_IFETCH + MIN2(req->type, 6));
    else
      STAT_EVENT(req->proc_id, L1_MISS_ONPATH_IFETCH + MIN2(req->type, 6));
  }

  if((req->type == MRT_WB) || (req->type == MRT_WB_NODIRTY)) {
    // if the request is a write back request then the processor just insert the
    // request to the L1 cache
    if(req->type == MRT_WB_NODIRTY)
      WARNING(0, "CMP: A WB_NODIRTY request found! Check it out!");

    if(req->done_func) {
      ASSERT(req->proc_id, ALLOW_TYPE_MATCHES);
      ASSERT(req->proc_id, req->wb_requested_back);
      if(req->done_func(req)) {
        if(!l1_fill_line(req)) {
          req->rdy_cycle = cycle_count + 1;
          return FALSE;
        }
        req->state     = MRS_L1_HIT_DONE;
        req->rdy_cycle = cycle_count + 1;
        mem_free_reqbuf(req);
        l1_queue_entry->priority = Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];
        return TRUE;
      } else {
        req->rdy_cycle = cycle_count + 1;
        return FALSE;
      }
    } else {
      STAT_EVENT(req->proc_id, WB_L1_MISS_FILL_L1);  // CMP remove this later
      if(!l1_fill_line(req)) {
        req->rdy_cycle = cycle_count + 1;
        return FALSE;
      }

      if(L1_WRITE_THROUGH && req->type == MRT_WB) {
        req->state     = MRS_BUS_NEW;
        req->rdy_cycle = cycle_count + L1Q_TO_FSB_TRANSFER_LATENCY;
      } else {  // CMP write back
        req->state     = MRS_L1_HIT_DONE;
        req->rdy_cycle = cycle_count + 1;
        mem_free_reqbuf(req);
      }
      l1_queue_entry->priority = Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];
      return TRUE;
    }
  }

  if(STALL_MEM_REQS_ONLY && !mem_req_type_is_stalling(req->type)) {
    // not calling done_func to avoid filling caches
    req->state     = MRS_INV;
    req->rdy_cycle = cycle_count + 1;
    mem_free_reqbuf(req);
    l1_queue_entry->priority = Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];
    return TRUE;
  }

  /* Mark the request as L1_miss */
  req->l1_miss       = TRUE;
  req->l1_miss_cycle = cycle_count;

  if((CONSTANT_MEMORY_LATENCY && !queue_full(&mem->l1fill_queue)) ||
     //(!CONSTANT_MEMORY_LATENCY && !queue_full(&mem->bus_out_queue))) {
     (!CONSTANT_MEMORY_LATENCY)) {
    // Ramulator: moving the lines below to where ramulator_send() is called

    //// cmp FIXME
    // if (TRACK_L1_MISS_DEPS || MARK_L1_MISSES)
    //    mark_ops_as_l1_miss(req);

    // req->state = MRS_BUS_NEW; // FIXME?
    // req->rdy_cycle = cycle_count + L1Q_TO_FSB_TRANSFER_LATENCY; /* this req
    // will be ready to be sent to memory in the next cycle */

    //// cmp FIXME
    // if (STREAM_PREFETCH_ON)
    //    stream_ul1_miss (req);

    ///* Set the priority so that this entry will be removed from the l1_queue
    ///*/
    // l1_queue_entry->priority = Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];

    // STAT_EVENT(req->proc_id, SEND_MISS_REQ_QUEUE);
    return TRUE;
  } else {
    // STAT_EVENT(req->proc_id, REJECTED_QUEUE_BUS_OUT);
    return FALSE;
  }
}

/**************************************************************************************/
/* mem_process_mlc_miss_access: */

static Flag mem_process_mlc_miss_access(Mem_Req*         req,
                                        Mem_Queue_Entry* mlc_queue_entry,
                                        Addr* line_addr, MLC_Data* data) {
  DEBUG(req->proc_id,
        "Mem request missed in the MLC  index:%ld  type:%s  addr:0x%s  "
        "mlc_bank:%d  size:%d  state: %s\n",
        (long int)(req - mem->req_buffer), Mem_Req_Type_str(req->type),
        hexstr64s(req->addr), req->mlc_bank, req->size,
        mem_req_state_names[req->state]);

  if(!req->mlc_miss) {  // have we marked this as MLC miss already (and thus
                        // collected the statistics)?
    if(req->type == MRT_DPRF || req->type == MRT_IPRF ||
       req->demand_match_prefetch) {
      STAT_EVENT(req->proc_id, MLC_PREF_REQ_MISS);
      STAT_EVENT(req->proc_id, CORE_MLC_PREF_REQ_MISS);
    } else if((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
              (req->type == MRT_IFETCH)) {
      STAT_EVENT(req->proc_id, MLC_DEMAND_MISS);
      STAT_EVENT(req->proc_id, CORE_MLC_DEMAND_MISS);
    } else {  // CMP Watch out RA
      STAT_EVENT(req->proc_id, MLC_WB_MISS);
      STAT_EVENT(req->proc_id, CORE_MLC_WB_MISS);
    }

    if((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
       (req->type == MRT_IFETCH)) {
      STAT_EVENT(req->proc_id, MLC_MISS);
      STAT_EVENT(req->proc_id, CORE_MLC_MISS);
      STAT_EVENT(req->proc_id, MLC_MISS_ONPATH + req->off_path);
    }
    STAT_EVENT(req->proc_id, MLC_MISS_ALL);
    STAT_EVENT(req->proc_id, MLC_MISS_ALL_ONPATH + req->off_path);

    if(req->off_path)
      STAT_EVENT(req->proc_id, MLC_MISS_OFFPATH_IFETCH + MIN2(req->type, 6));
    else
      STAT_EVENT(req->proc_id, MLC_MISS_ONPATH_IFETCH + MIN2(req->type, 6));
  }

  /* Mark the request as MLC_miss */
  req->mlc_miss       = TRUE;
  req->mlc_miss_cycle = cycle_count;

  if((req->type == MRT_WB) || (req->type == MRT_WB_NODIRTY)) {
    // if the request is a write back request then the processor just insert the
    // request to the MLC cache
    if(req->type == MRT_WB_NODIRTY)
      WARNING(0, "CMP: A WB_NODIRTY request found! Check it out!");

    if(req->done_func) {
      ASSERT(req->proc_id, ALLOW_TYPE_MATCHES);
      ASSERT(req->proc_id, req->wb_requested_back);
      if(req->done_func(req)) {
        mlc_fill_line(req);
        req->state     = MRS_MLC_HIT_DONE;
        req->rdy_cycle = cycle_count + 1;
        mem_free_reqbuf(req);
        mlc_queue_entry->priority = Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];
        return TRUE;
      } else {
        req->rdy_cycle = cycle_count + 1;
        return FALSE;
      }
    } else {
      STAT_EVENT(req->proc_id, WB_MLC_MISS_FILL_MLC);  // CMP remove this later
      mlc_fill_line(req);
      if(MLC_WRITE_THROUGH && req->type == MRT_WB) {
        req->state     = MRS_L1_NEW;
        req->rdy_cycle = cycle_count + MLCQ_TO_L1Q_TRANSFER_LATENCY;
      } else {  // CMP write back
        req->state     = MRS_MLC_HIT_DONE;
        req->rdy_cycle = cycle_count + 1;
        mem_free_reqbuf(req);
      }
      mlc_queue_entry->priority = Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];
      return TRUE;
    }
  }

  if(!queue_full(&mem->l1_queue)) {
    req->state     = MRS_L1_NEW;
    req->rdy_cycle = cycle_count +
                     MLCQ_TO_L1Q_TRANSFER_LATENCY; /* this req will be ready to
                                                      be sent to memory in the
                                                      next cycle */
    /* Set the priority so that this entry will be removed from the mlc_queue */
    mlc_queue_entry->priority = Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];
    return TRUE;
  } else {
    STAT_EVENT(req->proc_id, REJECTED_QUEUE_L1);
    return FALSE;
  }
}

/**************************************************************************************/
/* mem_complete_l1_access: */
/* Returns TRUE if l1 access is complete and needs to be removed from l1_queue
 */

static Flag mem_complete_l1_access(Mem_Req*         req,
                                   Mem_Queue_Entry* l1_queue_entry,
                                   int*             out_queue_insertion_count,
                                   int*             reserved_entry_count) {
  Addr     line_addr;
  L1_Data* data;
  int      lru_position  = -1;
  Flag     update_l1_lru = TRUE;

  if(L1_CACHE_HIT_POSITION_COLLECT ||
     (L1_DYNAMIC_PARTITION_ENABLE &&
      L1_DYNAMIC_PARTITION_POLICY == MARGINAL_UTIL)) {
    if((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
       (req->type == MRT_IFETCH)) {
      lru_position = cache_find_pos_in_lru_stack(
        &L1(req->proc_id)->cache, req->proc_id, req->addr, &line_addr);
      ASSERT(req->proc_id, lru_position < (int)L1_ASSOC);
    }
  }

  if(L1_DYNAMIC_PARTITION_ENABLE && L1_DYNAMIC_PARTITION_POLICY == UMON_DSS) {
    if((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
       (req->type == MRT_IFETCH)) {
      Addr             conv_addr, dummy_addr;
      Cache*           l1_cache;
      Cache*           umon_cache;
      Umon_Cache_Data* umon_data;
      uns              set;
      Addr             tag;
      int              lru_pos = -1;

      ASSERT(0, L1_CACHE_REPL_POLICY == REPL_PARTITION);
      ASSERT(0, ADDR_TRANSLATION == ADDR_TRANS_NONE);

      l1_cache = &L1(req->proc_id)->cache;
      set      = req->addr >> l1_cache->shift_bits & l1_cache->set_mask;
      if(set % 33 == 0) {
        set        = set / 33;  // converting the addr
        tag        = req->addr >> (l1_cache->shift_bits + l1_cache->set_bits);
        conv_addr  = tag << 5 | set;
        umon_cache = &mem->umon_cache_core[req->proc_id];

        lru_pos   = cache_find_pos_in_lru_stack(umon_cache, req->proc_id,
                                              conv_addr, &dummy_addr);
        umon_data = (Umon_Cache_Data*)cache_access(
          umon_cache, conv_addr, &dummy_addr, TRUE);  // acces umon cache

        if(!umon_data) {  // miss
          Addr repl_addr;
          umon_data = (Umon_Cache_Data*)cache_insert(
            umon_cache, req->proc_id, conv_addr, &dummy_addr, &repl_addr);
          ASSERT(req->proc_id, lru_pos == -1);
          umon_data->addr     = req->addr;
          umon_data->prefetch = FALSE;
        } else {  // hit
          ASSERT(req->proc_id, umon_data->addr == req->addr);
          ASSERT(req->proc_id, lru_pos > -1 && lru_pos < (int)L1_ASSOC);
          // increase the corresponding counter
          if(umon_data->prefetch) {
            umon_data->prefetch = FALSE;
          }
          mem->umon_cache_hit_count_core[req->proc_id][lru_pos]++;
        }
      }
    }
  }

  if(!PREFETCH_UPDATE_LRU_L1 &&
     (req->type == MRT_DPRF || req->type == MRT_IPRF))
    update_l1_lru = FALSE;
  data = (L1_Data*)cache_access(&L1(req->proc_id)->cache, req->addr, &line_addr,
                                update_l1_lru);  // access L2
  cache_part_l1_access(req);
  if(FORCE_L1_MISS)
    data = NULL;

  // cmp FIXME prefetchers
  if((req->type == MRT_DPRF || req->type == MRT_IPRF ||
      req->demand_match_prefetch) &&
     req->prefetcher_id != 0) {
    STAT_EVENT(req->proc_id, L1_PREF_ACCESS);
  } else {
    STAT_EVENT(req->proc_id, L1_DEMAND_ACCESS);
  }

  if(req->type == MRT_WB || req->type == MRT_WB_NODIRTY) {
    STAT_EVENT(req->proc_id, POWER_LLC_WRITE_ACCESS);
  } else {
    STAT_EVENT(req->proc_id, POWER_LLC_READ_ACCESS);
  }

  // cmp IGNORE
  if(L1_PREF_CACHE_ENABLE &&
     !data) /* do not put into L2 if this is a prefetch or off-path */
    data = l1_pref_cache_access(req);

  Flag access_done = TRUE;
  if(data || PERFECT_L1) { /* l1 hit */
    // if exclusive cache, invalidate the line in L2 if there is a done function
    // to transfer the data to L1 -- also need to propagate the dirty to L1
    Flag l1_hit_access = mem_process_l1_hit_access(
      req, l1_queue_entry, &line_addr, data, lru_position);
    if(!l1_hit_access)
      access_done = FALSE;
    else {
      if(!PREF_ORACLE_TRAIN_ON &&
         ((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
          (PREF_I_TOGETHER && req->type == MRT_IFETCH) ||
          (PREF_TRAIN_ON_PREF_MISSES && req->type == MRT_DPRF))) {
        // Train the Data prefetcher
        ASSERT(req->proc_id, PERFECT_L1 || data);
        ASSERT(req->proc_id, PERFECT_L1 || req->proc_id == data->proc_id);
        ASSERT(req->proc_id, req->proc_id == req->addr >> 58);
        pref_ul1_hit(req->proc_id, req->addr, req->loadPC, req->global_hist);
      }

      if(L1_WRITE_THROUGH && (req->type == MRT_WB) &&
         !CONSTANT_MEMORY_LATENCY) {
        // req->queue = &(mem->bus_out_queue);

        // mem_insert_req_into_queue (req, req->queue, ALL_FIFO_QUEUES ?
        // bus_out_seq_num : 0);
        ASSERT(req->proc_id, MRS_L1_WAIT == req->state);
        req->state    = MRS_MEM_NEW;
        l1_hit_access = ramulator_send(req);

        if(!l1_hit_access) {
          // request rejected by Ramulator, so restore state to
          // MRS_L1_WAIT to try again later
          req->state  = MRS_L1_WAIT;
          access_done = FALSE;
        } else {
          ASSERT(req->proc_id, req->mem_queue_cycle >= req->rdy_cycle);
          DEBUG(req->proc_id,
                "L1 write through request is sent to Ramulator\n");
          mem_seq_num++;
          // perf_pred_mem_req_start(req);
          mem_free_reqbuf(req);
        }

        // bus_out_seq_num++;
        //(*out_queue_insertion_count) += 1;
        // STAT_EVENT(req->proc_id, BUS_ACCESS);
      }
    }
    // CMP IGNORE
  } else { /* l1 miss */
    /* if req is wb then either fill l1 or try again */
    Flag l1_miss_send_bus = (L1_WRITE_THROUGH && (req->type == MRT_WB)) ||
                            ((req->type != MRT_WB) &&
                             (req->type != MRT_WB_NODIRTY));
    if(STALL_MEM_REQS_ONLY && !mem_req_type_is_stalling(req->type))
      l1_miss_send_bus = FALSE;
    Flag l1_miss_access = mem_process_l1_miss_access(req, l1_queue_entry,
                                                     &line_addr, data);
    if(l1_miss_access && l1_miss_send_bus) {
      if(CONSTANT_MEMORY_LATENCY) {
        mem->uncores[req->proc_id].num_outstanding_l1_misses++;
        mem_complete_bus_in_access(req, l1_queue_entry->priority);
        req->rdy_cycle       = cycle_count + freq_convert(FREQ_DOMAIN_MEMORY,
                                                    MEMORY_CYCLES,
                                                    FREQ_DOMAIN_L1);
        req->mem_queue_cycle = cycle_count;
        perf_pred_mem_req_start(req);
        STAT_EVENT(req->proc_id, POWER_MEMORY_ACCESS);
        STAT_EVENT(req->proc_id, POWER_MEMORY_CTRL_ACCESS);
        STAT_EVENT(req->proc_id, POWER_MEMORY_READ_ACCESS);  // writes not
                                                             // modeled under
                                                             // constant mem
                                                             // latency
        STAT_EVENT(req->proc_id, POWER_MEMORY_CTRL_READ);
        STAT_EVENT(req->proc_id,
                   POWER_DRAM_PRECHARGE);  // assume accesses are row conflicts
        STAT_EVENT(req->proc_id, POWER_DRAM_ACTIVATE);
        STAT_EVENT(req->proc_id, POWER_DRAM_READ);
      } else {
        // Ramulator remove
        // req->queue = &(mem->bus_out_queue);
        // mem_insert_req_into_queue (req, req->queue, ALL_FIFO_QUEUES ?
        // bus_out_seq_num : 0);

        ASSERT(req->proc_id, MRS_L1_WAIT == req->state);
        req->state     = MRS_MEM_NEW;
        l1_miss_access = ramulator_send(req);
        if(!l1_miss_access) {
          // STAT_EVENT(req->proc_id, REJECTED_QUEUE_BUS_OUT);

          req->state  = MRS_L1_WAIT;
          access_done = FALSE;
        } else {
          ASSERT(req->proc_id, req->mem_queue_cycle >= req->rdy_cycle);
          req->queue = NULL;

          DEBUG(req->proc_id, "l1 miss request is sent to ramulator\n");
          mem_seq_num++;
          perf_pred_mem_req_start(req);
          mem->uncores[req->proc_id].num_outstanding_l1_misses++;

          if(TRACK_L1_MISS_DEPS || MARK_L1_MISSES)
            mark_ops_as_l1_miss(req);

          // req->state = MRS_BUS_NEW; // FIXME?
          // req->rdy_cycle = cycle_count + L1Q_TO_FSB_TRANSFER_LATENCY; /* this
          // req will be ready to be sent to memory in the next cycle */

          // cmp FIXME
          if(STREAM_PREFETCH_ON)
            stream_ul1_miss(req);

          /* Set the priority so that this entry will be removed from the
           * l1_queue */
          l1_queue_entry->priority = Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];

          STAT_EVENT(req->proc_id, SEND_MISS_REQ_QUEUE);
          // return TRUE;

          // BEN: THIS IS NOT TRUE!!!!!!!!
          // if(req->type == MRT_DSTORE) {  // write requests can be informed as
          //                                // done as soon as they are enqueued
          //                                to
          //                                // Ramulator
          //   mem->uncores[req->proc_id].num_outstanding_l1_misses--;
          //   mem_free_reqbuf(req);
          // }

          ASSERTM(0,
                  req->type == MRT_DSTORE || req->type == MRT_IFETCH ||
                    req->type == MRT_DFETCH || req->type == MRT_IPRF ||
                    req->type == MRT_DPRF,
                  "ERROR: Issuing a currently unhandled request type (%s) to "
                  "Ramulator\n",
                  Mem_Req_Type_str(req->type));
        }

        // bus_out_seq_num++;
        if(HIER_MSHR_ON && (req->type != MRT_WB) &&
           (req->type != MRT_WB_NODIRTY)) {
          (*reserved_entry_count) += 1;  // writebacks are not reserved (they
                                         // never come back)
          req->reserved_entry_count += 1;
        }
        // STAT_EVENT(req->proc_id, BUS_ACCESS);
      }
      //(*out_queue_insertion_count) += 1;

      if(!PREF_ORACLE_TRAIN_ON &&
         ((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
          (PREF_I_TOGETHER && req->type == MRT_IFETCH) ||
          (PREF_TRAIN_ON_PREF_MISSES && req->type == MRT_DPRF))) {
        // Train the Data prefetcher
        pref_ul1_miss(req->proc_id, req->addr, req->loadPC, req->global_hist);
      }

      // cmp FIXME prefetchers
      if((req->type == MRT_DPRF || req->type == MRT_IPRF ||
          req->demand_match_prefetch) &&
         req->prefetcher_id !=
           0) {  // cmp FIXME What can I do for the prefetcher?

        pref_ul1sent(req->proc_id, req->addr, req->prefetcher_id);
        STAT_EVENT(req->proc_id, BUS_PREF_ACCESS);
      } else {
        STAT_EVENT(req->proc_id, BUS_DEMAND_ACCESS);
      }
    } else if(!l1_miss_access) {
      access_done = FALSE;
    }
  }

  if(access_done) {
    ASSERT(req->proc_id,
           mem->uncores[req->proc_id].num_outstanding_l1_accesses > 0);
    mem->uncores[req->proc_id].num_outstanding_l1_accesses--;
  }
  return access_done;
}

/**************************************************************************************/
/* mem_complete_mlc_access: */
/* Returns TRUE if mlc access is complete and needs to be removed from mlc_queue
 */

static Flag mem_complete_mlc_access(Mem_Req*         req,
                                    Mem_Queue_Entry* mlc_queue_entry,
                                    int*             l1_queue_insertion_count,
                                    int*             reserved_entry_count) {
  Addr      line_addr;
  MLC_Data* data;
  int       lru_position   = -1;
  Flag      update_mlc_lru = TRUE;

  if(!PREFETCH_UPDATE_LRU_MLC &&
     (req->type == MRT_DPRF || req->type == MRT_IPRF))
    update_mlc_lru = FALSE;
  data = (MLC_Data*)cache_access(&MLC(req->proc_id)->cache, req->addr,
                                 &line_addr, update_mlc_lru);  // access MLC

  if(data || PERFECT_MLC) { /* mlc hit */
    // if exclusive cache, invalidate the line in L2 if there is a done function
    // to transfer the data to MLC -- also need to propagate the dirty to MLC
    Flag mlc_hit_access = mem_process_mlc_hit_access(
      req, mlc_queue_entry, &line_addr, data, lru_position);
    if(!mlc_hit_access) {
      return FALSE;
    } else {
      if(!PREF_ORACLE_TRAIN_ON &&
         ((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
          (PREF_I_TOGETHER && req->type == MRT_IFETCH) ||
          (PREF_TRAIN_ON_PREF_MISSES && req->type == MRT_DPRF))) {
        // Train the Data prefetcher
        ASSERT(req->proc_id, data);
        ASSERT(req->proc_id, req->proc_id == data->proc_id);
        ASSERT(req->proc_id, req->proc_id == req->addr >> 58);
        pref_umlc_hit(req->proc_id, req->addr, req->loadPC, req->global_hist);
      }

      if(MLC_WRITE_THROUGH && (req->type == MRT_WB)) {
        req->queue = &(mem->l1_queue);
        mem_insert_req_into_queue(req, req->queue,
                                  ALL_FIFO_QUEUES ? l1_seq_num : 0);
        l1_seq_num++;
        (*l1_queue_insertion_count) += 1;
        STAT_EVENT(req->proc_id, L1_ACCESS);
      }
      return TRUE;
    }
  } else { /* mlc miss */
    /* if req is wb then either fill mlc or try again */
    Flag mlc_miss_send_l1 = (MLC_WRITE_THROUGH && (req->type == MRT_WB)) ||
                            ((req->type != MRT_WB) &&
                             (req->type != MRT_WB_NODIRTY));
    Flag mlc_miss_access = mem_process_mlc_miss_access(req, mlc_queue_entry,
                                                       &line_addr, data);
    if(mlc_miss_access && mlc_miss_send_l1) {
      DEBUG(
        req->proc_id,
        "mlc miss request is inserted to l1 queue rc:%d mlc:%d bo:%d lf:%d\n",
        mem->req_count, mem->mlc_queue.entry_count, mem->l1_queue.entry_count,
        mem->mlc_fill_queue.entry_count);

      req->queue = &(mem->l1_queue);
      mem_insert_req_into_queue(
        req, req->queue,
        ALL_FIFO_QUEUES ? l1_seq_num : 0);  // queue full check is done in
                                            // mem_process_mlc_miss_access
      l1_seq_num++;
      (*l1_queue_insertion_count) += 1;
      if(HIER_MSHR_ON && (req->type != MRT_WB) &&
         (req->type != MRT_WB_NODIRTY)) {
        (*reserved_entry_count) += 1;
        req->reserved_entry_count += 1;
      }
      STAT_EVENT(req->proc_id, L1_ACCESS);

      if(!PREF_ORACLE_TRAIN_ON &&
         ((req->type == MRT_DFETCH) || (req->type == MRT_DSTORE) ||
          (PREF_I_TOGETHER && req->type == MRT_IFETCH) ||
          (PREF_TRAIN_ON_PREF_MISSES && req->type == MRT_DPRF))) {
        // Train the Data prefetcher
        pref_umlc_miss(req->proc_id, req->addr, req->loadPC, req->global_hist);
      }

      return TRUE;
    } else if(!mlc_miss_access) {
      return FALSE;
    }
    return TRUE;
  }
  ASSERT(req->proc_id, 0);
}

/**************************************************************************************/
/* mem_process_new_reqs: */
/* Access L1 if port is ready - If L1 miss, then put the request into miss queue
 */

static void mem_process_l1_reqs() {
  Mem_Req* req = NULL;
  int      ii;
  int      reqbuf_id;
  int      l1_queue_removal_count       = 0;
  int      out_queue_insertion_count    = 0;
  int      l1_queue_reserve_entry_count = 0;

  /* Go thru the l1_queue and try to access L1 for each request */

  for(ii = 0; ii < mem->l1_queue.entry_count; ii++) {
    reqbuf_id = mem->l1_queue.base[ii].reqbuf;
    req       = &(mem->req_buffer[reqbuf_id]);

    // this is just a print
    if(req->state == MRS_INV) {
      print_mem_queue(QUEUE_L1 | QUEUE_BUS_OUT | QUEUE_L1FILL | QUEUE_MLC |
                      QUEUE_MLC_FILL);
    }

    ASSERTM(req->proc_id, req->state != MRS_INV,
            "id:%d state:%s type:%s rc:%d l1:%d bi:%d lf:%d\n", req->id,
            mem_req_state_names[req->state], Mem_Req_Type_str(req->type),
            mem->req_count, mem->l1_queue.entry_count,
            mem->bus_out_queue.entry_count, mem->l1fill_queue.entry_count);

    /* if the request is not yet ready, then try the next one */
    if(cycle_count < req->rdy_cycle)
      continue;

    /* Request is ready: see what state it is in */

    /* If this is a new request, reserve L1 port and transition to wait state */
    if(req->state == MRS_L1_NEW) {
      mem_start_l1_access(req);
      STAT_EVENT(req->proc_id, L1_ACCESS);
      if(req->type == MRT_DPRF || req->type == MRT_IPRF)
        STAT_EVENT(req->proc_id, L1_PREF_ACCESS);
      else
        STAT_EVENT(req->proc_id, L1_DEMAND_ACCESS);
    } else {
      ASSERTM(req->proc_id, req->state == MRS_L1_WAIT,
              "id:%d state:%s type:%s rc:%d l1:%d bi:%d lf:%d\n", req->id,
              mem_req_state_names[req->state], Mem_Req_Type_str(req->type),
              mem->req_count, mem->l1_queue.entry_count,
              mem->bus_out_queue.entry_count, mem->l1fill_queue.entry_count);

      if(mem_complete_l1_access(req, &(mem->l1_queue.base[ii]),
                                &out_queue_insertion_count,
                                &l1_queue_reserve_entry_count))
        l1_queue_removal_count++;
    }
  }

  ASSERT(req->proc_id, out_queue_insertion_count <= l1_queue_removal_count);
  ASSERT(req->proc_id,
         l1_queue_reserve_entry_count <= out_queue_insertion_count);

  /* Remove requests from l1 access queue */
  if(l1_queue_removal_count > 0) {
    /* After this sort requests that should be removed will be at the tail of
     * the l1_queue */
    DEBUG(0, "l1_queue removal\n");
    qsort(mem->l1_queue.base, mem->l1_queue.entry_count,
          sizeof(Mem_Queue_Entry), mem_compare_priority);
    mem->l1_queue.entry_count -= l1_queue_removal_count;
    ASSERT(req->proc_id, mem->l1_queue.entry_count >= 0);
    /* if HIER_MSHR_ON, requests stay in the queues until filled (by reserving
     * entries) */
    if(HIER_MSHR_ON) {
      mem->l1_queue.reserved_entry_count += l1_queue_reserve_entry_count;
    }
  }

  /* Sort the out queue if requests were inserted */
  if(!ALL_FIFO_QUEUES && (out_queue_insertion_count > 0)) {
    if(CONSTANT_MEMORY_LATENCY) {  // request went straight to L1 fill queue
      qsort(mem->l1fill_queue.base, mem->l1fill_queue.entry_count,
            sizeof(Mem_Queue_Entry), mem_compare_priority);
    } else {
      qsort(mem->bus_out_queue.base, mem->bus_out_queue.entry_count,
            sizeof(Mem_Queue_Entry), mem_compare_priority);
    }
  }
}

/**************************************************************************************/
/* mem_process_mlc_reqs: */
/* Access MLC if port is ready - If MLC miss, then put the request into miss
 * queue */

static void mem_process_mlc_reqs() {
  Mem_Req* req = NULL;
  int      ii;
  int      reqbuf_id;
  int      mlc_queue_removal_count       = 0;
  int      l1_queue_insertion_count      = 0;
  int      mlc_queue_reserve_entry_count = 0;

  /* Go thru the mlc_queue and try to access MLC for each request */

  for(ii = 0; ii < mem->mlc_queue.entry_count; ii++) {
    reqbuf_id = mem->mlc_queue.base[ii].reqbuf;
    req       = &(mem->req_buffer[reqbuf_id]);

    // this is just a print
    if(req->state == MRS_INV) {
      print_mem_queue(QUEUE_L1 | QUEUE_BUS_OUT | QUEUE_L1FILL | QUEUE_MLC |
                      QUEUE_MLC_FILL);
    }

    ASSERTM(req->proc_id, req->state != MRS_INV,
            "id:%d state:%s type:%s rc:%d mlc:%d l1:%d mf:%d\n", req->id,
            mem_req_state_names[req->state], Mem_Req_Type_str(req->type),
            mem->req_count, mem->mlc_queue.entry_count,
            mem->l1_queue.entry_count, mem->mlc_fill_queue.entry_count);

    /* if the request is not yet ready, then try the next one */
    if(cycle_count < req->rdy_cycle)
      continue;

    /* Request is ready: see what state it is in */

    /* If this is a new request, reserve MLC port and transition to wait state
     */
    if(req->state == MRS_MLC_NEW) {
      mem_start_mlc_access(req);
      STAT_EVENT(req->proc_id, MLC_ACCESS);
      if(req->type == MRT_DPRF || req->type == MRT_IPRF)
        STAT_EVENT(req->proc_id, MLC_PREF_ACCESS);
      else
        STAT_EVENT(req->proc_id, MLC_DEMAND_ACCESS);
    } else {
      ASSERTM(req->proc_id, req->state == MRS_MLC_WAIT,
              "id:%d state:%s type:%s rc:%d mlc:%d l1:%d mf:%d\n", req->id,
              mem_req_state_names[req->state], Mem_Req_Type_str(req->type),
              mem->req_count, mem->mlc_queue.entry_count,
              mem->l1_queue.entry_count, mem->mlc_fill_queue.entry_count);
      if(mem_complete_mlc_access(req, &(mem->mlc_queue.base[ii]),
                                 &l1_queue_insertion_count,
                                 &mlc_queue_reserve_entry_count))
        mlc_queue_removal_count++;
    }
  }

  ASSERT(req->proc_id, l1_queue_insertion_count <= mlc_queue_removal_count);
  ASSERT(req->proc_id,
         mlc_queue_reserve_entry_count <= l1_queue_insertion_count);

  /* Remove requests from mlc access queue */
  if((mlc_queue_removal_count > 0)) {
    /* After this sort requests that should be removed will be at the tail of
     * the mlc_queue */
    DEBUG(0, "mlc_queue removal\n");
    qsort(mem->mlc_queue.base, mem->mlc_queue.entry_count,
          sizeof(Mem_Queue_Entry), mem_compare_priority);
    mem->mlc_queue.entry_count -= mlc_queue_removal_count;
    ASSERT(req->proc_id, mem->mlc_queue.entry_count >= 0);
    /* if HIER_MSHR_ON, requests stay in the queues until filled (by reserving
     * entries) */
    if(HIER_MSHR_ON) {
      mem->mlc_queue.reserved_entry_count += mlc_queue_reserve_entry_count;
    }
  }

  /* Sort the l1 queue if requests were inserted */
  if(!ALL_FIFO_QUEUES && (l1_queue_insertion_count > 0)) {
    qsort(mem->l1_queue.base, mem->l1_queue.entry_count,
          sizeof(Mem_Queue_Entry), mem_compare_priority);
  }
}

/**************************************************************************************/
/* mem_process_bus_out_reqs: */
/* FIXME: need to busy the bus for the time a line is being sent */

static void mem_process_bus_out_reqs() {
  Mem_Req* req;
  int      ii;
  int      reqbuf_id;
  int      bus_schedule = FALSE;

  // Ramulator implements separate queues for read/write requests
  // per channel. Hence, requests in the bus_out_queue needs to be
  // checked to see if their target queue is available. Removing
  // early return case, i.e., MEM_QUEUE_FULL

  // if(mem->mem_queue.entry_count == MEM_MEM_QUEUE_ENTRIES ||
  if(mem->bus_out_queue.entry_count == 0) {
    // if (mem->bus_out_queue.entry_count > 0) {
    //    STAT_EVENT_ALL(MEM_QUEUE_FULL);
    //}
    // return; // VEYNU: if there is no room in the mem queue do nothing
    return;  // Ramulator: early return if bus_out_queue is empty
  }
  ASSERTM(0, FALSE,
          "ERROR: bus_out_queue should always be empty\n");  // Ramulator
  // Ramulator handles off-chip communication latency itself. So we
  // do not make use of bus_out_queue anymore.

  /* Go thru the bus_out_queue and try to get the bus for the highest priority
   * ready request */

  if(OLDEST_FIRST_TO_MEM_QUEUE) {  // Original scheduling
    for(ii = 0; ii < mem->bus_out_queue.entry_count; ii++) {
      reqbuf_id = mem->bus_out_queue.base[ii].reqbuf;
      req       = &(mem->req_buffer[reqbuf_id]);
      ASSERT(req->proc_id, req->state != MRS_INV);
      ASSERT(req->proc_id,
             req->state == MRS_BUS_NEW); /* only those requests that are new
                                            will be handled in this stage */

      if(cycle_count < req->rdy_cycle)
        continue;

      ASSERTM(
        0, !MEM_MEM_QUEUE_PARTITION_ENABLE,
        "ERROR: MEM_QUEUE partitioning is not implemented in Ramulator!\n");
      // if (MEM_MEM_QUEUE_PARTITION_ENABLE) {
      //    if (mem->mem_queue_entry_count_bank[req->mem_flat_bank] ==
      //    MEM_MEM_QUEUE_ENTRIES / (RAMULATOR_CHANNELS * RAMULATOR_BANKS)) {
      //        continue;
      //    }
      //}

      /* Adjust the request's priority so that it will be removed */
      bus_schedule = TRUE;
      mem->bus_out_queue.base[ii].priority =
        Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];

      DEBUG(req->proc_id,
            "Mem request acquired the bus out  index:%ld  type:%s  addr:0x%s  "
            "size:%d  state: %s\n",
            (long int)(req - mem->req_buffer), Mem_Req_Type_str(req->type),
            hexstr64s(req->addr), req->size, mem_req_state_names[req->state]);

      /* Send one at a time*/
      if(bus_schedule)
        break;
    }
  } else if(ROUND_ROBIN_TO_MEM_QUEUE) {
    uns8 proc_id;
    uns8 next_proc_id;

    ASSERTM(0, !MEM_MEM_QUEUE_PARTITION_ENABLE,
            "ERROR: MEM_QUEUE partitioning is not implemented in Ramulator!\n");
    ASSERT(0,
           MEM_MEM_QUEUE_PARTITION_ENABLE &&
             MEM_BUS_OUT_QUEUE_PARTITION_ENABLE);  // these have to be enabled

    for(proc_id = 0; proc_id < NUM_CORES;
        proc_id++) {  // initialize the round robin scheduling
      mem->bus_out_queue_index_core[proc_id] = -1;  // -1 = not ready for
                                                    // scheduling
      mem->bus_out_queue_seen_oldest_core[proc_id] = FALSE;
    }

    for(ii = 0; ii < mem->bus_out_queue.entry_count;
        ii++) {  // Set candidates for the all cores by searching the whole
                 // bus_out_queue
      reqbuf_id = mem->bus_out_queue.base[ii].reqbuf;
      req       = &(mem->req_buffer[reqbuf_id]);
      ASSERT(req->proc_id, req->state != MRS_INV);
      ASSERT(req->proc_id,
             req->state == MRS_BUS_NEW); /* only those requests that are new
                                            will be handled in this stage */

      if(cycle_count < req->rdy_cycle)
        continue;

      if(MEM_BUS_OUT_QUEUE_AS_FIFO) {  // Assuming bus out queue is a FIFO. The
                                       // oldest one can block others
        if(!mem->bus_out_queue_seen_oldest_core
              [req->proc_id]) {  // this looks at only the oldest req for each
                                 // bus_out_queue per core (assuming
                                 // bus_out_queue is a FIFO
          ASSERT(0, mem->bus_out_queue_entry_count_core[req->proc_id]);
          mem->bus_out_queue_seen_oldest_core[req->proc_id] = TRUE;

          // if (mem->mem_queue_entry_count_bank[req->mem_flat_bank] <
          // MEM_MEM_QUEUE_ENTRIES / (RAMULATOR_CHANNELS * RAMULATOR_BANKS)) {
          // // a candidate is found when the bank is availabe
          //    mem->bus_out_queue_index_core[req->proc_id] = ii;
          //}
        }
      } else {  // Assuming bus out queue can be searched through. Non-blocking
        if(mem->bus_out_queue_index_core[req->proc_id] == -1) {
          // if (mem->mem_queue_entry_count_bank[req->mem_flat_bank] <
          // MEM_MEM_QUEUE_ENTRIES / (RAMULATOR_CHANNELS * RAMULATOR_BANKS))
          // mem->bus_out_queue_index_core[req->proc_id] = ii;
        }
      }
    }

    next_proc_id = mem->bus_out_queue_round_robin_next_proc_id;  // really
                                                                 // scheduling,
                                                                 // the next
                                                                 // proc_id gets
                                                                 // the highest
                                                                 // priority
    for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      if(mem->bus_out_queue_index_core[next_proc_id] != -1) {  // found one
        bus_schedule = TRUE;
        mem->bus_out_queue.base[mem->bus_out_queue_index_core[next_proc_id]]
          .priority = Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];

        reqbuf_id = mem->bus_out_queue
                      .base[mem->bus_out_queue_index_core[next_proc_id]]
                      .reqbuf;
        req = &(mem->req_buffer[reqbuf_id]);

        // update round_robin for the next schedule only when this scheduling is
        // successful.
        mem->bus_out_queue_round_robin_next_proc_id =
          (mem->bus_out_queue_round_robin_next_proc_id + 1) % NUM_CORES;
        break;
      }
      next_proc_id = (next_proc_id + 1) % NUM_CORES;  // look at the next core
    }
  } else if(ONE_CORE_FIRST_TO_MEM_QUEUE) {
    uns8 proc_id;
    uns8 next_proc_id;

    ASSERTM(0, !MEM_MEM_QUEUE_PARTITION_ENABLE,
            "ERROR: MEM_QUEUE partitioning is not implemented in Ramulator!\n");
    ASSERT(0,
           MEM_MEM_QUEUE_PARTITION_ENABLE &&
             MEM_BUS_OUT_QUEUE_PARTITION_ENABLE);  // these have to be enabled

    for(proc_id = 0; proc_id < NUM_CORES;
        proc_id++) {  // initialize the round robin scheduling
      mem->bus_out_queue_index_core[proc_id] = -1;  // -1 = not ready for
                                                    // scheduling
      mem->bus_out_queue_seen_oldest_core[proc_id] = FALSE;
    }

    for(ii = 0; ii < mem->bus_out_queue.entry_count;
        ii++) {  // Set candidates for the all cores by searching the whole
                 // bus_out_queue
      reqbuf_id = mem->bus_out_queue.base[ii].reqbuf;
      req       = &(mem->req_buffer[reqbuf_id]);
      ASSERT(req->proc_id, req->state != MRS_INV);
      ASSERT(req->proc_id,
             req->state == MRS_BUS_NEW); /* only those requests that are new
                                            will be handled in this stage */

      if(cycle_count < req->rdy_cycle)
        continue;
      if(MEM_BUS_OUT_QUEUE_AS_FIFO) {  // Assuming bus out queue is a FIFO. The
                                       // oldest one can block others
        if(!mem->bus_out_queue_seen_oldest_core
              [req->proc_id]) {  // this looks at only the oldest req for each
                                 // bus_out_queue per core (assuming
                                 // bus_out_queue is a FIFO
          ASSERT(0, mem->bus_out_queue_entry_count_core[req->proc_id]);
          mem->bus_out_queue_seen_oldest_core[req->proc_id] = TRUE;

          // if (mem->mem_queue_entry_count_bank[req->mem_flat_bank] <
          // MEM_MEM_QUEUE_ENTRIES / (RAMULATOR_CHANNELS * RAMULATOR_BANKS)) {
          // // a candidate is found when the bank is availabe
          //    mem->bus_out_queue_index_core[req->proc_id] = ii; // a candidate
          //    is found
          //}
        }
      } else {  // Assuming bus out queue can be searched through. Non-blocking
        if(mem->bus_out_queue_index_core[req->proc_id] == -1) {
          // if (mem->mem_queue_entry_count_bank[req->mem_flat_bank] <
          // MEM_MEM_QUEUE_ENTRIES / (RAMULATOR_CHANNELS * RAMULATOR_BANKS))
          //    mem->bus_out_queue_index_core[req->proc_id] = ii;
        }
      }
    }


    next_proc_id = mem->bus_out_queue_round_robin_next_proc_id;  // really
                                                                 // scheduling,
                                                                 // the previous
                                                                 // proc_id gets
                                                                 // the highest
                                                                 // priority
    for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      if(mem->bus_out_queue_index_core[next_proc_id] != -1) {  // found one
        bus_schedule = TRUE;
        mem->bus_out_queue.base[mem->bus_out_queue_index_core[next_proc_id]]
          .priority = Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];

        reqbuf_id = mem->bus_out_queue
                      .base[mem->bus_out_queue_index_core[next_proc_id]]
                      .reqbuf;
        req = &(mem->req_buffer[reqbuf_id]);

        if(ONE_CORE_FIRST_TO_MEM_QUEUE_TH) {
          if(mem->bus_out_queue_round_robin_next_proc_id == next_proc_id) {
            mem->bus_out_queue_one_core_first_num_sent++;
          } else {
            mem->bus_out_queue_round_robin_next_proc_id = next_proc_id;
            mem->bus_out_queue_one_core_first_num_sent  = 1;
          }

          if(ONE_CORE_FIRST_TO_MEM_QUEUE_TH <=
             mem->bus_out_queue_one_core_first_num_sent) {
            mem->bus_out_queue_round_robin_next_proc_id = (next_proc_id + 1) %
                                                          NUM_CORES;
            mem->bus_out_queue_one_core_first_num_sent = 0;
          }
        } else {
          mem->bus_out_queue_round_robin_next_proc_id = next_proc_id;
        }
        break;
      }
      next_proc_id = (next_proc_id + 1) % NUM_CORES;  // look at the next core
    }
  } else
    ASSERTM(0, 0, "Set mem_queue scheduling policy!!\n");


  if(bus_schedule) {
    ASSERT(0, req);
    /* Request is accepted to the bus - change its state and ready cycle */
    req->state = MRS_MEM_NEW;

    /* Crossing frequency domain boundary between the chip and memory controller
     */
    req->rdy_cycle = freq_cycle_count(FREQ_DOMAIN_MEMORY) + 1;

    /* Insert the request into mem queue --- perhaps this should not really be a
     * queue */
    req->queue = NULL;  // &(mem->mem_queue); Ramulator_edit: not sure what to
                        // put in here
    req->mem_queue_cycle = cycle_count;  // Ramulator_note: this is currently
                                         // not used by Ramulator
    req->mem_seq_num = mem_seq_num;      // Ramulator_note: no idea what this is
                                     // doing. Currently not used by Ramulator
    STAT_EVENT(0, MEM_QUEUE_ARRIVAL_DISTANCE_0 +
                    MIN2((cycle_count - mem->last_mem_queue_cycle) / 10, 100));
    mem->last_mem_queue_cycle = cycle_count;
    memview_memqueue(MEMVIEW_MEMQUEUE_ARRIVE,
                     req);  // Ramulator_note: what is memview?

    STAT_EVENT(req->proc_id, POWER_MEMORY_CTRL_ACCESS);
    if(req->type == MRT_WB || req->type == MRT_WB_NODIRTY) {
      STAT_EVENT(req->proc_id, POWER_MEMORY_CTRL_WRITE);
    } else {
      STAT_EVENT(req->proc_id, POWER_MEMORY_CTRL_READ);
    }

    ASSERTM(0, !MEM_MEM_QUEUE_PARTITION_ENABLE,
            "ERROR: MEM_QUEUE partitioning is not implemented in Ramulator!\n");
    if(MEM_BUS_OUT_QUEUE_PARTITION_ENABLE) {
      ASSERT(0, mem->bus_out_queue_entry_count_core[req->proc_id] > 0);
      mem->bus_out_queue_entry_count_core[req->proc_id]--;
    }

    // if (!ORDER_BEYOND_BUS)
    //    mem_insert_req_into_queue (req, req->queue, mem_seq_num);
    // else
    //    mem_insert_req_into_queue (req, req->queue, ALL_FIFO_QUEUES ?
    //    mem_seq_num : 0);
    Flag sent = ramulator_send(
      req);  // Ramulator_note: Does ramulator need to do anything
             // with mem_seq_num?
    if(sent) {
      ASSERT(req->proc_id, req->mem_queue_cycle >= req->rdy_cycle);
    }

    mem_seq_num++;  // Ramulator_note: Do we need to move this after
                    // ramulator_send()?

    perf_pred_mem_req_start(
      req);  // Ramulator_note: Do we need to call this after ramulator_send()?
    if(mem->uncores[req->proc_id].num_outstanding_l1_misses == 0) {
      STAT_EVENT(req->proc_id, CORE_MLP_CLUSTERS);
    }
    mem->uncores[req->proc_id]
      .num_outstanding_l1_misses++;  // Ramulator_note: Do we need to move this
                                     // after ramulator_send()?
    // dram->proc_infos[req->proc_id].reqs_per_bank[req->mem_flat_bank]++; //
    // Ramulator_todo: replicate this stat

    // if (MEM_MEM_QUEUE_PARTITION_ENABLE) {
    //    ASSERT(0, mem->mem_queue_entry_count_bank[req->mem_flat_bank] <
    //    MEM_MEM_QUEUE_ENTRIES / (RAMULATOR_CHANNELS * RAMULATOR_BANKS));
    //    mem->mem_queue_entry_count_bank[req->mem_flat_bank]++;
    //}

    DEBUG(0, "bus_out_queue removal\n");
    qsort(mem->bus_out_queue.base, mem->bus_out_queue.entry_count,
          sizeof(Mem_Queue_Entry), mem_compare_priority);
    mem->bus_out_queue.entry_count--;
    ASSERT(req->proc_id, mem->bus_out_queue.entry_count >= 0);

    // Ramulator_remove: Ramulator implements its own request queues. This
    // is not needed anymore
    // if (ORDER_BEYOND_BUS)
    //    qsort(mem->mem_queue.base, mem->mem_queue.entry_count,
    //    sizeof(Mem_Queue_Entry), mem_compare_priority);
  }
}

/**************************************************************************************/
/* mem_complete_bus_in_access: */

void mem_complete_bus_in_access(Mem_Req* req, Counter priority) {
  DEBUG(req->proc_id,
        "Mem request completed bus in access  index:%ld  type:%s  addr:0x%s  "
        "size:%d  state: %s\n",
        (long int)(req - mem->req_buffer), Mem_Req_Type_str(req->type),
        hexstr64s(req->addr), req->size, mem_req_state_names[req->state]);

  req->state = MRS_FILL_L1;

  /* Crossing frequency domain boundary between the chip and memory controller
   */
  req->rdy_cycle = freq_cycle_count(FREQ_DOMAIN_L1) + 1;

  req->queue = &(mem->l1fill_queue);

  if(!ORDER_BEYOND_BUS)
    mem_insert_req_into_queue(req, req->queue,
                              ALL_FIFO_QUEUES ? l1fill_seq_num : priority);
  else
    mem_insert_req_into_queue(req, req->queue,
                              ALL_FIFO_QUEUES ? l1fill_seq_num : 0);

  l1fill_seq_num++;
  ASSERT(req->proc_id,
         mem->uncores[req->proc_id].num_outstanding_l1_misses > 0);
  mem->uncores[req->proc_id].num_outstanding_l1_misses--;

  if(!CONSTANT_MEMORY_LATENCY && !PERF_PRED_REQS_FINISH_AT_FILL)
    perf_pred_mem_req_done(req);

  if(req->type != MRT_WB_NODIRTY && req->type != MRT_WB) {
    INC_STAT_EVENT_ALL(TOTAL_MEM_LATENCY,
                       req->rdy_cycle - req->mem_queue_cycle);
    INC_STAT_EVENT(req->proc_id, CORE_MEM_LATENCY,
                   req->rdy_cycle - req->mem_queue_cycle);
    // INC_STAT_EVENT(req->proc_id, CORE_MEM_STALLING_LATENCY_IFETCH +
    // req->type, dram_sched_stalling_age(req)); // Ramulator_todo: replicate
    // this stat
    INC_STAT_EVENT(req->proc_id, CORE_MEM_LATENCY_IFETCH + req->type,
                   req->rdy_cycle - req->mem_queue_cycle);
    if(req->type != MRT_DPRF && req->type != MRT_IPRF &&
       !req->demand_match_prefetch) {
      INC_STAT_EVENT_ALL(TOTAL_MEM_LATENCY_DEMAND,
                         req->rdy_cycle - req->mem_queue_cycle);
      INC_STAT_EVENT(req->proc_id, CORE_MEM_LATENCY_DEMAND,
                     req->rdy_cycle - req->mem_queue_cycle);
    } else {
      INC_STAT_EVENT_ALL(TOTAL_MEM_LATENCY_PREF,
                         req->rdy_cycle - req->mem_queue_cycle);
      INC_STAT_EVENT(req->proc_id, CORE_MEM_LATENCY_PREF,
                     req->rdy_cycle - req->mem_queue_cycle);
    }
  }
}

static void remove_from_l1_fill_queue(uns  proc_id,
                                      int* p_l1fill_queue_removal_count) {
  /* Remove requests from l1 fill queue */
  if(*p_l1fill_queue_removal_count > 0) {
    /* After this sort requests that should be removed will be at the tail of
     * the l1_queue */
    DEBUG(0, "l1fill_queue removal\n");
    qsort(mem->l1fill_queue.base, mem->l1fill_queue.entry_count,
          sizeof(Mem_Queue_Entry), mem_compare_priority);
    mem->l1fill_queue.entry_count -= *p_l1fill_queue_removal_count;
    ASSERT(proc_id, mem->l1fill_queue.entry_count >= 0);
    /* free corresponding reserved entries in the L1 queue if HIER_MSHR_ON */
    if(HIER_MSHR_ON) {
      mem->l1_queue.reserved_entry_count -= *p_l1fill_queue_removal_count;
      ASSERT(0, mem->l1_queue.reserved_entry_count >= 0);
    }
  }

  *p_l1fill_queue_removal_count = 0;
}

/**************************************************************************************/
/* mem_process_l1_fill_reqs: */

static void mem_process_l1_fill_reqs() {
  Mem_Req* req = NULL;
  int      ii;
  int      reqbuf_id;
  int      l1fill_queue_removal_count = 0;

  /* Go thru the l1fill_queue */

  for(ii = 0; ii < mem->l1fill_queue.entry_count; ii++) {
    reqbuf_id = mem->l1fill_queue.base[ii].reqbuf;
    req       = &(mem->req_buffer[reqbuf_id]);

    ASSERT(req->proc_id, req->state != MRS_INV);
    ASSERT(req->proc_id, (req->type != MRT_WB) || req->wb_requested_back);
    ASSERT(req->proc_id, req->type != MRT_WB_NODIRTY);

    /* if the request is not yet ready, then try the next one */
    if(cycle_count < req->rdy_cycle)
      continue;

    if(req->state == MRS_FILL_L1) {
      DEBUG(req->proc_id,
            "Mem request about to fill L1  index:%ld  type:%s  addr:0x%s  "
            "size:%d  state: %s\n",
            (long int)(req - mem->req_buffer), Mem_Req_Type_str(req->type),
            hexstr64s(req->addr), req->size, mem_req_state_names[req->state]);
      if(l1_fill_line(req)) {
        ASSERT(0, req->type != MRT_WB && req->type != MRT_WB_NODIRTY);
        if(CONSTANT_MEMORY_LATENCY)
          perf_pred_mem_req_done(req);
        if(MLC_PRESENT && req->destination != DEST_L1) {
          req->state     = MRS_FILL_MLC;
          req->rdy_cycle = cycle_count + 1;
        } else {
          req->state     = MRS_FILL_DONE;
          req->rdy_cycle = cycle_count + 1;
        }
        if(PERF_PRED_REQS_FINISH_AT_FILL) {
          perf_pred_mem_req_done(req);
        }
        if(req->type == MRT_IFETCH || req->type == MRT_DFETCH ||
           req->type == MRT_DSTORE) {
          perf_pred_off_chip_effect_end(req);
        }
      }
    } else if(req->state == MRS_FILL_MLC) {
      ASSERT(req->proc_id, MLC_PRESENT);
      // insert into mlc queue
      req->queue = &(mem->mlc_fill_queue);
      if(!ORDER_BEYOND_BUS)
        mem_insert_req_into_queue(req, req->queue,
                                  ALL_FIFO_QUEUES ?
                                    mlc_fill_seq_num :
                                    mem->l1fill_queue.base[ii].priority);
      else
        mem_insert_req_into_queue(req, req->queue,
                                  ALL_FIFO_QUEUES ? mlc_fill_seq_num : 0);
      mlc_fill_seq_num++;
      // remove from l1fill queue - how do we handle this now?
      if(HIER_MSHR_ON)
        req->reserved_entry_count -= 1;
      l1fill_queue_removal_count++;
      mem->l1fill_queue.base[ii].priority =
        Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];
    } else {
      ASSERT(req->proc_id, req->state == MRS_FILL_DONE);
      if(!req->done_func) {
        if(HIER_MSHR_ON)
          req->reserved_entry_count -= 1;

        // Free the request buffer
        mem_free_reqbuf(req);

        // remove from l1fill queue
        l1fill_queue_removal_count++;
        mem->l1fill_queue.base[ii].priority =
          Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];

        remove_from_l1_fill_queue(req->proc_id, &l1fill_queue_removal_count);
      } else {
        req->rdy_cycle = freq_cycle_count(
          FREQ_DOMAIN_CORES[req->proc_id]);  // no +1 to match old performance
        // insert into core fill queue
        req->queue = &(mem->core_fill_queues[req->proc_id]);
        if(!ORDER_BEYOND_BUS)
          mem_insert_req_into_queue(req, req->queue,
                                    ALL_FIFO_QUEUES ?
                                      core_fill_seq_num[req->proc_id] :
                                      mem->l1fill_queue.base[ii].priority);
        else
          mem_insert_req_into_queue(
            req, req->queue,
            ALL_FIFO_QUEUES ? core_fill_seq_num[req->proc_id] : 0);
        core_fill_seq_num[req->proc_id]++;
        // remove from l1fill queue
        l1fill_queue_removal_count++;
        mem->l1fill_queue.base[ii].priority =
          Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];
      }
    }
  }

  if(req) {
    remove_from_l1_fill_queue(req->proc_id, &l1fill_queue_removal_count);
  }
}


/**************************************************************************************/
/* mem_process_mlc_fill_reqs: */

static void mem_process_mlc_fill_reqs() {
  Mem_Req* req;
  int      ii;
  int      reqbuf_id;
  int      mlc_fill_queue_removal_count = 0;

  /* Go thru the mlc_fill_queue */

  for(ii = 0; ii < mem->mlc_fill_queue.entry_count; ii++) {
    reqbuf_id = mem->mlc_fill_queue.base[ii].reqbuf;
    req       = &(mem->req_buffer[reqbuf_id]);

    ASSERT(req->proc_id, req->state != MRS_INV);
    ASSERT(req->proc_id, (req->type != MRT_WB) || req->wb_requested_back);
    ASSERT(req->proc_id, req->type != MRT_WB_NODIRTY);
    ASSERT(req->proc_id, req->destination < DEST_L1);

    /* if the request is not yet ready, then try the next one */
    if(cycle_count < req->rdy_cycle)
      continue;

    if(req->state == MRS_FILL_MLC) {
      DEBUG(req->proc_id,
            "Mem request about to fill MLC  index:%ld  type:%s  addr:0x%s  "
            "size:%d  state: %s\n",
            (long int)(req - mem->req_buffer), Mem_Req_Type_str(req->type),
            hexstr64s(req->addr), req->size, mem_req_state_names[req->state]);
      if(mlc_fill_line(req)) {
        req->state     = MRS_FILL_DONE;
        req->rdy_cycle = cycle_count + 1;
      }
    } else {
      ASSERT(req->proc_id, req->state == MRS_FILL_DONE);
      if(!req->done_func || req->done_func(req)) {
        if(HIER_MSHR_ON)
          req->reserved_entry_count -= 1;

        // Free the request buffer
        mem_free_reqbuf(req);

        // remove from mlc_fill queue - how do we handle this now?
        mlc_fill_queue_removal_count++;
        mem->mlc_fill_queue.base[ii].priority =
          Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];
      }
    }
  }

  /* Remove requests from mlc access queue */
  if(mlc_fill_queue_removal_count > 0) {
    /* After this sort requests that should be removed will be at the tail of
     * the mlc_queue */
    DEBUG(0, "mlc_fill_queue removal\n");
    qsort(mem->mlc_fill_queue.base, mem->mlc_fill_queue.entry_count,
          sizeof(Mem_Queue_Entry), mem_compare_priority);
    mem->mlc_fill_queue.entry_count -= mlc_fill_queue_removal_count;
    ASSERT(req->proc_id, mem->mlc_fill_queue.entry_count >= 0);
    /* free corresponding reserved entries in the MLC queue if HIER_MSHR_ON */
    if(HIER_MSHR_ON) {
      mem->mlc_queue.reserved_entry_count -= mlc_fill_queue_removal_count;
      ASSERT(0, mem->mlc_queue.reserved_entry_count >= 0);
    }
  }
}

/**************************************************************************************/
/* mem_process_core_fill_reqs: */

static void mem_process_core_fill_reqs(uns proc_id) {
  Mem_Req* req;
  int      ii;
  int      reqbuf_id;
  int      core_fill_queue_removal_count = 0;

  /* Go thru the core_fill_queue */

  Mem_Queue* core_fill_queue = &mem->core_fill_queues[proc_id];
  for(ii = 0; ii < core_fill_queue->entry_count; ii++) {
    reqbuf_id = core_fill_queue->base[ii].reqbuf;
    req       = &(mem->req_buffer[reqbuf_id]);

    ASSERT(req->proc_id, req->proc_id == proc_id);
    ASSERT(req->proc_id, req->state != MRS_INV);
    ASSERT(req->proc_id, (req->type != MRT_WB) || req->wb_requested_back);
    ASSERT(req->proc_id, req->type != MRT_WB_NODIRTY);
    ASSERT(req->proc_id, cycle_count >= req->rdy_cycle);
    ASSERT(proc_id,
           req->state == MRS_L1_HIT_DONE || req->state == MRS_FILL_DONE);
    ASSERT(proc_id,
           req->done_func);  // requests w/o done_func() should be done by now

    if(req->done_func(req)) {
      // Free the request buffer
      mem_free_reqbuf(req);

      // remove from core fill queue
      core_fill_queue_removal_count++;
      core_fill_queue->base[ii].priority =
        Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];
    }
  }

  /* Remove requests from core fill queue */
  if(core_fill_queue_removal_count > 0) {
    /* After this sort requests that should be removed will be at the tail of
     * the core_fill_queue */
    DEBUG(0, "core_fill_queue removal\n");
    qsort(core_fill_queue->base, core_fill_queue->entry_count,
          sizeof(Mem_Queue_Entry), mem_compare_priority);
    core_fill_queue->entry_count -= core_fill_queue_removal_count;
    ASSERT(req->proc_id, core_fill_queue->entry_count >= 0);
  }
}

/**************************************************************************************/
/* scan_stores: */

Flag scan_stores(Addr addr, uns size) {
  uns ii;

  for(ii = 0; ii < mem->total_mem_req_buffers; ii++) {
    Mem_Req* req = &mem->req_buffer[ii];
    if(req->state != MRS_INV && req->type == MRT_DSTORE &&
       BYTE_CONTAIN(req->addr, req->size, addr, size)) {
      uns load_proc_id = get_proc_id_from_cmp_addr(addr);
      ASSERTM(req->proc_id, req->proc_id == load_proc_id,
              "Load from %d matched a store from %d!\n", load_proc_id,
              req->proc_id);
      return SUCCESS;
    }
  }
  return FAILURE;
}


/**************************************************************************************/
/* mem_search_reqbuf: */

static inline Mem_Req* mem_search_queue(
  Mem_Queue* queue, uns8 proc_id, Addr addr, Mem_Req_Type type, uns size,
  Flag* demand_hit_prefetch, /* set if the matching req is a prefetch and a
                                demand hits it */
  Flag* demand_hit_writeback, Mem_Queue_Entry** queue_entry,
  Flag collect_stats) {
  int      used_reqbuf_id;
  Mem_Req* req          = NULL;
  Mem_Req* matching_req = NULL;
  Flag     match        = FALSE;
  int      ii           = 0;
  Addr     src_addr, dest_addr;

  if(proc_id)
    ASSERTM(proc_id, addr, "type %i\n", type);

  ASSERT(proc_id, size % L1_LINE_SIZE == 0);

  *demand_hit_prefetch = FALSE;

  // CMP ignore "size" from argument

  for(ii = 0; ii < queue->entry_count; ii++) {
    used_reqbuf_id = queue->base[ii].reqbuf;
    req            = &mem->req_buffer[used_reqbuf_id];
    dest_addr      = CACHE_SIZE_ADDR(req->size, req->addr);
    src_addr       = CACHE_SIZE_ADDR(req->size, addr);
    match          = FALSE;

    if((dest_addr == src_addr) &&
       !is_final_state(req->state)) { /* address match */
      ASSERTM(proc_id, proc_id == get_proc_id_from_cmp_addr(addr),
              "Proc ID does not match proc ID in address!\n");
      ASSERTM(proc_id, req->proc_id == get_proc_id_from_cmp_addr(req->addr),
              "Proc ID does not match proc ID in address!\n");
      ASSERTM(proc_id, req->proc_id == proc_id,
              "req_proc_id %u addr %.16llx, proc_id %u, addr %.16llx\n",
              req->proc_id, req->addr, proc_id, addr);
      if(req->type == type) {
        // if (req->size < size) then we can add new req to req already
        // outstanding
        match = TRUE; /* type match */
        if(collect_stats)
          STAT_EVENT(req->proc_id, WB_MATCH_WB_FILTERED);
      } else {
        switch(req->type) {
          case MRT_IFETCH:
            if(type == MRT_IPRF)
              match = TRUE;
            if(collect_stats && ((type == MRT_WB) || (type == MRT_WB_NODIRTY)))
              STAT_EVENT(req->proc_id, WB_MATCH_DEMAND);
            break;
          case MRT_DFETCH:
            if((type == MRT_DSTORE) || (type == MRT_DPRF))
              match = TRUE;
            if(collect_stats && ((type == MRT_WB) || (type == MRT_WB_NODIRTY)))
              STAT_EVENT(req->proc_id, WB_MATCH_DEMAND);
            break;
          case MRT_DSTORE:
            if((type == MRT_DFETCH) || (type == MRT_DPRF))
              match = TRUE;
            if(collect_stats && ((type == MRT_WB) || (type == MRT_WB_NODIRTY)))
              STAT_EVENT(req->proc_id, WB_MATCH_DEMAND);
            break;
          case MRT_IPRF:
            if(type == MRT_IFETCH) {
              match                = TRUE;
              *demand_hit_prefetch = TRUE;
            }
            if(collect_stats && ((type == MRT_WB) || (type == MRT_WB_NODIRTY)))
              STAT_EVENT(req->proc_id, WB_MATCH_PREF);
            break;
          case MRT_DPRF:
            if((type == MRT_DFETCH) || (type == MRT_DSTORE)) {
              match                = TRUE;
              *demand_hit_prefetch = TRUE;
            }
            if(collect_stats && ((type == MRT_WB) || (type == MRT_WB_NODIRTY)))
              STAT_EVENT(req->proc_id, WB_MATCH_PREF);
            break;
          case MRT_WB:
          case MRT_WB_NODIRTY:
            if(ALLOW_TYPE_MATCHES) {
              if((type == MRT_DFETCH) || (type == MRT_DSTORE) ||
                 (type == MRT_IFETCH) || (type == MRT_DPRF)) {
                match                 = TRUE;
                *demand_hit_writeback = TRUE;
              }
            }
            if(collect_stats && ((type == MRT_WB) || (type == MRT_WB_NODIRTY)))
              STAT_EVENT(req->proc_id, WB_MATCH_WB);
            break;
          default:
            break;
        }
      }
      if(match) {
        matching_req = req;
        if(MRS_INV == matching_req->state) {
          DEBUG(matching_req->proc_id,
                "Matching req invalid: id %d index:%ld type:%s addr:0x%s "
                "size:%d \n",
                matching_req->id, (long int)(matching_req - mem->req_buffer),
                Mem_Req_Type_str(matching_req->type),
                hexstr64s(matching_req->addr), matching_req->size);
        }
        ASSERT(matching_req->proc_id, matching_req->state != MRS_INV);
        *queue_entry = &(queue->base[ii]);
        if(collect_stats)
          STAT_EVENT(req->proc_id, MEM_REQ_MATCH_IFETCH + MIN2(req->type, 6));
        break;
      }
    }
  }

  return matching_req;
}

/**************************************************************************************/
/* mem_search_reqbuf: */

static inline Mem_Req* mem_search_reqbuf(
  uns8 proc_id, Addr addr, Mem_Req_Type type, uns size,
  Flag* demand_hit_prefetch, /* set if the matching req is a prefetch and a
                                demand hits it */
  Flag* demand_hit_writeback, uns queues_to_search,
  Mem_Queue_Entry** queue_entry) {
  Mem_Req* req;
  ASSERTM(proc_id, proc_id == get_proc_id_from_cmp_addr(addr),
          "Proc ID (%d) does not match proc ID in address (%d)!\n", proc_id,
          get_proc_id_from_cmp_addr(addr));

  if(queues_to_search & QUEUE_MLC_FILL) {
    req = mem_search_queue(&mem->mlc_fill_queue, proc_id, addr, type, size,
                           demand_hit_prefetch, demand_hit_writeback,
                           queue_entry, TRUE);
    if(req)
      return req;
  }

  if(queues_to_search & QUEUE_L1FILL) {
    req = mem_search_queue(&mem->l1fill_queue, proc_id, addr, type, size,
                           demand_hit_prefetch, demand_hit_writeback,
                           queue_entry, TRUE);
    if(req)
      return req;
  }

  ASSERT(proc_id, !(queues_to_search & QUEUE_MEM));
  // if (queues_to_search &  QUEUE_MEM) {
  // req = mem_search_queue(&mem->mem_queue, proc_id, addr, type, size,
  // demand_hit_prefetch, demand_hit_writeback, queue_entry, TRUE);  if (req)
  //    return req;
  //}

  if(queues_to_search & QUEUE_BUS_OUT) {
    req = mem_search_queue(&mem->bus_out_queue, proc_id, addr, type, size,
                           demand_hit_prefetch, demand_hit_writeback,
                           queue_entry, TRUE);
    if(req)
      return req;
  }

  if(queues_to_search & QUEUE_L1) {
    req = mem_search_queue(&mem->l1_queue, proc_id, addr, type, size,
                           demand_hit_prefetch, demand_hit_writeback,
                           queue_entry, TRUE);
    if(req)
      return req;
  }

  if(queues_to_search & QUEUE_MLC) {
    req = mem_search_queue(&mem->mlc_queue, proc_id, addr, type, size,
                           demand_hit_prefetch, demand_hit_writeback,
                           queue_entry, TRUE);
    if(req)
      return req;
  }

  return NULL;
}


/**************************************************************************************/
/* mem_adjust_matching_request: */
// cmp FIXME for cmp support
Flag mem_adjust_matching_request(Mem_Req* req, Mem_Req_Type type, Addr addr,
                                 uns size, Destination destination, uns delay,
                                 Op* op, Flag done_func(Mem_Req*),
                                 Counter unique_num, /* This counter is used
                                                        when op is NULL */
                                 Flag              demand_hit_prefetch,
                                 Flag              demand_hit_writeback,
                                 Mem_Queue_Entry** queue_entry,
                                 Counter           new_priority) {
  Flag    higher_priority;
  Counter old_priority = (*queue_entry)->priority; /* this is the old priority
                                                      of request in the queue */
  Op**     op_ptr    = NULL;
  Counter* op_unique = NULL;
  Counter  current_priority;

  current_priority = new_priority;


  higher_priority = current_priority < old_priority;

  STAT_EVENT(req->proc_id, MEM_REQ_BUFFER_HIT);
  wp_process_reqbuf_match(req, op);

  if(ALLOW_TYPE_MATCHES && demand_hit_writeback) {
    ASSERT(req->proc_id,
           (req->type == MRT_WB) || (req->type == MRT_WB_NODIRTY));
    if(!req->wb_requested_back) {
      ASSERT(req->proc_id, !req->done_func);
      req->done_func         = done_func;
      req->wb_requested_back = TRUE;
      STAT_EVENT(req->proc_id, DEMAND_MATCH_WB + (req->type == MRT_WB_NODIRTY));
      STAT_EVENT_ALL(DEMAND_MATCH_WBALL_IFETCH + type);
      if(req->type == MRT_WB_NODIRTY) {
        STAT_EVENT(req->proc_id, DEMAND_MATCH_WB_ND_IFETCH + type);
      } else {
        STAT_EVENT(req->proc_id, DEMAND_MATCH_WB_IFETCH + type);
      }
    } else {
      // somebody already requested this writeback
      if(!req->done_func) {
        if(done_func)
          STAT_EVENT(req->proc_id, DEMAND_MATCH_WB_NEW_DONE_FUNC);
        else
          STAT_EVENT(req->proc_id, DEMAND_MATCH_WB_NEW_DONE_FUNC_NULL);
        req->done_func = done_func;
      } else if(req->done_func != done_func) {
        STAT_EVENT(req->proc_id, DEMAND_MATCH_WB_DONE_FUNC_NOT_SAME);
        return FALSE;
      } else
        STAT_EVENT(req->proc_id, DEMAND_MATCH_WB_DONE_FUNC_SAME);
    }
  }

  Flag old_off_path_confirmed = req->off_path_confirmed;
  Flag old_type               = req->type;

  /* Adjust op related fields in the request */
  if(op) {
    ASSERT(req->proc_id, req->proc_id == op->proc_id);

    // writebacks do not have associated ops
    ASSERT(req->proc_id, req->type != MRT_WB && req->type != MRT_WB_NODIRTY);

    req->op_count++;
    op_ptr     = sl_list_add_tail(&req->op_ptrs);
    *op_ptr    = op;
    op_unique  = sl_list_add_tail(&req->op_uniques);
    *op_unique = op->unique_num;

    if(op->table_info->mem_type == MEM_ST && !op->off_path)
      req->dirty_l0 = TRUE;

    if(req->oldest_op_unique_num) {
      req->oldest_op_unique_num = ((op->unique_num <
                                    req->oldest_op_unique_num) ?
                                     op->unique_num :
                                     req->oldest_op_unique_num);
      req->oldest_op_op_num = ((op->unique_num < req->oldest_op_unique_num) ?
                                 op->op_num :
                                 req->oldest_op_op_num);
      req->oldest_op_addr   = ((op->unique_num < req->oldest_op_unique_num) ?
                               op->inst_info->addr :
                               req->oldest_op_addr);
    } else {
      req->oldest_op_unique_num = op->unique_num;
      req->oldest_op_op_num     = op->op_num;
      req->oldest_op_addr       = op->inst_info->addr;
    }
    if(req->off_path && !(op->off_path)) {  // cmp IGNORE
      STAT_EVENT(req->proc_id, MEM_REQ_MATCH_OFF_PATH_HIT_BY_ON_PATH);
    } else
      STAT_EVENT(req->proc_id, MEM_REQ_MATCH_OFF_PATH_HIT_BY_ON_PATH_ETC);

    req->off_path &= op->off_path;
    req->off_path_confirmed = FALSE;  // processor thinks op is on path;
                                      // otherwise it would have flushed it
    op->req = req;

    if(!req->done_func)
      req->done_func = done_func;
    if(req->mlc_miss)
      op->engine_info.mlc_miss = TRUE;
    if(req->l1_miss) {
      op->engine_info.l1_miss = TRUE;
      if(TRACK_L1_MISS_DEPS)
        mark_l1_miss_deps(op);
    }

    op->engine_info.mlc_miss_satisfied = req->mlc_miss_satisfied ?
                                           TRUE :
                                           op->engine_info.mlc_miss_satisfied;
    op->engine_info.l1_miss_satisfied = req->l1_miss_satisfied ?
                                          TRUE :
                                          op->engine_info.l1_miss_satisfied;

    // cmp FIXME prefetchers
    if(demand_hit_prefetch && type != MRT_DPRF && type != MRT_IPRF) {
      if(req->destination == DEST_MLC) {
        STAT_EVENT(req->proc_id, MLC_PREF_LATE);
      } else if(req->destination == DEST_L1) {
        STAT_EVENT(req->proc_id, L1_PREF_LATE);
        Counter l1_cycles = freq_cycle_count(FREQ_DOMAIN_L1);
        Counter diff      = l1_cycles >= req->start_cycle ?
                         l1_cycles - req->start_cycle :
                         0;
        INC_STAT_EVENT(req->proc_id, L1_LATE_PREF_CYCLES, diff);
        STAT_EVENT(req->proc_id,
                   L1_LATE_PREF_CYCLES_DIST_0 + MIN2(diff / 100, 20));
      }

      pref_ul1_pref_hit_late(req->proc_id, req->addr, req->loadPC,
                             req->global_hist, req->prefetcher_id);
      req->demand_match_prefetch = TRUE;
      req->type                  = type;  // type promotion
      req->done_func             = done_func;
      // if (DRAM_SCHED == DRAM_SCHED_FAIR_QUEUING_2LEVEL) {
      //    req->fq_start_time = MAX_CTR;
      //} // Ramulator_note: Ramulator implement the scheduling policy
      // internally
      memview_req_changed_type(req);
    }
  }

  /* Determine priority change and resort */
  if(higher_priority && !ALL_FIFO_QUEUES) {
    if((req->queue->type == QUEUE_MLC) || (req->queue->type == QUEUE_L1) ||
       (req->queue->type == QUEUE_BUS_OUT) ||
       ORDER_BEYOND_BUS) { /* FIXME: are we going to be able to promote mem &
                              l1fill requests? */
      req->priority            = new_priority; /* Change the priority of req */
      (*queue_entry)->priority = new_priority; /* Change the priority in the
                                                  queue entry */
      if(PROMOTE_TO_HIGHER_PRIORITY_MEM_REQ_TYPE &&
         Mem_Req_Priority[type] < Mem_Req_Priority[req->type]) {
        /* Promote to the higher priority type (DRAM model
           only looks at type priority). This may lead to a
           bit of inaccuracy, but quick_release perf diff is
           minimal. */
        req->type = type;
        memview_req_changed_type(req);
      }
      qsort(req->queue->base, req->queue->entry_count, sizeof(Mem_Queue_Entry),
            mem_compare_priority); /* Sort the associated queue */
    }

    switch(req->queue->type) {
      case QUEUE_MLC:
        STAT_EVENT(req->proc_id, PROMOTION_QMLC);
        break;
      case QUEUE_L1:
        STAT_EVENT(req->proc_id, PROMOTION_QL1);
        break;
      case QUEUE_BUS_OUT:
        STAT_EVENT(req->proc_id, PROMOTION_QBUSOUT);
        break;
      case QUEUE_MEM:
        if(!ORDER_BEYOND_BUS)
          STAT_EVENT(req->proc_id, NOPROMOTION_QMEM);
        else
          STAT_EVENT(req->proc_id, PROMOTION_QMEM);
        break;
      case QUEUE_MLC_FILL:
        if(!ORDER_BEYOND_BUS)
          STAT_EVENT(req->proc_id, NOPROMOTION_QMLC_FILL);
        else
          STAT_EVENT(req->proc_id, PROMOTION_QMLC_FILL);
        break;
      case QUEUE_L1FILL:
        if(!ORDER_BEYOND_BUS)
          STAT_EVENT(req->proc_id, NOPROMOTION_QL1FILL);
        else
          STAT_EVENT(req->proc_id, PROMOTION_QL1FILL);
        break;
      default:
        ASSERT(req->proc_id, 0);
        break;
    }
  }

  if(req->first_stalling_cycle == MAX_CTR && mem_req_type_is_stalling(type)) {
    req->first_stalling_cycle = freq_cycle_count(FREQ_DOMAIN_L1);
    cache_part_l1_access(req);
  }

  // CMP FIXME
  if((req->type == MRT_IFETCH || req->type == MRT_IPRF) && !req->done_func)
    req->done_func = done_func;

  if(req->off_path &&  // cmp IGNORE
     req->type == MRT_IFETCH && icache_off_path() == FALSE) {
    req->off_path           = FALSE;
    req->off_path_confirmed = FALSE;
  }

  update_mem_req_occupancy_counter(old_type, -1);
  update_mem_req_occupancy_counter(
    req->type, +1);  // BUG? req->type does not always match type

  // change destination to the one closer to the core
  // in case a demand matches an L2 prefetch, for example
  req->destination = MIN2(req->destination, destination);

  if((old_type == MRT_DPRF || old_type == MRT_IPRF) &&
     (type == MRT_IFETCH || type == MRT_DFETCH || type == MRT_DSTORE) &&
     req->l1_miss && req->state <= MRS_FILL_L1) {
    perf_pred_off_chip_effect_start(req);
  }
  if((old_type != MRT_IFETCH && old_type != MRT_DFETCH) &&
     (type == MRT_IFETCH || type == MRT_DFETCH)) {
    perf_pred_l0_miss_start(req);
  }

  if((req->state >= MRS_MEM_NEW && req->state < MRS_MEM_DONE) ||
     (req->state == MRS_BUS_IN_DONE) ||
     (CONSTANT_MEMORY_LATENCY && req->state == MRS_FILL_L1)) {
    perf_pred_update_mem_req_type(req, old_type, old_off_path_confirmed);
  }

  req->req_count++;
  return TRUE;
}

/**************************************************************************************/
/* mem_can_allocate_req_buffer: */

Flag mem_can_allocate_req_buffer(uns proc_id, Mem_Req_Type type) {
  if(type == MRT_IPRF || type == MRT_DPRF) {
    if(PRIVATE_MSHR_ON &&
       mem->num_req_buffers_per_core[proc_id] + MEM_REQ_BUFFER_PREF_WATERMARK >=
         MEM_REQ_BUFFER_ENTRIES) {
      return FALSE;
    } else if(!PRIVATE_MSHR_ON && mem->req_buffer_free_list.count <=
                                    MEM_REQ_BUFFER_PREF_WATERMARK) {
      return FALSE;
    }
  }

  if(type != MRT_WB && type != MRT_WB_NODIRTY) {
    if(PRIVATE_MSHR_ON &&
       mem->num_req_buffers_per_core[proc_id] + MEM_REQ_BUFFER_WB_VALVE >=
         MEM_REQ_BUFFER_ENTRIES) {
      return FALSE;
    } else if(!PRIVATE_MSHR_ON &&
              mem->req_buffer_free_list.count <= MEM_REQ_BUFFER_WB_VALVE) {
      return FALSE;
    }
  }

  if(PRIVATE_MSHR_ON) {
    ASSERT(proc_id,
           mem->num_req_buffers_per_core[proc_id] <= MEM_REQ_BUFFER_ENTRIES);
    if(mem->num_req_buffers_per_core[proc_id] == MEM_REQ_BUFFER_ENTRIES)
      return FALSE;
  }

  if(mem->req_count == mem->total_mem_req_buffers) {
    ASSERT(0, sl_list_remove_head(&mem->req_buffer_free_list) == 0);
    return FALSE;
  }

  return TRUE;
}

/**************************************************************************************/
/* mem_allocate_req_buffer: */
/* If queue is specified, only allocates if its entry_count < size */

static inline Mem_Req* mem_allocate_req_buffer(uns proc_id, Mem_Req_Type type) {
  if(!mem_can_allocate_req_buffer(proc_id, type))
    return FALSE;

  int* reqbuf_num_ptr = sl_list_remove_head(&mem->req_buffer_free_list);

  ASSERT(0, reqbuf_num_ptr);
  ASSERT(0, mem->req_buffer[*reqbuf_num_ptr].state == MRS_INV);
  mem->num_req_buffers_per_core[proc_id] += 1;
  update_mem_req_occupancy_counter(type, +1);
  return &(mem->req_buffer[*reqbuf_num_ptr]);
}

/**************************************************************************************/
/* mem_kick_out_prefetch_from_queue: */

static Mem_Req* mem_kick_out_prefetch_from_queue(uns mem_bank, Mem_Queue* queue,
                                                 Counter new_priority) {
  ASSERTM(0, !(queue->type & QUEUE_MEM),
          "Ramulator does not use QUEUE_MEM. Kicking out prefetch request from "
          "Ramulator's internal queues is not yet implemented!\n");
  ASSERT(0, !HIER_MSHR_ON);

  int kickout_reqbuf_num;

  // FIXME: May need to sort the queue here

  if(queue->entry_count == 0)
    return NULL;

  qsort(queue->base, queue->entry_count, sizeof(Mem_Queue_Entry),
        mem_compare_priority);

  if(KICKOUT_OLDEST_PREFETCH) {
    int      ii, oldest_index = 0;
    Mem_Req* req_kicked_out = NULL;
    Counter  oldest_req_age = MAX_CTR;

    if(KICKOUT_OLDEST_PREFETCH_WITHIN_BANK) {
      for(ii = 0; ii < queue->entry_count; ii++) {
        Mem_Req* req = &(mem->req_buffer[queue->base[ii].reqbuf]);
        if(req->type != MRT_IPRF && req->type != MRT_DPRF)
          continue;
        if(oldest_req_age > req->start_cycle &&
           mem_bank == req->mem_flat_bank) {
          if(req->state < MRS_MEM_WAIT) {
            oldest_req_age = req->start_cycle;
            req_kicked_out = req;
            oldest_index   = ii;
          }
        }
      }
    }

    if(!req_kicked_out) {
      oldest_req_age = MAX_CTR;
      // Search for the oldest prefetch
      for(ii = 0; ii < queue->entry_count; ii++) {
        Mem_Req* req = &(mem->req_buffer[queue->base[ii].reqbuf]);
        if(req->type != MRT_IPRF && req->type != MRT_DPRF)
          continue;
        if(oldest_req_age > req->start_cycle) {
          if(req->state < MRS_MEM_WAIT) {
            oldest_req_age = req->start_cycle;
            req_kicked_out = req;
            oldest_index   = ii;
          }
        }
      }
    }

    // If the oldest prefetch found
    if(req_kicked_out) {
      ASSERT(0, req_kicked_out->priority > new_priority);
      STAT_EVENT(req_kicked_out->proc_id, ONPATH_KICKED_OUT_PREFETCH);
      queue->base[oldest_index].priority =
        Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];
      DEBUG(0, "%s removal\n", queue->name);
      qsort(queue->base, queue->entry_count, sizeof(Mem_Queue_Entry),
            mem_compare_priority);
      queue->entry_count--;
      pref_req_drop_process(
        req_kicked_out->proc_id,
        mem->req_buffer[queue->base[oldest_index].reqbuf].prefetcher_id);
    }

    return req_kicked_out;
  } else {
    kickout_reqbuf_num = queue->base[queue->entry_count - 1].reqbuf;
    if(mem->req_buffer[kickout_reqbuf_num].type == MRT_DPRF &&
       mem->req_buffer[kickout_reqbuf_num].state < MRS_MEM_WAIT) {
      if(mem->req_buffer[kickout_reqbuf_num].priority <= new_priority) {
        printf("%s %s %s\n", queue->name,
               unsstr64(mem->req_buffer[kickout_reqbuf_num].priority),
               unsstr64(new_priority));
        print_mem_queue(QUEUE_L1 | QUEUE_BUS_OUT | QUEUE_L1FILL | QUEUE_MLC |
                        QUEUE_MLC_FILL);
      }
      ASSERT(0, mem->req_buffer[kickout_reqbuf_num].priority > new_priority);
      STAT_EVENT(mem->req_buffer[kickout_reqbuf_num].proc_id,
                 ONPATH_KICKED_OUT_PREFETCH);
      queue->base[queue->entry_count - 1].priority =
        Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];
      queue->entry_count--;
      pref_req_drop_process(mem->req_buffer[kickout_reqbuf_num].proc_id,
                            mem->req_buffer[kickout_reqbuf_num].prefetcher_id);
      return &(mem->req_buffer[kickout_reqbuf_num]);
    } else
      return NULL;
  }
}

/**************************************************************************************/
/* mem_kick_out_prefetch_from_queues: */

static Mem_Req* mem_kick_out_prefetch_from_queues(uns     mem_bank,
                                                  Counter new_priority,
                                                  uns     queues_to_search) {
  ASSERT(0, !HIER_MSHR_ON);
  Mem_Req* req;

  if(queues_to_search & QUEUE_L1) {
    req = mem_kick_out_prefetch_from_queue(mem_bank, &mem->l1_queue,
                                           new_priority);
    if(req)
      return req;
  }

  if(queues_to_search & QUEUE_BUS_OUT) {
    req = mem_kick_out_prefetch_from_queue(mem_bank, &mem->bus_out_queue,
                                           new_priority);
    if(req)
      return req;
  }

  if(queues_to_search & QUEUE_MEM) {
    ASSERTM(0, FALSE,
            "Kicking prefetch requests from Ramulator's internal queues is not "
            "yet implemented!\n");  // Ramulator_todo
    // req = mem_kick_out_prefetch_from_queue(mem_bank, &mem->mem_queue,
    // new_priority);  if (req)
    //    return req;
  }

  if(queues_to_search & QUEUE_L1FILL) {
    req = mem_kick_out_prefetch_from_queue(mem_bank, &mem->l1fill_queue,
                                           new_priority);
    if(req)
      return req;
  }

  return NULL;
}


/**************************************************************************************/
/* mem_kick_out_prefetch_from_queues: */

static Mem_Req* mem_kick_out_oldest_first_prefetch_from_queues(
  uns mem_bank, Counter new_priority, uns queues_to_search) {
  ASSERT(0, !HIER_MSHR_ON);
  Mem_Req* req;


  if(queues_to_search & QUEUE_L1FILL) {
    req = mem_kick_out_prefetch_from_queue(mem_bank, &mem->l1fill_queue,
                                           new_priority);
    if(req)
      return req;
  }

  if(queues_to_search & QUEUE_MEM) {
    ASSERTM(0, FALSE,
            "Kicking prefetch requests from Ramulator's internal queues is not "
            "yet implemented!\n");  // Ramulator_todo
    // req = mem_kick_out_prefetch_from_queue(mem_bank, &mem->mem_queue,
    // new_priority);  if (req)
    //    return req;
  }

  if(queues_to_search & QUEUE_BUS_OUT) {
    req = mem_kick_out_prefetch_from_queue(mem_bank, &mem->bus_out_queue,
                                           new_priority);
    if(req)
      return req;
  }

  if(queues_to_search & QUEUE_L1) {
    req = mem_kick_out_prefetch_from_queue(mem_bank, &mem->l1_queue,
                                           new_priority);
    if(req)
      return req;
  }

  return NULL;
}

/**************************************************************************************/
/* mem_init_new_req: */

static void mem_init_new_req(
  Mem_Req* new_req, Mem_Req_Type type, Mem_Queue_Type queue_type, uns8 proc_id,
  Addr addr, uns size, uns delay, Op* op, Flag done_func(Mem_Req*),
  Counter unique_num, /* This counter is used when op is NULL */
  Flag kicked_out_another, Counter new_priority) {
  ASSERT(0, queue_type & (QUEUE_L1 | QUEUE_MLC));
  Flag to_mlc = (queue_type == QUEUE_MLC);

  STAT_EVENT(proc_id, MEM_REQ_IFETCH + MIN2(type, 6));
  STAT_EVENT(proc_id, MEM_REQ_BUFFER_MISS);

  if(type == MRT_IFETCH || type == MRT_DFETCH || type == MRT_DSTORE) {
    DEBUG(proc_id, "Req index:%d has become a chip demand\n", new_req->id);
  }

  if(!kicked_out_another) {
    mem->req_count++;
  } else {
    mem_clear_reqbuf(new_req);
  }

  new_req->off_path           = op ? op->off_path : FALSE;
  new_req->off_path_confirmed = FALSE;
  new_req->state              = to_mlc ? MRS_MLC_NEW : MRS_L1_NEW;
  new_req->type               = type;
  new_req->queue              = to_mlc ? &mem->mlc_queue : &mem->l1_queue;
  new_req->proc_id            = proc_id;
  new_req->addr               = addr;
  new_req->phys_addr          = addr_translate(addr);

  if(MEMORY_RANDOM_ADDR)
    new_req->phys_addr = convert_to_cmp_addr(proc_id,
                                             rand() * VA_PAGE_SIZE_BYTES);
  new_req->priority = new_priority;
  new_req->size     = size;
  ASSERT(new_req->proc_id, new_req->size <= VA_PAGE_SIZE_BYTES);
  new_req->reserved_entry_count = 0;
  // TODO: actually populate mem_flat_bank, mem_channel, and mem_bank by
  // grabbing that information from Ramulator
  /*
  new_req->mem_flat_bank        = BANK(
    new_req->phys_addr, RAMULATOR_BANKS * RAMULATOR_CHANNELS,
    VA_PAGE_SIZE_BYTES);  // bank# = channel# + real_bank#_per_channel
  new_req->mem_channel = CHANNEL(new_req->mem_flat_bank, RAMULATOR_BANKS);
  new_req->mem_bank = BANK_IN_CHANNEL(new_req->mem_flat_bank, RAMULATOR_BANKS);
  */
  new_req->mlc_bank = BANK(addr, MLC(proc_id)->num_banks,
                           MLC_INTERLEAVE_FACTOR);
  new_req->l1_bank  = BANK(addr, L1(proc_id)->num_banks, L1_INTERLEAVE_FACTOR);
  new_req->start_cycle          = freq_cycle_count(FREQ_DOMAIN_L1) + delay;
  new_req->rdy_cycle            = freq_cycle_count(FREQ_DOMAIN_L1) + delay;
  new_req->first_stalling_cycle = mem_req_type_is_stalling(type) ?
                                    new_req->start_cycle :
                                    MAX_CTR;
  new_req->op_count             = 0;
  new_req->req_count            = 1;
  new_req->done_func            = done_func;
  new_req->mlc_miss             = FALSE;
  new_req->mlc_miss_satisfied   = FALSE;
  new_req->mlc_miss_cycle       = MAX_CTR;
  new_req->l1_miss              = FALSE;
  new_req->l1_miss_satisfied    = FALSE;
  new_req->l1_miss_cycle        = MAX_CTR;
  new_req->oldest_op_unique_num = (Counter)0;
  new_req->oldest_op_op_num     = (Counter)0;
  new_req->oldest_op_addr       = (Addr)0;
  new_req->unique_num = unique_num;  // this is for icache requests for now
  new_req->onpath_match_offpath  = FALSE;
  new_req->demand_match_prefetch = FALSE;
  new_req->dirty_l0 = op && op->table_info->mem_type == MEM_ST && !op->off_path;
  new_req->wb_requested_back   = FALSE;
  new_req->wb_used_onpath      = FALSE;
  new_req->mem_seq_num         = 0;
  new_req->fq_start_time       = MAX_CTR;
  new_req->fq_bank_finish_time = MAX_CTR;
  new_req->fq_finish_time      = MAX_CTR;
  new_req->dram_access_cycle   = 0;
  new_req->dram_latency        = 0;

  new_req->belong_to_batch = FALSE;
  new_req->rank            = 0;
  new_req->shadow_row_hit  = FALSE;
  new_req->destination     = DEST_NONE;

  if(op) {
    ASSERT(new_req->proc_id, new_req->proc_id == op->proc_id);

    Op**     op_ptr    = sl_list_add_tail(&new_req->op_ptrs);
    Counter* op_unique = sl_list_add_tail(&new_req->op_uniques);
    *op_ptr            = op;
    *op_unique         = op->unique_num;
    new_req->op_count++;

    new_req->oldest_op_unique_num = op->unique_num;
    new_req->oldest_op_op_num     = op->op_num;
    new_req->oldest_op_addr       = op->inst_info->addr;
    op->req                       = new_req;
  }

  if(new_req->type == MRT_IFETCH && icache_off_path())
    new_req->off_path = TRUE;

  STAT_EVENT(proc_id, MEM_REQ_INIT_IFETCH + type);
  STAT_EVENT(proc_id, MEM_REQ_INIT);
  STAT_EVENT(proc_id, MEM_REQ_INIT_ONPATH + new_req->off_path);
  if(new_req->off_path) {
    STAT_EVENT(proc_id, MEM_REQ_INIT_OFFPATH_IFETCH + type);
    STAT_EVENT(proc_id, REQBUF_CREATE_OFFPATH);
  } else {
    STAT_EVENT(proc_id, MEM_REQ_INIT_ONPATH_IFETCH + type);
    STAT_EVENT(proc_id, REQBUF_CREATE_ONPATH);

    if(type != MRT_WB) {
      STAT_EVENT(proc_id, DIST_REQBUF_ONPATH);
      STAT_EVENT(proc_id, DIST2_REQBUF_ONPATH);
    }

    if(type == MRT_IFETCH)
      STAT_EVENT(proc_id, REQBUF_CREATE_ONPATH_IFETCH);
    else if(op)
      STAT_EVENT(proc_id, REQBUF_CREATE_ONPATH_DATA);
  }

  // if this is a valid right path request, we check that the addr bits we're
  // masking out are actually all 0s (or 1s)
  if(!new_req->off_path && mem_req_type_is_demand(new_req->type)) {
    check_and_remove_addr_sign_extended_bits(
      addr, NUM_ADDR_NON_SIGN_EXTEND_BITS, TRUE);
  }

  DEBUG(new_req->proc_id,
        "New mem request is initiated index:%ld type:%s addr:0x%s state:%s\n",
        (long int)(new_req - mem->req_buffer), Mem_Req_Type_str(new_req->type),
        hexstr64s(new_req->addr), mem_req_state_names[new_req->state]);
}


/**************************************************************************************/
/* mem_insert_req_into_queue: */

static inline Mem_Queue_Entry* mem_insert_req_into_queue(Mem_Req*   new_req,
                                                         Mem_Queue* queue,
                                                         Counter    priority) {
  ASSERTM(0, !(queue->type & QUEUE_MEM),
          "Ramulator does not use QUEUE_MEM. A request should be issued using "
          "ramulator_send()!\n");

  if(queue->entry_count >= (queue->size - queue->reserved_entry_count)) {
    print_mem_queue(QUEUE_L1 | QUEUE_BUS_OUT | QUEUE_MEM | QUEUE_L1FILL |
                    QUEUE_MLC | QUEUE_MLC_FILL);
  }
  ASSERTM(new_req->proc_id,
          queue->entry_count < (queue->size - queue->reserved_entry_count),
          "name:%s  count:%d  size:%d  reserved:%d  reqbuf:%d  rc:%d l1:%d "
          "bo:%d lf:%d rf:%d\n",
          queue->name, queue->entry_count, queue->size,
          queue->reserved_entry_count, new_req->id, mem->req_count,
          mem->l1_queue.entry_count, mem->bus_out_queue.entry_count,
          mem->l1fill_queue.entry_count, mem->req_buffer_free_list.count);

  Mem_Queue_Entry* new_entry = &queue->base[queue->entry_count];
  new_entry->reqbuf          = new_req->id;
  new_entry->priority        = priority > 0 ? priority : new_req->priority;
  queue->entry_count++;


  DEBUG(new_req->proc_id,
        "Inserted into %s index:%d pri:%s rc:%d l1:%d bo:%d lf:%d\n",
        queue->name, new_req->id,
        unsstr64(priority > 0 ? priority : new_req->priority), mem->req_count,
        mem->l1_queue.entry_count, mem->bus_out_queue.entry_count,
        mem->l1fill_queue.entry_count);
  return new_entry;
}

/**************************************************************************************/
/* mem_insert_req_round_robin: */
void mem_insert_req_round_robin() {
  ASSERT(0, ROUND_ROBIN_TO_L1);
  uns8      proc_id;
  Mem_Req** req_ptr;

  while(l1_in_buf_count) {
    for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      req_ptr = sl_list_remove_head(&mem->l1_in_buffer_core[proc_id]);
      if(req_ptr) {
        (*req_ptr)->priority = ((*req_ptr)->type == MRT_DPRF ||
                                (*req_ptr)->type == MRT_IPRF) ?
                                 (*req_ptr)->priority :
                                 order_num;
        mem_insert_req_into_queue(
          *req_ptr, (*req_ptr)->queue,
          ((*req_ptr)->type == MRT_DPRF || (*req_ptr)->type == MRT_IPRF) ?
            0 :
            order_num);
        order_num++;
        l1_in_buf_count--;
      }
    }
  }

  ASSERT(0, mem->l1_in_buffer_core[0].count == 0);
}


/**************************************************************************************/
/* new_mem_req: */
/* Returns TRUE if the request is successfully entered into the memory system */

Flag new_mem_req(Mem_Req_Type type, uns8 proc_id, Addr addr, uns size,
                 uns delay, Op* op, Flag done_func(Mem_Req*),
                 Counter unique_num, /* This counter is used when op is NULL */
                 Pref_Req_Info* pref_info) {
  Mem_Req*         new_req              = NULL;
  Mem_Req*         matching_req         = NULL;
  Mem_Queue_Entry* queue_entry          = NULL;
  Flag             demand_hit_prefetch  = FALSE;
  Flag             demand_hit_writeback = FALSE;
  Flag             kicked_out =
    FALSE; /* did this request kick out another one in the queue */
  Counter priority_offset = freq_cycle_count(FREQ_DOMAIN_L1);
  Counter new_priority;
  Flag    to_mlc = MLC_PRESENT && (!pref_info || pref_info->dest != DEST_L1);
  Destination destination = (pref_info ? pref_info->dest : DEST_NONE);

  ASSERTM(proc_id, proc_id == get_proc_id_from_cmp_addr(addr),
          "Proc ID (%d) does not match proc ID in address (%d)!\n", proc_id,
          get_proc_id_from_cmp_addr(addr));

  if((type == MRT_DPRF) || (type == MRT_IPRF)) {
    if(!PRIORITIZE_PREFETCHES_WITH_UNIQUE)
      priority_offset = 0;
  }

  new_priority = Mem_Req_Priority_Offset[type] + priority_offset;

  /* Step 1: Figure out if this access is already in the request buffer */
  matching_req = mem_search_reqbuf(
    proc_id, addr, type, size, &demand_hit_prefetch, &demand_hit_writeback,
    QUEUE_MLC | QUEUE_L1 | QUEUE_BUS_OUT | /*QUEUE_MEM |*/ QUEUE_L1FILL |
      QUEUE_MLC_FILL,
    &queue_entry);

  // if HIER_MSHR_ON, we do not allow matching non-writebacks to writebacks
  // (otherwise the reserved entry counts get messed up)
  if(HIER_MSHR_ON && matching_req &&
     (matching_req->type == MRT_WB || matching_req->type == MRT_WB_NODIRTY)) {
    STAT_EVENT(proc_id, NEWREQ_WB_MATCH_IGNORED);
    matching_req = 0;
  }

  // if HIER_MSHR_ON, an MLC req matching an L2 prefetch has to reserve an entry
  // in the MLC queue simulation inaccuracy: the data may be in MLC, but we wait
  // on the L2 prefetch
  if(matching_req && to_mlc && (matching_req->destination == DEST_L1)) {
    if(HIER_MSHR_ON) {
      ASSERT(0, !ALLOW_TYPE_MATCHES);  // we rely on the adjust function always
                                       // returning true
      ASSERTM(0, ADDR_TRANSLATION == ADDR_TRANS_NONE,
              "MLC && HIER_MSHR_ON && ADDR_TRANSLATION not supported\n");
      if(queue_full(&mem->mlc_queue))
        return FALSE;
      mem->mlc_queue.reserved_entry_count += 1;
      matching_req->reserved_entry_count += 1;
    }
    STAT_EVENT(proc_id, MLC_NEWREQ_MATCHED_L2_PREF);
    Addr line_addr;

    if((MLC_Data*)cache_access(&MLC(proc_id)->cache, addr, &line_addr, FALSE)) {
      STAT_EVENT(proc_id, MLC_NEWREQ_MATCHED_L2_PREF_MLC_HIT);
    }
    matching_req->mlc_miss       = TRUE;
    matching_req->mlc_miss_cycle = cycle_count;
  }

  /* Step 2: Found matching request. Adjust it based on the current request */

  if(matching_req) {
    // Simulation inaccuracy: an L2-destined request can match a request in the
    // MLC queue, not the other way around
    if(!to_mlc && (matching_req->queue == &mem->mlc_queue))
      STAT_EVENT(proc_id, L1_NEWREQ_MATCHED_MLC_REQ);
    // a DCache miss can match an L2 prefetch
    if(type == MRT_DPRF) {
      if(to_mlc)
        STAT_EVENT(proc_id, PREF_NEWREQ_MATCHED);
      else
        STAT_EVENT(proc_id, PREF_NEWREQ_MATCHED);
    }
    ASSERT(matching_req->proc_id, queue_entry);
    DEBUG(matching_req->proc_id,
          "Hit in mem buffer  index:%d  type:%s  addr:0x%s  size:%d  op_num:%d "
          " off_path:%d\n",
          matching_req->id, Mem_Req_Type_str(matching_req->type),
          hexstr64s(matching_req->addr), matching_req->size,
          op ? (int)op->op_num : -1, op ? op->off_path : FALSE);
    if((type == MRT_DFETCH) || (type == MRT_DSTORE) || matching_req) {
      // Train the Data prefetcher as a miss
      //	    pref_ul1_miss(addr, (op ? op->inst_info->addr : 0));
      // Why? If it was a true miss, the original req would have matched.
      // Otherwise the pref_hit_late should have got it.
    }

    // cmp FIXME cmp support
    return (mem_adjust_matching_request(
      matching_req, type, addr, size, destination, delay, op, done_func,
      unique_num, demand_hit_prefetch, demand_hit_writeback, &queue_entry,
      new_priority));
  }

  /* Step 2.5: Check if there is space in the appropriate queue */
  if(to_mlc) {
    if(queue_full(&mem->mlc_queue)) {
      STAT_EVENT(proc_id, REJECTED_QUEUE_MLC);
      return FALSE;
    }
  } else {
    if(queue_full(&mem->l1_queue) ||
       ((type == MRT_IPRF || type == MRT_DPRF) &&
        queue_num_free(&mem->l1_queue) <= MEM_REQ_BUFFER_PREF_WATERMARK)) {
      STAT_EVENT(proc_id, REJECTED_QUEUE_L1);
      return FALSE;
    }
  }

  /* Step 3: Not already in request buffer. Figure out if a free request buffer
   * exists */
  new_req = mem_allocate_req_buffer(proc_id, type);

  /* Step 4: No free request buffer - If demand, try to kick
     something out from the l1 access queue (not bus_out and
     definitely not bus_in, because we may not be able to take stuff
     out of there) */
  if(new_req == NULL) {
    // cmp IGNORE (MLC IGNORE too =)
    ASSERTM(proc_id, !KICKOUT_PREFETCHES,
            "KICKOUT_PREFETCHES currently not supported, because the mem bank "
            "we use is wrong. Instead, we need a way to get "
            "the bank of the request from Ramulator");
    if(KICKOUT_PREFETCHES && (type != MRT_IPRF) && (type != MRT_DPRF)) {
      if(!KICKOUT_LOOK_FOR_OLDEST_FIRST)
        new_req = mem_kick_out_prefetch_from_queues(
          BANK(addr, RAMULATOR_BANKS * RAMULATOR_CHANNELS, VA_PAGE_SIZE_BYTES),
          new_priority,
          QUEUE_L1 | QUEUE_BUS_OUT | QUEUE_MEM); /* FIXME: need to make this
                                                    more realistic and modular
                                                  */
      else
        new_req = mem_kick_out_oldest_first_prefetch_from_queues(
          BANK(addr, RAMULATOR_BANKS * RAMULATOR_CHANNELS, VA_PAGE_SIZE_BYTES),
          new_priority, QUEUE_L1 | QUEUE_BUS_OUT | QUEUE_MEM);
    }


    if(new_req ==
       NULL) { /* Step 2.1.1: Cannot kick out anything - just return */
      DEBUG(proc_id,
            "Request denied in mem buffer  addr:%s rc:%d mlc:%d l1:%d bo:%d "
            "lf:%d mf:%d rf:%d\n",
            hexstr64s(addr), mem->req_count, mem->mlc_queue.entry_count,
            mem->l1_queue.entry_count, mem->bus_out_queue.entry_count,
            mem->l1fill_queue.entry_count, mem->mlc_fill_queue.entry_count,
            mem->req_buffer_free_list.count);
      STAT_EVENT(proc_id, MEM_REQ_BUFFER_FULL);
      if((type == MRT_IFETCH) || (type == MRT_DFETCH) || (type == MRT_DSTORE))
        STAT_EVENT(proc_id, MEM_REQ_BUFFER_FULL_DENIED_DEMAND);
      STAT_EVENT(proc_id, MEM_REQ_BUFFER_FULL_DENIED_IFETCH + type);
      return FALSE;
    } else {
      kicked_out = TRUE;
      DEBUG(new_req->proc_id,
            "Request kicked out in mem buffer index:%d type:%s  addr:0x%s  "
            "newpri:%s\n",
            new_req->id, Mem_Req_Type_str(new_req->type),
            hexstr64s(new_req->addr), unsstr64(new_priority));
    }
  }

  /* we model this more accurately by training the prefetcher when we actually
   * hit/miss if PREF_ORACLE_TRAIN_ON is off */
  // cmp FIXME What can I do for the prefetcher?
  if(!to_mlc) {
    if(PREF_ORACLE_TRAIN_ON &&
       ((type == MRT_DFETCH) || (type == MRT_DSTORE) ||
        (PREF_I_TOGETHER && type == MRT_IFETCH) ||
        (PREF_TRAIN_ON_PREF_MISSES && type == MRT_DPRF))) {
      // Train the Data prefetcher
      L1_Data* data;
      Addr     line_addr;

      ASSERTM(0, ADDR_TRANSLATION == ADDR_TRANS_NONE,
              "PREF_ORACLE_TRAIN_ON && ADDR_TRANSLATION not supported\n");
      data = (L1_Data*)cache_access(&L1(proc_id)->cache, addr, &line_addr,
                                    FALSE);

      if(data) {
        pref_ul1_hit(proc_id, addr, (op ? op->inst_info->addr : 0),
                     (op ? op->oracle_info.pred_global_hist : 0));
      } else {
        // TREAT queue hits as misses
        pref_ul1_miss(proc_id, addr, (op ? op->inst_info->addr : 0),
                      (op ? op->oracle_info.pred_global_hist : 0));
      }
    }
  } else {
    if(PREF_ORACLE_TRAIN_ON &&
       ((type == MRT_DFETCH) || (type == MRT_DSTORE) ||
        (PREF_I_TOGETHER && type == MRT_IFETCH) ||
        (PREF_TRAIN_ON_PREF_MISSES && type == MRT_DPRF))) {
      // Train the Data prefetcher
      MLC_Data* data;
      Addr      line_addr;

      ASSERTM(0, ADDR_TRANSLATION == ADDR_TRANS_NONE,
              "PREF_ORACLE_TRAIN_ON && ADDR_TRANSLATION not supported\n");
      data = (MLC_Data*)cache_access(&MLC(proc_id)->cache, addr, &line_addr,
                                     FALSE);

      if(data) {
        pref_umlc_hit(proc_id, addr, (op ? op->inst_info->addr : 0),
                      (op ? op->oracle_info.pred_global_hist : 0));
      } else {
        // TREAT queue hits as misses
        pref_umlc_miss(proc_id, addr, (op ? op->inst_info->addr : 0),
                       (op ? op->oracle_info.pred_global_hist : 0));
      }
    }
  }

  /* Step 5: Allocate a new request buffer -- new_req */

  mem_init_new_req(new_req, type, to_mlc ? QUEUE_MLC : QUEUE_L1, proc_id, addr,
                   size, delay, op, done_func, unique_num, kicked_out,
                   new_priority);

  /* Step 6: Insert the request into the appropriate queue if it is not already
   * there */

  new_req->loadPC        = op ? op->inst_info->addr : 0;
  new_req->prefetcher_id = (pref_info ? pref_info->prefetcher_id : 0);
  new_req->pref_distance = (pref_info ? pref_info->distance : 0);
  new_req->pref_loadPC   = (pref_info ? pref_info->loadPC : 0);
  new_req->global_hist   = (pref_info ? pref_info->global_hist : 0);
  new_req->bw_prefetch   = (pref_info ? pref_info->bw_limited : FALSE);
  new_req->destination   = destination;
  if(PREF_FRAMEWORK_ON) {
    new_req->bw_prefetchable = PREF_STREAM_ON &&
                               pref_stream_bw_prefetchable(proc_id, addr);
  } else {
    new_req->bw_prefetchable = FALSE;
  }

  perf_pred_l0_miss_start(new_req);

  if(to_mlc)
    return insert_new_req_into_mlc_queue(proc_id, new_req);
  else
    return insert_new_req_into_l1_queue(proc_id, new_req);
}

/**************************************************************************************/
/* insert_new_req_into_l1_queue: */

static Flag insert_new_req_into_l1_queue(uns proc_id, Mem_Req* new_req) {
  if(!ROUND_ROBIN_TO_L1) {
    if(queue_full(&mem->l1_queue)) {
      ASSERT(proc_id, 0);
    }
    mem_insert_req_into_queue(new_req, new_req->queue,
                              ALL_FIFO_QUEUES ? l1_seq_num : 0);
    cycle_l1q_insert_count++;
    l1_seq_num++;
  } else {
    ASSERT(proc_id, 0);
    Mem_Req** req_ptr = sl_list_add_tail(&mem->l1_in_buffer_core[proc_id]);
    *req_ptr          = new_req;
    l1_in_buf_count++;
  }
  return TRUE;
}

/**************************************************************************************/
/* insert_new_req_into_mlc_queue: */

static Flag insert_new_req_into_mlc_queue(uns proc_id, Mem_Req* new_req) {
  if(queue_full(&mem->mlc_queue)) {
    ASSERT(proc_id, 0);
  }
  mem_insert_req_into_queue(new_req, new_req->queue,
                            ALL_FIFO_QUEUES ? mlc_seq_num : 0);
  cycle_mlcq_insert_count++;
  mlc_seq_num++;
  return TRUE;
}

/**************************************************************************************/
/* new_mem_dc_wb_req: */
/* Returns TRUE if the request is successfully entered into the memory system */

Flag new_mem_dc_wb_req(Mem_Req_Type type, uns8 proc_id, Addr addr, uns size,
                       uns delay, Op* op, Flag done_func(Mem_Req*),
                       Counter unique_num, Flag used_onpath) {
  Mem_Req*         new_req              = NULL;
  Mem_Req*         matching_req         = NULL;
  Mem_Queue_Entry* queue_entry          = NULL;
  Flag             demand_hit_prefetch  = FALSE;
  Flag             demand_hit_writeback = FALSE;
  Flag             kicked_out =
    FALSE; /* did this request kick out another one in the queue */
  Counter priority_offset = freq_cycle_count(FREQ_DOMAIN_L1);
  Counter new_priority;

  ASSERT(proc_id, (type == MRT_WB) || (type == MRT_WB_NODIRTY));
  ASSERTM(proc_id, proc_id == get_proc_id_from_cmp_addr(addr),
          "Proc ID (%d) does not match proc ID in address (%d)!\n", proc_id,
          get_proc_id_from_cmp_addr(addr));

  new_priority = Mem_Req_Priority_Offset[type] + priority_offset;

  /* Step 1: Figure out if this access is already in the request buffer */

  matching_req = mem_search_reqbuf(
    proc_id, addr, type, size, &demand_hit_prefetch, &demand_hit_writeback,
    QUEUE_L1 | QUEUE_BUS_OUT | /*QUEUE_MEM |*/ QUEUE_L1FILL,
    &queue_entry);  // CMP: QUEUE_L1FILL: this is a bug? Seems like no.
                    // Doublecheck!!

  /* Step 2: Found matching request. Adjust it based on the current request */

  if(matching_req) {
    ASSERT(matching_req->proc_id, queue_entry);
    DEBUG(matching_req->proc_id,
          "Hit in mem buffer  index:%d  type:%s  addr:0x%s  size:%d  op_num:%d "
          " off_path:%d\n",
          matching_req->id, Mem_Req_Type_str(matching_req->type),
          hexstr64s(matching_req->addr), matching_req->size,
          op ? (int)op->op_num : -1, op ? op->off_path : FALSE);
    return (mem_adjust_matching_request(
      matching_req, type, addr, size, DEST_MLC, delay, op, done_func,
      unique_num, demand_hit_prefetch, demand_hit_writeback, &queue_entry,
      new_priority));
  }

  /* Step 2.5: Check if there is space in the appropriate queue */
  if(MLC_PRESENT) {
    if(queue_full(&mem->mlc_queue)) {
      STAT_EVENT(proc_id, REJECTED_QUEUE_MLC);
      return FALSE;
    }
  } else {
    if(queue_full(&mem->l1_queue)) {
      STAT_EVENT(proc_id, REJECTED_QUEUE_L1);
      return FALSE;
    }
  }

  /* Step 3: Not already in request buffer. Figure out if a free request buffer
   * exists */
  new_req = mem_allocate_req_buffer(proc_id, type);

  /* Step 4: No free request buffer - If demand, try to kick
     something out from the l1 access queue (not bus_out and
     definitely not bus_in, because we may not be able to take stuff
     out of there) */
  if(new_req == NULL) { /* Step 2.1.1: Cannot kick out anything - just return */
    DEBUG(proc_id,
          "Request denied in mem buffer  addr:%s rc:%d mlc:%d l1:%d bo:%d "
          "lf:%d mf:%d rf:%d\n",
          hexstr64s(addr), mem->req_count, mem->mlc_queue.entry_count,
          mem->l1_queue.entry_count, mem->bus_out_queue.entry_count,
          mem->l1fill_queue.entry_count, mem->mlc_fill_queue.entry_count,
          mem->req_buffer_free_list.count);
    STAT_EVENT(proc_id, MEM_REQ_BUFFER_FULL);
    if((type == MRT_IFETCH) || (type == MRT_DFETCH) || (type == MRT_DSTORE))
      STAT_EVENT(proc_id, MEM_REQ_BUFFER_FULL_DENIED_DEMAND);
    STAT_EVENT(proc_id, MEM_REQ_BUFFER_FULL_DENIED_IFETCH + type);
    return FALSE;
  }

  /* Step 5: Allocate a new request buffer -- new_req */
  mem_init_new_req(new_req, type, MLC_PRESENT ? QUEUE_MLC : QUEUE_L1, proc_id,
                   addr, size, delay, op, done_func, unique_num, kicked_out,
                   new_priority);
  new_req->wb_used_onpath = used_onpath;  // DC WB requests carry this flag

  /* Step 6: Insert the request into the l1 queue if it is not already there */
  if(MLC_PRESENT)
    insert_new_req_into_mlc_queue(proc_id, new_req);
  else
    insert_new_req_into_l1_queue(proc_id, new_req);

  return TRUE;
}

/**************************************************************************************/
/* new_mem_mlc_wb_req: */
/* Returns TRUE if the request is successfully entered into the memory system */

static Flag new_mem_mlc_wb_req(Mem_Req_Type type, uns8 proc_id, Addr addr,
                               uns size, uns delay, Op* op,
                               Flag done_func(Mem_Req*), Counter unique_num) {
  Mem_Req*         new_req              = NULL;
  Mem_Req*         matching_req         = NULL;
  Mem_Queue_Entry* queue_entry          = NULL;
  Flag             demand_hit_prefetch  = FALSE;
  Flag             demand_hit_writeback = FALSE;
  Flag             kicked_out =
    FALSE; /* did this request kick out another one in the queue */
  Counter priority_offset = freq_cycle_count(FREQ_DOMAIN_L1);
  Counter new_priority;

  ASSERT(proc_id, (type == MRT_WB) || (type == MRT_WB_NODIRTY));
  ASSERTM(proc_id, proc_id == get_proc_id_from_cmp_addr(addr),
          "Proc ID (%d) does not match proc ID in address (%d)!\n", proc_id,
          get_proc_id_from_cmp_addr(addr));

  new_priority = Mem_Req_Priority_Offset[type] + priority_offset;

  /* Step 1: Figure out if this access is already in the request buffer */

  matching_req = mem_search_reqbuf(
    proc_id, addr, type, size, &demand_hit_prefetch, &demand_hit_writeback,
    QUEUE_L1 | QUEUE_BUS_OUT | /*QUEUE_MEM |*/ QUEUE_L1FILL,
    &queue_entry);  // CMP: QUEUE_L1FILL: this is a bug? Seems like no.
                    // Doublecheck!!

  /* Step 2: Found matching request. Adjust it based on the current request */

  if(matching_req) {
    ASSERT(matching_req->proc_id, queue_entry);
    DEBUG(matching_req->proc_id,
          "Hit in mem buffer  index:%d  type:%s  addr:0x%s  size:%d  op_num:%d "
          " off_path:%d\n",
          matching_req->id, Mem_Req_Type_str(matching_req->type),
          hexstr64s(matching_req->addr), matching_req->size,
          op ? (int)op->op_num : -1, op ? op->off_path : FALSE);
    return (mem_adjust_matching_request(
      matching_req, type, addr, size, DEST_L1, delay, op, done_func, unique_num,
      demand_hit_prefetch, demand_hit_writeback, &queue_entry, new_priority));
  }

  /* Step 2.5: Check if there is space in the L1 queue */
  if(queue_full(&mem->l1_queue)) {
    STAT_EVENT(proc_id, REJECTED_QUEUE_L1);
    return FALSE;
  }

  /* Step 3: Not already in request buffer. Figure out if a free request buffer
   * exists */
  new_req = mem_allocate_req_buffer(proc_id, type);

  /* Step 4: No free request buffer - If demand, try to kick
     something out from the l1 access queue (not bus_out and
     definitely not bus_in, because we may not be able to take stuff
     out of there) */
  if(new_req == NULL) { /* Step 2.1.1: Cannot kick out anything - just return */
    DEBUG(proc_id,
          "Request denied in mem buffer  addr:%s rc:%d mlc:%d l1:%d bo:%d "
          "lf:%d mf:%d rf:%d\n",
          hexstr64s(addr), mem->req_count, mem->mlc_queue.entry_count,
          mem->l1_queue.entry_count, mem->bus_out_queue.entry_count,
          mem->l1fill_queue.entry_count, mem->mlc_fill_queue.entry_count,
          mem->req_buffer_free_list.count);
    STAT_EVENT(proc_id, MEM_REQ_BUFFER_FULL);
    if((type == MRT_IFETCH) || (type == MRT_DFETCH) || (type == MRT_DSTORE))
      STAT_EVENT(proc_id, MEM_REQ_BUFFER_FULL_DENIED_DEMAND);
    STAT_EVENT(proc_id, MEM_REQ_BUFFER_FULL_DENIED_IFETCH + type);
    return FALSE;
  }
  /* Step 5: Allocate a new request buffer -- new_req */
  mem_init_new_req(new_req, type, QUEUE_L1, proc_id, addr, size, delay, op,
                   done_func, unique_num, kicked_out, new_priority);

  /* Step 6: Insert the request into the l1 queue if it is not already there */
  insert_new_req_into_l1_queue(proc_id, new_req);

  // FIXME: Do we sort the queue right away, or should we do it at the beginning
  // of update memory? Perhaps we should keep a new_count and kill_count at each
  // queue and sort the queues with counts > 0 every cycle when we call
  // update_memory?

  return TRUE;
}


static Flag new_mem_l1_wb_req(Mem_Req_Type type, uns8 proc_id, Addr addr,
                              uns size, uns delay, Op* op,
                              Flag    done_func(Mem_Req*),
                              Counter unique_num) /* This counter is used when
                                                     op is NULL */
{
  Mem_Req*         new_req              = NULL;
  Mem_Req*         matching_req         = NULL;
  Mem_Queue_Entry* queue_entry          = NULL;
  Flag             demand_hit_prefetch  = FALSE;
  Flag             demand_hit_writeback = FALSE;
  Flag             kicked_out =
    FALSE; /* did this request kick out another one in the queue */
  Counter priority_offset = freq_cycle_count(FREQ_DOMAIN_L1);
  Counter new_priority;
  Flag    is_sent = FALSE;

  ASSERT(proc_id, type == MRT_WB);
  ASSERT(proc_id, NULL == done_func);
  ASSERTM(proc_id, proc_id == get_proc_id_from_cmp_addr(addr),
          "Proc ID (%d) does not match proc ID in address (%d)!\n", proc_id,
          get_proc_id_from_cmp_addr(addr));
  ASSERTM(proc_id, 0 == delay,
          "does not support non-zero delay, because we will try to send the wb "
          "request to Ramulator right away");

  new_priority = Mem_Req_Priority_Offset[type] + priority_offset;

  if(CONSTANT_MEMORY_LATENCY ||
     STALL_MEM_REQS_ONLY) {  // not modeling any contention
    return TRUE;
  }

  /* Step 1: Figure out if this access is already in the request buffer */
  // after integration with Ramulator, we should no longer be using the bus_out
  // queue
  ASSERT(proc_id, 0 == mem->bus_out_queue.entry_count);
  matching_req = mem_search_reqbuf(
    proc_id, addr, type, size, &demand_hit_prefetch, &demand_hit_writeback,
    /*QUEUE_BUS_OUT | QUEUE_MEM |*/ QUEUE_L1FILL, &queue_entry);

  /* Step 2: Found matching request. Adjust it based on the current request */

  if(matching_req) {
    ASSERT(matching_req->proc_id, queue_entry);
    DEBUG(matching_req->proc_id,
          "Hit in mem buffer  index:%d  type:%s  addr:0x%s  size:%d  op_num:%d "
          " off_path:%d\n",
          matching_req->id, Mem_Req_Type_str(matching_req->type),
          hexstr64s(matching_req->addr), matching_req->size,
          op ? (int)op->op_num : -1, op ? op->off_path : FALSE);
    return (mem_adjust_matching_request(
      matching_req, type, addr, size, DEST_MEM, delay, op, done_func,
      unique_num, demand_hit_prefetch, demand_hit_writeback, &queue_entry,
      new_priority));
  }

  // TODO: obsolete now that we don't have a bus_out queue after Ramulator
  // integration
  /* Step 2.5: Check if there is space in the bus_out queue */
  if(queue_full(&mem->bus_out_queue)) {
    STAT_EVENT(proc_id, REJECTED_QUEUE_BUS_OUT);
    return FALSE;
  }

  /* Step 3: Not already in request buffer. Figure out if a free request buffer
   * exists */

  ASSERT(proc_id, type == MRT_WB);
  new_req = mem_allocate_req_buffer(proc_id, type);

  /* Step 4: No free request buffer - If demand, try to kick
     something out from the l1 access queue (not bus_out and
     definitely not bus_in, because we may not be able to take stuff
     out of there) */
  if(new_req == NULL) {
    // cmp FIXME prefechers // MLC IGNORE
    ASSERTM(proc_id, !KICKOUT_PREFETCHES,
            "KICKOUT_PREFETCHES currently not supported, because the mem bank "
            "we use is wrong. Instead, we need a way to get "
            "the bank of the request from Ramulator");
    if(KICKOUT_PREFETCHES &&
       ((type != MRT_IPRF) && (type != MRT_DPRF))) {  // FIXME: do we kick out
                                                      // stuff for writebacks
                                                      // also?
      // all this bank computation is meaningless now that we use Ramulator
      if(KICKOUT_LOOK_FOR_OLDEST_FIRST)
        new_req = mem_kick_out_prefetch_from_queues(
          BANK(addr, RAMULATOR_BANKS * RAMULATOR_CHANNELS, VA_PAGE_SIZE_BYTES),
          new_priority, QUEUE_L1 | QUEUE_BUS_OUT | QUEUE_MEM);
      else
        new_req = mem_kick_out_oldest_first_prefetch_from_queues(
          BANK(addr, RAMULATOR_BANKS * RAMULATOR_CHANNELS, VA_PAGE_SIZE_BYTES),
          new_priority, QUEUE_L1 | QUEUE_BUS_OUT | QUEUE_MEM);
    }

    if(new_req ==
       NULL) { /* Step 2.1.1: Cannot kick out anything - just return */
      DEBUG(proc_id,
            "Request denied in mem buffer  addr:%s rc:%d mlc:%d l1:%d bo:%d "
            "lf:%d mf:%d rf:%d\n",
            hexstr64s(addr), mem->req_count, mem->mlc_queue.entry_count,
            mem->l1_queue.entry_count, mem->bus_out_queue.entry_count,
            mem->l1fill_queue.entry_count, mem->mlc_fill_queue.entry_count,
            mem->req_buffer_free_list.count);
      STAT_EVENT(proc_id, MEM_REQ_BUFFER_FULL);
      if((type == MRT_IFETCH) || (type == MRT_DFETCH) || (type == MRT_DSTORE))
        STAT_EVENT(proc_id, MEM_REQ_BUFFER_FULL_DENIED_DEMAND);
      STAT_EVENT(proc_id, MEM_REQ_BUFFER_FULL_DENIED_IFETCH + type);
      return FALSE;
    } else {
      kicked_out = TRUE;
      DEBUG(new_req->proc_id,
            "Request kicked out in mem buffer index:%d type:%s  addr:0x%s  "
            "newpri:%s\n",
            new_req->id, Mem_Req_Type_str(new_req->type),
            hexstr64s(new_req->addr), unsstr64(new_priority));
    }
  }

  /* Step 5: Allocate a new request buffer -- new_req */
  mem_init_new_req(new_req, type, QUEUE_L1 /*fake*/, proc_id, addr, size, delay,
                   op, done_func, unique_num, kicked_out, new_priority);
  new_req->queue = NULL;
  new_req->state = MRS_MEM_NEW;

  /* Step 6: Try to insert into the Ramulator queue*/
  if(!ROUND_ROBIN_TO_L1) {
    bus_out_seq_num++;  // RAMULATOR_remove: this is not currently used

    is_sent = ramulator_send(new_req);
    if(!is_sent) {
      mem_free_reqbuf(new_req);  // RAMULATOR_todo: optimize this
      return FALSE;
    } else {
      ASSERT(new_req->proc_id, new_req->mem_queue_cycle >= new_req->rdy_cycle);
      DEBUG(new_req->proc_id, "L1 WB request is sent to ramulator\n");

      mem_seq_num++;
      perf_pred_mem_req_start(new_req);

      mem_free_reqbuf(new_req);
    }
  } else {
    Mem_Req** req_ptr = sl_list_add_tail(&mem->l1_in_buffer_core[proc_id]);
    *req_ptr          = new_req;
    l1_in_buf_count++;
    ASSERTM(
      proc_id, FALSE,
      "Ramulator integration not complete if ROUND_ROBIN_TO_L1 is enabled");
  }

  return TRUE;
}

/**************************************************************************************/
// op_nuke_mem_req:

void op_nuke_mem_req(Op* op) {
  // FIXME: why is this here?
}

/**************************************************************************************/
/* l1_fill_line: */

Flag l1_fill_line(Mem_Req* req) {
  L1_Data* data;
  Addr     line_addr, repl_line_addr = 0;
  Op*      top;
  int      tmp_num = 0;
  UNUSED(tmp_num);

  if(req->op_count) {
    top     = *((Op**)list_start_head_traversal(&req->op_ptrs));
    tmp_num = top->unique_num;
  }


  DEBUG(req->proc_id,
        "Filling L1  index:%d addr:0x%s %7d cindex:%7d op_count:%d "
        "op_num[0]:0x%lld oldest_op_num:0x%d\n",
        req->id, hexstr64s(req->addr), (int)(req->addr),
        (int)(req->addr >> LOG2(DCACHE_LINE_SIZE)), req->op_count,
        (req->op_count ? req->oldest_op_unique_num : 0x0),
        (req->op_count ? tmp_num : 0x0));

  // cmp IGNORE
  if(L1_PREF_CACHE_ENABLE &&
     ((USE_CONFIRMED_OFF ? req->off_path_confirmed : req->off_path) ||
      (req->type == MRT_DPRF))) {  // ONURP: Add prefetches
    ASSERT(0, ADDR_TRANSLATION == ADDR_TRANS_NONE);
    data = (L1_Data*)cache_insert(&mem->pref_l1_cache, req->proc_id, req->addr,
                                  &line_addr, &repl_line_addr);
    STAT_EVENT(req->proc_id, L1_PREF_CACHE_FILL);
    req->l1_miss_satisfied = TRUE;

    ASSERT(req->id, !req->demand_match_prefetch);
    data->proc_id       = req->proc_id;
    data->prefetcher_id = req->prefetcher_id;
    data->pref_loadPC   = req->pref_loadPC;
    data->global_hist   = req->global_hist;

    if(TRACK_L1_MISS_DEPS || MARK_L1_MISSES)
      mark_ops_as_l1_miss_satisfied(req);
    return SUCCESS;
  }

  /* Do not insert the line yet, just check which line we
     need to replace. If that line is dirty, it's possible
     that we won't be able to insert the writeback into the
     memory system. */
  Flag repl_line_valid;
  data = (L1_Data*)get_next_repl_line(&L1(req->proc_id)->cache, req->proc_id,
                                      req->addr, &repl_line_addr,
                                      &repl_line_valid);

  /* If we are replacing anything, check if we need to write it back */
  if(repl_line_valid) {
    if(!L1_WRITE_THROUGH && !L1_IGNORE_WB && data->dirty) {
      /* need to do a write-back */
      DEBUG(data->proc_id, "Scheduling writeback of addr:0x%s\n",
            hexstr64s(repl_line_addr));
      if(0 && DEBUG_EXC_INSERTS)
        printf("Scheduling L2 writeback of addr:0x%s ins addr:0x%s\n",
               hexstr64s(repl_line_addr), hexstr64s(req->addr));
      if(!new_mem_l1_wb_req(MRT_WB, data->proc_id, repl_line_addr, L1_LINE_SIZE,
                            0, NULL, NULL, unique_count))
        return FAILURE;
      STAT_EVENT(req->proc_id, L1_FILL_DIRTY);
    }

    STAT_EVENT(data->proc_id, L1_DATA_EVICT);
    STAT_EVENT(data->proc_id, NORESET_L1_EVICT);

    if(data->dcache_touch)
      STAT_EVENT(data->proc_id, TOUCH_L1_REPLACE);
    else
      STAT_EVENT(data->proc_id, NO_TOUCH_L1_REPLACE);
    // cmp FIXME prefetchers
    pref_ul1evict(data->proc_id, repl_line_addr);
    if(data->prefetch) {
      uns log2_distance = data->pref_distance ?
                            MIN2(LOG2(data->pref_distance), 6) :
                            0;
      if(!data->seen_prefetch) {  // prefeched line not used
        pref_evictline_notused(data->proc_id, repl_line_addr, data->pref_loadPC,
                               data->global_hist);

        STAT_EVENT(data->proc_id, CORE_EVICTED_L1_PREF_NOT_USED);
        STAT_EVENT(data->proc_id, CORE_PREF_L1_NOT_USED);
        INC_STAT_EVENT(data->proc_id, CORE_MEM_LATENCY_AVE_PREF_NOT_USED,
                       data->l1miss_latency);
        STAT_EVENT(data->proc_id,
                   CORE_PREF_L1_NOT_USED_DISTANCE_1 + log2_distance);
        INC_STAT_EVENT(data->proc_id, L1_STAY_PREF_NOT_USED,
                       cycle_count - data->fetch_cycle);
        STAT_EVENT(data->proc_id, NORESET_L1_EVICT_PREF_UNUSED);

        if(data->l1miss_latency > 1600)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_NOT_USED_LATENCY1600MORE);
        else if(data->l1miss_latency > 1400)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_NOT_USED_LATENCY1600);
        else if(data->l1miss_latency > 1200)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_NOT_USED_LATENCY1400);
        else if(data->l1miss_latency > 1000)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_NOT_USED_LATENCY1200);
        else if(data->l1miss_latency > 800)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_NOT_USED_LATENCY1000);
        else if(data->l1miss_latency > 600)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_NOT_USED_LATENCY800);
        else if(data->l1miss_latency > 400)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_NOT_USED_LATENCY600);
        else if(data->l1miss_latency > 200)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_NOT_USED_LATENCY400);
        else
          STAT_EVENT(data->proc_id, CORE_PREF_L1_NOT_USED_LATENCY200);
      } else {  // prefeched line used
        pref_evictline_used(data->proc_id, repl_line_addr, data->pref_loadPC,
                            data->global_hist);

        STAT_EVENT(data->proc_id, CORE_EVICTED_L1_PREF_USED);
        INC_STAT_EVENT(data->proc_id, CORE_MEM_LATENCY_AVE_PREF_USED,
                       data->l1miss_latency);
        STAT_EVENT(data->proc_id, CORE_PREF_L1_USED_DISTANCE_1 + log2_distance);
        INC_STAT_EVENT(data->proc_id, L1_STAY_PREF_USED,
                       cycle_count - data->fetch_cycle);
        STAT_EVENT(data->proc_id, NORESET_L1_EVICT_PREF_USED);

        if(data->l1miss_latency > 1600)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_USED_LATENCY1600MORE);
        else if(data->l1miss_latency > 1400)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_USED_LATENCY1600);
        else if(data->l1miss_latency > 1200)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_USED_LATENCY1400);
        else if(data->l1miss_latency > 1000)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_USED_LATENCY1200);
        else if(data->l1miss_latency > 800)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_USED_LATENCY1000);
        else if(data->l1miss_latency > 600)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_USED_LATENCY800);
        else if(data->l1miss_latency > 400)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_USED_LATENCY600);
        else if(data->l1miss_latency > 200)
          STAT_EVENT(data->proc_id, CORE_PREF_L1_USED_LATENCY400);
        else
          STAT_EVENT(data->proc_id, CORE_PREF_L1_USED_LATENCY200);
      }
    } else {
      STAT_EVENT(data->proc_id, CORE_EVICTED_L1_DEMAND);
      INC_STAT_EVENT(data->proc_id, CORE_MEM_LATENCY_AVE_DEMAND,
                     data->l1miss_latency);
      INC_STAT_EVENT(data->proc_id, L1_STAY_DEMAND,
                     cycle_count - data->fetch_cycle);
      STAT_EVENT(data->proc_id, NORESET_L1_EVICT_NONPREF);

      if(data->l1miss_latency > 1000)
        STAT_EVENT(data->proc_id, CORE_PREF_L1_DEMAND_LATENCY1000MORE);
      else if(data->l1miss_latency > 900)
        STAT_EVENT(data->proc_id, CORE_PREF_L1_DEMAND_LATENCY1000);
      else if(data->l1miss_latency > 800)
        STAT_EVENT(data->proc_id, CORE_PREF_L1_DEMAND_LATENCY900);
      else if(data->l1miss_latency > 700)
        STAT_EVENT(data->proc_id, CORE_PREF_L1_DEMAND_LATENCY800);
      else if(data->l1miss_latency > 600)
        STAT_EVENT(data->proc_id, CORE_PREF_L1_DEMAND_LATENCY700);
      else if(data->l1miss_latency > 500)
        STAT_EVENT(data->proc_id, CORE_PREF_L1_DEMAND_LATENCY600);
      else if(data->l1miss_latency > 400)
        STAT_EVENT(data->proc_id, CORE_PREF_L1_DEMAND_LATENCY500);
      else if(data->l1miss_latency > 300)
        STAT_EVENT(data->proc_id, CORE_PREF_L1_DEMAND_LATENCY400);
      else
        STAT_EVENT(data->proc_id, CORE_PREF_L1_DEMAND_LATENCY300);
    }

    // cmp FIXME prefetchers
    if(PREF_ANALYZE_LOAD && data->prefetch && !data->seen_prefetch &&
       data->pref_loadPC != 0) {
      // Data was prefetched. Add loadPC to hash for debug
      Flag             new_entry;
      Pref_LoadPCInfo* data_ctr;

      data_ctr = hash_table_access_create(mem->pref_loadPC_hash,
                                          data->pref_loadPC, &new_entry);
      if(new_entry) {
        data_ctr->loadPC = data->pref_loadPC;
        data_ctr->count  = 0;
      }
      data_ctr->count = data_ctr->count + 1;
    }
  }

  // Put prefetches in the right position for replacement
  // cmp FIXME prefetchers
  if(req->type == MRT_DPRF || req->type == MRT_IPRF) {
    mem->pref_replpos = INSERT_REPL_DEFAULT;
    if(PREF_INSERT_LRU) {
      mem->pref_replpos = INSERT_REPL_LRU;
      STAT_EVENT(req->proc_id, PREF_REPL_LRU);
    } else if(PREF_INSERT_MIDDLE) {
      mem->pref_replpos = INSERT_REPL_MID;
      STAT_EVENT(req->proc_id, PREF_REPL_MID);
    } else if(PREF_INSERT_LOWQTR) {
      mem->pref_replpos = INSERT_REPL_LOWQTR;
      STAT_EVENT(req->proc_id, PREF_REPL_LOWQTR);
    } else if(PREF_INSERT_DYNACC && req->type == MRT_DPRF) {
      float pol = pref_get_ul1pollution(req->proc_id);
      if(pol > PREF_POL_THRESH_1) {
        mem->pref_replpos = INSERT_REPL_LRU;
        STAT_EVENT(req->proc_id, PREF_REPL_LRU);
      } else if(pol > PREF_POL_THRESH_2) {
        mem->pref_replpos = INSERT_REPL_LOWQTR;
        STAT_EVENT(req->proc_id, PREF_REPL_LOWQTR);
      } else {
        mem->pref_replpos = INSERT_REPL_MID;
        STAT_EVENT(req->proc_id, PREF_REPL_MID);
      }
    }
    data = (L1_Data*)cache_insert_replpos(
      &L1(req->proc_id)->cache, req->proc_id, req->addr, &line_addr,
      &repl_line_addr, mem->pref_replpos, TRUE);
    if(repl_line_addr &&
       (!data->prefetch ||
        (data->prefetch && data->seen_prefetch)))  // Prefetch kicks out demand
      pref_ul1evictOnPF(req->proc_id, repl_line_addr, data->proc_id);
  } else {
    data = (L1_Data*)cache_insert(&L1(req->proc_id)->cache, req->proc_id,
                                  req->addr, &line_addr, &repl_line_addr);
  }

  STAT_EVENT(req->proc_id, NORESET_L1_FILL);
  if(mem_req_type_is_prefetch(req->type) || req->demand_match_prefetch)
    STAT_EVENT(req->proc_id, NORESET_L1_FILL_PREF);
  else
    STAT_EVENT(req->proc_id, NORESET_L1_FILL_NONPREF);
  if(req->type == MRT_WB_NODIRTY || req->type == MRT_WB) {
    STAT_EVENT(req->proc_id, L1_WB_FILL);
    STAT_EVENT(req->proc_id, CORE_L1_WB_FILL);
  } else {
    STAT_EVENT(req->proc_id, L1_FILL);
    STAT_EVENT(req->proc_id, CORE_L1_FILL);
    INC_STAT_EVENT(req->proc_id, TOTAL_L1_MISS_LATENCY,
                   cycle_count - req->l1_miss_cycle);
    INC_STAT_EVENT(req->proc_id, CORE_L1_MISS_LATENCY,
                   cycle_count - req->l1_miss_cycle);


    if(req->type != MRT_DPRF && req->type != MRT_IPRF &&
       !req->demand_match_prefetch) {
      STAT_EVENT(req->proc_id, L1_DEMAND_FILL);
      STAT_EVENT(req->proc_id, CORE_L1_DEMAND_FILL);
      INC_STAT_EVENT_ALL(TOTAL_L1_MISS_LATENCY_DEMAND,
                         cycle_count - req->l1_miss_cycle);
      INC_STAT_EVENT(req->proc_id, CORE_L1_MISS_LATENCY_DEMAND,
                     cycle_count - req->l1_miss_cycle);
    } else {
      STAT_EVENT(req->proc_id, L1_PREF_FILL);
      STAT_EVENT(req->proc_id, CORE_L1_PREF_FILL);
      INC_STAT_EVENT_ALL(TOTAL_L1_MISS_LATENCY_PREF,
                         cycle_count - req->l1_miss_cycle);
      INC_STAT_EVENT(req->proc_id, CORE_L1_MISS_LATENCY_PREF,
                     cycle_count - req->l1_miss_cycle);
      if(req->demand_match_prefetch) {
        STAT_EVENT(req->proc_id, CORE_L1_PREF_FILL_PARTIAL_USED);
        STAT_EVENT(req->proc_id, CORE_PREF_L1_PARTIAL_USED);
        STAT_EVENT_ALL(PREF_L1_TOTAL_PARTIAL_USED);
      }
      // fill umon_cache

      if(PARTITION_UMON_DSS_PREF_ENABLE) {
        Cache *          l1_cache, *umon_cache;
        Umon_Cache_Data* umon_data;
        uns              set;
        Addr             tag, conv_addr, dummy_addr;

        l1_cache = &L1(req->proc_id)->cache;
        set      = req->addr >> l1_cache->shift_bits & l1_cache->set_mask;
        if(set % 33 == 0) {
          set        = set / 33;  // converting the addr
          tag        = req->addr >> (l1_cache->shift_bits + l1_cache->set_bits);
          conv_addr  = tag << 5 | set;
          umon_cache = &mem->umon_cache_core[req->proc_id];

          ASSERT(0, ADDR_TRANSLATION == ADDR_TRANS_NONE);
          umon_data = (Umon_Cache_Data*)cache_access(
            umon_cache, conv_addr, &dummy_addr, TRUE);  // acces umon cache

          if(!umon_data) {  // miss
            Addr repl_addr;
            umon_data = (Umon_Cache_Data*)cache_insert(
              umon_cache, req->proc_id, conv_addr, &dummy_addr, &repl_addr);
            umon_data->addr     = req->addr;
            umon_data->prefetch = TRUE;
          } else {  // hit
            ASSERT(req->proc_id, umon_data->addr == req->addr);
          }
        }
      }
    }
  }

  /* this will make it bring the line into the l1 and then modify it */
  data->proc_id = req->proc_id;
  data->dirty   = ((req->type == MRT_WB) &&
                 (req->state != MRS_FILL_L1));  // write back can fill l1
                                                // directly - reqs filling core
                                                // should not dirty the line
  data->prefetch = req->type == MRT_DPRF || req->type == MRT_IPRF ||
                   req->demand_match_prefetch;
  data->seen_prefetch = req->demand_match_prefetch; /* If demand matches
                                                       prefetch, then it is
                                                       already seen */
  data->prefetcher_id                  = req->prefetcher_id;
  data->pref_distance                  = req->pref_distance;
  data->pref_loadPC                    = req->pref_loadPC;
  data->global_hist                    = req->global_hist;
  data->dcache_touch                   = FALSE;
  data->fetched_by_offpath             = req->off_path;
  data->offpath_op_addr                = req->oldest_op_addr;
  data->offpath_op_unique              = req->oldest_op_unique_num;
  data->l0_modified_fetched_by_offpath = FALSE;
  data->l1miss_latency                 = (req->type == MRT_WB) ?
                           0 :
                           cycle_count - req->l1_miss_cycle;  // WB from dcache
                                                              // does not need a
                                                              // memory access
  data->fetch_cycle      = cycle_count;
  data->onpath_use_cycle = req->off_path ? 0 : cycle_count;

  req->l1_miss_satisfied = TRUE;

  // cmp FIXME
  // when was MRT_DSTORE commented out...?
  if(req->type == MRT_DFETCH || (req->type == MRT_DSTORE)) {
    uns latency = cycle_count - req->l1_miss_cycle;
    ASSERT(req->proc_id, req->l1_miss_cycle != MAX_CTR);
    INC_STAT_EVENT_ALL(TOTAL_DATA_MISS_LATENCY, latency);
    STAT_EVENT_ALL(TOTAL_DATA_MISS_COUNT);
  }
  req->l1_miss_cycle = MAX_CTR;

  // cmp FIXME
  if(TRACK_L1_MISS_DEPS || MARK_L1_MISSES)
    mark_ops_as_l1_miss_satisfied(req);

  // this is just a stat collection
  wp_process_l1_fill(data, req);

  return SUCCESS;
}


/**************************************************************************************/
/* mlc_fill_line: */

Flag mlc_fill_line(Mem_Req* req) {
  MLC_Data* data;
  Addr      line_addr, repl_line_addr = 0;
  Op*       top     = NULL;
  int       tmp_num = 0;
  UNUSED(tmp_num);

  if(req->op_count) {
    top     = *((Op**)list_start_head_traversal(&req->op_ptrs));
    tmp_num = top->unique_num;
  }


  DEBUG(req->proc_id,
        "Filling MLC  index:%d addr:0x%s %7d cindex:%7d op_count:%d "
        "op_num[0]:0x%lld oldest_op_num:0x%d &op:%p &req:%p &opnum:%p\n",
        req->id, hexstr64s(req->addr), (int)(req->addr),
        (int)(req->addr >> LOG2(DCACHE_LINE_SIZE)), req->op_count,
        (req->op_count ? req->oldest_op_unique_num : 0x0),
        (req->op_count ? tmp_num : 0x0), (req->op_count ? top : 0x0), req,
        (req->op_count ? &(top->unique_num) : 0x0));


  /* if it can't get a write port, fail */
  /* if (!get_write_port(&MLC(req->proc_id)->ports[req->mlc_bank])) return
   * FAILURE; */

  // Put prefetches in the right position for replacement
  // cmp FIXME prefetchers
  if(req->type == MRT_DPRF || req->type == MRT_IPRF) {
    mem->pref_replpos = INSERT_REPL_DEFAULT;
    if(PREF_INSERT_LRU) {
      mem->pref_replpos = INSERT_REPL_LRU;
      STAT_EVENT(req->proc_id, PREF_REPL_LRU);
    } else if(PREF_INSERT_MIDDLE) {
      mem->pref_replpos = INSERT_REPL_MID;
      STAT_EVENT(req->proc_id, PREF_REPL_MID);
    } else if(PREF_INSERT_LOWQTR) {
      mem->pref_replpos = INSERT_REPL_LOWQTR;
      STAT_EVENT(req->proc_id, PREF_REPL_LOWQTR);
    }
    data = (MLC_Data*)cache_insert_replpos(
      &MLC(req->proc_id)->cache, req->proc_id, req->addr, &line_addr,
      &repl_line_addr, mem->pref_replpos, TRUE);
  } else {
    data = (MLC_Data*)cache_insert(&MLC(req->proc_id)->cache, req->proc_id,
                                   req->addr, &line_addr, &repl_line_addr);
  }

  if(req->type == MRT_WB_NODIRTY || req->type == MRT_WB) {
    STAT_EVENT(req->proc_id, MLC_WB_FILL);
    STAT_EVENT(req->proc_id, CORE_MLC_WB_FILL);
  } else {
    STAT_EVENT(req->proc_id, MLC_FILL);
    STAT_EVENT(req->proc_id, CORE_MLC_FILL);
    INC_STAT_EVENT_ALL(TOTAL_MEM_LATENCY, cycle_count - req->mlc_miss_cycle);
    INC_STAT_EVENT(req->proc_id, CORE_MEM_LATENCY,
                   cycle_count - req->mlc_miss_cycle);

    if(req->type != MRT_DPRF && req->type != MRT_IPRF &&
       !req->demand_match_prefetch) {
      STAT_EVENT(req->proc_id, MLC_DEMAND_FILL);
      STAT_EVENT(req->proc_id, CORE_MLC_DEMAND_FILL);
      INC_STAT_EVENT_ALL(TOTAL_MEM_LATENCY_DEMAND,
                         cycle_count - req->mlc_miss_cycle);
      INC_STAT_EVENT(req->proc_id, CORE_MEM_LATENCY_DEMAND,
                     cycle_count - req->mlc_miss_cycle);
    } else {
      STAT_EVENT(req->proc_id, MLC_PREF_FILL);
      STAT_EVENT(req->proc_id, CORE_MLC_PREF_FILL);
      INC_STAT_EVENT_ALL(TOTAL_MEM_LATENCY_PREF,
                         cycle_count - req->mlc_miss_cycle);
      INC_STAT_EVENT(req->proc_id, CORE_MEM_LATENCY_PREF,
                     cycle_count - req->mlc_miss_cycle);
      if(req->demand_match_prefetch) {
        STAT_EVENT(req->proc_id, CORE_MLC_PREF_FILL_PARTIAL_USED);
        STAT_EVENT(req->proc_id, CORE_PREF_MLC_PARTIAL_USED);
        STAT_EVENT_ALL(PREF_MLC_TOTAL_PARTIAL_USED);
      }
    }
  }

  /* Do not insert the line yet, just check which line we
     need to replace. If that line is dirty, it's possible
     that we won't be able to insert the writeback into the
     memory system. */
  Flag repl_line_valid;
  data = (MLC_Data*)get_next_repl_line(&MLC(req->proc_id)->cache, req->proc_id,
                                       req->addr, &repl_line_addr,
                                       &repl_line_valid);

  /* If we are replacing anything, check if we need to write it back */
  if(repl_line_valid) {
    if(!MLC_WRITE_THROUGH && data->dirty) {
      /* need to do a write-back */
      DEBUG(req->proc_id, "Scheduling writeback of addr:0x%s\n",
            hexstr64s(repl_line_addr));
      if(0 && DEBUG_EXC_INSERTS)
        printf("Scheduling L2 writeback of addr:0x%s ins addr:0x%s\n",
               hexstr64s(repl_line_addr), hexstr64s(req->addr));
      if(!new_mem_mlc_wb_req(MRT_WB, data->proc_id, repl_line_addr,
                             MLC_LINE_SIZE, 1, NULL, NULL, unique_count))
        return FAILURE;
      STAT_EVENT(req->proc_id, MLC_FILL_DIRTY);
    }

    if(data->prefetch) {
      if(!data->seen_prefetch) {  // prefeched line not used
        pref_evictline_notused(data->proc_id, repl_line_addr, data->pref_loadPC,
                               data->global_hist);

        STAT_EVENT(data->proc_id, CORE_EVICTED_MLC_PREF_NOT_USED);
        INC_STAT_EVENT(data->proc_id, CORE_MEM_LATENCY_AVE_PREF_NOT_USED,
                       data->mlc_miss_latency);

        if(data->mlc_miss_latency > 1600)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_NOT_USED_LATENCY1600MORE);
        else if(data->mlc_miss_latency > 1400)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_NOT_USED_LATENCY1600);
        else if(data->mlc_miss_latency > 1200)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_NOT_USED_LATENCY1400);
        else if(data->mlc_miss_latency > 1000)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_NOT_USED_LATENCY1200);
        else if(data->mlc_miss_latency > 800)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_NOT_USED_LATENCY1000);
        else if(data->mlc_miss_latency > 600)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_NOT_USED_LATENCY800);
        else if(data->mlc_miss_latency > 400)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_NOT_USED_LATENCY600);
        else if(data->mlc_miss_latency > 200)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_NOT_USED_LATENCY400);
        else
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_NOT_USED_LATENCY200);
      } else {  // prefeched line used
        pref_evictline_used(data->proc_id, repl_line_addr, data->pref_loadPC,
                            data->global_hist);

        STAT_EVENT(data->proc_id, CORE_EVICTED_MLC_PREF_USED);
        INC_STAT_EVENT(data->proc_id, CORE_MEM_LATENCY_AVE_PREF_USED,
                       data->mlc_miss_latency);

        if(data->mlc_miss_latency > 1600)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_USED_LATENCY1600MORE);
        else if(data->mlc_miss_latency > 1400)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_USED_LATENCY1600);
        else if(data->mlc_miss_latency > 1200)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_USED_LATENCY1400);
        else if(data->mlc_miss_latency > 1000)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_USED_LATENCY1200);
        else if(data->mlc_miss_latency > 800)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_USED_LATENCY1000);
        else if(data->mlc_miss_latency > 600)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_USED_LATENCY800);
        else if(data->mlc_miss_latency > 400)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_USED_LATENCY600);
        else if(data->mlc_miss_latency > 200)
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_USED_LATENCY400);
        else
          STAT_EVENT(data->proc_id, CORE_PREF_MLC_USED_LATENCY200);
      }
    } else {
      STAT_EVENT(data->proc_id, CORE_EVICTED_MLC_DEMAND);
      INC_STAT_EVENT(data->proc_id, CORE_MEM_LATENCY_AVE_DEMAND,
                     data->mlc_miss_latency);

      if(data->mlc_miss_latency > 1000)
        STAT_EVENT(data->proc_id, CORE_PREF_MLC_DEMAND_LATENCY1000MORE);
      else if(data->mlc_miss_latency > 900)
        STAT_EVENT(data->proc_id, CORE_PREF_MLC_DEMAND_LATENCY1000);
      else if(data->mlc_miss_latency > 800)
        STAT_EVENT(data->proc_id, CORE_PREF_MLC_DEMAND_LATENCY900);
      else if(data->mlc_miss_latency > 700)
        STAT_EVENT(data->proc_id, CORE_PREF_MLC_DEMAND_LATENCY800);
      else if(data->mlc_miss_latency > 600)
        STAT_EVENT(data->proc_id, CORE_PREF_MLC_DEMAND_LATENCY700);
      else if(data->mlc_miss_latency > 500)
        STAT_EVENT(data->proc_id, CORE_PREF_MLC_DEMAND_LATENCY600);
      else if(data->mlc_miss_latency > 400)
        STAT_EVENT(data->proc_id, CORE_PREF_MLC_DEMAND_LATENCY500);
      else if(data->mlc_miss_latency > 300)
        STAT_EVENT(data->proc_id, CORE_PREF_MLC_DEMAND_LATENCY400);
      else
        STAT_EVENT(data->proc_id, CORE_PREF_MLC_DEMAND_LATENCY300);
    }
  }

  /* this will make it bring the line into the mlc and then modify it */
  data->proc_id = req->proc_id;
  data->dirty   = ((req->type == MRT_WB) &&
                 (req->state != MRS_FILL_MLC));  // write back can fill mlc
                                                 // directly - reqs filling core
                                                 // should not dirty the line
  data->prefetch = req->type == MRT_DPRF || req->type == MRT_IPRF ||
                   req->demand_match_prefetch;
  data->seen_prefetch = req->demand_match_prefetch; /* If demand matches
                                                       prefetch, then it is
                                                       already seen */
  data->prefetcher_id                  = req->prefetcher_id;
  data->pref_loadPC                    = req->pref_loadPC;
  data->global_hist                    = req->global_hist;
  data->dcache_touch                   = FALSE;
  data->fetched_by_offpath             = req->off_path;
  data->offpath_op_addr                = req->oldest_op_addr;
  data->offpath_op_unique              = req->oldest_op_unique_num;
  data->l0_modified_fetched_by_offpath = FALSE;
  data->mlc_miss_latency               = (req->type == MRT_WB) ?
                             0 :
                             cycle_count -
                               req->mlc_miss_cycle;  // WB from dcache does not
                                                     // need a memory access
  data->fetch_cycle      = cycle_count;
  data->onpath_use_cycle = req->off_path ? 0 : cycle_count;

  req->mlc_miss_satisfied = TRUE;

  if(req->type == MRT_DFETCH) {
    uns latency = cycle_count - req->mlc_miss_cycle;
    ASSERT(req->proc_id, req->mlc_miss_cycle != MAX_CTR);
    INC_STAT_EVENT_ALL(TOTAL_DATA_MISS_LATENCY, latency);
    STAT_EVENT_ALL(TOTAL_DATA_MISS_COUNT);
  }

  ASSERT(req->proc_id, req->mlc_miss_cycle != MAX_CTR);
  ASSERT(req->proc_id, req->mlc_miss);

  req->mlc_miss_cycle = MAX_CTR;

  return SUCCESS;
}


/**************************************************************************************/
/* mem_req_younger_than_uniquenum: */

Flag mem_req_younger_than_uniquenum(int reqbuf, Counter unique_num) {
  if(mem->req_buffer[reqbuf].oldest_op_unique_num == 0)
    return mem->req_buffer[reqbuf].off_path;
  else {
    if(mem->req_buffer[reqbuf].oldest_op_unique_num > unique_num)
      return TRUE;
    else
      return FALSE;
  }
}

/**************************************************************************************/
/* mem_req_older_than_uniquenum: */

Flag mem_req_older_than_uniquenum(int reqbuf, Counter unique_num) {
  if(mem->req_buffer[reqbuf].oldest_op_unique_num == 0)
    return FALSE;
  else {
    if(mem->req_buffer[reqbuf].oldest_op_unique_num < unique_num)
      return TRUE;
    else
      return FALSE;
  }
}

/**************************************************************************************/
/* do_l1_access: */

L1_Data* do_l1_access(Op* op) {
  L1_Data* hit;
  Addr     line_addr;

  hit = (L1_Data*)cache_access(&L1(op->proc_id)->cache, op->oracle_info.va,
                               &line_addr, FALSE);

  return hit;
}

/**************************************************************************************/
/* do_mlc_access: */

MLC_Data* do_mlc_access(Op* op) {
  MLC_Data* hit;
  Addr      line_addr;

  hit = (MLC_Data*)cache_access(&MLC(op->proc_id)->cache, op->oracle_info.va,
                                &line_addr, FALSE);

  return hit;
}

/**************************************************************************************/
/* do_l1_access_addr: */

L1_Data* do_l1_access_addr(Addr addr) {
  L1_Data* hit;
  Addr     line_addr;
  uns      proc_id = get_proc_id_from_cmp_addr(addr);

  hit = (L1_Data*)cache_access(&L1(proc_id)->cache, addr, &line_addr, FALSE);

  return hit;
}

/**************************************************************************************/
/* do_mlc_access_addr: */

MLC_Data* do_mlc_access_addr(Addr addr) {
  MLC_Data* hit;
  Addr      line_addr;
  uns       proc_id = get_proc_id_from_cmp_addr(addr);

  hit = (MLC_Data*)cache_access(&MLC(proc_id)->cache, addr, &line_addr, FALSE);

  return hit;
}

/**************************************************************************************/
/* mark_ops_as_l1_miss: */

static void mark_ops_as_l1_miss(Mem_Req* req) {
  Op*      op;
  Op**     op_p      = (Op**)list_start_head_traversal(&req->op_ptrs);
  Counter* op_unique = (Counter*)list_start_head_traversal(&req->op_uniques);

  for(; op_p; op_p = (Op**)list_next_element(&req->op_ptrs)) {
    ASSERT(req->proc_id, op_unique);
    op = *op_p;

    if(op->unique_num == *op_unique && op->op_pool_valid) {
      ASSERT(req->proc_id, req->proc_id == op->proc_id);
      if(op->req == req) {
        op->engine_info.l1_miss = TRUE;
        if(TRACK_L1_MISS_DEPS)
          mark_l1_miss_deps(op);
      }
    }
    op_unique = (Counter*)list_next_element(&req->op_uniques);
  }

  // collect stats on l1 misses during RA
}

/**************************************************************************************/
/* mark_ops_as_l1_miss_satisfied: */

void mark_ops_as_l1_miss_satisfied(Mem_Req* req) {
  Op*      op;
  Op**     op_p      = (Op**)list_start_head_traversal(&req->op_ptrs);
  Counter* op_unique = (Counter*)list_start_head_traversal(&req->op_uniques);

  for(; op_p; op_p = (Op**)list_next_element(&req->op_ptrs)) {
    ASSERT(req->proc_id, op_unique);

    op = *op_p;

    if(op->unique_num == *op_unique && op->op_pool_valid) {
      ASSERTM(req->proc_id, req->proc_id == op->proc_id,
              "req addr: %llx, valid_op: %u, op_proc_id: %u op_num: %llu, "
              "offpath: %u op_type: %u, mem_type: %u\n",
              req->addr, op->op_pool_valid, op->proc_id, op->op_num,
              op->off_path, op->table_info->op_type, op->table_info->mem_type);

      if(op->req == req) {
        op->engine_info.l1_miss_satisfied = TRUE;
        if(TRACK_L1_MISS_DEPS) {
          unmark_l1_miss_deps(op);
        }
      }
    }

    op_unique = (Counter*)list_next_element(&req->op_uniques);
  }
}

/**************************************************************************************/
/* mark_l1_miss_deps: */
/* recursively go through the wake up lists of the op and mark ops as
 * l1_miss_dep */
static void mark_l1_miss_deps(Op* op) {
  Wake_Up_Entry* temp;

  ASSERT(op->proc_id,
         (op->engine_info.l1_miss && !op->engine_info.l1_miss_satisfied) ||
           op->engine_info.dep_on_l1_miss);

  for(temp = op->wake_up_head; temp; temp = temp->next) {
    Op*     dep_op         = temp->op;
    Counter dep_unique_num = temp->unique_num;


    if(dep_op->unique_num == dep_unique_num && dep_op->op_pool_valid) {
      ASSERT(op->proc_id, op->proc_id == dep_op->proc_id);
      /*printf("MARK c: %s dep_op: %s %s %s %s op: %s %s %s %s\n",
         unsstr64(cycle_count), unsstr64(dep_op->unique_num),
         unsstr64(dep_op->exec_cycle), disasm_op(dep_op, TRUE),
         unsstr64(dep_op->oracle_info.va), unsstr64(op->unique_num),
         unsstr64(op->exec_cycle), disasm_op(op, TRUE),
         unsstr64(op->oracle_info.va)); */
      ASSERT(dep_op->proc_id, !dep_op->engine_info.l1_miss ||
                                dep_op->table_info->mem_type == MEM_ST);
      if(!dep_op->engine_info.dep_on_l1_miss) {
        dep_op->engine_info.dep_on_l1_miss = TRUE;
        mark_l1_miss_deps(dep_op);
      }
    }
  }
}

/**************************************************************************************/
/* unmark_l1_miss_deps: */
/* recursively go through the wake up lists of the op and unmark ops as
 * l1_miss_dep */

static void unmark_l1_miss_deps(Op* op) {
  Wake_Up_Entry* temp;

  ASSERT(op->proc_id, op->engine_info.l1_miss_satisfied ||
                        (!op->engine_info.dep_on_l1_miss &&
                         op->engine_info.was_dep_on_l1_miss));

  /* Go thru the wake up list and unmark ops if they are not dependent on
   * another l1 miss */
  for(temp = op->wake_up_head; temp; temp = temp->next) {
    Op*     dep_op         = temp->op;
    Counter dep_unique_num = temp->unique_num;

    if(dep_op->unique_num == dep_unique_num && dep_op->op_pool_valid) {
      int      ii;
      Op_Info* op_info              = &dep_op->oracle_info;
      Flag     still_dep_on_l1_miss = FALSE;

      ASSERT(op->proc_id, op->proc_id == dep_op->proc_id);
      ASSERT(dep_op->proc_id, dep_op->engine_info.dep_on_l1_miss ||
                                dep_op->engine_info.was_dep_on_l1_miss);

      if(dep_op->engine_info.dep_on_l1_miss) {
        /* Determine if the op is dependent on another l1_miss */
        for(ii = 0; ii < op_info->num_srcs; ii++) {
          Src_Info* src_info = &op_info->src_info[ii];
          Op*       src_op   = src_info->op;

          if(src_op->unique_num == src_info->unique_num &&
             src_op->op_pool_valid) {
            if(src_op->unique_num != op->unique_num)
              if((src_op->engine_info.l1_miss &&
                  !src_op->engine_info.l1_miss_satisfied) ||
                 src_op->engine_info.dep_on_l1_miss)
                still_dep_on_l1_miss = TRUE;
          }
          if(still_dep_on_l1_miss)
            break;
        }

        /* If the op is not dependent on another l1 miss, then go ahead and
           unmark it and figure out if we need to unmark its dependents */
        if(!still_dep_on_l1_miss) {
          dep_op->engine_info.dep_on_l1_miss     = FALSE;
          dep_op->engine_info.was_dep_on_l1_miss = TRUE;
          unmark_l1_miss_deps(dep_op);
        }
      }
    }
  }
}

L1_Data* l1_pref_cache_access(Mem_Req* req) {
  Addr     line_addr, repl_line_addr, pref_line_addr;
  L1_Data* data      = NULL;
  L1_Data* pref_data = (L1_Data*)cache_access(&mem->pref_l1_cache, req->addr,
                                              &pref_line_addr, FALSE);

  if(req->off_path && !PREFCACHE_MOVE_OFFPATH)
    return pref_data;  // offpath request doesn't change pref cache and l1 cache

  // if prefetch do not insert here
  if(req->type == MRT_DPRF)
    return pref_data;

  if(pref_data) {
    data = cache_insert(&L1(req->proc_id)->cache, req->proc_id, req->addr,
                        &line_addr, &repl_line_addr);
    STAT_EVENT(req->proc_id, L1_DATA_EVICT);
    STAT_EVENT(req->proc_id, L1_PREF_MOVE_L1);
    if(data) {
      if(data->dcache_touch)
        STAT_EVENT(req->proc_id, TOUCH_L1_REPLACE);
      else
        STAT_EVENT(req->proc_id, NO_TOUCH_L1_REPLACE);
    }

    if(data->dirty) {
      /* need to do a write-back */
      DEBUG(req->proc_id, "Scheduling writeback of addr:0x%s\n",
            hexstr64s(repl_line_addr));
      FATAL_ERROR(0, "This writeback code is wrong. Writebacks may be lost.");
      // new_mem_wb_req(MRT_WB, WB_L1, data->proc_id, repl_line_addr);
    }

    ASSERT(req->proc_id, req->proc_id == pref_data->proc_id);
    pref_ul1_pref_hit(req->proc_id, req->addr, req->loadPC, req->global_hist,
                      -1, pref_data->prefetcher_id);

    data->proc_id       = req->proc_id;
    data->dirty         = FALSE;
    data->prefetch      = TRUE;  // THIS IS A PREFETCH
    data->seen_prefetch = TRUE;  // Consider this as a prefetch hit by demand
    data->prefetcher_id = pref_data->prefetcher_id;
    data->pref_loadPC   = pref_data->pref_loadPC;
    data->global_hist   = pref_data->global_hist;
    data->dcache_touch  = FALSE;
    data->fetched_by_offpath = req->off_path;
    data->offpath_op_addr    = req->oldest_op_addr;
    data->offpath_op_unique  = req->oldest_op_unique_num;


    req->l1_miss_satisfied = TRUE;

    if(TRACK_L1_MISS_DEPS)
      mark_ops_as_l1_miss_satisfied(req);

    wp_process_l1_fill(data, req);
    STAT_EVENT(req->proc_id, L1_PREF_CACHE_HIT_PER + req->off_path);
    STAT_EVENT(req->proc_id, L1_PREF_CACHE_HIT + req->off_path);

    ASSERT(0, ADDR_TRANSLATION == ADDR_TRANS_NONE);
    cache_invalidate(&mem->pref_l1_cache, req->addr, &pref_line_addr);
  }
  return data;
}

/**************************************************************************************/
/* mem_get_req_count: */

int mem_get_req_count(uns proc_id) {
  return mem->num_req_buffers_per_core[proc_id];
}

/**************************************************************************************/
/* stats_per_core_collect */
void stats_per_core_collect(uns8 proc_id) {
  Counter pref_fill             = GET_STAT_EVENT(proc_id, CORE_L1_PREF_FILL);
  Counter pref_fill_patial_used = GET_STAT_EVENT(
    proc_id, CORE_L1_PREF_FILL_PARTIAL_USED);
  Counter pref_fill_used = GET_STAT_EVENT(proc_id, CORE_L1_PREF_FILL_USED);
  INC_STAT_EVENT(proc_id, CORE_L1_PREF_FILL_NOT_USED,
                 pref_fill - (pref_fill_patial_used + pref_fill_used));
  INC_STAT_EVENT(proc_id, CORE_PREF_L1_NOT_USED,
                 pref_fill - (pref_fill_patial_used + pref_fill_used));

  pref_fill             = GET_STAT_EVENT(proc_id, L1_PREF_FILL);
  pref_fill_patial_used = GET_STAT_EVENT(proc_id, PREF_L1_TOTAL_PARTIAL_USED);
  pref_fill_used        = GET_STAT_EVENT(proc_id, PREF_L1_TOTAL_USED);
  INC_STAT_EVENT(proc_id, PREF_L1_TOTAL_NOT_USED,
                 pref_fill - (pref_fill_patial_used + pref_fill_used));
}


/**************************************************************************************/
/* mem_done */
void finalize_memory() {
  perf_pred_done();
}

/***************************************************************************************/
/**************************************************************************************/
/* l1_cache_collect_stats  */

void l1_cache_collect_stats() {
  uns8 proc_id;
  uns  ii, jj;
  uns  lines_per_core[64];

  if(PRIVATE_L1) {
    WARNING(0, "Some L1 stats not collected with PRIVATE_L1 on\n");
    return;
  }
  Cache* l1_cache = &L1(0)->cache;

  ASSERT(0, NUM_CORES <= 64);

  for(proc_id = 0; proc_id < NUM_CORES; proc_id++)
    lines_per_core[proc_id] = 0;

  for(ii = 0; ii < l1_cache->num_sets; ii++) {
    for(jj = 0; jj < l1_cache->assoc; jj++) {
      if(l1_cache->entries[ii][jj].valid) {
        L1_Data* l1_line = l1_cache->entries[ii][jj].data;
        lines_per_core[l1_line->proc_id]++;
      }
    }
  }

  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    INC_STAT_EVENT(proc_id, CORE_TOTAL_SETS_ALL_INTERVALS, l1_cache->num_sets);
    INC_STAT_EVENT(proc_id, CORE_L1_AVG_NUM_WAYS, lines_per_core[proc_id]);
    mem->l1_ave_num_ways_per_core[proc_id] = (double)lines_per_core[proc_id] /
                                             l1_cache->num_sets;
  }
}

Flag is_final_state(Mem_Req_State state) {
  return (state == MRS_MLC_HIT_DONE) || (state == MRS_L1_HIT_DONE) ||
         (state == MRS_MEM_DONE) || (state == MRS_FILL_DONE);
}

/**************************************************************************************/
/* wp_process_l1_hit: */

void wp_process_l1_hit(L1_Data* line, Mem_Req* req) {
  if(!line) {
    ASSERT(req->proc_id, PERFECT_L1);
    return;
  }

  if(!WP_COLLECT_STATS)
    return;

  if(!req->off_path) {
    if(line->fetched_by_offpath) {
      STAT_EVENT(req->proc_id, L1_HIT_ONPATH_SAT_BY_OFFPATH);
      STAT_EVENT(req->proc_id, L1_USE_OFFPATH);
      STAT_EVENT(req->proc_id, JUST_L1_USE_OFFPATH);
      STAT_EVENT(req->proc_id, DIST_L1_FILL_OFFPATH_USED);
      STAT_EVENT(req->proc_id, DIST_REQBUF_OFFPATH_USED);
      STAT_EVENT(req->proc_id, DIST2_REQBUF_OFFPATH_USED_FULL);

      DEBUG(0,
            "L1 hit: On path hits off path. va:%s op:0x%s wp_op:0x%s opu:%s "
            "wpu:%s dist:%s%s\n",
            hexstr64s(req->addr), hexstr64s(req->oldest_op_addr),
            hexstr64s(line->offpath_op_addr),
            unsstr64(req->oldest_op_unique_num),
            unsstr64(line->offpath_op_unique),
            req->oldest_op_unique_num > line->offpath_op_unique ? " " : "-",
            req->oldest_op_unique_num > line->offpath_op_unique ?
              unsstr64(req->oldest_op_unique_num - line->offpath_op_unique) :
              unsstr64(line->offpath_op_unique - req->oldest_op_unique_num));
      switch(req->type) {
        case MRT_IFETCH:
          STAT_EVENT(req->proc_id, L1_HIT_ONPATH_IFETCH_SAT_BY_OFFPATH);
          STAT_EVENT(req->proc_id, L1_USE_OFFPATH_IFETCH);
          break;
        case MRT_DFETCH:
        case MRT_DSTORE:
          STAT_EVENT(req->proc_id, L1_HIT_ONPATH_DATA_SAT_BY_OFFPATH);
          STAT_EVENT(req->proc_id, L1_USE_OFFPATH_DATA);
          break;
        default:
          break;
      }
    } else {
      if(line->l0_modified_fetched_by_offpath) {
        STAT_EVENT(req->proc_id, JUST_L1_USE_OFFPATH);
        line->l0_modified_fetched_by_offpath = FALSE;
      }

      STAT_EVENT(req->proc_id, L1_HIT_ONPATH_SAT_BY_ONPATH);
      STAT_EVENT(req->proc_id, L1_USE_ONPATH);
      switch(req->type) {
        case MRT_IFETCH:
          STAT_EVENT(req->proc_id, L1_HIT_ONPATH_IFETCH_SAT_BY_ONPATH);
          STAT_EVENT(req->proc_id, L1_USE_ONPATH_IFETCH);
          break;
        case MRT_DFETCH:
        case MRT_DSTORE:
          STAT_EVENT(req->proc_id, L1_HIT_ONPATH_DATA_SAT_BY_ONPATH);
          STAT_EVENT(req->proc_id, L1_USE_ONPATH_DATA);
          break;
        default:
          break;
      }
    }
  } else {
    if(line->fetched_by_offpath) {
      STAT_EVENT(req->proc_id, L1_HIT_OFFPATH_SAT_BY_OFFPATH);
    } else {
      STAT_EVENT(req->proc_id, L1_HIT_OFFPATH_SAT_BY_ONPATH);
    }
  }

  if(!req->off_path)
    line->fetched_by_offpath = FALSE;
}


/**************************************************************************************/
/* wp_process_l1_fill: */

void wp_process_l1_fill(L1_Data* line, Mem_Req* req) {
  if(!WP_COLLECT_STATS)
    return;

  if((req->type == MRT_WB) || (req->type == MRT_WB_NODIRTY) ||
     (req->type == MRT_DPRF)) /* for now we don't consider prefetches */
    return;

  if(req->off_path) {
    STAT_EVENT(req->proc_id, L1_FILL_OFFPATH);
    switch(req->type) {
      case MRT_IFETCH:
        STAT_EVENT(req->proc_id, L1_FILL_OFFPATH_IFETCH);
        break;
      case MRT_DFETCH:
      case MRT_DSTORE:
        STAT_EVENT(req->proc_id, L1_FILL_OFFPATH_DATA);
        break;
      default:
        break;
    }
  } else {
    STAT_EVENT(req->proc_id, L1_FILL_ONPATH);
    if(req->onpath_match_offpath)
      STAT_EVENT(req->proc_id, DIST_L1_FILL_ONPATH_PARTIAL);
    else
      STAT_EVENT(req->proc_id, DIST_L1_FILL_ONPATH);

    switch(req->type) {
      case MRT_IFETCH:
        STAT_EVENT(req->proc_id, L1_FILL_ONPATH_IFETCH);
        break;
      case MRT_DFETCH:
      case MRT_DSTORE:
        STAT_EVENT(req->proc_id, L1_FILL_ONPATH_DATA);
        break;
      default:
        break;
    }
  }
  STAT_EVENT(req->proc_id, DIST_L1_FILL);
}

/**************************************************************************************/
/* wp_process_reqbuf_match: */

void wp_process_reqbuf_match(Mem_Req* req, Op* op) {
  if(!WP_COLLECT_STATS)
    return;

  if(op) {
    if(req->off_path) {
      if(!op->off_path) {
        STAT_EVENT(req->proc_id, REQBUF_ONPATH_MATCH_OFFPATH);
        STAT_EVENT(req->proc_id, REQBUF_ONPATH_MATCH_OFFPATH_DATA);
        STAT_EVENT(req->proc_id, DIST_REQBUF_OFFPATH_USED);
        STAT_EVENT(req->proc_id, DIST2_REQBUF_OFFPATH_USED_PARTIAL);
        req->onpath_match_offpath = TRUE;

        DEBUG(0,
              "Reqbuf match: On path hits off path. va:%s op:%s op:0x%s "
              "wp_op:0x%s opu:%s wpu:%s dist:%s%s\n",
              hexstr64s(op->oracle_info.va), disasm_op(op, TRUE),
              hexstr64s(op->inst_info->addr), hexstr64s(req->oldest_op_addr),
              unsstr64(op->unique_num), unsstr64(req->oldest_op_unique_num),
              op->unique_num > req->oldest_op_unique_num ? " " : "-",
              op->unique_num > req->oldest_op_unique_num ?
                unsstr64(op->unique_num - req->oldest_op_unique_num) :
                unsstr64(req->oldest_op_unique_num - op->unique_num));
      }
    }
  } else if(req->type == MRT_IFETCH) {
    if(req->off_path) {
      if(icache_off_path() == FALSE) {
        STAT_EVENT(req->proc_id, REQBUF_ONPATH_MATCH_OFFPATH);
        STAT_EVENT(req->proc_id, REQBUF_ONPATH_MATCH_OFFPATH_IFETCH);
        STAT_EVENT(req->proc_id, DIST_REQBUF_OFFPATH_USED);
        STAT_EVENT(req->proc_id, DIST2_REQBUF_OFFPATH_USED_PARTIAL);
        req->onpath_match_offpath = TRUE;
      }
    }
  }
}

static void update_mem_req_occupancy_counter(Mem_Req_Type type, int delta) {
  uns* counter;
  switch(type) {
    case MRT_IFETCH:
    case MRT_DFETCH:
    case MRT_DSTORE:
      counter = &mem_req_demand_entries;
      break;
    case MRT_IPRF:
    case MRT_DPRF:
      counter = &mem_req_pref_entries;
      break;
    case MRT_WB:
    case MRT_WB_NODIRTY:
      counter = &mem_req_wb_entries;
      break;
    default:
      FATAL_ERROR(0, "Unknown mem req state\n");
      break;
  }
  *counter += delta;
  ASSERT(0, *counter <= mem->total_mem_req_buffers);
}

uns num_offchip_stall_reqs(uns proc_id) {
  // return dram->proc_infos[proc_id].orig_stall_reqs; // HACKY
  return 0;  // Ramulator_todo: replicate this in Ramulator. Currently is used
             // only to collect statistics
}
