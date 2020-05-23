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
 * File         : op_info.h
 * Author       : HPS Research Group
 * Date         : 2/19/2001
 * Description  :
 ***************************************************************************************/

#ifndef __OP_INFO_H__
#define __OP_INFO_H__

#include "inst_info.h"
#include "table_info.h"

/**************************************************************************************/
// Defines


#define MAX_DEPS 128
#define MAX_OUTS 3


/**************************************************************************************/
typedef struct Generic_Op_Info_struct {
  Counter unique_num;
  Addr    addr; /* pc */
  Op*     op;
  Counter fetch_cycle;
} Generic_Op_Info;

// {{{ Dep_Type
typedef enum Dep_Type_enum {
  REG_DATA_DEP,
  MEM_ADDR_DEP,
  MEM_DATA_DEP,
  NUM_DEP_TYPES,
} Dep_Type;
// }}}

/*------------------------------------------------------------------------------------*/
// {{{ Src_Info
typedef struct Src_Info_struct {
  Dep_Type          type;
  struct Op_struct* op;
  Counter           op_num;
  Counter           unique_num;
  Quad              val;
} Src_Info;
// }}}

/*------------------------------------------------------------------------------------*/

/**************************************************************************************/
// {{{ Op_Info
// The 'Op_Info' struct holds information that is unique to the
// current instance of the instruction (data values, etc.)
// typedef in globals/global_types.h
struct Op_Info_struct {
  struct Table_Info_struct* table_info;  // copy of op->table_info
  struct Inst_Info_struct*  inst_info;   // copy of op->inst_info

  uns      num_srcs;            // number of dependencies to obey
  Src_Info src_info[MAX_DEPS];  // information about each source
  Flag     update_fpcr;         // need to update the fpcr
  UQuad    new_fpcr;            // fpcr value resulting from this op

  // mem op fields
  Addr va;        // virtual address for memory instructions
  uns  mem_size;  // memory data size now became dynamic property due to REP
                  // STRING

  // all op fields
  Addr npc;  // the true next pc after the instruction

  // control flow fields
  Addr target;     // decoded target of branch, set by oracle
  uns8 dir;        // true direction of branch, set by oracle
  Addr pred_npc;   // predicted next pc field
  Addr pred_addr;  // address used to predict branch (might be fetch_addr)
  uns8 pred;       // predicted direction of branch, set by the branch predictor
  Flag misfetch;   // true if target address is the ONLY thing that was wrong
  Flag mispred;  // true if the direction of the branch was mispredicted and the
                 // branch should cause a recovery, set by the branch predictor
  Flag btb_miss;           // true if the target is not known at prediction time
  Flag btb_miss_resolved;  // true if the btb miss is resolved by the pipeline.
  Flag no_target;  // true if there is no target for this branch at prediction
                   // time
  uns8 late_pred;  // predicted direction of branch, set by the multi-cycle
                   // branch predictor
  Addr late_pred_npc;  // predicted next pc field by the multi-cycle branch
                       // predictor
  Flag late_misfetch;  // true if target address is the ONLY thing that was
                       // wrong after the multi-cycle branch prediction kicks in
  Flag  late_mispred;  // true if the multi-cycle branch predictor mispredicted
  Flag  recovery_sch;  // true if this op has scheduled a recovery
  uns32 pred_global_hist;  // global history used to predict the branch


  uns64 pred_perceptron_global_hist;  // Only for perceptron, global history
                                      // used to predict the branch
  uns64 pred_conf_perceptron_global_hist;  // Only for perceptron, global
                                           // history used to confidence predict
                                           // the branch
  uns64 pred_conf_perceptron_global_misp_hist;  // Only for perceptron, global
                                                // history used to confidence
                                                // predict the branch uns32
                                                // pred_global_hist;      //
                                                // global history used to
                                                // predict the branch
  uns8* pred_gpht_entry;  // entry used for interference free pred
  uns8* pred_ppht_entry;  // entry used for interference free pred
  uns8* pred_spht_entry;  // entry used for interference free pred
  uns32 pred_local_hist;  // local history used to predict the branch
  uns32 pred_targ_hist;   // global history used to predict the indirect branch
  uns8  hybridgp_gpred;   // hybridgp's global prediction
  uns8  hybridgp_ppred;   // hybridgp's pred-address prediction
  uns8  pred_tc_selector_entry;  // which ibtb predicted this op?
  Flag  ibp_miss;  // true if the target is not predicted by the indirect pred

  Flag dcmiss;  // dcache miss has occurred

  Flag pred_conf;
  Addr pred_conf_index;
  uns  opc_index;

  Counter inst_sim_cycle;  // cycle oracle executes op

  Quad old_mem_value;
  Quad new_mem_value;
  Flag mlc_miss;            // is this op an MLC data miss?
  Flag mlc_miss_satisfied;  // mlc miss caused by this op is already satisfied
  Flag l1_miss;             // is this op an L1 data miss?
  Flag l1_miss_satisfied;   // l1 miss caused by this op is already satisfied
  Flag dep_on_l1_miss;      // op is waiting for an l1_miss to be satisfied
  Flag was_dep_on_l1_miss;  // op was waiting for an l1_miss to be satisfied,
                            // but not any more

  uns32 error_event;  // bit vector for the unexpected events generated by this
                      // op (error_event.h)
};
// }}}

/**************************************************************************************/

#endif /* #ifndef __OP_INFO_H__ */
