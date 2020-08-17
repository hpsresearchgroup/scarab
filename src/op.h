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
 * File         : op.h
 * Author       : HPS Research Group
 * Date         : 11/11/1997
 * Description  :
 ***************************************************************************************/

#ifndef __OP_H__
#define __OP_H__

#include "globals/enum.h"
#include "globals/global_types.h"
#include "inst_info.h"
#include "op_info.h"
#include "table_info.h"


/**************************************************************************************/
// {{{ Defines

#define OP_SRCS_RDY(x) \
  ((x)->srcs_not_rdy_vector == 0 && cycle_count >= (x)->rdy_cycle)
#define OP_DONE(x) (cycle_count >= (x)->done_cycle)
#define OP_BROADCAST(x) ((cycle_count + 1) >= (x)->done_cycle)
#define MULTI_CYCLE_OP(x)                       \
  ((x)->inst_info->latency > 1 + RFILE_STAGE || \
   (x)->table_info->mem_type == MEM_LD)
#define MAX_STRANDS 400
#define MAX_STRAND_BYTES (MAX_STRANDS / 8)
#define STRAND_BYTE(number) (((number) >> 3) % MAX_STRAND_BYTES)

#define STRAND_BIT_IS_SET(array, index) \
  (((array)[STRAND_BYTE((index))] & (1 << ((index)&7))) != 0)

// }}}

/**************************************************************************************/
// {{{ Op_State
// Op_State is the state of the op in the datapath

#define OP_STATE_LIST(elem)                                                    \
  elem(FETCHED)  /* op has been fetched, awaiting issue */                     \
    elem(ISSUED) /* op has been issued into the node table (reorder buffer) */ \
    elem(IN_RS)  /* op is in the scheduling window (RS), waiting for its       \
                    sources */                                                 \
    elem(SLEEP)  /* for pipelined schedule: wake up NEXT cycle */              \
    elem(WAIT_FWD)     /* op is waiting for forwarding to happen */            \
    elem(LOW_PRIORITY) /* op is waiting for forwarding to happen */            \
    elem(READY)        /* op is ready to fire, awaiting scheduling */          \
    elem(TENTATIVE)    /* op has been scheduled, but may fail and have to be   \
                          rescheduled */                                       \
    elem(SCHEDULED)    /* op has been scheduled and will complete */           \
    elem(MISS)         /* op has missed in the dcache */                       \
    elem(WAIT_DCACHE)  /* op is waiting for a dcache port */                   \
    elem(WAIT_MEM)     /* op is waiting for a miss_buffer entry */             \
    elem(DONE)         /* op is finished executing, awaiting retirement */

DECLARE_ENUM(Op_State, OP_STATE_LIST, OS_);
// }}}

/*------------------------------------------------------------------------------------*/
// {{{ Wake_Up_Entry
typedef struct Wake_Up_Entry_struct {
  Op*                          op;
  Counter                      unique_num;
  Dep_Type                     dep_type;
  uns8                         rdy_bit;
  struct Wake_Up_Entry_struct* next;
} Wake_Up_Entry;
// }}}

/*------------------------------------------------------------------------------------*/
// {{{ Recovery_Info
// this information is used when the op mispredicts

typedef struct Recovery_Info_struct {  // QUESTION no proc_id?
  uns   proc_id;
  uns32 pred_global_hist;  // the global history used for the prediction
  uns64 conf_perceptron_global_hist;  // Only for confidnece perceptron, a copy
                                      // of the correct global history
  uns64 conf_perceptron_global_misp_hist;  // Only for confidnece perceptron, a
                                           // copy of the correct global history
  uns32 targ_hist;  // a copy of the correct indirect branch pattern history
  Addr  npc;
  // next three are used to recover the realistic CRS
  uns     crs_tos;
  uns     crs_next;
  uns     crs_depth;
  Counter op_num;
  Addr    tos_addr;  // address on the top of CRS when this op was fetched

  Flag oracle_dir;  // filled by oracle
  Flag new_dir;     // used to repair predictor state (equals oracle_dir by
                    // default).

  Addr    PC;
  Cf_Type cf_type;
  Addr    branchTarget;
  int64   branch_id;  // set by the branch predictor timestamp_func().
} Recovery_Info;
// }}}


// {{{ Dp_Info struct

typedef struct Dp_Info_struct {
  Flag follows_off_path;  // op is target of mispredict / redirect
  Flag bogus_result;      // necessary because state can change from OS_MISS to
                          // OS_SCHEDULED
  unsigned char dep_strand_mask[MAX_STRAND_BYTES];  // dependence strand mask.
  Counter preceding_unique_num;  // unique_num of preceding op in program order.
  Counter strand_number;
} Dp_Info;
// }}}


/**************************************************************************************/
// {{{ Op
// typedef in globals/global_types.h
struct Op_struct {
  // {{{ op_pool stuff --- don't use outside of op pool management
  Flag op_pool_valid;  // is op allocated from the op_pool?
  Op*  op_pool_next;   // either next free or next active op
  uns  op_pool_id;     // unique identifier for op (doesn't change)
  // }}}

  // {{{ op numbers and info pointers
  uns     proc_id;     // processor id for cmp model
  uns     thread_id;   // id number for the thread to which this op belongs
  Flag    bom;         // begining of macro instruction when we use op as a uop
  Flag    eom;         // end of macro instruction when we use op as a uop
  Counter op_num;      // op number
  Counter unique_num;  // unique number for each instance of an op (not reset on
                       // recovery)
  Counter unique_num_per_proc;  // unique number per core
  uns64   inst_uid;  // unique number for the macro instruction provided
                     // by the frontend (PIN)
  Counter     addr_pred_num;  // unique number for each address prediction
  Table_Info* table_info;  // copy of info->table_info to limit pointer chasing
  Inst_Info* inst_info;  // pointer to unique struct for each static instruction
  Op_Info    oracle_info;  // information about the execution of the op in the
                           // oracle
  Op_Info engine_info;     // information about the execution of the op in the
                           // engine
  int oracle_cp_num;  // if the op has created an oracle checkpointed this is
                      // not -1
  // }}}

  int32 perceptron_output;       //
  int32 conf_perceptron_output;  // confidece perceptron
  // {{{ state and event cycle counters
  Op_State state;        // the state of the op in the datapath
  Counter  fetch_cycle;  // cycle an individual instruction is fetched
  Counter  bp_cycle;     // cycle a CF instruction accesses the branch predictor
  Counter  map_cycle;    // cycle an individual instruction enters the map stage
  Counter  issue_cycle;  // cycle an individual instruction is issued -- same as
                         // chkpt
  Counter rdy_cycle;    // cycle when the final source value is available to the
                        // op (only useful when vector is clear)
  Counter sched_cycle;  // cycle when the op is scheduled (arrives at the
                        // functional unit)
  Counter exec_cycle;   // cycle when execution (or addr gen) of op will be
                        // completed (result usable)
  Counter dcache_cycle;  // cycle when the op accesses the dcache
  Counter done_cycle;    // cycle when the op is ready to retire
  Counter retire_cycle;  // cycle when the op actually retires (useful if you
                         // keep the ops around after they leave the node
                         // talbes)
  Counter replay_cycle;  // cycle when the op catches a replay signal
  Counter pred_cycle;

  // }}}

  // {{{ path and fetch info
  Flag off_path;    // is the op on the correct path of the program? - oracle
                    // information
  Flag exit;        // is this the last instruction to execute?
  Flag prog_input;  // is this op directly related to an input value of the
                    // program ?
  Addr          fetch_addr;       // fetch address used to fetch the instruction
  uns           cf_within_fetch;  // branch number within a fetch cycle
  Recovery_Info recovery_info;    // information that will be used to recover a
                                  // mispredict by the op
  // }}}

  // {{{ scheduler information
  uns     fu_num;   // functional unit number the op will or did execute on
  Counter node_id;  // id for position in the node table
  Counter rs_id;    // id for which Reservation Station (RS) this op is assigned
                    // to
  Counter chkpt_num;  // id for chkpt (WARNING: this can change due to
                      // recoveries)

  struct Op_struct* next_rdy;      // pointer to next ready op (node table)
  Flag              in_rdy_list;   // is the op in the node stage's ready list?
  struct Op_struct* next_node;     // pointer to the next op in the node table
  Flag              in_node_list;  // is the op in the node list?
  Flag              replay;        // is the op waiting to replay?
  uns               replay_count;  // number of times the op has replayed
  Flag dont_cause_replays;  // true if the op should not cause other ops to
                            // replay (like a correct value prediction)
  uns exec_count;           // how many times has this op been executed?
  // }}}

  // {{{ dependency information
  uns  srcs_not_rdy_vector;  // bits as given by order in the src_info array
  Flag wake_up_signaled[NUM_DEP_TYPES];  // set to true once a wake up has been
                                         // signaled by the op for the given
                                         // type
  Wake_Up_Entry* wake_up_head;  // list of ops that are dependent on this op, by
                                // dependency type
  Wake_Up_Entry* wake_up_tail;  // last entry in each wake up list (for speed)
  uns wake_up_count;   // count of ops to be awakened by this op (wake up list
                       // length)
  Counter wake_cycle;  // used by wake up logic for time wake up signal is sent
  // }}}

  struct Mem_Req_struct* req;  // pointer to memory request responsible for
                               // waking up the op
  // }}}

  Flag marked;  // for algorithms that mark already seen ops

  /*------------------------------------------------------------------------------------*/
  // FIELDS BELOW THIS POINT SHOULD BE MOVED INTO OTHER HEADERS
  // (along with any related structs above)

  // {{{ pipelined scheduler specific fields (move these)
  struct Sched_Info_struct* sched_info;
  Counter request_cycle;  // first cycle inst can request func unit i.e. is
                          // awake
  uns gps_not_rdy;        // vector for determining which gs's aren't ready.
  uns delay_bit;          // rejected ops in pipelined schedule is delayed
  uns first;  // op's sources were ready when dispatched => op is first in dep
              // chain
  uns src_same_chkpt;  // bookkeeping info: set if any parent is in same chkpt
  uns parent_load;     // vector for determining which parents are loads
  Counter same_src_last_op;  // bit vector indicating if any src of last op in
                             // same slot was the same
  int dup_fu_num;
  int dup_cluster;
  // }}}

  /* predict wait time specific fields */

  // {{{ predict wait time specific fields (move these)
  uns      trigger_parent;  // parent number from which trigger is received.
  Counter  pred_wait_time;  // number of cycles the op should wait before waking
  Counter  reject_count;    // number of times the op has been rejected
  Src_Info wakeup_trigger;  // op used to trigger wakeup. not necessarily parent
                            // op.
  uns trigger_type;         // does op have a valid trigger.

  uns  fetch_lag;  // num cycles since the previous group was issued.
  Flag dcache_miss;
  // }}}

  struct Mbp7gshare_Info_struct* mbp7_info;  // multiple branch predictor
                                             // information

  // {{{ temporary fields -> will be deleted later (move these)
  int  derived_from_prog_input;  // derivation level from program read()
  int  min_input_id;
  int  max_input_id;
  Flag sources_addr_reg;
  uns  addr_pred_flags;
  uns  stephan_corr_index;
  Addr pred_addr;
  Flag recovery_scheduled;
  Flag redirect_scheduled;
  // }}}
};
// }}}

/**************************************************************************************/

#endif  // #ifndef __OP_H__
