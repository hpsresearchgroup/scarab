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

#ifndef __STATISTICAL_CORRECTOR_H_
#define __STATISTICAL_CORRECTOR_H_

#include "loop_predictor.h"
#include "tage.h"
#include "utils.h"

/* A table of counters used for SC Gehl thresholds.
 * The accessors automatically creates a valid index by creating a hash of PC.*/
template <int width, int log_table_size>
class Threshold_Table {
 public:
  using Counter_Type = Saturating_Counter<width, true>;

  Threshold_Table(int init_value) {
    for(int i = 0; i < table_size; ++i) {
      table_[i].set(init_value);
    }
  }

  const Counter_Type& get_entry(uint64_t br_pc) const {
    return table_[(br_pc ^ (br_pc >> 2)) & (table_size - 1)];
  }

  Counter_Type& get_entry(uint64_t br_pc) {
    return table_[(br_pc ^ (br_pc >> 2)) & (table_size - 1)];
  }

  int temp_get_index(uint64_t br_pc) const {
    return (br_pc ^ (br_pc >> 2)) & (table_size - 1);
  }

 private:
  static constexpr int table_size = 1 << log_table_size;
  Counter_Type         table_[table_size];
};

template <int log_table_size, int pc_shift>
class Local_History_Table {
 public:
  Local_History_Table() : table_() {}

  int64_t get_history(uint64_t br_pc) const { return table_[get_index(br_pc)]; }
  int64_t& get_history(uint64_t br_pc) { return table_[get_index(br_pc)]; }

 private:
  static constexpr int table_size = 1 << log_table_size;

  int get_index(uint64_t br_pc) const {
    return (br_pc ^ (br_pc >> pc_shift)) & (table_size - 1);
  }

  int     pc_shift_;
  int64_t table_[table_size];
};

// A GEHL Table. Used by Statistical Corrector.
template <int counter_width, class Histories, int log_table_size>
class Gehl {
 public:
  using Counter_Type = Saturating_Counter<counter_width, true>;

  Gehl() : tables_() {
    for(int i = 0; i < num_histories; ++i) {
      for(int j = 0; j < ((1 << log_table_size) - 1); ++j) {
        tables_[i][j].set((j & 1) ? 0 : -1);
      }
    }
  }

  int get_prediction_sum(uint64_t br_pc, int64_t history) const {
    int sum = 0;
    for(int i = 0; i < num_histories; i++) {
      int index = get_index(br_pc, history, i);
      sum += (2 * tables_[i][index].get() + 1);
    }
    return sum;
  }

  void update(uint64_t br_pc, int64_t history, bool resolve_dir) {
    for(int i = 0; i < num_histories; i++) {
      int index = get_index(br_pc, history, i);
      tables_[i][index].update(resolve_dir);
    }
  }

 private:
  static constexpr int num_histories = sizeof(Histories::arr) /
                                       sizeof(Histories::arr[0]);

  int get_index(uint64_t br_pc, int64_t history, int history_id) const {
    int64_t masked_history = history &
                             ((int64_t(1) << Histories::arr[history_id]) - 1);
    int64_t index = br_pc ^ masked_history;
    index ^= masked_history >> (8 - history_id);
    index ^= masked_history >> (16 - 2 * history_id);
    index ^= masked_history >> (24 - 3 * history_id);
    index ^= masked_history >> (32 - 3 * history_id);
    index ^= masked_history >> (40 - 4 * history_id);
    index &= ((1 << (log_table_size - (history_id >= (num_histories - 2)))) -
              1);
    return static_cast<int>(index);
  }

  Counter_Type tables_[num_histories][1 << log_table_size];
};

struct SC_Histories_Snapshot {
  int64_t global_history;
  int64_t path;
  int64_t first_local_history;
  int64_t second_local_history;
  int64_t third_local_history;
  int64_t imli_counter;
  int64_t imli_local_history;
};

struct SC_Prediction_Info {
  int  gehls_sum;
  int  thresholds_sum;
  bool prediction;

  SC_Histories_Snapshot history_snapshot;
};

template <class CONFIG>
class Statistical_Corrector {
 public:
  Statistical_Corrector();

  void get_prediction(
    uint64_t                                           br_pc,
    const Tage_Prediction_Info<typename CONFIG::TAGE>& tage_prediction_info,
    bool tage_or_loop_prediction, SC_Prediction_Info* prediction_info);

  void commit_state(
    uint64_t br_pc, bool resolve_dir,
    const Tage_Prediction_Info<typename CONFIG::TAGE>& tage_prediction_info,
    const SC_Prediction_Info& sc_prediction_info, bool tage_or_loop_prediction);

  void update_speculative_state(uint64_t br_pc, bool resolve_dir,
                                uint64_t br_taret, Branch_Type br_type,
                                SC_Prediction_Info* prediction_info);

  void commit_state_at_retire() {}

  void global_recover_speculative_state(
    const SC_Prediction_Info& prediction_info) {
    global_history_ = prediction_info.history_snapshot.global_history;
    path_           = prediction_info.history_snapshot.path;
  }

  void local_recover_speculative_state(
    uint64_t br_pc, const SC_Prediction_Info& prediction_info) {
    if(CONFIG::SC::USE_LOCAL_HISTORY) {
      first_local_history_table_.get_history(
        br_pc) = prediction_info.history_snapshot.first_local_history;
      if(CONFIG::SC::USE_SECOND_LOCAL_HISTORY) {
        second_local_history_table_.get_history(
          br_pc) = prediction_info.history_snapshot.second_local_history;
      }
      if(CONFIG::SC::USE_THIRD_LOCAL_HISTORY) {
        third_local_history_table_.get_history(
          br_pc) = prediction_info.history_snapshot.third_local_history;
      }
    }
    if(CONFIG::SC::USE_IMLI) {
      imli_counter_.set(prediction_info.history_snapshot.imli_counter);
      imli_table_[imli_counter_.get()] =
        prediction_info.history_snapshot.imli_local_history;
    }
  }

 private:
  using Counter_Type = Saturating_Counter<CONFIG::SC::PRECISION, true>;
  using Per_PC_Threshold_Table_Type =
    Threshold_Table<CONFIG::SC::PERPC_UPDATE_THRESHOLD_WIDTH,
                    CONFIG::SC::LOG_SIZE_PERPC_THRESHOLD_TABLE>;
  using Variable_Threshold_Table_Type =
    Threshold_Table<CONFIG::SC::VARIABLE_THRESHOLD_WIDTH,
                    CONFIG::SC::LOG_SIZE_VARIABLE_THRESHOLD_TABLE>;

  void initialize_bias_tables(void);

  int get_threshold_table_index(uint64_t br_pc);

  template <int PRECISION, class Gehl_Histories, int gehl_log_table_size,
            int threshold_width, int log_threshold_table_size>
  int get_gehl_prediction_sum(
    const Gehl<PRECISION, Gehl_Histories, gehl_log_table_size>& gehl,
    const Threshold_Table<threshold_width, log_threshold_table_size>&
             threshold_table,
    uint64_t br_pc, int64_t history) const {
    int prediction = gehl.get_prediction_sum(br_pc, history);
    if(CONFIG::SC::USE_VARIABLE_THRESHOLD) {
      if(threshold_table.get_entry(br_pc).get() >= 0) {
        prediction *= 2;
      }
    }
    return prediction;
  }

  template <int PRECISION, class Gehl_Histories, int gehl_log_table_size,
            int threshold_width, int log_threshold_table_size>
  void update_gehl_and_threshold(
    Gehl<PRECISION, Gehl_Histories, gehl_log_table_size>*       gehl,
    Threshold_Table<threshold_width, log_threshold_table_size>* threshold_table,
    uint64_t br_pc, int64_t history, bool resolve_dir,
    int total_prediction_sum) {
    int gehl_sum = gehl->get_prediction_sum(br_pc, history);
    gehl->update(br_pc, history, resolve_dir);

    if(CONFIG::SC::USE_VARIABLE_THRESHOLD) {
      int total_sum_without_doubled_gehl =
        total_prediction_sum -
        (threshold_table->get_entry(br_pc).get() >= 0) * gehl_sum;
      bool prediction_without_multiplier = (total_sum_without_doubled_gehl >=
                                            0);
      bool prediction_with_multiplier    = (total_sum_without_doubled_gehl +
                                           gehl_sum >=
                                         0);
      if(prediction_without_multiplier != prediction_with_multiplier) {
        threshold_table->get_entry(br_pc).update((gehl_sum >= 0) ==
                                                 resolve_dir);
      }
    }
  }

  int get_bias_table_index(
    uint64_t                                           br_pc,
    const Tage_Prediction_Info<typename CONFIG::TAGE>& tage_prediction_info,
    bool                                               tage_or_loop_prediction);

  int get_bias_sk_table_index(
    uint64_t                                           br_pc,
    const Tage_Prediction_Info<typename CONFIG::TAGE>& tage_prediction_info,
    bool                                               tage_or_loop_prediction);

  int get_bias_bank_table_index(
    uint64_t                                           br_pc,
    const Tage_Prediction_Info<typename CONFIG::TAGE>& tage_prediction_info,
    bool                                               tage_or_loop_prediction);

  int64_t global_history_ = 0;
  int64_t path_           = 0;
  Local_History_Table<CONFIG::SC::FIRST_LOCAL_HISTORY_LOG_TABLE_SIZE,
                      CONFIG::SC::FIRST_LOCAL_HISTORY_SHIFT>
    first_local_history_table_;
  Local_History_Table<CONFIG::SC::SECOND_LOCAL_HISTORY_LOG_TABLE_SIZE,
                      CONFIG::SC::SECOND_LOCAL_HISTORY_SHIFT>
    second_local_history_table_;
  Local_History_Table<CONFIG::SC::THIRD_LOCAL_HISTORY_LOG_TABLE_SIZE,
                      CONFIG::SC::THIRD_LOCAL_HISTORY_SHIFT>
                                                            third_local_history_table_;
  Saturating_Counter<CONFIG::SC::IMLI_COUNTER_WIDTH, false> imli_counter_;
  int64_t imli_table_[CONFIG::SC::IMLI_TABLE_SIZE];

  Saturating_Counter<CONFIG::CONFIDENCE_COUNTER_WIDTH, true>
    first_high_confidence_ctr_;
  Saturating_Counter<CONFIG::CONFIDENCE_COUNTER_WIDTH, true>
    second_high_confidence_ctr_;

  Saturating_Counter<CONFIG::SC::UPDATE_THRESHOLD_WIDTH, true>
                              update_threshold_;
  Per_PC_Threshold_Table_Type p_update_thresholds_;

  Gehl<CONFIG::SC::PRECISION,
       typename CONFIG::SC::GLOBAL_HISTORY_GEHL_HISTORIES,
       CONFIG::SC::LOG_SIZE_GLOBAL_HISTORY_GEHL>
    global_history_gehl_;
  Gehl<CONFIG::SC::PRECISION, typename CONFIG::SC::PATH_GEHL_HISTORIES,
       CONFIG::SC::LOG_SIZE_PATH_GEHL>
    path_gehl_;
  Gehl<CONFIG::SC::PRECISION, typename CONFIG::SC::FIRST_LOCAL_GEHL_HISTORIES,
       CONFIG::SC::LOG_SIZE_FIRST_LOCAL_GEHL>
    first_local_gehl_;
  Gehl<CONFIG::SC::PRECISION, typename CONFIG::SC::SECOND_LOCAL_GEHL_HISTORIES,
       CONFIG::SC::LOG_SIZE_SECOND_LOCAL_GEHL>
    second_local_gehl_;
  Gehl<CONFIG::SC::PRECISION, typename CONFIG::SC::THIRD_LOCAL_GEHL_HISTORIES,
       CONFIG::SC::LOG_SIZE_THIRD_LOCAL_GEHL>
    third_local_gehl_;
  Gehl<CONFIG::SC::PRECISION, typename CONFIG::SC::FIRST_IMLI_GEHL_HISTORIES,
       CONFIG::SC::log_size_first_imli_gehl>
    first_imli_gehl_;
  Gehl<CONFIG::SC::PRECISION, typename CONFIG::SC::SECOND_IMLI_GEHL_HISTORIES,
       CONFIG::SC::LOG_SIZE_SECOND_IMLI_GEHL>
    second_imli_gehl_;

  Variable_Threshold_Table_Type global_history_threshold_table_;
  Variable_Threshold_Table_Type path_threshold_table_;
  Variable_Threshold_Table_Type first_local_threshold_table_;
  Variable_Threshold_Table_Type second_local_threshold_table_;
  Variable_Threshold_Table_Type third_local_threshold_table_;
  Variable_Threshold_Table_Type first_imli_threshold_table_;
  Variable_Threshold_Table_Type second_imli_threshold_table_;
  Variable_Threshold_Table_Type bias_threshold_table_;

  std::vector<Counter_Type> bias_table_;
  std::vector<Counter_Type> bias_sk_table_;
  std::vector<Counter_Type> bias_bank_table_;
};

template <class CONFIG>
Statistical_Corrector<CONFIG>::Statistical_Corrector() :
    first_local_history_table_(), second_local_history_table_(),
    third_local_history_table_(), imli_counter_(0), imli_table_(),
    first_high_confidence_ctr_(0), second_high_confidence_ctr_(0),
    update_threshold_(CONFIG::SC::INITIAL_UPDATE_THRESHOLD),
    p_update_thresholds_(0), global_history_gehl_(), path_gehl_(),
    first_local_gehl_(), second_local_gehl_(), third_local_gehl_(),
    first_imli_gehl_(), second_imli_gehl_(),
    global_history_threshold_table_(CONFIG::SC::INITIAL_VARIABLE_THRESHOLD),
    path_threshold_table_(CONFIG::SC::INITIAL_VARIABLE_THRESHOLD),
    first_local_threshold_table_(CONFIG::SC::INITIAL_VARIABLE_THRESHOLD),
    second_local_threshold_table_(CONFIG::SC::INITIAL_VARIABLE_THRESHOLD),
    third_local_threshold_table_(CONFIG::SC::INITIAL_VARIABLE_THRESHOLD),
    first_imli_threshold_table_(CONFIG::SC::INITIAL_VARIABLE_THRESHOLD),
    second_imli_threshold_table_(0),
    bias_threshold_table_(CONFIG::SC::INITIAL_VARIABLE_THRESHOLD_FOR_BIAS),
    bias_table_(1 << CONFIG::SC::LOG_BIAS_ENTRIES, Counter_Type(0)),
    bias_sk_table_(1 << CONFIG::SC::LOG_BIAS_ENTRIES, Counter_Type(0)),
    bias_bank_table_(1 << CONFIG::SC::LOG_BIAS_ENTRIES, Counter_Type(0)) {
  initialize_bias_tables();
};

template <class CONFIG>
void Statistical_Corrector<CONFIG>::get_prediction(
  uint64_t                                           br_pc,
  const Tage_Prediction_Info<typename CONFIG::TAGE>& tage_prediction_info,
  bool tage_or_loop_prediction, SC_Prediction_Info* prediction_info) {
  int components_sum = 0;
  int thresholds_sum = (update_threshold_.get() >> 3) +
                       p_update_thresholds_.get_entry(br_pc).get();

  // Add bias.
  int bias_table_index = get_bias_table_index(br_pc, tage_prediction_info,
                                              tage_or_loop_prediction);
  components_sum += 2 * bias_table_[bias_table_index].get() + 1;

  // Add bias_sk.
  int bias_sk_table_index = get_bias_sk_table_index(br_pc, tage_prediction_info,
                                                    tage_or_loop_prediction);
  components_sum += 2 * bias_sk_table_[bias_sk_table_index].get() + 1;

  // Add bias_sk.
  int bias_bank_table_index = get_bias_bank_table_index(
    br_pc, tage_prediction_info, tage_or_loop_prediction);
  components_sum += 2 * bias_bank_table_[bias_bank_table_index].get() + 1;

  if(CONFIG::SC::USE_VARIABLE_THRESHOLD) {
    if(bias_threshold_table_.get_entry(br_pc).get() >= 0) {
      components_sum *= 2;
      thresholds_sum += 12;
    }
  }

  // Add global history and path GEHL components.
  components_sum += get_gehl_prediction_sum(
    global_history_gehl_, global_history_threshold_table_,
    (br_pc << 1) + (tage_or_loop_prediction ? 1 : 0), global_history_);
  components_sum += get_gehl_prediction_sum(path_gehl_, path_threshold_table_,
                                            br_pc, path_);

  if(CONFIG::SC::USE_VARIABLE_THRESHOLD) {
    thresholds_sum += 12 *
                      (global_history_threshold_table_.get_entry(br_pc).get() >=
                       0);
    thresholds_sum += 12 * (path_threshold_table_.get_entry(br_pc).get() >= 0);
  }

  // Add local history GEHL components.
  if(CONFIG::SC::USE_LOCAL_HISTORY) {
    components_sum += get_gehl_prediction_sum(
      first_local_gehl_, first_local_threshold_table_, br_pc,
      first_local_history_table_.get_history(br_pc));

    if(CONFIG::SC::USE_SECOND_LOCAL_HISTORY) {
      components_sum += get_gehl_prediction_sum(
        second_local_gehl_, second_local_threshold_table_, br_pc,
        second_local_history_table_.get_history(br_pc));
    }
    if(CONFIG::SC::USE_THIRD_LOCAL_HISTORY) {
      components_sum += get_gehl_prediction_sum(
        third_local_gehl_, third_local_threshold_table_, br_pc,
        third_local_history_table_.get_history(br_pc));
    }
  }
  if(CONFIG::SC::USE_VARIABLE_THRESHOLD) {
    thresholds_sum += 12 *
                      (first_local_threshold_table_.get_entry(br_pc).get() >=
                       0);
    if(CONFIG::SC::USE_SECOND_LOCAL_HISTORY) {
      thresholds_sum += 12 *
                        (second_local_threshold_table_.get_entry(br_pc).get() >=
                         0);
    }
    if(CONFIG::SC::USE_THIRD_LOCAL_HISTORY) {
      thresholds_sum += 12 *
                        (third_local_threshold_table_.get_entry(br_pc).get() >=
                         0);
    }
  }
  if(CONFIG::SC::USE_IMLI) {
    components_sum += get_gehl_prediction_sum(
      second_imli_gehl_, second_imli_threshold_table_, br_pc,
      imli_table_[imli_counter_.get()]);
    components_sum += get_gehl_prediction_sum(first_imli_gehl_,
                                              first_imli_threshold_table_,
                                              br_pc, imli_counter_.get());
    if(CONFIG::SC::USE_VARIABLE_THRESHOLD) {
      thresholds_sum += 12 *
                        (first_imli_threshold_table_.get_entry(br_pc).get() >=
                         0);
      // REVISIT: addition of the second IMLI component because it does
      // not
      // exist is Seznec's version, but it's probably a bug and should be
      // added
      // in.
      // thresholds_sum += (second_imli_threshold_table_.get_entry(br_pc)
      // >=
      // 0);
    }
  }

  prediction_info->gehls_sum      = components_sum;
  prediction_info->thresholds_sum = thresholds_sum;
  bool sc_prediction              = components_sum >= 0;

#ifdef DEBUG_PRINTS
  std::cout << "Tage pred: " << (int)tage_prediction_info.prediction
            << std::endl;
  std::cout << "Tage or loop pred: " << (int)tage_or_loop_prediction
            << std::endl;
  std::cout << "High confidence: " << (int)tage_prediction_info.high_confidence
            << std::endl;
  std::cout << "Medium confidence: "
            << (int)tage_prediction_info.medium_confidence << std::endl;
  std::cout << "Low confidence: " << (int)tage_prediction_info.low_confidence
            << std::endl;
  std::cout << "longest match prediction: "
            << (int)tage_prediction_info.longest_match_prediction << std::endl;
  std::cout << "alt prediction: " << (int)tage_prediction_info.alt_prediction
            << std::endl;
  std::cout << "alt confidence: " << (int)tage_prediction_info.alt_confidence
            << std::endl;
  std::cout << "hit bank: " << tage_prediction_info.hit_bank << std::endl;
  std::cout << "alt bank: " << tage_prediction_info.alt_bank << std::endl;
  std::cout << "gehl sum: " << components_sum << std::endl;
  std::cout << "thrs sum: " << thresholds_sum << std::endl;
#endif

  if(sc_prediction != tage_or_loop_prediction) {
    prediction_info->prediction = sc_prediction;

    if(tage_prediction_info.high_confidence) {
      if(std::abs(components_sum) < (thresholds_sum / 4)) {
        prediction_info->prediction = tage_or_loop_prediction;
      } else if(std::abs(components_sum) < (thresholds_sum / 2)) {
        prediction_info->prediction = (second_high_confidence_ctr_.get() < 0) ?
                                        sc_prediction :
                                        tage_or_loop_prediction;
      }
    }

    if(tage_prediction_info.medium_confidence) {
      if(std::abs(components_sum) < (thresholds_sum / 4)) {
        prediction_info->prediction = (first_high_confidence_ctr_.get() < 0) ?
                                        sc_prediction :
                                        tage_or_loop_prediction;
      } else {
        prediction_info->prediction = sc_prediction;
      }
    }
  } else {
    // SC and Tage_L predictions are equal, it does not matter which one we
    // choose.
    prediction_info->prediction = tage_or_loop_prediction;
  }
}

template <class CONFIG>
void Statistical_Corrector<CONFIG>::commit_state(
  uint64_t br_pc, bool resolve_dir,
  const Tage_Prediction_Info<typename CONFIG::TAGE>& tage_prediction_info,
  const SC_Prediction_Info& sc_prediction_info, bool tage_or_loop_prediction) {
  imli_table_[imli_counter_.get()];
  bool sc_prediction = (sc_prediction_info.gehls_sum >= 0);
  if(tage_or_loop_prediction != sc_prediction) {
    // REVIST: the first if statement seems to be redundant
    if(std::abs(sc_prediction_info.gehls_sum) <
       sc_prediction_info.thresholds_sum) {
      if(tage_prediction_info.high_confidence &&
         (std::abs(sc_prediction_info.gehls_sum) <
          sc_prediction_info.thresholds_sum / 2) &&
         (std::abs(sc_prediction_info.gehls_sum) >=
          sc_prediction_info.thresholds_sum / 4)) {
        second_high_confidence_ctr_.update(tage_or_loop_prediction ==
                                           resolve_dir);
      }
    }
    if(tage_prediction_info.medium_confidence &&
       (std::abs(sc_prediction_info.gehls_sum) <
        sc_prediction_info.thresholds_sum / 4)) {
      first_high_confidence_ctr_.update(tage_or_loop_prediction == resolve_dir);
    }
  }

  if(sc_prediction != resolve_dir || std::abs(sc_prediction_info.gehls_sum) <
                                       sc_prediction_info.thresholds_sum) {
    // update threshold counters.
    if(sc_prediction != resolve_dir) {
      update_threshold_.increment();
      p_update_thresholds_.get_entry(br_pc).increment();
    } else {
      update_threshold_.decrement();
      p_update_thresholds_.get_entry(br_pc).decrement();
    }

    // update biases.
    int bias_table_index    = get_bias_table_index(br_pc, tage_prediction_info,
                                                tage_or_loop_prediction);
    int bias_sk_table_index = get_bias_sk_table_index(
      br_pc, tage_prediction_info, tage_or_loop_prediction);
    int bias_bank_table_index = get_bias_bank_table_index(
      br_pc, tage_prediction_info, tage_or_loop_prediction);

    if(CONFIG::SC::USE_VARIABLE_THRESHOLD) {
      int biases_sum = 2 * bias_table_[bias_table_index].get() + 1;
      biases_sum += 2 * bias_sk_table_[bias_sk_table_index].get() + 1;
      biases_sum += 2 * bias_bank_table_[bias_bank_table_index].get() + 1;

      int gehls_sum_without_doubled_biases =
        sc_prediction_info.gehls_sum -
        (bias_threshold_table_.get_entry(br_pc).get() >= 0) * biases_sum;

      bool prediction_without_multiplier = (gehls_sum_without_doubled_biases >=
                                            0);
      bool prediction_with_multiplier    = (gehls_sum_without_doubled_biases +
                                           biases_sum >=
                                         0);
      if(prediction_without_multiplier != prediction_with_multiplier) {
        bias_threshold_table_.get_entry(br_pc).update((biases_sum >= 0) ==
                                                      resolve_dir);
      }
    }
    bias_table_[bias_table_index].update(resolve_dir);
    bias_sk_table_[bias_sk_table_index].update(resolve_dir);
    bias_bank_table_[bias_bank_table_index].update(resolve_dir);

    update_gehl_and_threshold(
      &global_history_gehl_, &global_history_threshold_table_,
      (br_pc << 1) + (tage_or_loop_prediction ? 1 : 0),
      sc_prediction_info.history_snapshot.global_history, resolve_dir,
      sc_prediction_info.gehls_sum);
    update_gehl_and_threshold(&path_gehl_, &path_threshold_table_, br_pc,
                              sc_prediction_info.history_snapshot.path,
                              resolve_dir, sc_prediction_info.gehls_sum);

    if(CONFIG::SC::USE_LOCAL_HISTORY) {
      update_gehl_and_threshold(
        &first_local_gehl_, &first_local_threshold_table_, br_pc,
        sc_prediction_info.history_snapshot.first_local_history, resolve_dir,
        sc_prediction_info.gehls_sum);
      if(CONFIG::SC::USE_SECOND_LOCAL_HISTORY) {
        update_gehl_and_threshold(
          &second_local_gehl_, &second_local_threshold_table_, br_pc,
          sc_prediction_info.history_snapshot.second_local_history, resolve_dir,
          sc_prediction_info.gehls_sum);
      }
      if(CONFIG::SC::USE_THIRD_LOCAL_HISTORY) {
        update_gehl_and_threshold(
          &third_local_gehl_, &third_local_threshold_table_, br_pc,
          sc_prediction_info.history_snapshot.third_local_history, resolve_dir,
          sc_prediction_info.gehls_sum);
      }
    }

    if(CONFIG::SC::USE_IMLI) {
      update_gehl_and_threshold(
        &second_imli_gehl_, &second_imli_threshold_table_, br_pc,
        sc_prediction_info.history_snapshot.imli_local_history, resolve_dir,
        sc_prediction_info.gehls_sum);
      update_gehl_and_threshold(
        &first_imli_gehl_, &first_imli_threshold_table_, br_pc,
        sc_prediction_info.history_snapshot.imli_counter, resolve_dir,
        sc_prediction_info.gehls_sum);
    }
  }
}

template <class CONFIG>
void Statistical_Corrector<CONFIG>::update_speculative_state(
  uint64_t br_pc, bool resolve_dir, uint64_t br_target, Branch_Type br_type,
  SC_Prediction_Info* prediction_info) {
  prediction_info->history_snapshot.global_history = global_history_;
  prediction_info->history_snapshot.path           = path_;
  if(CONFIG::SC::USE_LOCAL_HISTORY) {
    prediction_info->history_snapshot.first_local_history =
      first_local_history_table_.get_history(br_pc);
    if(CONFIG::SC::USE_SECOND_LOCAL_HISTORY) {
      prediction_info->history_snapshot.second_local_history =
        second_local_history_table_.get_history(br_pc);
    }
    if(CONFIG::SC::USE_THIRD_LOCAL_HISTORY) {
      prediction_info->history_snapshot.third_local_history =
        third_local_history_table_.get_history(br_pc);
    }
  }
  if(CONFIG::SC::USE_IMLI) {
    prediction_info->history_snapshot.imli_counter = imli_counter_.get();
    prediction_info->history_snapshot.imli_local_history =
      imli_table_[imli_counter_.get()];
  }

  if((br_type.is_conditional) && CONFIG::SC::USE_IMLI) {
    int table_index          = imli_counter_.get();
    imli_table_[table_index] = (imli_table_[table_index] << 1) + resolve_dir;
    if(br_target < br_pc) {
      // This branch corresponds to a loop
      if(!resolve_dir) {
        // exit of the "loop"
        imli_counter_.set(0);
      } else {
        imli_counter_.increment();
      }
    }
  }

  if(br_type.is_conditional) {
    global_history_ = (global_history_ << 1) +
                      (resolve_dir & (br_target < br_pc));
    int64_t& first_local_history = first_local_history_table_.get_history(
      br_pc);
    first_local_history = (first_local_history << 1) + resolve_dir;

    int64_t& second_local_history = second_local_history_table_.get_history(
      br_pc);
    second_local_history = ((second_local_history << 1) + resolve_dir) ^
                           (br_pc & 15);

    int64_t& third_local_history = third_local_history_table_.get_history(
      br_pc);
    third_local_history = (third_local_history << 1) + resolve_dir;
  }

  // REVIST: redoing the path update already done in Tage. Tage and Sc
  // should probably share the same histories and TAGE_SC_L should be
  // responsible for updates.
  int num_bit_inserts = 2;
  if(br_type.is_conditional) {
    num_bit_inserts = 2;
  } else if((br_type.is_indirect)) {  // unconditional indirect branches
    num_bit_inserts = 3;
  }
  int path_hash = br_pc ^ (br_pc >> 2) ^ (br_pc >> 4);
  if((br_type.is_conditional && br_type.is_indirect) & resolve_dir) {
    path_hash = path_hash ^ (br_target >> 2) ^ (br_target >> 4);
  }

  for(int i = 0; i < num_bit_inserts; ++i) {
    path_ = (path_ << 1) ^ (path_hash & 127);
    path_hash >>= 1;
  }
  path_ = path_ & ((1 << CONFIG::SC::SC_PATH_HISTORY_WIDTH) - 1);
}

template <class CONFIG>
void Statistical_Corrector<CONFIG>::initialize_bias_tables(void) {
  int min_value = -(1 << (CONFIG::SC::PRECISION - 1));
  int max_value = (1 << (CONFIG::SC::PRECISION - 1)) - 1;
  for(int i = 0; i < (1 << CONFIG::SC::LOG_BIAS_ENTRIES); ++i) {
    switch(i & 3) {
      case 0:
        bias_table_[i].set(min_value);
        bias_bank_table_[i].set(min_value);
        bias_sk_table_[i].set(min_value / 4);
        break;
      case 1:
        bias_table_[i].set(max_value);
        bias_bank_table_[i].set(max_value);
        bias_sk_table_[i].set(max_value / 4);
        break;
      case 2:
        bias_table_[i].set(-1);
        bias_bank_table_[i].set(-1);
        bias_sk_table_[i].set(min_value);
        break;
      case 3:
        bias_table_[i].set(0);
        bias_bank_table_[i].set(0);
        bias_sk_table_[i].set(max_value);
        break;
    }
  }
}

template <class CONFIG>
int Statistical_Corrector<CONFIG>::get_threshold_table_index(uint64_t br_pc) {
  return (br_pc ^ (br_pc >> 2)) &
         ((1 << CONFIG::SC::LOG_SIZE_VARIABLE_THRESHOLD_TABLE) - 1);
}

template <class CONFIG>
int Statistical_Corrector<CONFIG>::get_bias_table_index(
  uint64_t                                           br_pc,
  const Tage_Prediction_Info<typename CONFIG::TAGE>& tage_prediction_info,
  bool                                               tage_or_loop_prediction) {
  int index = ((br_pc ^ (br_pc >> 2)) << 1);
  index ^= tage_prediction_info.low_confidence &
           (tage_prediction_info.longest_match_prediction !=
            tage_prediction_info.alt_prediction);
  index = (index << 1) + tage_or_loop_prediction;
  index = index & ((1 << CONFIG::SC::LOG_BIAS_ENTRIES) - 1);
  return index;
}

template <class CONFIG>
int Statistical_Corrector<CONFIG>::get_bias_sk_table_index(
  uint64_t                                           br_pc,
  const Tage_Prediction_Info<typename CONFIG::TAGE>& tage_prediction_info,
  bool                                               tage_or_loop_prediction) {
  int index = ((br_pc ^ (br_pc >> (CONFIG::SC::LOG_BIAS_ENTRIES - 2))) << 1);
  index ^= tage_prediction_info.high_confidence;
  index = (index << 1) + tage_or_loop_prediction;
  index = index & ((1 << CONFIG::SC::LOG_BIAS_ENTRIES) - 1);
  return index;
}

template <class CONFIG>
int Statistical_Corrector<CONFIG>::get_bias_bank_table_index(
  uint64_t                                           br_pc,
  const Tage_Prediction_Info<typename CONFIG::TAGE>& tage_prediction_info,
  bool                                               tage_or_loop_prediction) {
  int index = (br_pc ^ (br_pc >> 2)) << 7;
  index += ((tage_prediction_info.hit_bank + 1) / 4) << 4;
  index += (tage_prediction_info.alt_bank != 0) << 3;
  index += tage_prediction_info.low_confidence << 2;
  index += tage_prediction_info.high_confidence << 1;
  index += tage_or_loop_prediction;
  index = index & ((1 << CONFIG::SC::LOG_BIAS_ENTRIES) - 1);
  return index;
}

#endif  // __STATISTICAL_CORRECTOR_H_
