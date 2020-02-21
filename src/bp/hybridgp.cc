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

#include "bp/hybridgp.h"

#include <vector>

extern "C" {
#include "bp/bp.param.h"
#include "globals/assert.h"
#include "libs/cache_lib.h"
#include "libs/hash_lib.h"
#include "statistics.h"
}

#include "bp/template_lib/utils.h"

#define PHT_INIT_VALUE (1 << (PHT_CTR_BITS - 1)) /* weakly taken */
#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_BP_DIR, ##args)

namespace {
FILE* brmispred = NULL;

struct Hybridgp_In_Flight_State {
  bool  updated_local_history;
  uns32 pred_phist;
  Addr  bht_addr;
};

struct Hybridgp_State {
  Cache              bht;
  Hash_Table         bht_hash;
  std::vector<uns8>  hybspht;
  std::vector<uns8>  hybgpht;
  std::vector<uns8>  hybppht;
  Hash_Table         hybgpht_hash;
  std::vector<uns32> filter;

  // Used for update and recovery (checkpointing).
  Circular_Buffer<Hybridgp_In_Flight_State> in_flight;

  Hybridgp_State(uns max_in_flight_branches) :
      in_flight(max_in_flight_branches) {}
};

std::vector<Hybridgp_State> hybridgp_state_all_cores;

uns32* get_local_history_entry(Hybridgp_State& hybridgp_state,
                               const Addr      addr) {
  if(INF_HYBRIDGP) {
    Flag        new_entry = 0;
    const int64 key       = addr;

    uns32* local_hist_entry = (uns32*)hash_table_access_create(
      &hybridgp_state.bht_hash, key, &new_entry);

    if(new_entry)
      *local_hist_entry = 0;

    return local_hist_entry;

  } else {
    Addr bht_line_addr;
    return (uns32*)cache_access(&hybridgp_state.bht, addr, &bht_line_addr,
                                TRUE);
  }
}

uns32 get_local_history(Hybridgp_State& hybridgp_state, const Addr addr) {
  uns32* local_history_entry = get_local_history_entry(hybridgp_state, addr);
  if(local_history_entry) {
    return *local_history_entry;
  } else {
    return 0;
  }
}

void update_local_history(Hybridgp_State& hybridgp_state, const uns proc_id,
                          const Addr addr, const Flag new_dir) {
  uns32* local_history_entry = get_local_history_entry(hybridgp_state, addr);
  if(local_history_entry) {
    *local_history_entry >>= 1;
    *local_history_entry |= new_dir << 31;
  } else {
    ASSERT(proc_id, INF_HYBRIDGP == FALSE);
    Addr   bht_line_addr, repl_line_addr;
    uns32* bht_line = (uns32*)cache_insert(&hybridgp_state.bht, proc_id, addr,
                                           &bht_line_addr, &repl_line_addr);
    *bht_line       = new_dir << 31;
  }
}

struct Hybridgp_Indices {
  uns32 spht;
  uns32 gpht;
  uns32 ppht;
  uns32 filter;
};

Hybridgp_Indices cook_indices(const Addr addr, const uns32 ghist,
                              const uns32 phist) {
  auto cook_history = [](auto hist, auto length, auto shift_factor) -> uns32 {
    return hist >> (32 - (length - length / shift_factor));
  };

  auto cook_addr = [](auto addr, auto width) -> uns32 {
    return (addr >> 2) & N_BIT_MASK(width);
  };

  auto cook_gindex = [](auto hist, auto addr, auto length,
                        auto shift_factor) -> uns32 {
    const uns32 component1 = (hist ^ addr) << (length / shift_factor) &
                             N_BIT_MASK(length);
    const uns32 component2 = addr & N_BIT_MASK(length / shift_factor);
    return component1 | component2;
  };

  auto cook_pindex = [](auto hist, auto addr, auto length,
                        auto shift_factor) -> uns32 {
    const uns32 component1 = hist & N_BIT_MASK(length / shift_factor);
    const uns32 component2 = addr & N_BIT_MASK(length / shift_factor);
    const uns32 component3 = addr & (N_BIT_MASK(length / shift_factor)
                                     << (length / shift_factor));
    return (component1 ^ component2) | component3;
  };

  const uns32 cooked_ghist = cook_history(ghist, HYBRIDG_HIST_LENGTH, 5);
  const uns32 cooked_phist = cook_history(phist, HYBRIDP_HIST_LENGTH, 2);
  const uns32 cooked_saddr = cook_addr(addr, HYBRIDS_INDEX_LENGTH);
  const uns32 cooked_gaddr = cook_addr(addr, HYBRIDG_HIST_LENGTH);
  const uns32 cooked_paddr = cook_addr(addr, HYBRIDP_HIST_LENGTH);
  const uns32 cooked_faddr = cook_addr(addr, FILTER_INDEX_LENGTH);
  const uns32 spht_index   = cook_gindex(cooked_ghist, cooked_saddr,
                                       HYBRIDS_INDEX_LENGTH, 5);
  const uns32 gpht_index   = cook_gindex(cooked_ghist, cooked_gaddr,
                                       HYBRIDG_HIST_LENGTH, 5);
  const uns32 ppht_index   = cook_pindex(cooked_phist, cooked_paddr,
                                       HYBRIDP_HIST_LENGTH, 2);

  return {spht_index, gpht_index, ppht_index, cooked_faddr};
}

struct Loop_Filter_Features {
  uns32 packed_entry;
  bool  dir;
  bool  is_counter_max;
  bool  use_counter;
  bool  end_loop;
  bool  repeated_loop;
  bool  long_loop;
};

Loop_Filter_Features get_loop_filter_features(
  const Hybridgp_State& hybridgp_state, const uns32 filter_index) {
  const uns32 packed_entry   = hybridgp_state.filter[filter_index];
  const uns32 counter        = packed_entry & 0x7f;
  const uns32 last_max       = (packed_entry & 0x7f00) >> 0x8;
  const uns32 repeat_counter = (packed_entry & 0xf0000) >> 0x10;

  const bool dir            = (packed_entry & 0x80) >> 0x7;
  const bool is_counter_max = (counter >= 126);
  const bool use_counter    = (counter >= 31);
  const bool end_loop       = (counter == last_max);
  const bool repeated_loop  = (repeat_counter >= 7);
  const bool long_loop      = ((repeat_counter >= 2) & ((last_max) >= 7));

  return {packed_entry, dir,           is_counter_max, use_counter,
          end_loop,     repeated_loop, long_loop};
}

bool is_loop_filter_prediction_valid(const Loop_Filter_Features features) {
  return features.is_counter_max ||
         (features.use_counter && (!features.end_loop)) || features.long_loop ||
         features.repeated_loop;
}

bool get_loop_filter_prediction(const Loop_Filter_Features features) {
  return (features.end_loop & (!features.is_counter_max)) ^ features.dir;
}

void update_loop_filter(Op* op, Hybridgp_State& hybridgp_state,
                        const uns32                 filter_index,
                        const Loop_Filter_Features& features) {
  auto update_counter = [](uns32 old_counter, uns32 maximum,
                           bool is_correct) -> uns32 {
    return is_correct ? (old_counter == maximum ? maximum : old_counter + 1) :
                        0;
  };

  const uns32 counter        = features.packed_entry & 0x7f;
  const uns32 repeat_counter = (features.packed_entry & 0xf0000) >> 0x10;

  if(features.dir == op->oracle_info.dir) {
    hybridgp_state.filter[filter_index] = (0xFFF00000 |
                                           (features.packed_entry &
                                            0xFFFFFF80) |
                                           update_counter(counter, 126, true));
  } else {
    const uns32 new_repeat_counter = update_counter(repeat_counter, 7,
                                                    features.end_loop);
    const uns32 new_last_max       = counter;
    const uns32 new_counter        = counter ? 0 : 1;
    const uns32 new_dir            = counter ? features.dir : !features.dir;
    const uns32 new_packed_entry = (0xFFF00000 | (new_repeat_counter << 0x10) |
                                    (new_last_max << 0x8) | (new_dir << 0x7) |
                                    (new_counter));
    hybridgp_state.filter[filter_index] = new_packed_entry;
  }
}

bool get_spred(const Hybridgp_State& hybridgp_state, const uns32 spht_index) {
  const auto spht_entry = hybridgp_state.hybspht[spht_index];
  return spht_entry >> (PHT_CTR_BITS - 1);
}

bool get_gpred(Op* op, Hybridgp_State& hybridgp_state, const Addr addr,
               const uns32 gpht_index) {
  uns8 gpht_entry;
  if(INF_HYBRIDGP) {
    Flag        new_entry;
    const int64 key = addr << 32 | (Addr)op->oracle_info.pred_global_hist;
    uns8* entry = (uns8*)hash_table_access_create(&hybridgp_state.hybgpht_hash,
                                                  key, &new_entry);
    if(new_entry) {
      *entry = PHT_INIT_VALUE;
    }
    gpht_entry                      = *entry;
    op->oracle_info.pred_gpht_entry = entry;  // need for update
  } else {
    gpht_entry = hybridgp_state.hybgpht[gpht_index];
  }
  return gpht_entry >> (PHT_CTR_BITS - 1);
}

bool get_ppred(const Hybridgp_State& hybridgp_state, const uns32 ppht_index) {
  const auto ppht_entry = hybridgp_state.hybppht[ppht_index];
  return ppht_entry >> (PHT_CTR_BITS - 1);
}

void update_all_phts(const Op* op, Hybridgp_State& hybridgp_state,
                     const Hybridgp_Indices& indices) {
  uns8* gpht_entry = INF_HYBRIDGP ? op->oracle_info.pred_gpht_entry :
                                    &hybridgp_state.hybgpht[indices.gpht];
  uns8* spht_entry = &hybridgp_state.hybspht[indices.spht];
  uns8* ppht_entry = &hybridgp_state.hybppht[indices.ppht];

  const uns8 gpred = USE_FILTER ? (*gpht_entry >> (PHT_CTR_BITS - 1)) :
                                  op->oracle_info.hybridgp_gpred;
  const uns8 ppred = *ppht_entry >> (PHT_CTR_BITS - 1);

  DEBUG(op->proc_id, "Writing hybridgp PHT for op_num:%s\n",
        unsstr64(op->op_num));


  if(op->oracle_info.dir) {
    *gpht_entry = SAT_INC(*gpht_entry, N_BIT_MASK(PHT_CTR_BITS));
    *ppht_entry = SAT_INC(*ppht_entry, N_BIT_MASK(PHT_CTR_BITS));
  } else {
    *gpht_entry = SAT_DEC(*gpht_entry, 0);
    *ppht_entry = SAT_DEC(*ppht_entry, 0);
  }

  if((gpred == op->oracle_info.dir) && (ppred != op->oracle_info.dir)) {
    *spht_entry = SAT_INC(*spht_entry, N_BIT_MASK(PHT_CTR_BITS));
  } else if((gpred != op->oracle_info.dir) && (ppred == op->oracle_info.dir)) {
    *spht_entry = SAT_DEC(*spht_entry, 0);
  }
}

}  // namespace

void bp_hybridgp_init() {
  for(uns i = 0; i < NUM_CORES; ++i) {
    hybridgp_state_all_cores.emplace_back(NODE_TABLE_SIZE);
  }
  for(auto& hybridgp_state : hybridgp_state_all_cores) {
    hybridgp_state.hybspht.resize(1 << HYBRIDS_INDEX_LENGTH, PHT_INIT_VALUE);
    hybridgp_state.hybppht.resize(1 << HYBRIDP_HIST_LENGTH, PHT_INIT_VALUE);
    if(INF_HYBRIDGP) {
      // only the gpht and the bht are interference free
      init_hash_table(&hybridgp_state.bht_hash, "", 1 << 16, sizeof(uns32));
      init_hash_table(&hybridgp_state.hybgpht_hash, "", 1 << 16, sizeof(uns8));
    } else {
      // line size for table set to 1
      init_cache(&hybridgp_state.bht, "BHT", BHT_ENTRIES, BHT_ASSOC, 1,
                 sizeof(Addr), REPL_TRUE_LRU);
      hybridgp_state.hybgpht.resize(1 << HYBRIDG_HIST_LENGTH, PHT_INIT_VALUE);
    }

    hybridgp_state.filter.resize(1 << FILTER_INDEX_LENGTH, 0);

    if(BR_MISPRED_FILE) {
      char* brmispredfile = BR_MISPRED_FILE;
      brmispred           = fopen(brmispredfile, "w");
    }
  }
}

uns8 bp_hybridgp_pred(Op* op) {
  const uns proc_id        = op->proc_id;
  auto&     hybridgp_state = hybridgp_state_all_cores.at(proc_id);

  const Addr  addr    = op->oracle_info.pred_addr;
  const uns32 ghist   = op->oracle_info.pred_global_hist;
  const uns32 phist   = get_local_history(hybridgp_state, addr);
  const auto  indices = cook_indices(addr, ghist, phist);

  const bool spred = get_spred(hybridgp_state, indices.spht);
  const bool gpred = get_gpred(op, hybridgp_state, addr, indices.gpht);
  const bool ppred = get_ppred(hybridgp_state, indices.ppht);

  uns8 pred = spred ? gpred : ppred;
  if(USE_FILTER) {
    const auto loop_filter_features = get_loop_filter_features(hybridgp_state,
                                                               indices.filter);
    if(is_loop_filter_prediction_valid(loop_filter_features)) {
      pred = get_loop_filter_prediction(loop_filter_features);
    }
  }

  op->pred_cycle                  = cycle_count;
  op->oracle_info.hybridgp_gpred  = gpred;
  op->oracle_info.hybridgp_ppred  = ppred;
  op->oracle_info.pred_local_hist = phist;

  const auto branch_id = op->recovery_info.branch_id;
  hybridgp_state.in_flight[branch_id].updated_local_history = true;
  hybridgp_state.in_flight[branch_id].pred_phist            = phist;
  hybridgp_state.in_flight[branch_id].bht_addr              = addr;

  // FIXME: the following code should speculatively update the local
  // history.  However, there is currently no way to recover
  // histories that were modified by off_path branches, so the
  // updates are disabled for now.
  if(!op->off_path) {
    update_local_history(hybridgp_state, proc_id, addr, pred);
  }

  return pred;
}

void bp_hybridgp_spec_update(Op* op) {}

void bp_hybridgp_update(Op* op) {
  if(op->table_info->cf_type != CF_CBR) {
    // If op is not a conditional branch, we do not interact with hybridgp.
    return;
  }

  const uns proc_id        = op->proc_id;
  auto&     hybridgp_state = hybridgp_state_all_cores.at(proc_id);

  const Addr  addr    = op->oracle_info.pred_addr;
  const uns32 ghist   = op->oracle_info.pred_global_hist;
  const uns32 phist   = op->oracle_info.pred_local_hist;
  const auto  indices = cook_indices(addr, ghist, phist);

  const uns32 resolution_time = cycle_count -
                                op->pred_cycle;  // a bucket of 10s
  const uns32 resolution_bucket = (cycle_count - op->pred_cycle) / 10;

  if(KNOB_PRINT_BRINFO)
    STAT_EVENT(proc_id, PRED_TO_UPDATE_CYCLES_0 + MIN2(50, resolution_bucket));
  else
    STAT_EVENT(proc_id,
               PRED_TO_UPDATE_CYCLES_0 +
                 MIN2(30, cycle_count - op->pred_cycle - DECODE_CYCLES));
  ASSERT(proc_id, cycle_count - op->pred_cycle - DECODE_CYCLES > 0);

  if(USE_FILTER) {
    const auto loop_filter_features = get_loop_filter_features(hybridgp_state,
                                                               indices.filter);
    update_loop_filter(op, hybridgp_state, indices.filter,
                       loop_filter_features);
    if(!is_loop_filter_prediction_valid(loop_filter_features)) {
      update_all_phts(op, hybridgp_state, indices);
    }
  } else {
    update_all_phts(op, hybridgp_state, indices);
  }

  // 0: think branch will mispredict
  // 1: confident branch will go the right direction
  if(KNOB_PRINT_BRINFO) {
    ASSERT(proc_id, brmispred != NULL);
    fprintf(brmispred, "%16llx %d %d %d %d %d\n", addr,
            op->oracle_info.mispred ? 1 : 0, op->oracle_info.misfetch ? 1 : 0,
            op->oracle_info.pred_conf ? 1 : 0, op->oracle_info.dir ? 1 : 0,
            resolution_time);
  }
}

void bp_hybridgp_recover(Recovery_Info* recovery_info) {
  const uns proc_id        = recovery_info->proc_id;
  auto&     hybridgp_state = hybridgp_state_all_cores.at(proc_id);

  const auto branch_id = recovery_info->branch_id;
  hybridgp_state.in_flight.deallocate_after(branch_id);

  if(recovery_info->cf_type != CF_CBR) {
    // If op is not a conditional branch, we do not interact with hybridgp.
    return;
  }

  // FIXME: this doesn't recover all of the other branches that had
  // their local histories modified on the wrong path (hack fix
  // exists in pred function)
  ASSERT(proc_id, hybridgp_state.in_flight[branch_id].updated_local_history);

  DEBUG(proc_id, "Recovering hybridgp local history\n");

  const Addr addr            = hybridgp_state.in_flight[branch_id].bht_addr;
  uns32* local_history_entry = get_local_history_entry(hybridgp_state, addr);
  if(!local_history_entry) {
    ASSERT(proc_id, INF_HYBRIDGP == FALSE);
    Addr bht_line_addr, repl_line_addr;
    local_history_entry = (uns32*)cache_insert(
      &hybridgp_state.bht, proc_id, addr, &bht_line_addr, &repl_line_addr);
  }
  *local_history_entry = (hybridgp_state.in_flight[branch_id].pred_phist >> 1) |
                         (recovery_info->new_dir << 31);
}

void bp_hybridgp_timestamp(Op* op) {
  const uns proc_id        = op->proc_id;
  auto&     hybridgp_state = hybridgp_state_all_cores.at(proc_id);

  const int64 branch_id = hybridgp_state.in_flight.allocate_back();
  hybridgp_state.in_flight[branch_id].updated_local_history = false;
  op->recovery_info.branch_id                               = branch_id;
}

void bp_hybridgp_retire(Op* op) {
  const uns proc_id        = op->proc_id;
  auto&     hybridgp_state = hybridgp_state_all_cores.at(proc_id);

  hybridgp_state.in_flight.deallocate_front(op->recovery_info.branch_id);
}
