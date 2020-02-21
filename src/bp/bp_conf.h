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
 * File         : bp//bp_conf.h
 * Author       : HPS Research Group
 * Date         : 11/19/2001
 * Description  :
 ***************************************************************************************/

#ifndef __BP_CONF_H__
#define __BP_CONF_H__

#include "bp/bp.h"
#include "globals/global_types.h"

/**************************************************************************************/
// Macros

#define IS_CONF_CF(op)                    \
  ((op)->table_info->cf_type == CF_CBR || \
   (op)->table_info->cf_type == CF_IBR || \
   (op)->table_info->cf_type == CF_ICALL)


/**************************************************************************************/
/* Types */

typedef struct Opc_Table_struct {
  Flag    off_path;
  Flag    mispred;
  Flag    pred_conf;
  Flag    verified;
  Counter op_num;  // here for debugging only
} Opc_Table;

typedef struct Bpc_Data_struct {
  uns8 proc_id;
  uns* bpc_ctr_table;    // used to predict confidence for a particular branch
  Opc_Table* opc_table;  // used to calculate the on_path conf, stores
                         // confidence of in-flight branches
  uns count;             // size of opc_table
  uns head;              // head index in opc_table
  uns tail;              // tail index in opc_table
} Bpc_Data;


typedef struct PERCEP_Bpc_Data_struct {
  Perceptron* conf_pt;
  uns64 conf_perceptron_global_hist;       // global history only for confidence
                                           // perceptron to support long history
  uns64 conf_perceptron_global_misp_hist;  // global history only for confidence
                                           // perceptron to support long history
} PERCEP_Bpc_Data;

/**************************************************************************************/
/* Prototypes */

void init_bp_conf(void);
void bp_conf_pred(Op*);
void bp_update_conf(Op*);

void pred_onpath_conf(Op*);
void update_onpath_conf(Op*);
void recover_onpath_conf(void);
uns  read_conf_head(void);
uns8 compute_spawn_path_conf(uns);


/**************************************************************************************/
/* perceptron based confidece estimator */
void conf_perceptron_init(void);
void conf_perceptron_pred(Op*);
void conf_perceptron_update(Op*);


/**************************************************************************************/

#endif /* #ifndef __BP_CONF_H__ */
