/**
 * @file cbp_to_scarab.c
 * @author Stephen Pruett (stephen.pruett@utexas.edu)
 * @brief
 * @version 0.1
 * @date 2021-01-12
 *
 */

#include "cbp_to_scarab.h"
#include "mtage_unlimited.h"

#ifdef CBP_PREDICTOR

std::vector<CBP_PREDICTOR> cbp_predictors;

void SCARAB_BP_INTF_FUNC(init)() {
  if(cbp_predictors.size() == 0) {
    cbp_predictors.reserve(NUM_CORES);
    for(uns i = 0; i < NUM_CORES; ++i) {
      cbp_predictors.emplace_back();
    }
  }
  ASSERTM(0, cbp_predictors.size() == NUM_CORES,
          "cbp_predictors not initialized correctly");
}

void SCARAB_BP_INTF_FUNC(timestamp)(Op* op) {
  /* CBP Interface does not support speculative updates */
  op->recovery_info.branch_id = 0;
}

uns8 SCARAB_BP_INTF_FUNC(pred)(Op* op) {
  uns proc_id = op->proc_id;
  if(op->off_path)
    return NOT_TAKEN;
  return cbp_predictors.at(proc_id).GetPrediction(op->inst_info->addr);
}

void SCARAB_BP_INTF_FUNC(spec_update)(Op* op) {
  /* CBP Interface does not support speculative updates */
  if (op->off_path) return;

  uns    proc_id = op->proc_id;
  OpType optype  = scarab_to_cbp_optype(op);

  if(is_conditional_branch(op)) {
    cbp_predictors.at(proc_id).UpdatePredictor(
      op->inst_info->addr, optype, op->oracle_info.dir, op->oracle_info.pred,
      op->oracle_info.target);
  } else {
    cbp_predictors.at(proc_id).TrackOtherInst(
      op->inst_info->addr, optype, op->oracle_info.dir, op->oracle_info.target);
  }
}

void SCARAB_BP_INTF_FUNC(update)(Op* op) {
  /* CBP Interface does not support update at exec */
}

void SCARAB_BP_INTF_FUNC(retire)(Op* op) {
  /* CBP Interface updates predictor at speculative update time */
}

void SCARAB_BP_INTF_FUNC(recover)(Recovery_Info*) {
  /* CBP Interface does not support speculative updates */
}

#endif