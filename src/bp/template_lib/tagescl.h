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

#include "statistical_corrector.h"
#include "tage.h"
#include "tagescl_configs.h"
#include "utils.h"

template <class CONFIG>
struct Tage_SC_L_Prediction_Info {
  Tage_Prediction_Info<typename CONFIG::TAGE> tage;
  Loop_Prediction_Info<typename CONFIG::LOOP> loop;
  bool                                        tage_or_loop_prediction;
  SC_Prediction_Info                          sc;
  bool                                        final_prediction;
};

class Tage_SC_L_Base {
 public:
  virtual int64_t get_new_branch_id()                                 = 0;
  virtual bool    get_prediction(int64_t branch_id, uint64_t br_pc)   = 0;
  virtual void    update_speculative_state(int64_t branch_id, uint64_t br_pc,
                                           Branch_Type br_type, bool branch_dir,
                                           uint64_t br_target)        = 0;
  virtual void    commit_state(int64_t branch_id, uint64_t br_pc,
                               Branch_Type br_type, bool resolve_dir) = 0;
  virtual void    commit_state_at_retire(int64_t branch_id, uint64_t br_pc,
                                         Branch_Type br_type, bool resolve_dir,
                                         uint64_t br_target)          = 0;
  virtual void flush_branch_and_repair_state(int64_t branch_id, uint64_t br_pc,
                                             Branch_Type br_type,
                                             bool        resolve_dir,
                                             uint64_t    br_target)      = 0;
};

/* Interface functions:
 *
 * warmup() a wrapper for updating predictor state during the warmup phase of a
 * simulation.
 *
 * predict_and_update() a wrapper for consecutive simultaneous prediction and
 * update that implement the idealistic algorithms without considering pipeline
 * requirements. (same as Championship Branch Prediction Interface)
 */
template <class CONFIG>
class Tage_SC_L : public Tage_SC_L_Base {
 public:
  Tage_SC_L(int max_in_flight_branches) :
      tage_(random_number_gen_, max_in_flight_branches),
      statistical_corrector_(), loop_predictor_(random_number_gen_),
      loop_predictor_beneficial_(-1),
      prediction_info_buffer_(max_in_flight_branches) {}

  // Gets a new branch_id for a new in-flight branch. The id remains valid
  // until
  // the branch is retired or flushed. The class internally maintains metadata
  // for each in-flight branch. The rest of the public functions in this class
  // need the id of a branch to work on.
  int64_t get_new_branch_id() override {
    int64_t branch_id       = prediction_info_buffer_.allocate_back();
    auto&   prediction_info = prediction_info_buffer_[branch_id];
    Tage<typename CONFIG::TAGE>::build_empty_prediction(&prediction_info.tage);
    Loop_Predictor<typename CONFIG::LOOP>::build_empty_prediction(
      &prediction_info.loop);
    return branch_id;
  }

  // It uses the speculative state of the predictor to generate a prediction.
  // Should be called before update_speculative_state.
  bool get_prediction(int64_t branch_id, uint64_t br_pc) override;

  // It updates the speculative state (e.g. to insert history bits in Tage's
  // global history register). For conditional branches, it should be called
  // after get_prediction() in the front-end of a pipeline. For unconditional
  // branches, it should be the only function called in the front-end.
  void update_speculative_state(int64_t branch_id, uint64_t br_pc,
                                Branch_Type br_type, bool branch_dir,
                                uint64_t br_target) override;

  // Invokes the default update algorithm for updating the predictor state.
  // Can
  // be called either at the end of execute or retire. Note that even though
  // updating at the end of execute is speculative, committing the state
  // cannot
  // be undone.
  void commit_state(int64_t branch_id, uint64_t br_pc, Branch_Type br_type,
                    bool resolve_dir) override;

  // Updates predictor states that are critical for algorithm correctness.
  // Thus, should always be called in the retire state and after
  // commit_state()
  // is called. branch_id is invalidated and should not be used anymore.
  void commit_state_at_retire(int64_t branch_id, uint64_t br_pc,
                              Branch_Type br_type, bool resolve_dir,
                              uint64_t br_target) override;

  // Flushes the branch and all branches that came after it and repairs the
  // speculative state of the predictor. It invalidated all branch_id of
  // flushed
  // branches.
  void flush_branch_and_repair_state(int64_t branch_id, uint64_t br_pc,
                                     Branch_Type br_type, bool resolve_dir,
                                     uint64_t br_target) override;

 private:
  Random_Number_Generator               random_number_gen_;
  Tage<typename CONFIG::TAGE>           tage_;
  Statistical_Corrector<CONFIG>         statistical_corrector_;
  Loop_Predictor<typename CONFIG::LOOP> loop_predictor_;

  // Counter for choosing between Tage and Loop Predictor.
  Saturating_Counter<CONFIG::CONFIDENCE_COUNTER_WIDTH, true>
    loop_predictor_beneficial_;

  // Used for remembering necessary information gathered during prediction
  // that
  // are needed for update.
  Circular_Buffer<Tage_SC_L_Prediction_Info<CONFIG>> prediction_info_buffer_;
};

template <class CONFIG>
bool Tage_SC_L<CONFIG>::get_prediction(int64_t branch_id, uint64_t br_pc) {
  auto& prediction_info = prediction_info_buffer_[branch_id];

  // First, use Tage to make a prediction.
  tage_.get_prediction(br_pc, &prediction_info.tage);
  prediction_info.tage_or_loop_prediction = prediction_info.tage.prediction;

  if(CONFIG::USE_LOOP_PREDICTOR) {
    // Then, look up the loop predictor and override Tage's prediction if
    // the
    // loop predictor is found to be beneficial.
    loop_predictor_.get_prediction(br_pc, &prediction_info.loop);
    if(loop_predictor_beneficial_.get() >= 0 && prediction_info.loop.valid) {
      prediction_info.tage_or_loop_prediction = prediction_info.loop.prediction;
    }
  }

  if(!CONFIG::USE_SC) {
    prediction_info.final_prediction = prediction_info.tage_or_loop_prediction;
  } else {
    statistical_corrector_.get_prediction(
      br_pc, prediction_info.tage, prediction_info.tage_or_loop_prediction,
      &prediction_info.sc);
    prediction_info.final_prediction = prediction_info.sc.prediction;
  }
  return prediction_info.final_prediction;
}

template <class CONFIG>
void Tage_SC_L<CONFIG>::commit_state(int64_t branch_id, uint64_t br_pc,
                                     Branch_Type br_type, bool resolve_dir) {
  if(!br_type.is_conditional) {
    return;
  }
  auto& prediction_info = prediction_info_buffer_[branch_id];
  if(CONFIG::USE_SC) {
    statistical_corrector_.commit_state(
      br_pc, resolve_dir, prediction_info.tage, prediction_info.sc,
      prediction_info.tage_or_loop_prediction);
  }

  if(CONFIG::USE_LOOP_PREDICTOR) {
    if(prediction_info.loop.valid) {
      if(prediction_info.final_prediction != prediction_info.loop.prediction) {
        loop_predictor_beneficial_.update(resolve_dir ==
                                          prediction_info.loop.prediction);
      }
    }
    loop_predictor_.commit_state(
      br_pc, resolve_dir, prediction_info.loop,
      prediction_info.final_prediction != resolve_dir,
      prediction_info.tage.prediction);
    loop_predictor_.commit_state_at_retire(
      br_pc, resolve_dir, prediction_info.loop,
      prediction_info.final_prediction != resolve_dir,
      prediction_info.tage.prediction);
  }

  tage_.commit_state(br_pc, resolve_dir, prediction_info.tage,
                     prediction_info.final_prediction);
}

template <class CONFIG>
void Tage_SC_L<CONFIG>::flush_branch_and_repair_state(int64_t     branch_id,
                                                      uint64_t    br_pc,
                                                      Branch_Type br_type,
                                                      bool        resolve_dir,
                                                      uint64_t    br_target) {
  // First iterate over all flushed branches from youngest to oldest and call
  // local recovery functions.
  for(int64_t id = prediction_info_buffer_.back_id(); id >= branch_id; --id) {
    auto& prediction_info = prediction_info_buffer_[id];
    tage_.local_recover_speculative_state(prediction_info.tage);
    if(CONFIG::USE_LOOP_PREDICTOR) {
      loop_predictor_.local_recover_speculative_state(prediction_info.loop);
    }
    if(CONFIG::USE_SC) {
      statistical_corrector_.local_recover_speculative_state(
        br_pc, prediction_info.sc);
    }
  }
  prediction_info_buffer_.deallocate_after(branch_id);

  // Now call global recovery functions.
  auto& prediction_info = prediction_info_buffer_[branch_id];
  tage_.global_recover_speculative_state(prediction_info.tage);
  if(CONFIG::USE_LOOP_PREDICTOR) {
    loop_predictor_.global_recover_speculative_state(prediction_info.loop);
  }
  if(CONFIG::USE_SC) {
    statistical_corrector_.global_recover_speculative_state(prediction_info.sc);
  }

  // Finally, update the speculative histories again using the resolved
  // direction of the branch.
  tage_.update_speculative_state(br_pc, br_target, br_type, resolve_dir,
                                 &prediction_info.tage);
  if(CONFIG::USE_LOOP_PREDICTOR) {
    loop_predictor_.update_speculative_state(prediction_info.loop);
  }
  if(CONFIG::USE_SC) {
    statistical_corrector_.update_speculative_state(
      br_pc, resolve_dir, br_target, br_type, &prediction_info.sc);
  }
}

template <class CONFIG>
void Tage_SC_L<CONFIG>::commit_state_at_retire(int64_t     branch_id,
                                               uint64_t    br_pc,
                                               Branch_Type br_type,
                                               bool        resolve_dir,
                                               uint64_t    br_target) {
  auto& prediction_info = prediction_info_buffer_[branch_id];
  // if (CONFIG::USE_LOOP_PREDICTOR) {
  //  loop_predictor_.commit_state_at_retire(
  //      br_pc, resolve_dir, prediction_info.loop,
  //      prediction_info.final_prediction != resolve_dir,
  //      prediction_info.tage.prediction);
  //}
  tage_.commit_state_at_retire(prediction_info.tage);
  if(CONFIG::USE_SC) {
    statistical_corrector_.commit_state_at_retire();
  }
  prediction_info_buffer_.deallocate_front(branch_id);
}

template <class CONFIG>
void Tage_SC_L<CONFIG>::update_speculative_state(int64_t     branch_id,
                                                 uint64_t    br_pc,
                                                 Branch_Type br_type,
                                                 bool        branch_dir,
                                                 uint64_t    br_target) {
  auto& prediction_info = prediction_info_buffer_[branch_id];
  tage_.update_speculative_state(br_pc, br_target, br_type, branch_dir,
                                 &prediction_info.tage);
  if(CONFIG::USE_LOOP_PREDICTOR) {
    loop_predictor_.update_speculative_state(prediction_info.loop);
  }
  if(CONFIG::USE_SC) {
    statistical_corrector_.update_speculative_state(
      br_pc, branch_dir, br_target, br_type, &prediction_info.sc);
  }
}
