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
 * File         : bp/bp.h
 * Author       : HPS Research Group
 * Date         : 12/9/1998
 * Description  :
 ***************************************************************************************/

#ifndef __BP_H__
#define __BP_H__

#include "globals/global_types.h"
#include "libs/cache_lib.h"
#include "libs/hash_lib.h"
#include "op.h"

/**************************************************************************************/
// Branch prediction recovery information

typedef struct Bp_Recovery_Info_struct {
  uns     proc_id;
  Counter recovery_cycle;      /* cycle that begins misprediction recovery */
  Addr    recovery_fetch_addr; /* address to redirect the istream */
  Counter recovery_op_num;     /* op_num of op that caused recovery */
  Counter recovery_cf_type;    /* cf_type of op that caused recovery */
  Recovery_Info
             recovery_info; /* information about the op causing the recovery */
  Inst_Info* recovery_inst_info;  // pointer to inst causing recovery
  Flag       recovery_force_offpath;

  Counter redirect_cycle;  /* cycle that begins a redirection (eg. btb miss) */
  Counter redirect_op_num; /* op_num of op that caused redirect */
  Op*     redirect_op;     /* pointer to op that caused redirect */

  Op* recovery_op;   /* pointer to op that caused recovery */
  int oracle_cp_num; /* checkpoint num that we need to return to - if using
                        oracle checkpointing */
  Counter recovery_unique_num; /* unique_num of op that caused recovery */
  uns64   recovery_inst_uid;   /* unique id of the instruction that caused  */

  Flag    wpe_flag;  /* This CFI has a WPE associated with it */
  Counter wpe_cycle; /* The cycle in which the WPE occurred */

  Flag late_bp_recovery;        // TRUE if recovery is due to a late branch
                                // prediction.
  Flag late_bp_recovery_wrong;  // TRUE if recovery is due to a late branch
                                // prediction that is wrong.

} Bp_Recovery_Info;

/**************************************************************************************/
/* Conditional_Branch_Info  */

typedef struct Ra_Conditional_Branch_Info_struct {
  Addr    pred_addr;
  uns32   old_history;
  uns32   new_history;
  Counter op_num;
  Counter unique_num;
  Op*     op;
  uns8    dir;
  Flag    off_path;
  Flag    init_mispred;
  Flag    resolved;
  int     updates_made;  // how many updates did this branch make
  Flag    futgshare_changed;
} Ra_Conditional_Branch_Info;

/**************************************************************************************/
// Branch prediction state

typedef struct Crs_Entry_struct {
  Addr    addr;
  Counter op_num;
  uns     nos;  // next on stack
} Crs_Entry;

typedef struct Loop_Entry_struct {
  uns8 dir;       // direction branch takes on loop exit
  uns  count;     // consecutive times the non-exit outcome has been seen
  uns  last_max;  // period of the last completed loop pattern
  uns  repeats;   // number of times in a row the pattern has repeated
} Loop_Entry;

/* branch predictor/btb instruction level stats */
typedef struct Br_Inst_Stats_struct {
  Addr                         pathhist;
  Addr                         addr;
  Addr                         target_addr;
  uns32                        dyn_cnt;
  uns32                        taken_cnt;
  uns32                        misspred_cnt;
  uns32                        interf_misspred_cnt;
  uns32                        prev_dir;
  Addr                         prev_target;
  uns32                        trans_cnt;
  uns32                        num_path;
  uns32                        num_chain;
  struct Br_Inst_Stats_struct* path;
  struct Br_Inst_Stats_struct* chain;
} Br_Inst_Stats;

struct Bp_struct;
struct Bp_Btb_struct;
struct Bp_Ibtb_struct;  // added _struct, compiler randomly started complaining
struct Br_Conf_struct;

typedef struct Perceptron_struct {
  int32* weights;
} Perceptron;

typedef struct Bp_Data_struct {
  uns proc_id;
  /* predictor data */
  struct Bp_struct* bp;       // main branch predictor.
  struct Bp_struct* late_bp;  // late multi-cycle branch predictor. (Could be
                              // null)
  struct Bp_Btb_struct*  bp_btb;
  struct Bp_Ibtb_struct* bp_ibtb;
  struct Br_Conf_struct* br_conf;

  uns32 global_hist;
  Cache btb;

  struct {
    Crs_Entry* entries;
    Flag*      off_path;
    uns        depth;
    uns        head;
    uns        tail;
    uns        tail_save;
    uns        depth_save;
    // for realistic crs
    uns tos;   // top of stack
    uns next;  // next return address will be written here
  } crs;

  Cache tc_tagged;
  Addr* tc_tagless;
  uns8* tc_selector;
  uns32 targ_hist;
  uns32 targ_index;
  uns8  target_bit_length;

  Flag on_path_pred;

  List cbrs_in_machine;

} Bp_Data;

/**************************************************************************************/
/* Branch Predictor Interfaces */

/* IMPORTANT: please make sure that this enum matches EXACTLY the names and
 * order in bp/bp_table.def !!!!!!! */
typedef enum Bp_Id_enum {
  GSHARE_BP,
  HYBRIDGP_BP,
  TAGESCL_BP,
  TAGESCL80_BP,
#define DEF_CBP(CBP_NAME, CBP_CLASS) CBP_CLASS##_BP,
#include "cbp_table.def"
#undef DEF_CBP
  NUM_BP,
} Bp_Id;

typedef enum Btb_Id_enum {
  GENERIC_BTB,
  NUM_BTB,
} Btb_Id;

typedef enum Ibtb_Id_enum {
  TC_TAGLESS_IBTB,
  TC_TAGGED_IBTB,
  TC_HYBRID_IBTB,
  NUM_IBTB,
} Ibtb_Id;

typedef enum Br_Conf_Id_eunm {
  ONPATH_CONF,
  PERCEPTRON_CONF,
  NUM_BR_CONF,
} Br_Conf_Id;

typedef struct Bp_struct {
  Bp_Id       id;
  const char* name;
  void (*init_func)(void);       /* called to initialize the predictor */
  void (*timestamp_func)(Op*);   /* called to timestamp a branch for prediction,
                                    update, and recovery */
  uns8 (*pred_func)(Op*);        /* called to predict a branch instruction */
  void (*spec_update_func)(Op*); /* called to update the speculative state of
                                    the predictor in the front-end */
  void (*update_func)(Op*); /* called to update the bp when a branch is resolved
                               (at the end of execute or retire) */
  void (*retire_func)(Op*); /* called to retire a branch and update the state of
                               the bp that has to be updated after retirement*/
  void (*recover_func)(Recovery_Info*); /* called to recover the bp when a
                                           misprediction is realized */
} Bp;

typedef struct Bp_Btb_struct {
  Btb_Id      id;
  const char* name;
  void (*init_func)(
    Bp_Data*); /* called to initialize the branch target buffer */
  Addr* (*pred_func)(Bp_Data*, Op*);  /* called to predict the branch target */
  void (*update_func)(Bp_Data*, Op*); /* */
  void (*recover_func)(Bp_Data*, Recovery_Info*); /* */
} Bp_Btb;

typedef struct Bp_Ibtb_struct {
  Ibtb_Id     id;
  const char* name;
  void (*init_func)(Bp_Data*); /* called to initialize the indirect target
                                  predictor */
  Addr (*pred_func)(Bp_Data*,
                    Op*); /* called to predict an indirect branch target */
  void (*update_func)(Bp_Data*, Op*); /* called to update the indirect branch
                                         target when a branch is resolved */
  void (*recover_func)(Bp_Data*, Recovery_Info*); /* called to recover the
                                                     indirect branch target when
                                                     a misprediction is realized
                                                   */
} Bp_Ibtb;

typedef struct Br_Conf_struct {
  Br_Conf_Id  id;
  const char* name;
  void (*init_func)(void);  /* called to initialize the confidence estimator */
  void (*pred_func)(Op*);   /* called to predict confidence */
  void (*update_func)(Op*); /* called to update the confidence estimator when a
                               branch is resolved */
  void (*recover_func)(void); /* called to recover the confidence estimator
                                 when a misprediction is realized */
} Br_Conf;

/**************************************************************************************/
/* External variables */

extern Bp                bp_table[];
extern Bp_Btb            bp_btb_table[];
extern Bp_Ibtb           bp_ibtb_table[];
extern Bp_Data*          g_bp_data;
extern Bp_Recovery_Info* bp_recovery_info;
extern Br_Conf           br_conf_table[];

/**************************************************************************************/
/* Prototypes */
void set_bp_data(Bp_Data* new_bp_data);
void set_bp_recovery_info(Bp_Recovery_Info* new_bp_recovery_info);

void init_bp_recovery_info(uns8, Bp_Recovery_Info*);
void bp_sched_recovery(Bp_Recovery_Info* bp_recovery_info, Op* op,
                       Counter cycle, Flag late_bp_recovery,
                       Flag force_offpath);
void bp_sched_redirect(Bp_Recovery_Info*, Op*, Counter);

void init_bp_data(uns8, Bp_Data*);
Addr bp_predict_op(Bp_Data*, Op*, uns, Addr);
void bp_target_known_op(Bp_Data*, Op*);
void bp_resolve_op(Bp_Data*, Op*);
void bp_retire_op(Bp_Data*, Op*);
void bp_recover_op(Bp_Data*, Cf_Type, Recovery_Info*);


/**************************************************************************************/

#endif /* #ifndef __BP_H__ */
