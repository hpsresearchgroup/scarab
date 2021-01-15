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

#ifndef __TAGE_H_
#define __TAGE_H_

#include <cmath>
#include <vector>

#include "utils.h"

/* The main history register suitable for very large history. The history is
 * implemented as a circular buffer for efficiency. The API only allows
 * insertions of bits into the most recent position of the history and provides
 * an accessor for random access of individual bits. It also provides an API for
 * rewinding the history to support recovery from mispeculation */
template <int history_size>
class Long_History_Register {
 public:
  // Buffer_size needs to be a power of 2. (buffer_size - history_size) should
  // be large enough to cover speculative branches that are not yet retired.
  Long_History_Register(int max_in_flight_branches) : history_bits_() {
    int log_buffer_size       = get_min_num_bits_to_represent(history_size +
                                                        max_in_flight_branches);
    buffer_size_              = 1 << log_buffer_size;
    buffer_access_mask_       = (1 << log_buffer_size) - 1;
    max_num_speculative_bits_ = buffer_size_ - history_size;
    history_bits_.resize(buffer_size_);
  }

  // Pushes one bit into the history at the head. Increments
  // num_speculative_bits_
  // (saturated at max_num_speculative_bits_).
  void push_bit(bool bit) {
    // TODO: it will be cleaner to mask head_ with (size_ - 1) now. But I
    // want to keep it compatible with Seznec.
    head_ -= 1;
    history_bits_[head_ & buffer_access_mask_] = bit;

    num_speculative_bits_ += 1;
    assert(num_speculative_bits_ <= max_num_speculative_bits_);
  }

  // Rewinds num_rewind_bits branches out of the history.
  void rewind(int num_rewind_bits) {
    assert(num_rewind_bits > 0 && num_rewind_bits <= num_speculative_bits_);
    num_speculative_bits_ -= num_rewind_bits;
    head_ += num_rewind_bits;
  }

  // Retire speculative bits.
  void retire(int num_retire_bits) {
    assert(num_retire_bits > 0 && num_retire_bits <= num_speculative_bits_);
    num_speculative_bits_ -= num_retire_bits;
  }

  // Random access interface, i=0 is the most recent branch (head).
  bool operator[](size_t i) const {
    return history_bits_[(head_ + i) & buffer_access_mask_];
  }

  int64_t head_idx() const { return head_; }

 private:
  int num_speculative_bits_ = 0;  // keeps track of how many bits can be
                                  // discarded during a rewind without losing
                                  // bits in the most significant position.
  std::vector<bool> history_bits_;
  int64_t           head_ = 0;
  int64_t           buffer_size_;
  int64_t           buffer_access_mask_;
  int64_t           max_num_speculative_bits_;
};

/* Computes the a folded history of a large history, as bits are shifted into
 * the history. The caller should update the folded history everytime  */
template <int history_size>
class Folded_History {
 public:
  Folded_History(int original_length, int compressed_length) :
      current_value_(0), original_length_(original_length),
      compressed_length_(compressed_length),
      outpoint_(original_length % compressed_length) {}

  int64_t get_value() const { return current_value_; }

  void update(const Long_History_Register<history_size>& history_register) {
    // Shift in the most recent GHR bit.
    current_value_ = (current_value_ << 1) ^ history_register[0];

    // Shift out the least recent GHR bit.
    current_value_ ^= history_register[original_length_] << outpoint_;

    // Fold shifted-out bit in.
    current_value_ ^= current_value_ >> compressed_length_;

    // Mask out the unused bits.
    current_value_ &= (1 << compressed_length_) - 1;
  }

  void update_reverse(
    const Long_History_Register<history_size>& history_register) {
    // Fold out the most recent GHR bit.
    current_value_ ^= history_register[0];

    // Fold out the least recent GHR bit.
    current_value_ ^= history_register[original_length_] << outpoint_;

    // Rotate the low bit around to high bit.
    current_value_ = ((current_value_ & 1) << (compressed_length_ - 1)) |
                     (current_value_ >> 1);

    // Mask out the unused bits.
    current_value_ &= (1 << compressed_length_) - 1;
  }

 private:
  int64_t current_value_;
  int     original_length_;
  int     compressed_length_;
  int     outpoint_;
};

template <class TAGE_CONFIG>
struct Tage_History_Sizes {
  static constexpr int N = TAGE_CONFIG::NUM_HISTORIES;
  constexpr Tage_History_Sizes() : arr() {
    double max_history   = static_cast<double>(TAGE_CONFIG::MAX_HISTORY_SIZE);
    double min_history   = static_cast<double>(TAGE_CONFIG::MIN_HISTORY_SIZE);
    double min_max_ratio = max_history / min_history;

    for(int i = 0; i < N; ++i) {
      double geometric_power = static_cast<double>(i) /
                               static_cast<double>(N - 1);
      double geometric_multiplier = pow(min_max_ratio, geometric_power);
      arr[i] = static_cast<int>(min_history * geometric_multiplier + 0.5);
    }
  }
  int arr[N];
};

template <class TAGE_CONFIG>
struct Tage_Tables_Enabled {
  static constexpr int N = TAGE_CONFIG::NUM_HISTORIES;
  constexpr Tage_Tables_Enabled() : arr() {
    // Use 2-way tables for the middle tables and use direct-mapped for
    // others.
    for(int i = 1; i <= 2 * N; ++i) {
      bool is_even_table   = (i - 1) & 1;
      bool is_middle_table = (i >= TAGE_CONFIG::FIRST_2WAY_TABLE) &&
                             (i <= TAGE_CONFIG::LAST_2WAY_TABLE);
      arr[i] = is_even_table || is_middle_table;
    }

    // Eliminate some of the history sizes completely. From Seznec's
    // comments,
    // it has "very very marginal" effect on accruacy.
    arr[4]         = false;
    arr[2 * N - 2] = false;
    arr[8]         = false;
    arr[2 * N - 6] = false;
  }
  bool arr[2 * N + 1];
};

template <class TAGE_CONFIG>
struct Tage_Tag_Bits {
  static constexpr int N = TAGE_CONFIG::NUM_HISTORIES;
  constexpr Tage_Tag_Bits() : arr() {
    for(int i = 0; i < N; ++i) {
      if((2 * i + 1) < TAGE_CONFIG::FIRST_LONG_HISTORY_TABLE) {
        arr[i] = TAGE_CONFIG::SHORT_HISTORY_TAG_BITS;
      } else {
        arr[i] = TAGE_CONFIG::LONG_HISTORY_TAG_BITS;
      }
    }
  }
  int arr[N];
};

struct Bimodal_Output {
  bool prediction;
  bool confidence;
};

struct Matched_Table_Banks {
  int hit_bank;
  int alt_bank;
};

template <class TAGE_CONFIG>
struct Tage_Prediction_Info {
  // Overal prediction and confidence.
  bool prediction;
  bool high_confidence;
  bool medium_confidence;
  bool low_confidence;

  // Other useful intermediate predictions.
  bool longest_match_prediction;
  bool alt_prediction;
  bool alt_confidence;
  int  hit_bank;
  int  alt_bank;

  // Extra information needed for updates.
  int     indices[2 * TAGE_CONFIG::NUM_HISTORIES + 1];
  int     tags[2 * TAGE_CONFIG::NUM_HISTORIES + 1];
  int     num_global_history_bits;
  int64_t global_history_head_checkpoint_;
  int64_t path_history_checkpoint;
};

template <class TAGE_CONFIG>
class Tage_Histories {
 public:
  Tage_Histories(int max_in_flight_branches) :
      history_register_(max_in_flight_branches) {
    path_history_ = 0;
    intialize_folded_history();
  }

  void push_into_history(uint64_t br_pc, uint64_t br_target,
                         Branch_Type br_type, bool branch_dir,
                         Tage_Prediction_Info<TAGE_CONFIG>* prediction_info) {
    head_old_           = history_register_.head_idx();
    int num_bit_inserts = 2;
    if(br_type.is_indirect && !br_type.is_conditional) {
      num_bit_inserts = 3;
    }

    int pc_dir_hash = ((br_pc ^ (br_pc >> 2))) ^ branch_dir;
    int path_hash   = br_pc ^ (br_pc >> 2) ^ (br_pc >> 4);
    if((br_type.is_indirect && br_type.is_conditional) & branch_dir) {
      pc_dir_hash = (pc_dir_hash ^ (br_target >> 2));
      path_hash   = path_hash ^ (br_target >> 2) ^ (br_target >> 4);
    }

    prediction_info->num_global_history_bits = num_bit_inserts;
    prediction_info->path_history_checkpoint = path_history_;
    prediction_info->global_history_head_checkpoint_ =
      history_register_.head_idx();

    for(int i = 0; i < num_bit_inserts; ++i) {
      history_register_.push_bit(pc_dir_hash & 1);
      pc_dir_hash >>= 1;

      path_history_ = (path_history_ << 1) ^ (path_hash & 127);
      path_hash >>= 1;

      for(int j = 0; j < TAGE_CONFIG::NUM_HISTORIES; ++j) {
        folded_histories_for_indices_[j].update(history_register_);
        folded_histories_for_tags_0_[j].update(history_register_);
        folded_histories_for_tags_1_[j].update(history_register_);
      }
    }

    path_history_ = path_history_ &
                    ((1 << TAGE_CONFIG::PATH_HISTORY_WIDTH) - 1);
  }

  void intialize_folded_history(void);

  // Hash function for the path history used in creating table indices.
  int64_t compute_path_hash(int64_t path_history, int max_width, int bank,
                            int index_size) const;

  // Derived constants
  static constexpr int twice_num_histories_ = 2 * TAGE_CONFIG::NUM_HISTORIES;
  static constexpr Tage_History_Sizes<TAGE_CONFIG> history_sizes_ = {};
  static constexpr Tage_Tag_Bits<TAGE_CONFIG>      tag_bits_      = {};

  // Predictor State
  Long_History_Register<TAGE_CONFIG::MAX_HISTORY_SIZE> history_register_;
  std::vector<Folded_History<TAGE_CONFIG::MAX_HISTORY_SIZE>>
    folded_histories_for_indices_;
  std::vector<Folded_History<TAGE_CONFIG::MAX_HISTORY_SIZE>>
    folded_histories_for_tags_0_;
  std::vector<Folded_History<TAGE_CONFIG::MAX_HISTORY_SIZE>>
    folded_histories_for_tags_1_;

  int64_t path_history_;
  int64_t head_old_;
  int64_t path_history_old_;
};

template <class TAGE_CONFIG>
class Tage {
 public:
  Tage(Random_Number_Generator& random_number_gen, int max_in_flight_branches) :
      tagged_table_ptrs_(), tage_histories_(max_in_flight_branches),
      low_history_tagged_table_(), high_history_tagged_table_(),
      alt_selector_table_(), random_number_gen_(random_number_gen) {
    initialize_table_sizes();
    intialize_predictor_state();
  }

  void get_prediction(
    uint64_t br_pc, Tage_Prediction_Info<TAGE_CONFIG>* prediction_info) const {
    fill_table_indices_tags(br_pc, prediction_info);
    auto& indices = prediction_info->indices;
    auto& tags    = prediction_info->tags;

    // First use the bimodal table to make an initial prediction.
    Bimodal_Output bimodal_output    = get_bimodal_prediction_confidence(br_pc);
    prediction_info->alt_prediction  = bimodal_output.prediction;
    prediction_info->alt_confidence  = bimodal_output.confidence;
    prediction_info->high_confidence = prediction_info->alt_confidence;
    prediction_info->medium_confidence = false;
    prediction_info->low_confidence    = !prediction_info->high_confidence;
    prediction_info->prediction        = prediction_info->alt_prediction;
    prediction_info->longest_match_prediction = prediction_info->alt_prediction;

    // Find matching tagged tables and update prediction and alternate
    // prediction
    // if necessary.

    Matched_Table_Banks matched_banks = get_two_longest_matching_tables(indices,
                                                                        tags);
    prediction_info->hit_bank         = matched_banks.hit_bank;
    prediction_info->alt_bank         = matched_banks.alt_bank;
    if(prediction_info->hit_bank != 0) {
      int8_t longest_match_counter =
        tagged_table_ptrs_[prediction_info->hit_bank]
                          [indices[prediction_info->hit_bank]]
                            .pred_counter.get();
      prediction_info->longest_match_prediction = longest_match_counter >= 0;
      if(prediction_info->alt_bank != 0) {
        int8_t alt_match_counter =
          tagged_table_ptrs_[prediction_info->alt_bank]
                            [indices[prediction_info->alt_bank]]
                              .pred_counter.get();
        prediction_info->alt_prediction = alt_match_counter >= 0;
        prediction_info->alt_confidence = std::abs(2 * alt_match_counter + 1) >
                                          1;
      }

      int alt_selector_table_index = (((prediction_info->hit_bank - 1) / 8)
                                      << 1) +
                                     (prediction_info->alt_confidence ? 1 : 0);
      alt_selector_table_index =
        alt_selector_table_index %
        ((1 << TAGE_CONFIG::ALT_SELECTOR_LOG_TABLE_SIZE) - 1);
      bool use_alt = alt_selector_table_[alt_selector_table_index].get() >= 0;
      if((!use_alt) || std::abs(2 * longest_match_counter + 1) > 1) {
        prediction_info->prediction = prediction_info->longest_match_prediction;
      } else {
        prediction_info->prediction = prediction_info->alt_prediction;
      }

      // REVISIT: this seems buggy, only works for COUNTER_BITS = 3
      prediction_info->high_confidence =
        std::abs(2 * longest_match_counter + 1) >=
        ((1 << TAGE_CONFIG::PRED_COUNTER_WIDTH) - 1);
      prediction_info->medium_confidence = std::abs(2 * longest_match_counter +
                                                    1) == 5;
      prediction_info->low_confidence    = std::abs(2 * longest_match_counter +
                                                 1) == 1;
    }
  }

  void update_speculative_state(
    uint64_t br_pc, uint64_t br_target, Branch_Type br_type,
    bool final_prediction, Tage_Prediction_Info<TAGE_CONFIG>* prediction_info) {
    tage_histories_.push_into_history(br_pc, br_target, br_type,
                                      final_prediction, prediction_info);
  }

  void commit_state(uint64_t br_pc, bool resolve_dir,
                    const Tage_Prediction_Info<TAGE_CONFIG>& prediction_info,
                    bool                                     final_prediction) {
    const int* indices = prediction_info.indices;
    const int* tags    = prediction_info.tags;

    bool allocate_new_entry =
      (prediction_info.prediction != resolve_dir) &&
      (prediction_info.hit_bank <
       Tage_Histories<TAGE_CONFIG>::twice_num_histories_);

    if(prediction_info.hit_bank > 0) {
      // Manage the selection between longest matching and alternate
      // matching
      // for "pseudo"-newly allocated longest matching entry.
      // This is extremely important for TAGE only, not that important
      // when the
      // overall predictor is implemented.
      // An entry is considered as newly allocated if its prediction
      // counter is
      // weak.
      Tagged_Entry& matched_entry =
        tagged_table_ptrs_[prediction_info.hit_bank]
                          [indices[prediction_info.hit_bank]];
      if(std::abs(2 * matched_entry.pred_counter.get() + 1) <= 1) {
        if(prediction_info.longest_match_prediction == resolve_dir) {
          // If it was delivering the correct prediction, no need to
          // allocate a
          // new entry even if the overall prediction was false.
          allocate_new_entry = false;
        }

        if(prediction_info.longest_match_prediction !=
           prediction_info.alt_prediction) {
          int alt_selector_table_index = (((prediction_info.hit_bank - 1) / 8)
                                          << 1);
          alt_selector_table_index += prediction_info.alt_confidence ? 1 : 0;
          alt_selector_table_index =
            alt_selector_table_index %
            ((1 << TAGE_CONFIG::ALT_SELECTOR_LOG_TABLE_SIZE) - 1);

          alt_selector_table_[alt_selector_table_index].update(
            prediction_info.alt_prediction == resolve_dir);
        }
      }
    }

    if(final_prediction == resolve_dir) {
      if((random_number_gen_() & 31) != 0) {
        allocate_new_entry = false;
      }
    }

    if(allocate_new_entry) {
      int num_extra_entries_to_allocate =
        TAGE_CONFIG::EXTRA_ENTRIES_TO_ALLOCATE;
      int tick_penalty  = 0;
      int num_allocated = 0;

      int temp_value = 1;
      if((random_number_gen_() & 127) < 32) {
        temp_value = 2;
      }
      int allocation_bank = ((((prediction_info.hit_bank - 1 + 2 * temp_value) &
                               0xffe)) ^
                             (random_number_gen_() & 1));

      for(; allocation_bank < Tage_Histories<TAGE_CONFIG>::twice_num_histories_;
          allocation_bank += 2) {
        int  i    = allocation_bank + 1;  // REVISIT: is i needed?
        bool done = false;
        if(tables_enabled_.arr[i]) {
          Tagged_Entry& bank_entry = tagged_table_ptrs_[i][indices[i]];
          if(bank_entry.useful.get() == 0) {
            if(std::abs(2 * bank_entry.pred_counter.get() + 1) <= 3) {
              bank_entry.tag = tags[i];
              bank_entry.pred_counter.set(resolve_dir ? 0 : -1);
              num_allocated += 1;
              if(num_extra_entries_to_allocate <= 0) {
                break;
              }
              allocation_bank += 2;
              done = true;
              num_extra_entries_to_allocate -= 1;
            } else {
              if(bank_entry.pred_counter.get() > 0) {
                bank_entry.pred_counter.decrement();
              } else {
                bank_entry.pred_counter.increment();
              }
            }
          } else {
            tick_penalty += 1;
          }
        }

        // REVISIT: this the repeat of the code above on a different
        // bank. The
        // code should be abstracted in a function.
        if(!done) {
          i = (allocation_bank ^ 1) + 1;
          if(tables_enabled_.arr[i]) {
            Tagged_Entry& bank_entry = tagged_table_ptrs_[i][indices[i]];

            if(bank_entry.useful.get() == 0) {
              if(std::abs(2 * bank_entry.pred_counter.get() + 1) <= 3) {
                bank_entry.tag = tags[i];
                bank_entry.pred_counter.set(resolve_dir ? 0 : -1);
                num_allocated += 1;
                if(num_extra_entries_to_allocate <= 0) {
                  break;
                }
                allocation_bank += 2;
                num_extra_entries_to_allocate -= 1;
              } else {
                if(bank_entry.pred_counter.get() > 0) {
                  bank_entry.pred_counter.decrement();
                } else {
                  bank_entry.pred_counter.increment();
                }
              }
            } else {
              tick_penalty += 1;
            }
          }
        }
      }

      tick_ += (tick_penalty - 2 * num_allocated);
      tick_ = std::max(tick_, 0);
      if(tick_ >= TAGE_CONFIG::TICKS_UNTIL_USEFUL_SHIFT) {
        shift_tage_useful_bits(low_history_tagged_table_,
                               TAGE_CONFIG::SHORT_HISTORY_NUM_BANKS *
                                 (1 << TAGE_CONFIG::LOG_ENTRIES_PER_BANK));
        shift_tage_useful_bits(high_history_tagged_table_,
                               TAGE_CONFIG::LONG_HISTORY_NUM_BANKS *
                                 (1 << TAGE_CONFIG::LOG_ENTRIES_PER_BANK));
        tick_ = 0;
      }
    }

    // Update prediction
    if(prediction_info.hit_bank > 0) {
      Tagged_Entry& matched_entry =
        tagged_table_ptrs_[prediction_info.hit_bank]
                          [indices[prediction_info.hit_bank]];
      if(std::abs(2 * matched_entry.pred_counter.get() + 1) == 1) {
        if(prediction_info.longest_match_prediction !=
           resolve_dir) {  // acts as a protection
          if(prediction_info.alt_bank > 0) {
            Tagged_Entry& alt_matched_entry =
              tagged_table_ptrs_[prediction_info.alt_bank]
                                [indices[prediction_info.alt_bank]];
            alt_matched_entry.pred_counter.update(resolve_dir);
          } else {
            update_bimodal(br_pc, resolve_dir);
          }
        }
      }

      matched_entry.pred_counter.update(resolve_dir);
      // sign changes: no way it can have been useful
      if(std::abs(2 * matched_entry.pred_counter.get() + 1) == 1) {
        matched_entry.useful.set(0);
      }
      if(prediction_info.alt_prediction == resolve_dir &&
         prediction_info.alt_bank > 0) {
        Tagged_Entry& alt_matched_entry =
          tagged_table_ptrs_[prediction_info.alt_bank]
                            [indices[prediction_info.alt_bank]];
        if(std::abs(2 * alt_matched_entry.pred_counter.get() + 1) == 7 &&
           matched_entry.useful.get() == 1 &&
           prediction_info.longest_match_prediction == resolve_dir) {
          matched_entry.useful.set(0);
        }
      }
    } else {
      update_bimodal(br_pc, resolve_dir);
    }

    if(prediction_info.longest_match_prediction !=
         prediction_info.alt_prediction &&
       prediction_info.longest_match_prediction == resolve_dir) {
      Tagged_Entry& matched_entry =
        tagged_table_ptrs_[prediction_info.hit_bank]
                          [indices[prediction_info.hit_bank]];
      matched_entry.useful.increment();
    }
  }

  void commit_state_at_retire(
    const Tage_Prediction_Info<TAGE_CONFIG>& prediction_info) {
    tage_histories_.history_register_.retire(
      prediction_info.num_global_history_bits);
    tage_histories_.path_history_old_ = tage_histories_.path_history_;
  }

  void global_recover_speculative_state(
    const Tage_Prediction_Info<TAGE_CONFIG>& prediction_info) {
    int64_t num_flushed_bits =
      (prediction_info.global_history_head_checkpoint_ -
       tage_histories_.history_register_.head_idx());
    for(int i = 0; i < num_flushed_bits; ++i) {
      for(int j = 0; j < TAGE_CONFIG::NUM_HISTORIES; ++j) {
        tage_histories_.folded_histories_for_indices_[j].update_reverse(
          tage_histories_.history_register_);
        tage_histories_.folded_histories_for_tags_0_[j].update_reverse(
          tage_histories_.history_register_);
        tage_histories_.folded_histories_for_tags_1_[j].update_reverse(
          tage_histories_.history_register_);
      }
      tage_histories_.history_register_.rewind(1);
    }
    tage_histories_.path_history_ = prediction_info.path_history_checkpoint;
  }

  void local_recover_speculative_state(
    const Tage_Prediction_Info<TAGE_CONFIG>& prediction_info) {}

  static void build_empty_prediction(
    Tage_Prediction_Info<TAGE_CONFIG>* prediction_info) {
    *prediction_info = {};
  }

 private:
  struct Bimodal_Entry {
    int8_t hysteresis = 1;
    int8_t prediction = 0;
  };

  struct Tagged_Entry {
    Saturating_Counter<TAGE_CONFIG::PRED_COUNTER_WIDTH, true> pred_counter;
    Saturating_Counter<TAGE_CONFIG::USEFUL_BITS, false>       useful;
    int                                                       tag = 0;

    Tagged_Entry() : pred_counter(0), useful(0) {}
  };

  void initialize_tag_bits(void);
  void initialize_table_sizes(void);
  void intialize_predictor_state(void);

  // Produce indices and tags for all Tagged table look-ups.
  void fill_table_indices_tags(
    uint64_t br_pc, Tage_Prediction_Info<TAGE_CONFIG>* tage_output) const;

  // Get the prediction and confidence of the bimodal table.
  Bimodal_Output get_bimodal_prediction_confidence(uint64_t br_pc) const;

  void update_bimodal(uint64_t br_pc, bool resolve_dir);

  // Get the banks IDs of matching tables with longest histories.
  // A bank of 0 means a match was not found.
  Matched_Table_Banks get_two_longest_matching_tables(int indices[],
                                                      int tags[]) const;

  void shift_tage_useful_bits(Tagged_Entry* table, int size);


  // Derived constants
  static constexpr Tage_Tables_Enabled<TAGE_CONFIG> tables_enabled_ = {};

  Tagged_Entry*
    tagged_table_ptrs_[Tage_Histories<TAGE_CONFIG>::twice_num_histories_ + 1];

  // Predictor State
  Tage_Histories<TAGE_CONFIG> tage_histories_;
  Bimodal_Entry bimodal_table_[1 << TAGE_CONFIG::BIMODAL_LOG_TABLES_SIZE];
  Tagged_Entry
    low_history_tagged_table_[TAGE_CONFIG::SHORT_HISTORY_NUM_BANKS *
                              (1 << TAGE_CONFIG::LOG_ENTRIES_PER_BANK)];
  Tagged_Entry
    high_history_tagged_table_[TAGE_CONFIG::LONG_HISTORY_NUM_BANKS *
                               (1 << TAGE_CONFIG::LOG_ENTRIES_PER_BANK)];

  Saturating_Counter<TAGE_CONFIG::ALT_SELECTOR_ENTRY_WIDTH, true>
      alt_selector_table_[1 << TAGE_CONFIG::ALT_SELECTOR_LOG_TABLE_SIZE];
  int tick_;  // for resetting the useful bits

  Random_Number_Generator& random_number_gen_;
};

template <class TAGE_CONFIG>
constexpr Tage_History_Sizes<TAGE_CONFIG>
  Tage_Histories<TAGE_CONFIG>::history_sizes_;

template <class TAGE_CONFIG>
constexpr Tage_Tables_Enabled<TAGE_CONFIG> Tage<TAGE_CONFIG>::tables_enabled_;

template <class TAGE_CONFIG>
constexpr Tage_Tag_Bits<TAGE_CONFIG> Tage_Histories<TAGE_CONFIG>::tag_bits_;

template <class TAGE_CONFIG>
void Tage<TAGE_CONFIG>::initialize_table_sizes(void) {
  for(int i = 1; i < TAGE_CONFIG::FIRST_LONG_HISTORY_TABLE; ++i) {
    tagged_table_ptrs_[i] = low_history_tagged_table_;
  }
  for(int i = TAGE_CONFIG::FIRST_LONG_HISTORY_TABLE;
      i <= Tage_Histories<TAGE_CONFIG>::twice_num_histories_; ++i) {
    tagged_table_ptrs_[i] = high_history_tagged_table_;
  }
}

template <class TAGE_CONFIG>
void Tage_Histories<TAGE_CONFIG>::intialize_folded_history(void) {
  // insert dummy entries for the zeroth entry of the vector
  for(int i = 0; i < TAGE_CONFIG::NUM_HISTORIES; i++) {
    // For some reason I cannot pass LOG_ENTRIES_PER_BANK to emplace_bank()
    // directly (gcc complains that variable is undefined), do not know why.
    // REVISIT: since I got rid of LOG_ENTRIES_PER_BANK as a constant, this
    // should be fine now.
    const int LOG_ENTRIES_PER_BANK2 = TAGE_CONFIG::LOG_ENTRIES_PER_BANK;
    folded_histories_for_indices_.emplace_back(history_sizes_.arr[i],
                                               LOG_ENTRIES_PER_BANK2);
    folded_histories_for_tags_0_.emplace_back(history_sizes_.arr[i],
                                              tag_bits_.arr[i]);
    folded_histories_for_tags_1_.emplace_back(history_sizes_.arr[i],
                                              tag_bits_.arr[i] - 1);
  }
}

template <class TAGE_CONFIG>
void Tage<TAGE_CONFIG>::intialize_predictor_state(void) {
  tick_                           = 0;
  random_number_gen_.phist_ptr_   = &tage_histories_.path_history_old_;
  random_number_gen_.ptghist_ptr_ = &tage_histories_.head_old_;
}

template <class TAGE_CONFIG>
int64_t Tage_Histories<TAGE_CONFIG>::compute_path_hash(int64_t path_history,
                                                       int max_width, int bank,
                                                       int index_size) const {
  int64_t temp1, temp2;

  // truncate path history to index size.
  path_history = (path_history & ((1 << max_width) - 1));
  temp1        = (path_history & ((1 << index_size) - 1));

  // Take high part of path history and left rotate it by "bank" ammount
  // this is just to generate a unique hash for each bank
  temp2 = (path_history >> index_size);
  if(bank < index_size) {
    temp2 = ((temp2 << bank) & ((1 << index_size) - 1)) +
            (temp2 >> (index_size - bank));
  }

  // fold rotated high part of path into low part of path
  path_history = temp1 ^ temp2;

  // left rotate that chunk by "bank"
  if(bank < index_size) {
    path_history = ((path_history << bank) & ((1 << index_size) - 1)) +
                   (path_history >> (index_size - bank));
  }
  return path_history;
}

template <class TAGE_CONFIG>
void Tage<TAGE_CONFIG>::fill_table_indices_tags(
  uint64_t br_pc, Tage_Prediction_Info<TAGE_CONFIG>* output) const {
  // Generate tags and indices, ignore bank bits for now.
  for(int i = 1; i <= Tage_Histories<TAGE_CONFIG>::twice_num_histories_;
      i += 2) {
    if(tables_enabled_.arr[i] || tables_enabled_.arr[i + 1]) {
      int max_path_width = (tage_histories_.history_sizes_.arr[(i - 1) / 2] >
                            TAGE_CONFIG::PATH_HISTORY_WIDTH) ?
                             TAGE_CONFIG::PATH_HISTORY_WIDTH :
                             tage_histories_.history_sizes_.arr[(i - 1) / 2];
      int64_t path_hash = tage_histories_.compute_path_hash(
        tage_histories_.path_history_, max_path_width, i,
        TAGE_CONFIG::LOG_ENTRIES_PER_BANK);
      int64_t index = br_pc;
      index ^= br_pc >> (std::abs(TAGE_CONFIG::LOG_ENTRIES_PER_BANK - i) + 1);
      index ^=
        tage_histories_.folded_histories_for_indices_[(i - 1) / 2].get_value();
      index ^= path_hash;
      output->indices[i] = index &
                           ((1 << TAGE_CONFIG::LOG_ENTRIES_PER_BANK) - 1);

      int64_t tag = br_pc;
      tag ^=
        tage_histories_.folded_histories_for_tags_0_[(i - 1) / 2].get_value();
      tag ^=
        tage_histories_.folded_histories_for_tags_1_[(i - 1) / 2].get_value()
        << 1;
      output->tags[i] = tag &
                        ((1 << tage_histories_.tag_bits_.arr[(i - 1) / 2]) - 1);

      output->tags[i + 1]    = output->tags[i];
      output->indices[i + 1] = output->indices[i] ^
                               (output->tags[i] &
                                ((1 << TAGE_CONFIG::LOG_ENTRIES_PER_BANK) - 1));
    }
  }

  // Now add bank bits to the indices of high history tables.
  int temp = (br_pc ^
              (tage_histories_.path_history_ &
               ((int64_t(1)
                 << tage_histories_.history_sizes_
                      .arr[(TAGE_CONFIG::FIRST_LONG_HISTORY_TABLE - 1) / 2]) -
                1))) %
             TAGE_CONFIG::LONG_HISTORY_NUM_BANKS;
  for(int i = TAGE_CONFIG::FIRST_LONG_HISTORY_TABLE;
      i <= Tage_Histories<TAGE_CONFIG>::twice_num_histories_; ++i) {
    if(tables_enabled_.arr[i]) {
      output->indices[i] += (temp << TAGE_CONFIG::LOG_ENTRIES_PER_BANK);
      temp++;
      temp = temp % TAGE_CONFIG::LONG_HISTORY_NUM_BANKS;
    }
  }

  // Now add bank bits to the indices of low history tables.
  temp = (br_pc ^ (tage_histories_.path_history_ &
                   ((1 << tage_histories_.history_sizes_.arr[0]) - 1))) %
         TAGE_CONFIG::SHORT_HISTORY_NUM_BANKS;
  for(int i = 1; i <= TAGE_CONFIG::FIRST_LONG_HISTORY_TABLE - 1; ++i) {
    if(tables_enabled_.arr[i]) {
      output->indices[i] += (temp << TAGE_CONFIG::LOG_ENTRIES_PER_BANK);
      temp++;
      temp = temp % TAGE_CONFIG::SHORT_HISTORY_NUM_BANKS;
    }
  }
}

template <class TAGE_CONFIG>
Bimodal_Output Tage<TAGE_CONFIG>::get_bimodal_prediction_confidence(
  uint64_t br_pc) const {
  Bimodal_Output output;
  int            index = (br_pc ^ (br_pc >> 2)) &
              ((1 << TAGE_CONFIG::BIMODAL_LOG_TABLES_SIZE) - 1);
  int8_t bimodal_output =
    (bimodal_table_[index].prediction << 1) +
    (bimodal_table_[index >> TAGE_CONFIG::BIMODAL_HYSTERESIS_SHIFT].hysteresis);

  output.prediction = bimodal_table_[index].prediction > 0;
  output.confidence = (bimodal_output == 0) || (bimodal_output == 3);
  return output;
}

template <class TAGE_CONFIG>
void Tage<TAGE_CONFIG>::update_bimodal(uint64_t br_pc, bool resolve_dir) {
  int index = (br_pc ^ (br_pc >> 2)) &
              ((1 << TAGE_CONFIG::BIMODAL_LOG_TABLES_SIZE) - 1);
  int8_t bimodal_output =
    (bimodal_table_[index].prediction << 1) +
    (bimodal_table_[index >> TAGE_CONFIG::BIMODAL_HYSTERESIS_SHIFT].hysteresis);
  if(resolve_dir && bimodal_output < 3) {
    bimodal_output += 1;
  } else if(!resolve_dir && bimodal_output > 0) {
    bimodal_output -= 1;
  }
  bimodal_table_[index].prediction = bimodal_output >> 1;
  bimodal_table_[index >> TAGE_CONFIG::BIMODAL_HYSTERESIS_SHIFT].hysteresis =
    (bimodal_output & 1);
}

template <class TAGE_CONFIG>
Matched_Table_Banks Tage<TAGE_CONFIG>::get_two_longest_matching_tables(
  int indices[], int tags[]) const {
  int first_match  = 0;
  int second_match = 0;
  for(int i = 2 * TAGE_CONFIG::NUM_HISTORIES; i > 0; --i) {
    if(tables_enabled_.arr[i]) {
      if(tagged_table_ptrs_[i][indices[i]].tag == tags[i]) {
        if(first_match == 0) {
          first_match = i;
        } else {
          second_match = i;
          break;
        }
      }
    }
  }
  return Matched_Table_Banks{first_match, second_match};
}

template <class TAGE_CONFIG>
void Tage<TAGE_CONFIG>::shift_tage_useful_bits(Tagged_Entry* table, int size) {
  for(int i = 0; i < size; ++i) {
    table[i].useful.set(table[i].useful.get() >> 1);
  }
}

#endif  // __TAGE_H_
