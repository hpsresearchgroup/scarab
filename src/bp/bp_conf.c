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
 * File         : bp_conf.c
 * Author       : HPS Research Group
 * Date         : 11/19/2001
 * Description  :
 ***************************************************************************************/

#include "debug/debug_macros.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"

#include "bp/bp.h"
#include "bp/bp_conf.h"
#include "icache_stage.h"
#include "op.h"

#include "bp/bp.param.h"
#include "debug/debug.param.h"
#include "statistics.h"


/***************************************************************************************/
/* Defines */

#define OPC_SIZE 512

/**************************************************************************************/
/* Global Variables */

static Bpc_Data*        bpc_data        = NULL;
static PERCEP_Bpc_Data* percep_bpc_data = NULL;

/**************************************************************************************/
/* Local prototypes */

static Flag compute_onpath_conf(Flag);
static void print_onpath_conf(void);
static uns  count_zeros(uns, uns);


/**************************************************************************************/
// init_bp_conf:

void init_bp_conf() {
  uns ii;

  bpc_data                = (Bpc_Data*)malloc(sizeof(Bpc_Data));
  bpc_data->bpc_ctr_table = (uns*)malloc(sizeof(uns) * (1 << BPC_BITS));
  ASSERT(0, bpc_data->bpc_ctr_table);
  for(ii = 0; ii < (1 << BPC_BITS); ii++) {
    if(BPC_MECH)  // counter
      bpc_data->bpc_ctr_table[ii] = 0;
    else  // majority vote
      bpc_data->bpc_ctr_table[ii] = N_BIT_MASK(BPC_CIT_BITS);
  }

  bpc_data->opc_table = (Opc_Table*)malloc(sizeof(Opc_Table) * OPC_SIZE);
  bpc_data->count     = 0;
  bpc_data->head      = 0;
  bpc_data->tail      = 0;
}


  /**************************************************************************************/
  // bp_conf_pred: called by bp_predict_op in bp.c
  // 0: think branch will mispredict
  // 1: confident branch will go the right direction

#define COOK_HIST_BITS(hist, untouched) \
  ((uns32)(hist) >> (32 - BPC_BITS + (untouched)) << (untouched))
#define COOK_ADDR_BITS(addr, shift) \
  (((uns32)(addr) >> (shift)) & (N_BIT_MASK(BPC_BITS)))

void bp_conf_pred(Op* op) {
  uns32 index;
  uns   entry;
  Flag  pred_conf;
  Flag  mispred = op->oracle_info.mispred | op->oracle_info.misfetch;

  // only updated on conditional branches
  Addr  addr        = op->inst_info->addr;
  uns32 hist        = g_bp_data->global_hist;
  uns32 cooked_hist = COOK_HIST_BITS(hist, 0);
  uns32 cooked_addr = COOK_ADDR_BITS(addr, 2);
  index             = cooked_hist ^ cooked_addr;

  entry = bpc_data->bpc_ctr_table[index];

  if(BPC_MECH) {  // counter
    pred_conf = (entry == N_BIT_MASK(BPC_CTR_BITS)) ? TRUE : FALSE;
  } else {  // majority vote
    uns8  ii, count = 0;
    uns32 mask = 1;

    // count ones
    for(ii = 0; ii < BPC_CIT_BITS; ii++, mask <<= 1)
      if(entry & mask)
        count++;
    pred_conf = count > (BPC_CIT_BITS * BPC_CIT_TH) / 100 ? TRUE : FALSE;
  }

  if(PERF_BP_CONF_PRED)
    pred_conf = !(op->oracle_info.mispred || op->oracle_info.misfetch);

  _DEBUG(0, DEBUG_BP_CONF, "bp_conf_pred: op:%s mispred:%d, pred:%d,%d\n",
         unsstr64(op->op_num), mispred, pred_conf, pred_conf != mispred);

  op->oracle_info.pred_conf_index = index;
  op->oracle_info.pred_conf       = pred_conf;

  STAT_EVENT(op->proc_id, BP_ON_PATH_CONF_MISPRED + 2 * op->off_path +
                            (pred_conf != mispred));
  STAT_EVENT(op->proc_id, BP_ON_PATH_PRED_MIS_CONF_MISPRED + 4 * op->off_path +
                            2 * pred_conf + (pred_conf != mispred));
}


/**************************************************************************************/
// bp_update_conf: called by bp_resolve_op in bp.c
// 0: think branch will mispredict
// 1: confident branch will go the right direction

void bp_update_conf(Op* op) {
  uns32 index   = op->oracle_info.pred_conf_index;
  uns*  entry   = &bpc_data->bpc_ctr_table[index];
  Flag  mispred = op->oracle_info.mispred | op->oracle_info.misfetch;

  _DEBUG(0, DEBUG_BP_CONF, "bp_update_conf: op:%s mispred:%d\n",
         unsstr64(op->op_num), mispred);

  // update the counters
  if(BPC_MECH)
    // counter
    if(mispred)
      if(BPC_CTR_RESET)
        // biased towards confidence
        *entry = 0;
      else
        *entry = SAT_DEC(*entry, 0);
    else
      *entry = SAT_INC(*entry, N_BIT_MASK(BPC_CTR_BITS));
  else
    // majority vote
    *entry = ((*entry << 1) | !mispred) & N_BIT_MASK(BPC_CIT_BITS);
}


/**************************************************************************************/
// pred_onpath_conf: called by bp_predict_op in bp.c
// 1: onpath
// 0: offpath

void pred_onpath_conf(Op* op) {
  Flag       pred_onpath;
  uns        head      = bpc_data->head;
  Opc_Table* opc_table = &bpc_data->opc_table[head];

  // update the opc_table
  ASSERT(0, bpc_data->count < OPC_SIZE);
  opc_table->mispred   = op->oracle_info.mispred | op->oracle_info.misfetch;
  opc_table->pred_conf = op->oracle_info.pred_conf;
  opc_table->off_path  = op->off_path;
  opc_table->verified  = FALSE;
  opc_table->op_num    = op->op_num;
  bpc_data->head       = CIRC_INC(head, OPC_SIZE);
  ;
  bpc_data->count++;

  op->oracle_info.opc_index = head;

  pred_onpath = compute_onpath_conf(FALSE);

  _DEBUG(0, DEBUG_ONPATH_CONF,
         "pred_onpath_conf: op:%s ind:%u 0x%llx mispred:%d pred_ok:%d,%c "
         "off_path:%d pred_onpath:%d,%c\n",
         unsstr64(op->op_num), head, op->inst_info->addr, opc_table->mispred,
         opc_table->pred_conf,
         opc_table->mispred != opc_table->pred_conf ? 'c' : 'm',
         opc_table->off_path, pred_onpath,
         opc_table->off_path != pred_onpath ? 'c' : 'm');

  print_onpath_conf();

  STAT_EVENT(op->proc_id, ONPATH_CONF_MISPRED + (pred_onpath != op->off_path));
  STAT_EVENT(op->proc_id, ONPATH_ON_PATH_CONF_MISPRED + 2 * op->off_path +
                            (pred_onpath != op->off_path));
  STAT_EVENT(op->proc_id, PRED_ONPATH_CONF_MISPRED + 2 * !pred_onpath +
                            (pred_onpath != op->off_path));

  g_bp_data->on_path_pred = pred_onpath;
}


/**************************************************************************************/
// update_onpath_conf: called by bp_resolve_op in bp.c

void update_onpath_conf(Op* op) {
  uns  index   = op->oracle_info.opc_index;
  Flag mispred = op->oracle_info.mispred | op->oracle_info.misfetch;
  uns  ii;

  _DEBUG(0, DEBUG_ONPATH_CONF,
         "update_onpath_conf: %s ind:%u mispred:%d off_path:%d\n",
         unsstr64(op->op_num), index, mispred, op->off_path);

  bpc_data->opc_table[index].pred_conf = !mispred;
  bpc_data->opc_table[index].verified  = TRUE;

  // update the tail, and recompute size
  ii = bpc_data->tail;
  while(ii != bpc_data->head) {
    Opc_Table* opc_table = &bpc_data->opc_table[ii];
    if(!opc_table->verified || opc_table->off_path || opc_table->mispred)
      break;
    bpc_data->count--;
    ii = CIRC_INC(ii, OPC_SIZE);
  }
  bpc_data->tail = ii;

  print_onpath_conf();

  g_bp_data->on_path_pred = compute_onpath_conf(TRUE);
}


/**************************************************************************************/
// recover_onpath_conf: called from bp_recover_op in bp.c

void recover_onpath_conf() {
  uns ii;
  uns count = 0;

  _DEBUG(0, DEBUG_ONPATH_CONF, "recovering: op:%s\n",
         unsstr64(bp_recovery_info->recovery_op_num));

  // reposition the head index, and recompute count
  ii = bpc_data->tail;
  while(ii != bpc_data->head) {
    if(bpc_data->opc_table[ii].mispred || bpc_data->opc_table[ii].off_path)
      break;
    count++;
    ii = CIRC_INC(ii, OPC_SIZE);
  }
  bpc_data->head  = ii;
  bpc_data->count = count;

  print_onpath_conf();

  g_bp_data->on_path_pred = compute_onpath_conf(TRUE);
}


/**************************************************************************************/
// compute_onpath_conf:
// 1: onpath
// 0: offpath

static Flag compute_onpath_conf(Flag include_last) {
  Flag pred_onpath;
  uns  count;
  uns  ii  = bpc_data->tail;
  Flag set = FALSE;

  _DEBUG(0, DEBUG_ONPATH_CONF, "compute_onpath_conf:\n");

  // AND bits in confidence bit vector
  while(ii != bpc_data->head) {
    if(!bpc_data->opc_table[ii].pred_conf) {
      set = TRUE;
      if(CIRC_INC(ii, OPC_SIZE) == bpc_data->head && !include_last)
        pred_onpath = TRUE;
      else
        pred_onpath = FALSE;
      STAT_EVENT(bpc_data->proc_id,
                 FIRST_ONE_MIS + (pred_onpath != ic->off_path));
      break;
    }
    ii = CIRC_INC(ii, OPC_SIZE);
  }
  // if we get here, we think we are on-path
  if(!set) {
    pred_onpath = TRUE;
    STAT_EVENT(bpc_data->proc_id, ALL_ONES_MIS + (pred_onpath != ic->off_path));
  }

  count = count_zeros(bpc_data->tail, bpc_data->head);

  // {{{ stats
  STAT_EVENT(bpc_data->proc_id, OPC_LENGTH_0_7_MIS +
                                  2 * MIN2(bpc_data->count >> 3, 10) +
                                  (pred_onpath != ic->off_path));

  STAT_EVENT(bpc_data->proc_id, ZEROS_0_1_MIS + 2 * MIN2(count >> 1, 8) +
                                  (pred_onpath != ic->off_path));
  // }}}

  if(bpc_data->count > 128) {
    if(ic->off_path)
      STAT_EVENT(bpc_data->proc_id, LONG_OVWT_MIS);
    else
      STAT_EVENT(bpc_data->proc_id, LONG_OVWT_COR);
    pred_onpath = FALSE;
  }

  return pred_onpath;
}


/**************************************************************************************/
// count_zeros:

static uns count_zeros(uns tail, uns head) {
  uns ii;
  uns count = 0;

  for(ii = tail; ii != head; ii = CIRC_INC(ii, OPC_SIZE))
    if(!bpc_data->opc_table[ii].pred_conf)
      count++;
  return count;
}


/**************************************************************************************/
// print_onpath_conf:

static void print_onpath_conf() {
  uns        ii = bpc_data->tail;
  Opc_Table* opc_table;

  _DEBUG(bpc_data->proc_id, DEBUG_ONPATH_CONF,
         "tail:%u(op:%llu) head:%u count:%u\n", bpc_data->tail,
         bpc_data->opc_table[bpc_data->tail].op_num, bpc_data->head,
         bpc_data->count);

  if(ENABLE_GLOBAL_DEBUG_PRINT && DEBUG_RANGE_COND(0) &&
     DEBUG_ONPATH_CONF) {  // QUESTION use debug_macro
    while(ii != bpc_data->head) {
      opc_table = &bpc_data->opc_table[ii];
      printf("%d", opc_table->pred_conf);
      ii = CIRC_INC(ii, OPC_SIZE);
    }
    printf("\n");
  }
}


/**************************************************************************************/
// read_conf_head:

uns read_conf_head() {
  ASSERT(0, ENABLE_BP_CONF);
  return bpc_data->head;
}


  /**************************************************************************************/
  /* Akkary, Haitham, et al. "Perceptron-based branch confidence estimation."
   * 10th International Symposium on High Performance Computer Architecture
   * (HPCA'04). IEEE, 2004.*/
  /**************************************************************************************/
  /* init conf_percentron */

#define CONF_PERCEPTRON_INIT_VALUE 0
/* bp_perceptron_init: */

void conf_perceptron_init(void) {
  uns ii;
  percep_bpc_data          = (PERCEP_Bpc_Data*)malloc(sizeof(PERCEP_Bpc_Data));
  percep_bpc_data->conf_pt = (Perceptron*)malloc(sizeof(Perceptron) *
                                                 (CONF_PERCEPTRON_ENTRIES));
  for(ii = 0; ii < CONF_PERCEPTRON_ENTRIES; ii++) {
    uns jj;
    percep_bpc_data->conf_pt[ii].weights = (int32*)malloc(
      sizeof(int32) * (CONF_HIST_LENGTH + 1));
    for(jj = 0; jj < (CONF_HIST_LENGTH + 1); jj++) {
      percep_bpc_data->conf_pt[ii].weights[jj] = CONF_PERCEPTRON_INIT_VALUE;
    }
  }
}


  /**************************************************************************************/
  /* bp_perceptron_pred: */
  // To increase the length of history change these things
  // uns32 hist
  // uns32 mask
  // mask=1<<31

#define CONF_PERCEPTRON_HASH(addr) (addr % CONF_PERCEPTRON_ENTRIES)
#define PERCEPTRON_HIS(hist, misp_hist)                        \
  (((hist) >> PERCEPTRON_CONF_HIS_BOTH_LENGTH) |               \
   ((((misp_hist) >> (64 - PERCEPTRON_CONF_HIS_BOTH_LENGTH)) & \
     N_BIT_MASK(PERCEPTRON_CONF_HIS_BOTH_LENGTH))              \
    << (64 - PERCEPTRON_CONF_HIS_BOTH_LENGTH)))

void conf_perceptron_pred(Op* op) {
  Addr        addr      = op->inst_info->addr;
  uns64       hist      = 0;
  uns32       index     = CONF_PERCEPTRON_HASH(addr);
  uns8        pred_conf = 0;
  Flag        mispred   = op->oracle_info.mispred | op->oracle_info.misfetch;
  int32       output    = 0;
  uns         ii;
  uns64       mask;
  Perceptron* p;
  int32*      w;
  int         x_i;


  hist = percep_bpc_data->conf_perceptron_global_hist;

  if(PERCEPTRON_CONF_HIS_BOTH) {
    hist = PERCEPTRON_HIS(percep_bpc_data->conf_perceptron_global_hist,
                          percep_bpc_data->conf_perceptron_global_misp_hist);
  }


  /* get pointers to that perceptron and its weights */
  p = &(percep_bpc_data->conf_pt[index]);
  w = &(p->weights[0]);

  /* initialize the output to the bias weight, and bump the pointer
   * to the weights
   */

  output = *(w++);

  /* find the (rest of the) dot product of the history register
   * and the perceptron weights.  note that, instead of actually
   * doing the expensive multiplies, we simply add a weight when the
   * corresponding branch in the history register is taken, or
   * subtract a weight when the branch is not taken.  this also lets
   * us use binary instead of bipolar logic to represent the history
   * register
   */
  for(mask = ((uns64)1) << 63, ii = 0; ii < CONF_HIST_LENGTH;
      ii++, mask >>= 1, w++) {
    if(!!(hist & mask))
      output += *w;
    else
      output += -(*w);
  }

  /* record the various values needed to update the predictor */
  /* output < th :: high confidence */
  /* output > th : low confidence */
  /* output = 0 : low confidence output=1: high confidence  */
  /* output < th : 1 output > th 0 */

  pred_conf = (output < CONF_PERCEPTRON_TH) ? 1 : 0;

  if(PERCEPTRON_CONF_TRAIN_CONF) {
    if(output > CONF_PERCEPTRON_TH)
      pred_conf = 0;  // low confidenct
    else
      pred_conf = 1;  // high confident
  }

  if(PERCEPTRON_CONF_TRAIN_HIS) {
    if((output < CONF_PERCEPTRON_TH) && (output > -CONF_PERCEPTRON_TH))
      pred_conf = 0;  // low confidenct
    else
      pred_conf = 1;  // high confident
  }

  _DEBUG(0, DEBUG_BP_CONF,
         "index:%d hist:%s output:%d conf_th:%d pred_conf:%d bp_pred:%d \n",
         index, hexstr64(hist), output, CONF_PERCEPTRON_TH, pred_conf,
         op->oracle_info.mispred);

  x_i = op->oracle_info.dir ? 1 : -1;

  op->oracle_info.pred_conf_perceptron_global_hist =
    percep_bpc_data->conf_perceptron_global_hist;
  percep_bpc_data->conf_perceptron_global_hist >>= 1;
  percep_bpc_data->conf_perceptron_global_misp_hist >>= 1;

  if(PERCEPTRON_CONF_USE_CONF) {
    // mispred x_i = 1, correct pred: 0
    if((op->oracle_info.mispred && !pred_conf) ||
       (!(op->oracle_info.mispred) && pred_conf))
      x_i = 0;
    else
      x_i = 1;

    op->recovery_info.conf_perceptron_global_hist =
      (percep_bpc_data->conf_perceptron_global_hist) | (((uns64)x_i) << 63);
    percep_bpc_data->conf_perceptron_global_hist |= (((uns64)x_i) << 63);
  } else {
    op->recovery_info.conf_perceptron_global_hist =
      (percep_bpc_data->conf_perceptron_global_hist) |
      (((uns64)op->oracle_info.dir) << 63);

    percep_bpc_data->conf_perceptron_global_hist |=
      (((uns64)(op->oracle_info.dir)) << 63);

    op->recovery_info.conf_perceptron_global_misp_hist =
      (percep_bpc_data->conf_perceptron_global_misp_hist) |
      ((uns64)(op->oracle_info.mispred) << 63);

    percep_bpc_data->conf_perceptron_global_misp_hist |=
      (((uns64)(op->oracle_info.mispred)) << 63);
  }


  op->conf_perceptron_output = output;
  op->oracle_info.pred_conf  = pred_conf;

  STAT_EVENT(op->proc_id, BP_ON_PATH_CONF_MISPRED + 2 * op->off_path +
                            (pred_conf != mispred));
  STAT_EVENT(op->proc_id, BP_ON_PATH_PRED_MIS_CONF_MISPRED + 4 * op->off_path +
                            2 * pred_conf + (pred_conf != mispred));
}

  /**************************************************************************************/
  /* bp_perceptron_update: */

#define CONF_PERCEPTRON_THRESHOLD                                       \
  (int32)((CONF_PERCEPTRON_THRESH_OVRD) ? CONF_PERCEPTRON_THRESH_OVRD : \
                                          (1.93 * (CONF_HIST_LENGTH) + 14))
#define MAX_WEIGHT ((1 << (CONF_PERCEPTRON_CTR_BITS - 1)) - 1)
#define MIN_WEIGHT (-(MAX_WEIGHT + 1))

void conf_perceptron_update(Op* op) {
  Addr   addr   = op->inst_info->addr;
  uns64  hist   = 0;
  uns32  index  = CONF_PERCEPTRON_HASH(addr);
  int32  output = op->conf_perceptron_output;
  int    y;
  uns64  mask;
  int32* w;
  uns    ii = 0;


  int p;  // p = 1: branch is incorrectly predicted , p = -1: branch correctly
          // predicted
  int c;  // c = 1 : low confidence , c = -1: high confidence

  if(op->oracle_info.mispred)
    p = 1;
  else
    p = -1;

  if(op->oracle_info.pred_conf)
    c = -1;  // high confidnece
  else
    c = 1;  // low confidence

  w = &(percep_bpc_data->conf_pt[index].weights[0]);

  // overwrite his
  hist = op->oracle_info.pred_conf_perceptron_global_hist;

  if(PERCEPTRON_CONF_HIS_BOTH) {
    hist = PERCEPTRON_HIS(op->oracle_info.pred_conf_perceptron_global_hist,
                          op->recovery_info.conf_perceptron_global_misp_hist);
  }

  /* if the output of the perceptron predictor is outside of
   * the range [-THETA,THETA] *and* the prediction was correct,
   * then we don't need to adjust the weights
   */

  if(output > CONF_PERCEPTRON_THRESHOLD)
    y = 1;
  else if(output < -CONF_PERCEPTRON_THRESHOLD)
    y = 0;
  else
    y = 2;

  if(PERCEPTRON_CONF_TRAIN_HIS) {
    int old_w;
    old_w = *w;
    UNUSED(old_w);
    if(op->oracle_info.dir)
      (*w)++;
    else
      (*w)--;
    if(*w > MAX_WEIGHT)
      *w = MAX_WEIGHT;
    if(*w < MIN_WEIGHT)
      *w = MIN_WEIGHT;

    _DEBUG(0, DEBUG_BP_CONF,
           "index:%d *w[%d] :%d->%d  p:%d c:%d bp_mis_pred:%d conf:%d y:%d \n",
           index, ii, old_w, *w, p, c, op->oracle_info.mispred,
           op->oracle_info.pred_conf, y);

    w++;

    for(mask = ((uns64)1) << 63, ii = 0; ii < CONF_HIST_LENGTH;
        ii++, mask >>= 1, w++) {
      /* if the i'th bit in the history positively correlates
       * with this branch outcome, increment the corresponding
       * weight, else decrement it, with saturating arithmetic
       */
      int old_w;
      old_w = *w;
      UNUSED(old_w);
      if(!!(hist & mask) == op->oracle_info.dir) {
        (*w)++;
        if(*w > MAX_WEIGHT)
          *w = MAX_WEIGHT;
      } else {
        (*w)--;
        if(*w < MIN_WEIGHT)
          *w = MIN_WEIGHT;
      }
      _DEBUG(
        0, DEBUG_BP_CONF,
        "index:%d *w[%d] :%d->%d  p:%d c:%d  bp_mis_pred:%d conf:%d y:%d \n",
        index, ii, old_w, *w, p, c, op->oracle_info.mispred,
        op->oracle_info.pred_conf, y);
    }
    return;
  }

  if(PERCEPTRON_CONF_TRAIN_CONF) {
    int old_w;
    old_w = *w;
    UNUSED(old_w);

    if(PERCEPTRON_CONF_TRAIN_OFFSET_CONF) {
      if(p)
        (*w) = (*w) + PERCEPTRON_TRAIN_MISP_FACTOR;
      else
        (*w) = (*w) - PERCEPTRON_TRAIN_CORR_FACTOR;
    } else {
      if(op->oracle_info.dir)
        (*w)++;
      else
        (*w)--;
    }

    if(*w > MAX_WEIGHT)
      *w = MAX_WEIGHT;
    if(*w < MIN_WEIGHT)
      *w = MIN_WEIGHT;

    _DEBUG(0, DEBUG_BP_CONF,
           "index:%d *w[%d] :%d->%d  p:%d c:%d bp_mis_pred:%d conf:%d y:%d \n",
           index, ii, old_w, *w, p, c, op->oracle_info.mispred,
           op->oracle_info.pred_conf, y);

    w++;
    if((y == 2) || (c != p)) {
      for(mask = ((uns64)1) << 63, ii = 0; ii < CONF_HIST_LENGTH;
          ii++, mask >>= 1, w++) {
        /* if the i'th bit in the history positively correlates
         * with this branch outcome, increment the corresponding
         * weight, else decrement it, with saturating arithmetic
         */
        int old_w;
        old_w = *w;
        UNUSED(old_w);

        if(p == 1) {
          // mispredicted so increase the weight vector values
          if(!!(hist & mask)) {
            (*w) = (*w) + PERCEPTRON_TRAIN_MISP_FACTOR;
            if(*w > MAX_WEIGHT)
              *w = MAX_WEIGHT;
          } else {
            (*w) = (*w) - PERCEPTRON_TRAIN_MISP_FACTOR;
            if(*w < MIN_WEIGHT)
              *w = MIN_WEIGHT;
          }
        } else {
          // no mispredicted so decrease the weight vector values
          if(!!(hist & mask)) {
            (*w) = (*w) - PERCEPTRON_TRAIN_CORR_FACTOR;

            if(*w < MAX_WEIGHT)
              *w = MAX_WEIGHT;
          } else {
            (*w) = (*w) + PERCEPTRON_TRAIN_CORR_FACTOR;
            if(*w > MIN_WEIGHT)
              *w = MIN_WEIGHT;
          }
        }
        _DEBUG(
          0, DEBUG_BP_CONF,
          "index:%d *w[%d] :%d->%d  p:%d c:%d  bp_mis_pred:%d conf:%d y:%d \n",
          index, ii, old_w, *w, p, c, op->oracle_info.mispred,
          op->oracle_info.pred_conf, y);
      }
    }
    return;
  }

  // akary's paper original paper
  if((y == 2) || (c != p)) {
    // change the counter value
    for(mask = ((uns64)1) << 63, ii = 0; ii < CONF_HIST_LENGTH;
        ii++, mask >>= 1, w++) {
      int x_i = !!(hist & mask);
      int old_w;
      old_w = *w;
      UNUSED(old_w);
      if(PERCEPTRON_CONF_USE_CONF) {
        // mispred x_i = 1, correct pred: 0
        if(x_i == 0)
          x_i = -1;
        // fix thise x_i mispredicted and c!=p then increament the counter
        if(x_i == p)
          (*w)++;
        else
          (*w)--;
      } else {
        if(x_i == 0)
          x_i = -1;  // not taken is -1 in the paper
        (*w) = (*w) + p * x_i;
      }

      if(*w > MAX_WEIGHT)
        *w = MAX_WEIGHT;
      if(*w < MIN_WEIGHT)
        *w = MIN_WEIGHT;

      _DEBUG(0, DEBUG_BP_CONF,
             "index:%d *w[%d] :%d->%d  p:%d c:%d x_i:%d bp_mis_pred:%d conf:%d "
             "y:%d \n",
             index, ii, old_w, *w, p, c, x_i, op->oracle_info.mispred,
             op->oracle_info.pred_conf, y);
    }
  }
}
