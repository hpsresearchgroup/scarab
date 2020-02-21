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

#ifndef __LOOP_PREDICTOR_H_
#define __LOOP_PREDICTOR_H_

#include <vector>
#include "utils.h"

struct Loop_Predictor_Indices {
  int bank[4];
};

template <class LOOP_CONFIG>
struct Loop_Prediction_Info {
  int8_t hit_bank;
  bool   valid;
  bool   prediction;

  // Information needed for table updates.
  Loop_Predictor_Indices indices;
  int16_t                tag;
  Saturating_Counter<LOOP_CONFIG::ITERATION_COUNTER_WIDTH, false>
    current_iter_checkpoint;
};

template <class LOOP_CONFIG>
class Loop_Predictor {
 public:
  Loop_Predictor(Random_Number_Generator& random_number_gen) :
      table_(1 << LOOP_CONFIG::LOG_NUM_ENTRIES),
      random_number_gen_(random_number_gen) {}

  void get_prediction(
    uint64_t br_pc, Loop_Prediction_Info<LOOP_CONFIG>* prediction_info) const {
    prediction_info->valid      = false;
    prediction_info->prediction = false;
    prediction_info->hit_bank   = -1;

    prediction_info->indices = get_indices(br_pc);
    int tag                  = get_tag(br_pc);
    prediction_info->tag     = tag;

    for(int i = 0; i < 4; i++) {
      int index = prediction_info->indices.bank[i];

      if(table_[index].tag == tag) {
        prediction_info->hit_bank = i;
        prediction_info->valid =
          ((table_[index].confidence == LOOP_CONFIG::CONFIDENCE_THRESHOLD) ||
           (table_[index].confidence * table_[index].total_iterations > 128));

        prediction_info->current_iter_checkpoint =
          table_[index].speculative_current_iter.get();
        if(table_[index].speculative_current_iter.get() + 1 ==
           table_[index].total_iterations) {
          prediction_info->prediction = !table_[index].dir;
          break;
        } else {
          prediction_info->prediction = table_[index].dir;
          break;
        }
      }
    }
  }

  void update_speculative_state(
    const Loop_Prediction_Info<LOOP_CONFIG>& prediction_info) {
    if(prediction_info.hit_bank >= 0) {
      int index = prediction_info.indices.bank[prediction_info.hit_bank];
      if(table_[index].total_iterations != 0) {
        table_[index].speculative_current_iter.increment();
        if(table_[index].speculative_current_iter.get() >=
           table_[index].total_iterations) {
          table_[index].speculative_current_iter.set(0);
        }
      }
    }
  }

  void commit_state(uint64_t br_pc, bool resolve_dir,
                    const Loop_Prediction_Info<LOOP_CONFIG>& prediction_info,
                    bool finally_mispredicted, bool tage_prediction) {}

  void commit_state_at_retire(
    uint64_t br_pc, bool resolve_dir,
    const Loop_Prediction_Info<LOOP_CONFIG>& prediction_info,
    bool finally_mispredicted, bool tage_prediction) {
    if(prediction_info.hit_bank >= 0) {
      int index = prediction_info.indices.bank[prediction_info.hit_bank];
      if(table_[index].tag != prediction_info.tag) {
        // The entry must have been replaced by anoher entry.
        return;
      }

      if(prediction_info.valid) {
        if(resolve_dir != prediction_info.prediction) {
          // free the entry
          table_[index].total_iterations = 0;
          table_[index].confidence       = 0;
          table_[index].age              = 0;
          table_[index].current_iter.set(0);
          table_[index].speculative_current_iter.set(0);
          return;
        } else if((prediction_info.prediction != tage_prediction) ||
                  ((random_number_gen_() & 7) == 0)) {
          if(table_[index].age < LOOP_CONFIG::CONFIDENCE_THRESHOLD) {
            table_[index].age += 1;
          }
        }
      }

      table_[index].current_iter.increment();
      if(table_[index].current_iter.get() > table_[index].total_iterations) {
        // treat like the 1st encounter of the loop
        table_[index].total_iterations = 0;
        table_[index].confidence       = 0;
      }

      if(resolve_dir != table_[index].dir) {
        if(table_[index].current_iter.get() == table_[index].total_iterations) {
          if(table_[index].confidence < LOOP_CONFIG::CONFIDENCE_THRESHOLD) {
            table_[index].confidence += 1;
          }

          if(table_[index].total_iterations < 3) {
            // just do not predict when the loop count is 1 or 2
            // free the entry
            table_[index].dir              = resolve_dir;
            table_[index].total_iterations = 0;
            table_[index].age              = 0;
            table_[index].current_iter.set(0);
            table_[index].speculative_current_iter.set(0);
          }
        } else {
          if(table_[index].total_iterations == 0) {
            // first complete nest;
            table_[index].confidence       = 0;
            table_[index].total_iterations = table_[index].current_iter.get();
            table_[index].speculative_current_iter.set(0);
          } else {
            // not the same number of iterations as last time: free
            // the entry
            table_[index].total_iterations = 0;
            table_[index].confidence       = 0;
          }
        }
        table_[index].current_iter.set(0);
      }

      if(finally_mispredicted) {
        table_[index].speculative_current_iter = table_[index].current_iter;
      }
    } else if(finally_mispredicted) {
      int random_bank = random_number_gen_() & 3;

      if((random_number_gen_() & 3) == 0) {
        int tag   = get_tag(br_pc);
        int index = prediction_info.indices.bank[random_bank];
        if(table_[index].age == 0) {
          // most of mispredictions are on last iterations
          table_[index].dir              = !resolve_dir;
          table_[index].tag              = tag;
          table_[index].total_iterations = 0;
          table_[index].age              = 7;
          table_[index].confidence       = 0;
          table_[index].current_iter.set(0);
          table_[index].speculative_current_iter.set(0);
        } else {
          table_[index].age -= 1;
        }
      }
    }
  }

  void global_recover_speculative_state(
    const Loop_Prediction_Info<LOOP_CONFIG>& prediction_info) {}

  void local_recover_speculative_state(
    const Loop_Prediction_Info<LOOP_CONFIG>& prediction_info) {
    if(prediction_info.hit_bank >= 0) {
      int index = prediction_info.indices.bank[prediction_info.hit_bank];
      if(table_[index].tag != prediction_info.tag) {
        // The entry must have been replaced by anoher entry.
        return;
      }
      table_[index].speculative_current_iter =
        prediction_info.current_iter_checkpoint;
    }
  }

  static void build_empty_prediction(
    Loop_Prediction_Info<LOOP_CONFIG>* prediction_info) {
    prediction_info->hit_bank = -1;
  }

 private:
  struct LoopPredictorEntry {
    int16_t total_iterations = 0;  // 10 bits
    int16_t tag              = 0;  // 10 bits
    int8_t  confidence       = 0;  // 4 bits
    int8_t  age              = 0;  // 4 bits
    bool    dir              = 0;  // 1 bit
    Saturating_Counter<LOOP_CONFIG::ITERATION_COUNTER_WIDTH, false>
      speculative_current_iter;  // 10 bits
    Saturating_Counter<LOOP_CONFIG::ITERATION_COUNTER_WIDTH, false>
      current_iter;  // 10 bits

    LoopPredictorEntry() : current_iter(0) {}
  };

  Loop_Predictor_Indices get_indices(uint64_t br_pc) const;
  int                    get_tag(uint64_t br_pc) const;

  std::vector<LoopPredictorEntry> table_;

  Random_Number_Generator& random_number_gen_;
};

template <class LOOP_CONFIG>
Loop_Predictor_Indices Loop_Predictor<LOOP_CONFIG>::get_indices(
  uint64_t br_pc) const {
  Loop_Predictor_Indices indices;
  int                    component1 =
    ((br_pc ^ (br_pc >> 2)) & ((1 << (LOOP_CONFIG::LOG_NUM_ENTRIES - 2)) - 1))
    << 2;
  int component2 = (br_pc >> (LOOP_CONFIG::LOG_NUM_ENTRIES - 2)) &
                   ((1 << (LOOP_CONFIG::LOG_NUM_ENTRIES - 2)) - 1);

  for(int i = 0; i < 4; ++i) {
    indices.bank[i] = (component1 ^ ((component2 >> i) << 2)) + i;
  }
  return indices;
}

template <class LOOP_CONFIG>
int Loop_Predictor<LOOP_CONFIG>::get_tag(uint64_t br_pc) const {
  int tag = (br_pc >> (LOOP_CONFIG::LOG_NUM_ENTRIES - 2)) &
            ((1 << 2 * LOOP_CONFIG::TAG_BITS) - 1);
  tag = tag ^ (tag >> LOOP_CONFIG::TAG_BITS);
  tag = tag & ((1 << LOOP_CONFIG::TAG_BITS) - 1);
  return tag;
}

#endif  // __LOOP_PREDICTOR_H_
