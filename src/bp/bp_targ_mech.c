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
 * File         : bp_targ_mech.c
 * Author       : HPS Research Group
 * Date         : 12/9/1998
 * Description  :
 ***************************************************************************************/

#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"
#include "isa/isa_macros.h"

#include "bp/bp.h"
#include "bp/bp_targ_mech.h"
#include "libs/cache_lib.h"

#include "bp/bp.param.h"
#include "core.param.h"
#include "debug/debug.param.h"
#include "statistics.h"


/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_BP, ##args)
#define DEBUG_CRS(proc_id, args...) _DEBUG(proc_id, DEBUG_CRS, ##args)
#define DEBUGU_CRS(proc_id, args...) _DEBUGU(proc_id, DEBUG_CRS, ##args)
#define DEBUG_BTB(proc_id, args...) _DEBUG(proc_id, DEBUG_BTB, ##args)


/**************************************************************************************/
/* bp_crs_push: */

void bp_crs_push(Bp_Data* bp_data, Op* op) {
  Addr addr = ADDR_PLUS_OFFSET(op->inst_info->addr,
                               op->inst_info->trace_info.inst_size);

  Flag       flag = op->off_path;
  Crs_Entry* ent  = &bp_data->crs.entries[bp_data->crs.tail << 1 | flag];

  ASSERT(bp_data->proc_id, bp_data->proc_id == op->proc_id);

  ent->addr                                = addr;
  ent->op_num                              = op->op_num;
  bp_data->crs.off_path[bp_data->crs.tail] = op->off_path;
  bp_data->crs.tail = CIRC_INC2(bp_data->crs.tail, CRS_ENTRIES);

  if(bp_data->crs.depth == CRS_ENTRIES) {
    bp_data->crs.head = CIRC_INC2(bp_data->crs.head, CRS_ENTRIES);
    DEBUG_CRS(bp_data->proc_id, "CLOBBER    head:%d  tail:%d\n",
              bp_data->crs.head, bp_data->crs.tail_save);
    STAT_EVENT(bp_data->proc_id, CRS_CLOBBER);
  } else {
    bp_data->crs.depth++;
    ASSERTM(bp_data->proc_id, bp_data->crs.depth <= CRS_ENTRIES,
            "bp_data->crs_depth:%d\n", bp_data->crs.depth);
  }

  if(!op->off_path) {
    bp_data->crs.tail_save  = bp_data->crs.tail;
    bp_data->crs.depth_save = bp_data->crs.depth;
  }

  DEBUG_CRS(bp_data->proc_id,
            "PUSH       head:%d  tail:%d  depth:%d  op:%s  addr:0x%s  type:%s  "
            "offpath:%d\n",
            bp_data->crs.head, bp_data->crs.tail, bp_data->crs.depth,
            unsstr64(op->op_num), hexstr64s(addr),
            cf_type_names[op->table_info->cf_type], op->off_path);
}


/**************************************************************************************/
/* bp_crs_pop: */

Addr bp_crs_pop(Bp_Data* bp_data, Op* op) {
  uns  new_tail = CIRC_DEC2(bp_data->crs.tail, CRS_ENTRIES);
  Flag flag     = bp_data->crs.off_path[new_tail];
  Addr addr     = bp_data->crs.entries[new_tail << 1 | flag].addr;
  Flag mispred;

  ASSERT(bp_data->proc_id, bp_data->proc_id == op->proc_id);
  if(bp_data->crs.depth == 0) {
    DEBUG_CRS(bp_data->proc_id, "UNDERFLOW  head:%d  tail:%d  offpath:%d\n",
              bp_data->crs.head, bp_data->crs.tail, op->off_path);
    STAT_EVENT(op->proc_id, CRS_MISS_ON_PATH + PERFECT_CRS + 2 * op->off_path);
    return PERFECT_CRS ? op->oracle_info.target :
                         convert_to_cmp_addr(bp_data->proc_id, 0);
  }
  bp_data->crs.tail = new_tail;
  bp_data->crs.depth--;
  ASSERT(bp_data->proc_id, bp_data->crs.depth >= 0);
  if(!op->off_path) {
    if(addr != op->oracle_info.npc)
      DEBUG_CRS(bp_data->proc_id, "MISS       addr:0x%s  true:0x%s\n",
                hexstr64s(addr), hexstr64s(op->oracle_info.npc));
    bp_data->crs.tail_save  = bp_data->crs.tail;
    bp_data->crs.depth_save = bp_data->crs.depth;
  }

  DEBUG_CRS(
    bp_data->proc_id,
    "POP        head:%d  tail:%d  depth:%d  op:%s  addr:0x%s  type:%s  "
    "offpath:%d  true:0x%s  miss:%d\n",
    bp_data->crs.head, bp_data->crs.tail, bp_data->crs.depth,
    unsstr64(bp_data->crs.entries[bp_data->crs.tail << 1 | flag].op_num),
    hexstr64s(addr), cf_type_names[op->table_info->cf_type], op->off_path,
    hexstr64s(op->oracle_info.npc), addr != op->oracle_info.npc);
  mispred = PERFECT_CRS ? 0 : addr != op->oracle_info.npc;
  STAT_EVENT(op->proc_id, CRS_MISS_ON_PATH + !mispred + 2 * op->off_path);
  return PERFECT_CRS ? op->oracle_info.target : addr;
}


/**************************************************************************************/
/* bp_crs_recover: */

void bp_crs_recover(Bp_Data* bp_data) {
  uns8 ii;
  for(ii = 0; ii < CRS_ENTRIES; ii++)
    bp_data->crs.off_path[ii] = FALSE;
  bp_data->crs.tail  = bp_data->crs.tail_save;
  bp_data->crs.depth = bp_data->crs.depth_save;
  DEBUG_CRS(bp_data->proc_id, "RECOVER    head:%d  tail:%d  depth:%d\n",
            bp_data->crs.head, bp_data->crs.tail_save, bp_data->crs.depth);
}

/**************************************************************************************/
/* bp_crs_realistic_push: */

void bp_crs_realistic_push(Bp_Data* bp_data, Op* op) {
  Addr       addr = ADDR_PLUS_OFFSET(op->inst_info->addr,
                               op->inst_info->trace_info.inst_size);
  Crs_Entry* ent  = &bp_data->crs.entries[bp_data->crs.next];

  ASSERT(bp_data->proc_id, bp_data->proc_id == op->proc_id);

  ent->addr                                = addr;
  ent->op_num                              = op->op_num;
  ent->nos                                 = bp_data->crs.tos;
  bp_data->crs.off_path[bp_data->crs.next] = op->off_path;
  bp_data->crs.tos                         = bp_data->crs.next;
  bp_data->crs.next = CIRC_INC2(bp_data->crs.next, CRS_ENTRIES);

  if(bp_data->crs.depth == CRS_ENTRIES) {
    DEBUG_CRS(bp_data->proc_id, "CLOBBER    next:%d  tos:%d  depth:%d\n",
              bp_data->crs.next, bp_data->crs.tos, bp_data->crs.depth);
    STAT_EVENT(bp_data->proc_id, CRS_CLOBBER);
  } else {
    bp_data->crs.depth++;
    ASSERTM(bp_data->proc_id, bp_data->crs.depth <= CRS_ENTRIES,
            "bp_data->crs_depth:%d\n", bp_data->crs.depth);
  }

  op->recovery_info.crs_next  = bp_data->crs.next;
  op->recovery_info.crs_tos   = bp_data->crs.tos;
  op->recovery_info.crs_depth = bp_data->crs.depth;

  DEBUG_CRS(bp_data->proc_id,
            "PUSH       next:%d  tos:%d  depth:%d  op:%s  addr:0x%s  type:%s  "
            "offpath:%d\n",
            bp_data->crs.next, bp_data->crs.tos, bp_data->crs.depth,
            unsstr64(op->op_num), hexstr64s(addr),
            cf_type_names[op->table_info->cf_type], op->off_path);
}


/**************************************************************************************/
/* bp_crs_realistic_pop: */

Addr bp_crs_realistic_pop(Bp_Data* bp_data, Op* op) {
  uns  new_next = CIRC_DEC2(bp_data->crs.next, CRS_ENTRIES);
  uns  old_tos  = bp_data->crs.tos;
  Addr addr;
  uns  new_tos = bp_data->crs.entries[bp_data->crs.tos].nos;
  Flag mispred;

  UNUSED(old_tos);
  ASSERT(bp_data->proc_id, bp_data->proc_id == op->proc_id);

  switch(CRS_REALISTIC) {
    case 1:
      addr = bp_data->crs.entries[bp_data->crs.tos].addr;
      break;
    case 2:
      addr = bp_data->crs.entries[new_next].addr;
      break;
    default:
      old_tos = 0;
      ASSERT(bp_data->proc_id, 0);  // old_tos is messed with because of the
                                    // stupid compiler warning about unused
                                    // variables
  }

  if(bp_data->crs.depth == 0) {
    DEBUG_CRS(bp_data->proc_id, "UNDERFLOW  next:%d  tos: %d  offpath:%d\n",
              bp_data->crs.next, bp_data->crs.tos, op->off_path);
    STAT_EVENT(op->proc_id, CRS_MISS_ON_PATH + PERFECT_CRS + 2 * op->off_path);
    return PERFECT_CRS ? op->oracle_info.target :
                         convert_to_cmp_addr(bp_data->proc_id, 0);
  }

  if(CRS_REALISTIC == 2)
    bp_data->crs.next = new_next;
  bp_data->crs.depth--;
  ASSERT(bp_data->proc_id, bp_data->crs.depth >= 0);
  bp_data->crs.tos = new_tos;

  if(addr != op->oracle_info.npc)
    DEBUG_CRS(bp_data->proc_id, "MISS       addr:0x%s  true:0x%s\n",
              hexstr64s(addr), hexstr64s(op->oracle_info.npc));

  op->recovery_info.crs_next  = bp_data->crs.next;
  op->recovery_info.crs_tos   = bp_data->crs.tos;
  op->recovery_info.crs_depth = bp_data->crs.depth;

  DEBUG_CRS(bp_data->proc_id,
            "POP        next:%d  tos:%d  depth:%d  old_tos:%d  op:%s  "
            "addr:0x%s  type:%s  offpath:%d  true:0x%s  miss:%d\n",
            bp_data->crs.next, bp_data->crs.tos, bp_data->crs.depth, old_tos,
            unsstr64(bp_data->crs.entries[old_tos].op_num), hexstr64s(addr),
            cf_type_names[op->table_info->cf_type], op->off_path,
            hexstr64s(op->oracle_info.npc), addr != op->oracle_info.npc);
  mispred = PERFECT_CRS ? 0 : addr != op->oracle_info.npc;
  STAT_EVENT(op->proc_id, CRS_MISS_ON_PATH + !mispred + 2 * op->off_path);
  return PERFECT_CRS ? op->oracle_info.target : addr;
}


/**************************************************************************************/
/* bp_crs_realistic_recover: */

void bp_crs_realistic_recover(Bp_Data* bp_data, Recovery_Info* info) {
  bp_data->crs.next  = info->crs_next;
  bp_data->crs.depth = info->crs_depth;
  bp_data->crs.tos   = info->crs_tos;
  DEBUG_CRS(bp_data->proc_id, "RECOVER    next:%d  tos:%d  depth:%d\n",
            bp_data->crs.next, bp_data->crs.tos, bp_data->crs.depth);
}


/**************************************************************************************/
/* bp_btb_init: */

void bp_btb_gen_init(Bp_Data* bp_data) {
  // btb line size set to 1
  init_cache(&bp_data->btb, "BTB", BTB_ENTRIES, BTB_ASSOC, 1, sizeof(Addr),
             REPL_TRUE_LRU);
}


/**************************************************************************************/
/* bp_btb_gen_pred: */

Addr* bp_btb_gen_pred(Bp_Data* bp_data, Op* op) {
  Addr line_addr;

  return PERFECT_BTB ?
           &op->oracle_info.target :
           (Addr*)cache_access(&bp_data->btb, op->oracle_info.pred_addr,
                               &line_addr, TRUE);
}


/**************************************************************************************/
/* bp_btb_gen_update: */

void bp_btb_gen_update(Bp_Data* bp_data, Op* op) {
  Addr  fetch_addr = op->oracle_info.pred_addr;
  Addr *btb_line, btb_line_addr, repl_line_addr;

  ASSERT(bp_data->proc_id, bp_data->proc_id == op->proc_id);
  if(BTB_OFF_PATH_WRITES || !op->off_path) {
    DEBUG_BTB(bp_data->proc_id, "Writing BTB  addr:0x%s  target:0x%s\n",
              hexstr64s(fetch_addr), hexstr64s(op->oracle_info.target));
    STAT_EVENT(op->proc_id, BTB_ON_PATH_WRITE + op->off_path);
    btb_line  = (Addr*)cache_insert(&bp_data->btb, bp_data->proc_id, fetch_addr,
                                   &btb_line_addr, &repl_line_addr);
    *btb_line = op->oracle_info.target;
    // FIXME: the exceptions to this assert are really about x86 vs Alpha
    ASSERT(bp_data->proc_id, (fetch_addr == btb_line_addr) || TRUE);
  }
}


/**************************************************************************************/
/* bp_tc_tagged_init: */

void bp_ibtb_tc_tagged_init(Bp_Data* bp_data) {
  // line size set to 1
  init_cache(&bp_data->tc_tagged, "TC", TC_ENTRIES, TC_ASSOC, 1, sizeof(Addr),
             REPL_TRUE_LRU);
}


/**************************************************************************************/
/* bp_tc_tagged_pred: */

Addr bp_ibtb_tc_tagged_pred(Bp_Data* bp_data, Op* op) {
  Addr  addr;
  uns32 hist;
  uns32 tc_index;
  Addr* tc_entry;
  Addr  line_addr;
  Addr  target;

  if(PERFECT_IBP)
    return op->oracle_info.target;

  /* branch history can be updated in one of two ways */
  /* 1. branch history (USE_PAT_HIST) */
  /* 2. path history */
  if(USE_PAT_HIST) {
    addr               = op->oracle_info.pred_addr;
    bp_data->targ_hist = bp_data->global_hist; /* use global history from
                                                  conditional branches */
    hist                           = bp_data->targ_hist;
    op->oracle_info.pred_targ_hist = bp_data->targ_hist;
    op->recovery_info.targ_hist    = bp_data->targ_hist;
  } else {
    addr                           = op->oracle_info.pred_addr;
    hist                           = bp_data->targ_hist;
    op->oracle_info.pred_targ_hist = bp_data->targ_hist;
    bp_data->targ_hist >>= bp_data->target_bit_length;
    op->recovery_info.targ_hist = bp_data->targ_hist |
                                  (op->oracle_info.target >> 2 &
                                   N_BIT_MASK(bp_data->target_bit_length)
                                     << (32 - bp_data->target_bit_length));
    bp_data->targ_hist |= op->oracle_info.target >> 2 &
                          N_BIT_MASK(bp_data->target_bit_length)
                            << (32 - bp_data->target_bit_length);
  }
  tc_index = hist ^ addr;
  if(IBTB_HASH_TOS)
    tc_index = tc_index ^ op->recovery_info.tos_addr;
  tc_entry = (Addr*)cache_access(&bp_data->tc_tagged, tc_index, &line_addr,
                                 TRUE);

  if(tc_entry)
    target = *tc_entry;
  else
    target = 0;

  if(!op->off_path)
    STAT_EVENT(op->proc_id,
               TARG_ON_PATH_MISS + (target == op->oracle_info.npc));
  else
    STAT_EVENT(op->proc_id,
               TARG_OFF_PATH_MISS + (target == op->oracle_info.npc));

  return target;
}


/**************************************************************************************/
/* bp_tc_tagged_update: */

void bp_ibtb_tc_tagged_update(Bp_Data* bp_data, Op* op) {
  Addr  addr     = op->oracle_info.pred_addr;
  uns32 hist     = op->oracle_info.pred_targ_hist;
  uns32 tc_index = hist ^ addr;
  Addr* tc_line;
  Addr  tc_line_addr;
  Addr  repl_line_addr;

  if(IBTB_HASH_TOS)
    tc_index = tc_index ^ op->recovery_info.tos_addr;

  DEBUG(bp_data->proc_id, "Writing target cache target for op_num:%s\n",
        unsstr64(op->op_num));
  tc_line = (Addr*)cache_insert(&bp_data->tc_tagged, bp_data->proc_id, tc_index,
                                &tc_line_addr, &repl_line_addr);
  *tc_line = op->oracle_info.target;

  STAT_EVENT(op->proc_id, TARG_ON_PATH_WRITE + op->off_path);
}


/**************************************************************************************/
/* bp_tc_tagged_recover */

void bp_ibtb_tc_tagged_recover(Bp_Data* bp_data, Recovery_Info* info) {
  DEBUG(bp_data->proc_id, "Recovering target cache history\n");
  bp_data->targ_hist = info->targ_hist;
}


/**************************************************************************************/
/* bp_tc_tagless_init */

void bp_ibtb_tc_tagless_init(Bp_Data* bp_data) {
  uns ii;
  bp_data->tc_tagless = (Addr*)malloc(sizeof(Addr) * (0x1 << IBTB_HIST_LENGTH));
  for(ii = 0; ii < 0x1 << IBTB_HIST_LENGTH; ii++)
    bp_data->tc_tagless[ii] = 0;
}


  /**************************************************************************************/
  /* bp_tc_tagless_pred */

#define COOK_HIST_BITS(hist, untouched) \
  ((hist) >> (32 - IBTB_HIST_LENGTH + untouched) << untouched)
#define COOK_ADDR_BITS(addr, addr_shift) \
  (((addr) >> addr_shift) & (N_BIT_MASK(IBTB_HIST_LENGTH)))

Addr bp_ibtb_tc_tagless_pred(Bp_Data* bp_data, Op* op) {
  Addr  addr;
  uns32 hist;
  uns32 cooked_hist;
  uns32 cooked_addr;
  uns32 tc_index;
  Addr  tc_entry;

  if(PERFECT_IBP)
    return op->oracle_info.target;

  /* branch history can be updated in one of two ways */
  /* 1. branch history (USE_PAT_HIST) */
  /* 2. path history */
  if(USE_PAT_HIST) {
    addr               = op->oracle_info.pred_addr;
    bp_data->targ_hist = bp_data->global_hist; /* use global history from
                                                  conditional branches */
    hist                           = bp_data->targ_hist;
    op->oracle_info.pred_targ_hist = bp_data->targ_hist;
    op->recovery_info.targ_hist    = bp_data->targ_hist;
  } else {
    addr                           = op->oracle_info.pred_addr;
    hist                           = bp_data->targ_hist;
    op->oracle_info.pred_targ_hist = bp_data->targ_hist;
    bp_data->targ_hist >>= bp_data->target_bit_length;
    op->recovery_info.targ_hist = bp_data->targ_hist |
                                  (op->oracle_info.target >> 2 &
                                   N_BIT_MASK(bp_data->target_bit_length)
                                     << (32 - bp_data->target_bit_length));
    bp_data->targ_hist |= op->oracle_info.target >> 2 &
                          N_BIT_MASK(bp_data->target_bit_length)
                            << (32 - bp_data->target_bit_length);
  }
  cooked_hist = COOK_HIST_BITS(hist, 0);
  cooked_addr = COOK_ADDR_BITS(addr, 2);
  tc_index    = cooked_hist ^ cooked_addr;
  tc_entry    = bp_data->tc_tagless[tc_index];

  if(IBTB_HASH_TOS) {
    uns32 cooked_tos_addr;
    cooked_tos_addr = COOK_ADDR_BITS(op->recovery_info.tos_addr, 2);
    tc_index        = tc_index ^ cooked_tos_addr;
    tc_entry        = bp_data->tc_tagless[tc_index];
  }

  if(!op->off_path)
    STAT_EVENT(op->proc_id,
               TARG_ON_PATH_MISS + (tc_entry == op->oracle_info.npc));
  else
    STAT_EVENT(op->proc_id,
               TARG_OFF_PATH_MISS + (tc_entry == op->oracle_info.npc));

  return tc_entry;
}


/**************************************************************************************/
/* bp_tc_tagless_update */

void bp_ibtb_tc_tagless_update(Bp_Data* bp_data, Op* op) {
  Addr  addr        = op->oracle_info.pred_addr;
  uns32 hist        = op->oracle_info.pred_targ_hist;
  uns32 cooked_hist = COOK_HIST_BITS(hist, 0);
  uns32 cooked_addr = COOK_ADDR_BITS(addr, 2);
  uns32 tc_index    = cooked_hist ^ cooked_addr;

  if(IBTB_HASH_TOS) {
    uns32 cooked_tos_addr;
    cooked_tos_addr = COOK_ADDR_BITS(op->recovery_info.tos_addr, 2);
    tc_index        = tc_index ^ cooked_tos_addr;
  }

  DEBUG(bp_data->proc_id, "Writing target cache target for op_num:%s\n",
        unsstr64(op->op_num));
  bp_data->tc_tagless[tc_index] = op->oracle_info.target;

  STAT_EVENT(op->proc_id, TARG_ON_PATH_WRITE + op->off_path);
}

/**************************************************************************************/
/* bp_tc_tagless_recover */

void bp_ibtb_tc_tagless_recover(Bp_Data* bp_data, Recovery_Info* info) {
  DEBUG(bp_data->proc_id, "Recovering target cache history\n");
  bp_data->targ_hist = info->targ_hist;
}


typedef enum {
  TC_SELECTOR_TAGLESS_STRONG,  // 0
  TC_SELECTOR_TAGLESS_WEAK,    // 1
  TC_SELECTOR_TAGGED_WEAK,     // 2
  TC_SELECTOR_TAGGED_STRONG    // 3
} Tc_Selector_Table_entry_Value;


/**************************************************************************************/
/* bp_tc_hybrid_init: */

void bp_ibtb_tc_hybrid_init(Bp_Data* bp_data) {
  uns ii;

  /* Init the meta-predictor */
  bp_data->tc_selector = (uns8*)malloc(sizeof(uns8) *
                                       (0x1 << IBTB_HIST_LENGTH));
  for(ii = 0; ii < 0x1 << IBTB_HIST_LENGTH; ii++)
    bp_data->tc_selector[ii] = TC_SELECTOR_TAGLESS_WEAK;

  /* Init the tagless predictor */
  bp_data->tc_tagless = (Addr*)malloc(sizeof(Addr) * (0x1 << IBTB_HIST_LENGTH));
  for(ii = 0; ii < 0x1 << IBTB_HIST_LENGTH; ii++)
    bp_data->tc_tagless[ii] = 0;

  /* Init the tagged predictor */
  // line size set to 1
  init_cache(&bp_data->tc_tagged, "TC", TC_ENTRIES, TC_ASSOC, 1, sizeof(Addr),
             REPL_TRUE_LRU);
}


/**************************************************************************************/
/* bp_tc_hybrid_pred: */

Addr bp_ibtb_tc_hybrid_pred(Bp_Data* bp_data, Op* op) {
  Addr  target;
  Addr  addr        = op->oracle_info.pred_addr;
  uns32 hist        = bp_data->global_hist;
  uns32 cooked_hist = COOK_HIST_BITS(hist, 0);
  uns32 cooked_addr = COOK_ADDR_BITS(addr, 2);
  uns32 sel_index   = cooked_hist ^ cooked_addr;
  uns8  sel_entry   = bp_data->tc_selector[sel_index];

  ASSERT(bp_data->proc_id, bp_data->proc_id == op->proc_id);

  if(IBTB_HASH_TOS) {
    uns32 cooked_tos_addr;
    cooked_tos_addr = COOK_ADDR_BITS(op->recovery_info.tos_addr, 2);
    sel_index       = sel_index ^ cooked_tos_addr;
    sel_entry       = bp_data->tc_selector[sel_index];
  }

  ASSERT(bp_data->proc_id, sel_entry <= TC_SELECTOR_TAGGED_STRONG);

  if(sel_entry >= TC_SELECTOR_TAGGED_WEAK) {
    target = bp_ibtb_tc_tagged_pred(bp_data, op);
  } else {
    target = bp_ibtb_tc_tagless_pred(bp_data, op);
  }

  op->oracle_info.pred_global_hist       = bp_data->global_hist;
  op->oracle_info.pred_tc_selector_entry = sel_entry;

  return target;
}


/**************************************************************************************/
/* bp_tc_hybrid_update: */

void bp_ibtb_tc_hybrid_update(Bp_Data* bp_data, Op* op) {
  Addr  addr             = op->oracle_info.pred_addr;
  uns32 hist             = op->oracle_info.pred_global_hist;
  uns32 cooked_hist      = COOK_HIST_BITS(hist, 0);
  uns32 cooked_addr      = COOK_ADDR_BITS(addr, 2);
  uns32 sel_index        = cooked_hist ^ cooked_addr;
  uns8  sel_entry        = bp_data->tc_selector[sel_index];
  Flag  predicted_tagged = op->oracle_info.pred_tc_selector_entry >=
                          TC_SELECTOR_TAGGED_WEAK;

  ASSERT(bp_data->proc_id, bp_data->proc_id == op->proc_id);

  if(IBTB_HASH_TOS) {
    uns32 cooked_tos_addr;
    cooked_tos_addr = COOK_ADDR_BITS(op->recovery_info.tos_addr, 2);
    sel_index       = sel_index ^ cooked_tos_addr;
    sel_entry       = bp_data->tc_selector[sel_index];
  }

  ASSERT(bp_data->proc_id, !op->oracle_info.mispred);

  if(op->oracle_info.no_target) {  // branch was not predicted at all
    // Update both predictors
    // No change to selector
    bp_ibtb_tc_tagged_update(bp_data, op);
    bp_ibtb_tc_tagless_update(bp_data, op);
    if(!op->off_path)
      STAT_EVENT(op->proc_id, TARG_HYBRID_NO_PRED);
  } else if(op->oracle_info.misfetch) {
    // Update the predictor that made the prediction
    // Change the selector so that it does not use this predictor again
    if(predicted_tagged) {  // predicted by tagged predictor
      bp_data->tc_selector[sel_index] = SAT_DEC(sel_entry, 0);
      bp_ibtb_tc_tagged_update(bp_data, op);
      if(!op->off_path)
        STAT_EVENT(op->proc_id, TARG_HYBRID_MISPRED_TAGGED);
    } else {  // predicted by tagless predictor
      bp_data->tc_selector[sel_index] = SAT_INC(sel_entry,
                                                TC_SELECTOR_TAGGED_STRONG);
      bp_ibtb_tc_tagless_update(bp_data, op);
      if(!op->off_path)
        STAT_EVENT(op->proc_id, TARG_HYBRID_MISPRED_TAGLESS);
    }
  } else {                  // branch was correctly predicted
    if(predicted_tagged) {  // correct pred by tagged predictor
      bp_data->tc_selector[sel_index] = SAT_INC(sel_entry,
                                                TC_SELECTOR_TAGGED_STRONG);
      bp_ibtb_tc_tagged_update(bp_data, op);
      if(!op->off_path)
        STAT_EVENT(op->proc_id, TARG_HYBRID_CORRECT_TAGGED);
    } else {  // correct pred by tagless predictor
      bp_data->tc_selector[sel_index] = SAT_DEC(sel_entry, 0);
      bp_ibtb_tc_tagless_update(bp_data, op);
      if(!op->off_path)
        STAT_EVENT(op->proc_id, TARG_HYBRID_CORRECT_TAGLESS);
    }
  }
}


/**************************************************************************************/
/* bp_tc_hybrid_recover */

void bp_ibtb_tc_hybrid_recover(Bp_Data* bp_data, Recovery_Info* info) {
  DEBUG(bp_data->proc_id, "Recovering target cache history\n");
  bp_data->targ_hist = info->targ_hist;
}
