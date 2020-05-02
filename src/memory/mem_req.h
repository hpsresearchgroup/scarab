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
 * File         : memory/mem_req.h
 * Author       : HPS Research Group
 * Date         : 8/14/2013
 * Description  : Memory request
 ***************************************************************************************/

#ifndef __MEM_REQ_H__
#define __MEM_REQ_H__

#include "globals/enum.h"
#include "libs/list_lib.h"

/**************************************************************************************/
/* Forward Declarations */

struct Mem_Queue_struct;

/**************************************************************************************/
/* Types */

typedef enum Mem_Req_State_enum {
  MRS_INV, /* if you change this order or add anything, fix mem_req_state_names
              [] and is_final_state() in memory.c and */
  MRS_MLC_NEW,
  MRS_MLC_WAIT,
  MRS_MLC_HIT_DONE, /* final state */
  MRS_L1_NEW,
  MRS_L1_WAIT,
  MRS_L1_HIT_DONE, /* final state */
  MRS_BUS_NEW,
  // MRS_BUS_WAIT,
  MRS_MEM_NEW,
  MRS_MEM_SCHEDULED,  // This is only used for new dram model
  MRS_MEM_WAIT,
  MRS_BUS_BUSY,
  MRS_BUS_WAIT,  // This state is not used for new dram model
  MRS_MEM_DONE,  /* final state */
  MRS_BUS_IN_DONE,
  MRS_FILL_L1,
  MRS_FILL_MLC,
  MRS_FILL_DONE, /* final state */
} Mem_Req_State;

#define MRT_LIST(elem)                               \
  elem(IFETCH)         /* instruction fetch */       \
    elem(DFETCH)       /* data fetch */              \
    elem(DSTORE)       /* data store */              \
    elem(IPRF)         /* instruction prefetch */    \
    elem(DPRF)         /* data prefetch */           \
    elem(WB)           /* writeback of dirty data */ \
    elem(WB_NODIRTY)   /* writeback of clean data */ \
    elem(MIN_PRIORITY) /* request of minimal priority */

DECLARE_ENUM(Mem_Req_Type, MRT_LIST, MRT_);

/*
   Destination(s) of the request.
   - This is currently used if a demand matches a prefetch.
   - But it can be generalized if done_func is done with...
*/
typedef enum Destination_enum {
  DEST_NONE   = 0,
  DEST_DCACHE = 1 << 0,
  DEST_ICACHE = 1 << 1,
  DEST_MLC    = 1 << 2,
  DEST_L1     = 1 << 3,
  DEST_MEM    = 1 << 4
} Destination;

#define DRAM_REQ_STATUS_LIST(elem) elem(CONFLICT) elem(MISS) elem(HIT)

DECLARE_ENUM(Dram_Req_Status, DRAM_REQ_STATUS_LIST, DRAM_REQ_ROW_);

// typedef in globals/global_types.h
struct Mem_Req_struct {
  uns  proc_id;            /* processor id that generates the request */
  int  id;                 /* request buffer num */
  Flag off_path;           /* is the mem_req entirely off path? */
  Flag off_path_confirmed; /* does the processor know that this is off-path -
                              set after the branch resolves */
  Mem_Req_State            state;    /* what state is the miss in? */
  Mem_Req_Type             type;     /* what kind of miss is it? */
  struct Mem_Queue_struct* queue;    /* Pointer to the queue this entry is in */
  Counter                  priority; /* priority of the miss */
  Addr                     addr;     /* address to fetch */
  Addr                     phys_addr;   /* physical address */
  uns                      size;        /* size to fetch */
  uns                      mlc_bank;    /* which MLC bank it is going to */
  uns                      l1_bank;     /* which l1 bank it is going to */
  uns                      mem_channel; /* mutiple channel support */
  uns                      mem_bank;    /* which memory bank it is going to */
  uns     mem_flat_bank;    /* flattened bank index across channels */
  Counter start_cycle;      /* cycle that the request is ready to process */
  Counter rdy_cycle;        /* cycle when the current operation is complete */
  uns reserved_entry_count; /* how many entries are reserved for this request */
  Counter first_stalling_cycle; /* cycle this request became a type considered
                                   stalling */
  Counter oldest_op_unique_num; /* unique num of the oldest op that is waiting
                                   for this req - may not be in the machine any
                                   more */
  Counter oldest_op_op_num; /* op num of the oldest op that is waiting for this
                               req - may not be in the machine any more */
  Counter oldest_op_addr; /* PC of the oldest op that is waiting for this req -
                             may not be in the machine any more */
  List op_ptrs;
  List op_uniques;
  uns  op_count;  /* number of ops that are waiting for the miss */
  uns  req_count; /* number of requests coalesced into this one */
  Flag (*done_func)(struct Mem_Req_struct*); /* pointer to function to call when
                                                the memory request is finished
                                              */
  Flag mlc_miss;                             /* did this request miss in MLC */
  Flag mlc_miss_satisfied;   /* did this request miss in MLC and it is already
                                satisfied? */
  Counter mlc_miss_cycle;    /* cycle when this req missed in MLC */
  Flag    l1_miss;           /* did this request miss in L1? */
  Flag    l1_miss_satisfied; /* did this request miss in L1 and it is already
                                satisfied? */
  Counter l1_miss_cycle;     /* cycle when this req missed in L1 */
  Counter mem_queue_cycle;   /* cycle this request entered the mem_queue */
  Counter mem_crit_path_at_entry; /* DVFS perf pred: the global critical path
                                     estimate when the req entered the memory
                                     controller */
  Counter window_num;    /* Window number, used for DCache miss MLP estimate */
  uns     longest_chain; /* Used for dcache miss MLP estimate */
  Counter unique_num;    /* unique num that allocated this request - for icache
                            this is the unique_count */
  Flag onpath_match_offpath;  /* is this an offpath req matched by onpath op? */
  Flag demand_match_prefetch; /* is this a prefetch req matched by a demand? */
  Flag bw_prefetch;           /* is this request a bandwidth prefetch? */
  Flag bw_prefetchable;   /* would this request be a bandwidth prefetch if there
                             was more BW? */
  Flag dirty_l0;          /* should this request dirty the L0 (dcache) line? */
  Flag wb_requested_back; /* is this a writeback that is requested by the core
                             again? */
  Destination destination; /* which cache level are we filling (only value of L1
                            * matters)
                            */
  Flag wb_used_onpath;     /* is this a writeback that was used onpath? */
  Addr loadPC;        /* load PC of the oldest load requesting this address */
  uns8 prefetcher_id; /* which Prefetcher sent this prefetch */

  uns pref_distance; /* prefetch distance (currently works for pref_stream only)
                      */
  Addr  pref_loadPC;
  uns32 global_hist;  // Used for perf hfilter
  Mem_Req_Type
               perf_pred_type; /* Req type seen by perf_pred (cannot change once set) */
  Mem_Req_Type perf_pred_off_path_confirmed; /* off_path_confirmed seen by
                                                perf_pred (cannot change once
                                                set) */
  Counter mem_seq_num;   /* Mem queue sequence number (for prioritizing aged
                            requests) */
  Counter fq_start_time; /* Virtual start time (for fair queuing) */
  Counter fq_bank_finish_time; /* Virtual bank finish time (for fair queuing) */
  Counter fq_finish_time;      /* Virtual finish time (for fair queuing) */
  // things below is used ony for batch scheduling
  Flag            belong_to_batch;   /* dose it belong to the current batch */
  uns8            rank;              /* the larger the high priority */
  Dram_Req_Status row_access_status; /* row hit, miss or conflict */
  Flag
          shadow_row_hit; /* row conflict but predicted to be row hit if run alone */
  Counter dram_access_cycle; /* cycle of DRAM access (in L1 cycles) */
  Counter dram_latency;      /* DRAM latency (in L1 cycles) */
  Counter dram_core_service_cycles_at_start; /* "Virtual clock" timestamp */
};

/**************************************************************************************/
/* Prototypes */

Flag mem_req_type_is_demand(Mem_Req_Type type);
Flag mem_req_type_is_prefetch(Mem_Req_Type type);
Flag mem_req_type_is_stalling(Mem_Req_Type type);

/**************************************************************************************/
/* Externs */

extern const char* const mem_req_state_names[];

#endif /* #ifndef __MEM_REQ_H__*/
