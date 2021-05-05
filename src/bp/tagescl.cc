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

#include "tagescl.h"

#include <iostream>
#include <memory>

extern "C" {
#include "core.param.h"
#include "bp.param.h"
#include "globals/assert.h"
#include "table_info.h"
}

#include "bp/template_lib/tagescl.h"

namespace {
// A vector of TAGE-SC-L tables. One table per core.
std::vector<std::unique_ptr<Tage_SC_L_Base>> tagescl_predictors;

// Helper function for producing a Branch_Type struct.
Branch_Type get_branch_type(uns proc_id, Cf_Type cf_type) {
  Branch_Type br_type;
  switch(cf_type) {
    case CF_BR:
    case CF_CALL:
      br_type.is_conditional = false;
      br_type.is_indirect    = false;
      break;
    case CF_CBR:
      br_type.is_conditional = true;
      br_type.is_indirect    = false;
      break;
    case CF_IBR:
    case CF_ICALL:
    case CF_ICO:
    case CF_RET:
    case CF_SYS:
      br_type.is_conditional = false;
      br_type.is_indirect    = true;
      break;
    default:
      // Should never see non-control flow instructions or invalid CF
      // types in the branch predictor.
      ASSERT(proc_id, false);
      break;
  }
  return br_type;
}
}  // end of anonymous namespace

void bp_tagescl_init() {
  if(tagescl_predictors.size() == 0) {
    tagescl_predictors.reserve(NUM_CORES);
    for(uns i = 0; i < NUM_CORES; ++i) {
      if (BP_MECH == TAGESCL_BP) {
        tagescl_predictors.push_back(std::make_unique<Tage_SC_L<TAGE_SC_L_CONFIG_64KB>>(NODE_TABLE_SIZE));
      } else {
        tagescl_predictors.push_back(std::make_unique<Tage_SC_L<TAGE_SC_L_CONFIG_80KB>>(NODE_TABLE_SIZE));
      }
    }
  }
  ASSERTM(0, tagescl_predictors.size() == NUM_CORES,
          "tagescl_predictors not initialized correctly");
}

void bp_tagescl_timestamp(Op* op) {
  uns proc_id = op->proc_id;
  op->recovery_info.branch_id =
    tagescl_predictors.at(proc_id)->get_new_branch_id();
}

uns8 bp_tagescl_pred(Op* op) {
  uns proc_id = op->proc_id;
  return tagescl_predictors.at(proc_id)->get_prediction(
    op->recovery_info.branch_id, op->inst_info->addr);
}

void bp_tagescl_spec_update(Op* op) {
  uns proc_id = op->proc_id;
  tagescl_predictors.at(proc_id)->update_speculative_state(
    op->recovery_info.branch_id, op->inst_info->addr,
    get_branch_type(proc_id, op->table_info->cf_type), op->oracle_info.pred,
    op->oracle_info.target);
}

void bp_tagescl_update(Op* op) {
  uns proc_id = op->proc_id;
  tagescl_predictors.at(proc_id)->commit_state(
    op->recovery_info.branch_id, op->inst_info->addr,
    get_branch_type(proc_id, op->table_info->cf_type), op->oracle_info.dir);
}

void bp_tagescl_retire(Op* op) {
  uns proc_id = op->proc_id;
  tagescl_predictors.at(proc_id)->commit_state_at_retire(
    op->recovery_info.branch_id, op->inst_info->addr,
    get_branch_type(proc_id, op->table_info->cf_type), op->oracle_info.dir,
    op->oracle_info.target);
}

void bp_tagescl_recover(Recovery_Info* recovery_info) {
  uns proc_id = recovery_info->proc_id;
  tagescl_predictors.at(proc_id)->flush_branch_and_repair_state(
    recovery_info->branch_id, recovery_info->PC,
    get_branch_type(proc_id, recovery_info->cf_type), recovery_info->new_dir,
    recovery_info->branchTarget);
}
