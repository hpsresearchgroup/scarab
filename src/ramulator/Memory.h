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

#ifndef __MEMORY_H
#define __MEMORY_H

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <cmath>
#include <deque>
#include <functional>
#include <limits.h>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Config.h"
#include "Controller.h"
#include "DDR3.h"
#include "DDR4.h"
#include "DRAM.h"
#include "DSARP.h"
#include "GDDR5.h"
#include "HBM.h"
#include "LPDDR3.h"
#include "LPDDR4.h"
#include "Request.h"
#include "SpeedyController.h"
#include "Statistics.h"
#include "WideIO2.h"

using namespace std;

namespace ramulator {

class MemoryBase {
 public:
  MemoryBase() {}
  virtual ~MemoryBase() {}
  virtual double clk_ns()                                                  = 0;
  virtual void   tick()                                                    = 0;
  virtual bool   send(Request& req)                                        = 0;
  virtual void   warmup_process_req(Request& req)                          = 0;
  virtual void   warmup_perform_periodic_copy(const int num_rows_to_remap) = 0;
  virtual int    pending_requests()                                        = 0;
  virtual void   finish(void)                                              = 0;
  virtual long   page_allocator(long addr, int coreid)                     = 0;
  virtual void   record_core(int coreid)                                   = 0;
  virtual void   set_high_writeq_watermark(const float watermark)          = 0;
  virtual void   set_low_writeq_watermark(const float watermark)           = 0;

  virtual int get_chip_width() const           = 0;
  virtual int get_chip_size() const            = 0;
  virtual int get_num_chips() const            = 0;
  virtual int get_chip_row_buffer_size() const = 0;
  // virtual int get_tCK() = 0;
  // virtual int get_nCL() = 0;
  // virtual int get_nCCD() = 0;
  // virtual int get_nCWL() = 0;
  // virtual int get_nBL() = 0;
  // virtual int get_nWTR() = 0;
  // virtual int get_nRP() = 0;
  // virtual int get_nRPpb() = 0;
  // virtual int get_nRPab() = 0;
  // virtual int get_nRCD() = 0;
  // virtual int get_nRAS() = 0;
  // virtual int get_nRC() = 0;
};

template <class T, template <typename> class Controller = Controller>
class Memory : public MemoryBase {
 protected:
  ScalarStat dram_capacity;
  ScalarStat num_dram_cycles;
  ScalarStat num_incoming_requests;
  VectorStat num_read_requests;
  VectorStat num_write_requests;
  ScalarStat ramulator_active_cycles;
  VectorStat incoming_requests_per_channel;
  VectorStat incoming_read_reqs_per_channel;

  ScalarStat physical_page_replacement;
  ScalarStat maximum_bandwidth;
  ScalarStat in_queue_req_num_sum;
  ScalarStat in_queue_read_req_num_sum;
  ScalarStat in_queue_write_req_num_sum;
  ScalarStat in_queue_req_num_avg;
  ScalarStat in_queue_read_req_num_avg;
  ScalarStat in_queue_write_req_num_avg;

  VectorStat channel_reuses_by_ch;
  VectorStat channel_prev_written_reuses_by_ch;
  VectorStat channel_pages_touched_by_ch;
  VectorStat reuse_threshold_at_50_by_ch;
  VectorStat reuse_threshold_at_80_by_ch;
  VectorStat access_threshold_at_50_by_ch;
  VectorStat access_threshold_at_80_by_ch;
  VectorStat num_pages_threshold_at_50_reuse_by_ch;
  VectorStat num_pages_threshold_at_80_reuse_by_ch;
  VectorStat num_pages_threshold_at_50_access_by_ch;
  VectorStat num_pages_threshold_at_80_access_by_ch;

  VectorStat num_reserved_pages_allocated_by_ch;
  VectorStat num_reserved_rows_allocated_by_ch;
  VectorStat num_accesses_to_reserved_rows_by_ch;
  VectorStat num_reads_to_reserved_rows_by_ch;
  VectorStat num_writes_to_reserved_rows_by_ch;
  VectorStat num_copy_reads_by_ch;
  VectorStat num_copy_writes_by_ch;

  VectorStat num_reserved_pages_allocated_by_core;
  VectorStat num_reserved_rows_allocated_by_core;

#ifndef INTEGRATED_WITH_GEM5
  VectorStat record_read_requests;
  VectorStat record_write_requests;
#endif

  long max_address;

  // callback function for passing stats to Scarab when an event occurs
  void (*stats_callback)(int, unsigned, int) = nullptr;

 public:
  enum class Translation {
    None,
    Random,
    MAX,
  } translation = Translation::None;

  std::map<string, Translation> name_to_translation = {
    {"None", Translation::None},
    {"Random", Translation::Random},
  };

  vector<int>                free_physical_pages;
  long                       free_physical_pages_remaining;
  map<pair<int, long>, long> page_translation;

  vector<Controller<T>*> ctrls;
  T*                     spec;
  vector<int>            addr_bits;

  int tx_bits;

  Memory(const Config& configs, vector<Controller<T>*> ctrls,
         void (*_stats_callback)(int, unsigned, int)) :
      ctrls(ctrls),
      spec(ctrls[0]->channel->spec), addr_bits(int(T::Level::MAX)) {
    stats_callback = _stats_callback;
    // make sure 2^N channels/ranks
    // TODO support channel number that is not powers of 2
    int* sz = spec->org_entry.count;
    assert((sz[0] & (sz[0] - 1)) == 0);
    assert((sz[1] & (sz[1] - 1)) == 0);
    // validate size of one transaction
    int tx  = (spec->prefetch_size * spec->channel_width / 8);
    tx_bits = calc_log2(tx);
    assert((1 << tx_bits) == tx);

    type                   = parse_addr_map_type(configs);
    remap_policy           = parse_addr_remap_policy(configs);
    remap_copy_mode        = parse_addr_remap_copy_mode(configs);
    remap_copy_granularity = parse_addr_remap_copy_granularity(configs);
    remap_copy_time        = parse_addr_remap_copy_time(configs);
    remap_periodic_copy_select_policy =
      parse_addr_remap_periodic_copy_select_policy(configs);
    remap_periodic_copy_intracore_select_policy =
      parse_addr_remap_periodic_copy_intracore_select_policy(configs);
    remap_periodic_copy_candidates_org =
      parse_addr_remap_periodic_copy_candidates_org(configs);
    track_os_pages            = configs.get_config("track_os_page_reuse");
    remap_to_partitioned_rows = configs.get_config(
      "addr_remap_to_partitioned_rows");
    channels_share_tables = configs.get_config(
      "addr_remap_channels_share_tables");
    choose_minuse_candidate = configs.get_config(
      "addr_remap_choose_minuse_candidate");
    row_always_0 = configs.get_config("row_always_0");
    for(int ch = 0; ch < sz[0]; ch++)
      os_page_tracking_map_by_ch.push_back(
        std::unordered_map<long, OsPageInfo>());

    addr_bits_start_pos = vector<int>(int(T::Level::MAX), -1);

    num_cores                        = configs.get_core_num();
    addr_remap_page_access_threshold = configs.get_int(
      "addr_remap_page_access_threshold");
    addr_remap_page_reuse_threshold = configs.get_int(
      "addr_remap_page_reuse_threshold");
    const int addr_remap_max_per_core_limit_mb = configs.get_int(
      "addr_remap_max_per_core_limit_mb");
    if(addr_remap_max_per_core_limit_mb >= 0) {
      addr_remap_max_allocated_pages_per_core = addr_remap_max_per_core_limit_mb
                                                << (20 - os_page_offset_bits);
    } else {
      addr_remap_max_allocated_pages_per_core = INT_MAX;
    }

    addr_remap_num_reserved_rows = configs.get_int(
      "addr_remap_num_reserved_rows");

    addr_remap_dram_cycles_between_periodic_copy = configs.get_int(
      "addr_remap_dram_cycles_between_periodic_copy");

    // If hi address bits will not be assigned to Rows
    // then the chips must not be LPDDRx 6Gb, 12Gb etc.
    if(type != Type::RoBaRaCoCh && spec->standard_name.substr(0, 5) == "LPDDR")
      assert((sz[int(T::Level::Row)] & (sz[int(T::Level::Row)] - 1)) == 0);

    max_address = spec->channel_width / 8;

    for(unsigned int lev = 0; lev < addr_bits.size(); lev++) {
      addr_bits[lev] = calc_log2(sz[lev]);
      max_address *= sz[lev];
    }

    addr_bits[int(T::Level::MAX) - 1] -= calc_log2(spec->prefetch_size);

    // Initiating translation
    if(configs.contains("translation")) {
      translation = name_to_translation[configs["translation"]];
    }
    if(translation != Translation::None) {
      // construct a list of available pages
      // TODO: this should not assume a 4KB page!
      free_physical_pages_remaining = max_address >> os_page_offset_bits;

      free_physical_pages.resize(free_physical_pages_remaining, -1);
    }

    use_rest_of_addr_as_row_addr = configs.get_config(
      "use_rest_of_addr_as_row_addr");

    dram_capacity.name("dram_capacity")
      .desc("Number of bytes in simulated DRAM")
      .precision(0);
    dram_capacity = max_address;

    num_dram_cycles.name("dram_cycles")
      .desc("Number of DRAM cycles simulated")
      .precision(0);
    num_incoming_requests.name("incoming_requests")
      .desc("Number of incoming requests to DRAM")
      .precision(0);
    num_read_requests.init(configs.get_core_num())
      .name("read_requests")
      .desc("Number of incoming read requests to DRAM per core")
      .precision(0);
    num_write_requests.init(configs.get_core_num())
      .name("write_requests")
      .desc("Number of incoming write requests to DRAM per core")
      .precision(0);
    incoming_requests_per_channel.init(sz[int(T::Level::Channel)])
      .name("incoming_requests_per_channel")
      .desc("Number of incoming requests to each DRAM channel");
    incoming_read_reqs_per_channel.init(sz[int(T::Level::Channel)])
      .name("incoming_read_reqs_per_channel")
      .desc("Number of incoming read requests to each DRAM channel");

    ramulator_active_cycles.name("ramulator_active_cycles")
      .desc(
        "The total number of cycles that the DRAM part is active (serving R/W)")
      .precision(0);
    physical_page_replacement.name("physical_page_replacement")
      .desc("The number of times that physical page replacement happens.")
      .precision(0);
    maximum_bandwidth.name("maximum_bandwidth")
      .desc("The theoretical maximum bandwidth (Bps)")
      .precision(0);
    in_queue_req_num_sum.name("in_queue_req_num_sum")
      .desc("Sum of read/write queue length")
      .precision(0);
    in_queue_read_req_num_sum.name("in_queue_read_req_num_sum")
      .desc("Sum of read queue length")
      .precision(0);
    in_queue_write_req_num_sum.name("in_queue_write_req_num_sum")
      .desc("Sum of write queue length")
      .precision(0);
    in_queue_req_num_avg.name("in_queue_req_num_avg")
      .desc("Average of read/write queue length per memory cycle")
      .precision(6);
    in_queue_read_req_num_avg.name("in_queue_read_req_num_avg")
      .desc("Average of read queue length per memory cycle")
      .precision(6);
    in_queue_write_req_num_avg.name("in_queue_write_req_num_avg")
      .desc("Average of write queue length per memory cycle")
      .precision(6);

    channel_reuses_by_ch.init(sz[int(T::Level::Channel)])
      .name("channel_reuses_by_ch")
      .desc("Number of times a read/write was to a previously accessed cache "
            "line in "
            "each DRAM channel");
    channel_prev_written_reuses_by_ch.init(sz[int(T::Level::Channel)])
      .name("channel_prev_written_reuses_by_ch")
      .desc("Number of times a read/write was to a previously written cache "
            "line in "
            "each DRAM channel");
    channel_pages_touched_by_ch.init(sz[int(T::Level::Channel)])
      .name("channel_pages_touched_by_ch")
      .desc("Number of 4KB OS pages touched in this channel");
    reuse_threshold_at_50_by_ch.init(sz[int(T::Level::Channel)])
      .name("reuse_threshold_at_50_by_ch")
      .desc("What the reuse threshold per page should be if we want to capture "
            "50% of all reuses");
    reuse_threshold_at_80_by_ch.init(sz[int(T::Level::Channel)])
      .name("reuse_threshold_at_80_by_ch")
      .desc("What the reuse threshold per page should be if we want to capture "
            "80% of all reuses");
    access_threshold_at_50_by_ch.init(sz[int(T::Level::Channel)])
      .name("access_threshold_at_50_by_ch")
      .desc(
        "What the access threshold per page should be if we want to capture "
        "50% of all accesses");
    access_threshold_at_80_by_ch.init(sz[int(T::Level::Channel)])
      .name("access_threshold_at_80_by_ch")
      .desc(
        "What the access threshold per page should be if we want to capture "
        "80% of all accesses");
    num_pages_threshold_at_50_reuse_by_ch.init(sz[int(T::Level::Channel)])
      .name("num_pages_threshold_at_50_reuse_by_ch")
      .desc("How many pages were needed to achieve 50% reuse");
    num_pages_threshold_at_80_reuse_by_ch.init(sz[int(T::Level::Channel)])
      .name("num_pages_threshold_at_80_reuse_by_ch")
      .desc("How many pages were needed to achieve 80% reuse");
    num_pages_threshold_at_50_access_by_ch.init(sz[int(T::Level::Channel)])
      .name("num_pages_threshold_at_50_access_by_ch")
      .desc("How many pages were needed to achieve 50% access");
    num_pages_threshold_at_80_access_by_ch.init(sz[int(T::Level::Channel)])
      .name("num_pages_threshold_at_80_access_by_ch")
      .desc("How many pages were needed to achieve 80% access");

    num_reserved_pages_allocated_by_ch.init(sz[int(T::Level::Channel)])
      .name("num_reserved_pages_allocated_by_ch")
      .desc("How many total reserved pages were allocated");
    num_reserved_pages_allocated_by_core.init(num_cores)
      .name("num_reserved_pages_allocated_by_core")
      .desc("How many reserved pages have been allocated for this core");
    num_reserved_rows_allocated_by_ch.init(sz[int(T::Level::Channel)])
      .name("num_reserved_rows_allocated_by_ch")
      .desc("How many total reserved rows were allocated");
    num_reserved_rows_allocated_by_core.init(num_cores)
      .name("num_reserved_rows_allocated_by_core")
      .desc("How many total reserved rows were allocated per core");
    num_accesses_to_reserved_rows_by_ch.init(sz[int(T::Level::Channel)])
      .name("num_accesses_to_reserved_rows_by_ch")
      .desc("How many accesses were there to reserved rows");
    num_reads_to_reserved_rows_by_ch.init(sz[int(T::Level::Channel)])
      .name("num_reads_to_reserved_rows_by_ch")
      .desc("How many reads were there to reserved rows");
    num_writes_to_reserved_rows_by_ch.init(sz[int(T::Level::Channel)])
      .name("num_writes_to_reserved_rows_by_ch")
      .desc("How many writes were there to reserved rows");
    num_copy_reads_by_ch.init(sz[int(T::Level::Channel)])
      .name("num_copy_reads_by_ch")
      .desc("How many reads needed to be performed for copies");
    num_copy_writes_by_ch.init(sz[int(T::Level::Channel)])
      .name("num_copy_writes_by_ch")
      .desc("How many writes needed to be performed for copies");

#ifndef INTEGRATED_WITH_GEM5
    record_read_requests.init(configs.get_core_num())
      .name("record_read_requests")
      .desc("record read requests for this core when it reaches request limit "
            "or to the end");

    record_write_requests.init(configs.get_core_num())
      .name("record_write_requests")
      .desc("record write requests for this core when it reaches request limit "
            "or to the end");
#endif
    if(remap_policy != RemapPolicy::None) {
      // has to come before initial call to set_req_ddr_vec
      frame_index_start_pos = os_page_offset_bits - tx_bits;
      vector<int> dummy_addr_vec(addr_bits.size());
      set_req_addr_vec(
        0, dummy_addr_vec);  // just to initialize addr_bits_start_pos
      row_start_pos = addr_bits_start_pos[int(T::Level::Row)];
      assert(0 != row_start_pos);
      assert(0 != tx_bits);

      log2_num_frames_per_row = row_start_pos - frame_index_start_pos;
      assert(log2_num_frames_per_row > 0);
      num_frames_per_row = (1 << log2_num_frames_per_row);

      for(int core = 0; core < num_cores; core++) {
        core_to_ch_to_oldest_unfilled_row[core] = unordered_map<int, long>();
        core_to_ch_to_newest_unfilled_row[core] = unordered_map<int, long>();
      }

      if(remap_copy_time == CopyTime::Whenever) {
        setup_for_remap_copy_whenever(sz);
      } else if(remap_copy_time == CopyTime::Periodic) {
        setup_for_remap_copy_periodic(sz);
      } else {
        assert(false);
      }
    }
  }

  ~Memory() {
    for(auto ctrl : ctrls)
      delete ctrl;
    delete spec;
  }

  double clk_ns() { return spec->speed_entry.tCK; }

  int get_chip_width() const { return spec->org_entry.dq; }

  int get_chip_size() const { return spec->org_entry.size; }

  int get_num_chips() const {
    uint64_t dram_capacity_bytes = max_address;
    uint64_t chip_capacity_bytes = (uint64_t(get_chip_size()) * 1024 * 1024) /
                                   8;  // MegaBits to Bytes
    // cout << "dram_capacity_bytes: " << dram_capacity_bytes << ",
    // chip_capacity_bytes: " << chip_capacity_bytes << endl;
    return dram_capacity_bytes / chip_capacity_bytes;
  }

  int get_chip_row_buffer_size() const {
    return spec->org_entry.count[int(T::Level::Column)] * spec->org_entry.dq;
  }

  void record_core(int coreid) {
#ifndef INTEGRATED_WITH_GEM5
    record_read_requests[coreid]  = num_read_requests[coreid];
    record_write_requests[coreid] = num_write_requests[coreid];
#endif
    for(auto ctrl : ctrls) {
      ctrl->record_core(coreid);
    }
  }

  void tick() {
    ++num_dram_cycles;
    int cur_que_req_num      = 0;
    int cur_que_readreq_num  = 0;
    int cur_que_writereq_num = 0;
    for(auto ctrl : ctrls) {
      cur_que_req_num += ctrl->readq.size() + ctrl->writeq.size() +
                         ctrl->pending.size();
      cur_que_readreq_num += ctrl->readq.size() + ctrl->pending.size();
      cur_que_writereq_num += ctrl->writeq.size();
    }
    in_queue_req_num_sum += cur_que_req_num;
    in_queue_read_req_num_sum += cur_que_readreq_num;
    in_queue_write_req_num_sum += cur_que_writereq_num;

    const long dram_cycles_so_far = num_dram_cycles.value();
    if(addr_remap_dram_cycles_between_periodic_copy > 0) {
      if(0 ==
         (dram_cycles_so_far % addr_remap_dram_cycles_between_periodic_copy)) {
        if(channels_share_tables) {
          do_periodic_remap(0, 1, false);
        } else {
          for(auto ctrl : ctrls) {
            do_periodic_remap(ctrl->channel->id, 1, false);
          }
        }
      }
    }

    if(channels_share_tables) {
      process_pending_row_replacements(0);
    } else {
      for(auto ctrl : ctrls) {
        process_pending_row_replacements(ctrl->channel->id);
      }
    }

    bool is_active = false;
    for(auto ctrl : ctrls) {
      is_active = is_active || ctrl->is_active();
      ctrl->tick();
    }
    if(is_active) {
      ramulator_active_cycles++;
    }
  }

  void compute_req_addr(Request& req) {
    req.addr_vec.resize(addr_bits.size());
    assert(get_coreid_from_addr(req.addr) == req.coreid);

    if(!req.is_remapped && is_read_or_write_req(req)) {
      assert(req.orig_addr == req.addr);
      if(remap_policy == RemapPolicy::AlwaysToAlternate) {
        remap_if_alt_frame_exists(req);
      } else if(remap_policy == RemapPolicy::Flexible) {
        assert(false);
      }
    }

    if(!req.is_remapped)
      set_req_addr_vec(req.addr, req.addr_vec);

    if(row_always_0 && is_read_or_write_req(req)) {
      req.addr_vec[int(T::Level::Row)] = 0;
    }
  }

  void copy_warmup_rows_pages_allocated_per_core() {
    for(int coreid = 0; coreid < num_cores; coreid++) {
      stats_callback(int(StatCallbackType::ROW_ALLOCATED), coreid,
                     num_reserved_rows_allocated_by_core[coreid].value());
      stats_callback(int(StatCallbackType::PAGE_REMAPPED), coreid,
                     num_reserved_pages_allocated_by_core[coreid].value());
    }
  }

  bool send(Request& req) {
    if(!has_copied_warmup_stats_yet) {
      copy_warmup_rows_pages_allocated_per_core();
      has_copied_warmup_stats_yet = true;
    }
    compute_req_addr(req);
    return enqueue_req_to_ctrl(req, req.coreid);
  }

  void warmup_process_req(Request& req) {
    if(remap_policy != RemapPolicy::None) {
      req.addr_vec.resize(addr_bits.size());
      set_req_addr_vec(req.addr, req.addr_vec);
      int channel = req.addr_vec[int(T::Level::Channel)];
      if(track_os_pages) {
        update_os_page_info(req, channel);
      }
    }
  }

  void warmup_perform_periodic_copy(const int num_rows_to_remap) {
    if((remap_policy != RemapPolicy::None) &&
       (CopyTime::Periodic == remap_copy_time)) {
      size_t channel_limit = channels_share_tables ? 1 : ctrls.size();
      for(size_t ch = 0; ch < channel_limit; ch++) {
        do_periodic_remap(ch, num_rows_to_remap, true);
        while(!ch_to_pending_row_replacements.at(ch).empty()) {
          process_pending_row_replacements(ch);
        }
      }
    }
  }

  int pending_requests() {
    int reqs = 0;
    for(auto ctrl : ctrls)
      reqs += ctrl->readq.size() + ctrl->writeq.size() + ctrl->otherq.size() +
              ctrl->actq.size() + ctrl->pending.size();
    return reqs;
  }

  void set_high_writeq_watermark(const float watermark) {
    for(auto ctrl : ctrls)
      ctrl->set_high_writeq_watermark(watermark);
  }

  void set_low_writeq_watermark(const float watermark) {
    for(auto ctrl : ctrls)
      ctrl->set_low_writeq_watermark(watermark);
  }

  void finish(void) {
    dram_capacity     = max_address;
    int* sz           = spec->org_entry.count;
    maximum_bandwidth = spec->speed_entry.rate * 1e6 * spec->channel_width *
                        sz[int(T::Level::Channel)] / 8;
    long dram_cycles = num_dram_cycles.value();
    for(auto ctrl : ctrls) {
      long read_req = long(
        incoming_read_reqs_per_channel[ctrl->channel->id].value());
      ctrl->finish(read_req, dram_cycles);
      if(track_os_pages) {
        compute_os_page_stats(ctrl->channel->id);
        num_accesses_to_reserved_rows_by_ch[ctrl->channel->id] =
          num_reads_to_reserved_rows_by_ch[ctrl->channel->id].value() +
          num_writes_to_reserved_rows_by_ch[ctrl->channel->id].value();
      }
    }

    // finalize average queueing requests
    in_queue_req_num_avg      = in_queue_req_num_sum.value() / dram_cycles;
    in_queue_read_req_num_avg = in_queue_read_req_num_sum.value() / dram_cycles;
    in_queue_write_req_num_avg = in_queue_write_req_num_sum.value() /
                                 dram_cycles;
  }

  long page_allocator(long addr, int coreid) {
    long virtual_page_number = addr >> os_page_offset_bits;

    switch(int(translation)) {
      case int(Translation::None): {
        return addr;
      }
      case int(Translation::Random): {
        auto target = make_pair(coreid, virtual_page_number);
        if(page_translation.find(target) == page_translation.end()) {
          // page doesn't exist, so assign a new page
          // make sure there are physical pages left to be assigned

          // if physical page doesn't remain, replace a previous assigned
          // physical page.
          if(!free_physical_pages_remaining) {
            physical_page_replacement++;
            long phys_page_to_read = lrand() % free_physical_pages.size();
            assert(free_physical_pages[phys_page_to_read] != -1);
            page_translation[target] = phys_page_to_read;
          } else {
            // assign a new page
            long phys_page_to_read = lrand() % free_physical_pages.size();
            // if the randomly-selected page was already assigned
            if(free_physical_pages[phys_page_to_read] != -1) {
              long starting_page_of_search = phys_page_to_read;

              do {
                // iterate through the list until we find a free page
                // TODO: does this introduce serious non-randomness?
                ++phys_page_to_read;
                phys_page_to_read %= free_physical_pages.size();
              } while((phys_page_to_read != starting_page_of_search) &&
                      free_physical_pages[phys_page_to_read] != -1);
            }

            assert(free_physical_pages[phys_page_to_read] == -1);

            page_translation[target]               = phys_page_to_read;
            free_physical_pages[phys_page_to_read] = coreid;
            --free_physical_pages_remaining;
          }
        }

        // SAUGATA TODO: page size should not always be fixed to 4KB
        return map_addr_to_new_frame(addr, page_translation[target]);
      }
      default:
        assert(false);
    }
  }

 private:
  enum class Type {
    ChRaBaRoCo,
    RoBaRaCoCh,
    SkylakeDDR4,
    MAX,
  } type = Type::RoBaRaCoCh;

  enum class RemapPolicy {
    None,
    AlwaysToAlternate,
    Flexible,
    MAX,
  } remap_policy = RemapPolicy::None;

  enum class CopyMode {
    Free,
    Real,
    MAX,
  } remap_copy_mode = CopyMode::Real;

  enum class CopyGranularity {
    Page,
    Line,
    MAX,
  } remap_copy_granularity = CopyGranularity::Line;

  enum class CopyTime {
    Periodic,
    Whenever,
    MAX,
  } remap_copy_time = CopyTime::Whenever;

  enum class PeriodicCopySelectPolicy {
    CoreAccessFrac,
    TotalAccessFrac,
    InverseCoreRowFrac,
    MAX,
  } remap_periodic_copy_select_policy =
    PeriodicCopySelectPolicy::CoreAccessFrac;

  enum class PeriodicCopyIntraCoreSelectPolicy {
    MostAccesses,
    Oldest,
    MAX,
  } remap_periodic_copy_intracore_select_policy =
    PeriodicCopyIntraCoreSelectPolicy::MostAccesses;

  enum class PeriodicCopyCandidatesOrg {
    NonrowIndex_FrameFreq,
    SeqBatchFreq,
    MAX,
  } remap_periodic_copy_candidates_org =
    PeriodicCopyCandidatesOrg::NonrowIndex_FrameFreq;

  struct OsPageInfo {
    uint64_t lines_seen;
    uint64_t lines_written;
    long     reuse_count;
    long     prev_written_reuse_count;
    long     access_count;
  };
  std::vector<std::unordered_map<long, OsPageInfo>> os_page_tracking_map_by_ch;
  bool                                              track_os_pages;
  bool                                              row_always_0;
  int                                               num_cores;

  bool use_rest_of_addr_as_row_addr;

  const int os_page_offset_bits  = 12;
  const int coreid_start_bit_pos = 58;

  vector<int> addr_bits_start_pos;

  const int   stride_to_upper_xored_bit               = 4;
  int         addr_remap_page_access_threshold        = -1;
  int         addr_remap_page_reuse_threshold         = -1;
  int         addr_remap_max_allocated_pages_per_core = INT_MAX;
  bool        remap_to_partitioned_rows;
  bool        channels_share_tables;
  bool        choose_minuse_candidate;
  vector<int> channel_xor_bits_pos;
  vector<int> row_channel_xor_bits_pos;
  vector<int> frame_index_channel_xor_bits_pos;
  vector<int> frame_index_nonrow_channel_xor_bits_pos;

  unordered_map<int, long> global_ch_to_oldest_unfilled_row;
  unordered_map<int, long> global_ch_to_newest_unfilled_row;
  unordered_map<int, unordered_map<int, long>>
    core_to_ch_to_oldest_unfilled_row;
  unordered_map<int, unordered_map<int, long>>
    core_to_ch_to_newest_unfilled_row;

  unordered_map<int, unordered_map<long, unordered_map<int, vector<long>>>>
    ch_to_row_to_ch_freebits_parity_to_avail_frames;
  unordered_map<int, unordered_map<long, std::pair<long, uint64_t>>>
    ch_to_page_index_remapping;

  unordered_map<int, unordered_map<int, unordered_map<long, vector<long>>>>
            associativity_to_ch_to_cache_index_to_pageindex_vector;
  const int max_associativity = 32;

  bool has_copied_warmup_stats_yet                  = false;
  int  row_start_pos                                = -1;
  int  frame_index_start_pos                        = -1;
  int  log2_num_frames_per_row                      = -1;
  int  num_frames_per_row                           = -1;
  int  addr_remap_num_reserved_rows                 = -1;
  int  addr_remap_dram_cycles_between_periodic_copy = -1;
  long max_possible_pages_allocated                 = 0;
  struct AllocatedRowInfo {
    int  coreid;
    long reserved_row;
  };
  struct CandidatePageInfo {
    long access_count;
  };
  struct CandidateSeqBatchInfo {
    long                access_count;
    vector<long>        seq_frames;
    unordered_set<long> frames_set;
    bool                already_chosen;
    array<int, 2>       framebits_ch_xor_parity_seen;

    CandidateSeqBatchInfo() {
      access_count   = 0;
      seq_frames     = vector<long>();
      frames_set     = unordered_set<long>();
      already_chosen = false;

      framebits_ch_xor_parity_seen.at(0) = 0;
      framebits_ch_xor_parity_seen.at(1) = 0;
    }
  };
  struct CoreCandidateSeqBatchesInfo {
    unordered_map<long, size_t>   frame_to_seq_batch_num;
    vector<CandidateSeqBatchInfo> candidate_seq_batches;

    array<size_t, 2> insert_ptr_by_framebits_ch_xor_parity;

    CoreCandidateSeqBatchesInfo() {
      frame_to_seq_batch_num = unordered_map<long, size_t>();
      candidate_seq_batches  = vector<CandidateSeqBatchInfo>();

      candidate_seq_batches.push_back(CandidateSeqBatchInfo());
      insert_ptr_by_framebits_ch_xor_parity.at(0) = 0;
      insert_ptr_by_framebits_ch_xor_parity.at(1) = 0;
    }
  };
  struct PendingFrameReplacementInfo {
    int          nonrow_index_bits;
    bool         orig_frame_index_valid;
    long         orig_frame_index;
    bool         new_frame_index_valid;
    long         new_frame_index;
    uint64_t     new_frame_new_lines_on_page_mask;
    vector<long> pending_writeback_reads, pending_writeback_writes,
      pending_fill_reads, pending_fill_writes;
  };
  struct PendingRowReplacementInfo {
    long                                reserved_row;
    int                                 old_coreid;
    int                                 new_coreid;
    bool                                during_warmup;
    vector<PendingFrameReplacementInfo> pending_frame_replacements;
  };
  unordered_map<int, vector<long>>            ch_to_free_rows;
  unordered_map<int, deque<AllocatedRowInfo>> ch_to_allocated_row_info_in_order;
  unordered_map<int, unordered_map<int, long>> ch_to_core_to_interval_accesses;
  unordered_map<int, unordered_map<long, long>>
    ch_to_reserved_row_to_interval_accesses;
  unordered_map<int, unordered_map<long, unordered_map<int, long>>>
    ch_to_reserved_row_to_nonrow_index_bits_to_remapped_src_page_index;
  unordered_map<
    int, unordered_map<
           int, unordered_map<int, unordered_map<long, CandidatePageInfo>>>>
    ch_to_core_to_nonrow_index_bits_to_page_index_to_candidate_info;
  unordered_map<int, unordered_map<int, CoreCandidateSeqBatchesInfo>>
    ch_to_core_to_candidate_seq_batches_info;
  unordered_map<int, vector<PendingRowReplacementInfo>>
                                          ch_to_pending_row_replacements;
  unordered_map<int, unordered_set<long>> ch_to_pending_frame_remaps;

  bool enqueue_req_to_ctrl(Request& req, const int coreid) {
    int channel = req.addr_vec[int(T::Level::Channel)];
    if(ctrls[channel]->enqueue(req)) {
      if(track_os_pages) {
        update_os_page_info(req, channel);
      }

      // tally stats here to avoid double counting for requests that aren't
      // enqueued
      ++num_incoming_requests;
      if(req.type == Request::Type::READ) {
        ++num_read_requests[coreid];
        ++incoming_read_reqs_per_channel[channel];
        if(req.is_remapped && !req.is_copy) {
          ++num_reads_to_reserved_rows_by_ch[channel];
          stats_callback(int(StatCallbackType::REMAPPED_DATA_ACCESS),
                         req.coreid, 0);
        }
      }
      if(req.type == Request::Type::WRITE) {
        ++num_write_requests[coreid];
        if(req.is_remapped && !req.is_copy) {
          ++num_writes_to_reserved_rows_by_ch[channel];
          stats_callback(int(StatCallbackType::REMAPPED_DATA_ACCESS),
                         req.coreid, 0);
        }
      }
      ++incoming_requests_per_channel[channel];
      return true;
    }

    return false;
  }

  void assert_coreid_in_remapped_addr(const long addr, const int coreid) {
    if((CopyTime::Whenever == remap_copy_time) && remap_to_partitioned_rows) {
      assert(get_coreid_from_addr(addr) == num_cores + coreid);
    } else {
      assert(get_coreid_from_addr(addr) == num_cores);
    }
  }

  void verify_remapped_addr(const int channel, const long addr,
                            const long orig_addr, const int coreid) {
    assert_coreid_in_remapped_addr(addr, coreid);

    if(CopyTime::Periodic == remap_copy_time) {
      const long reserved_row      = get_row_from_addr(addr);
      const long orig_frame_index  = (orig_addr >> os_page_offset_bits);
      const long new_frame_index   = (addr >> os_page_offset_bits);
      const int  nonrow_index_bits = get_nonrow_index_bits_from_page_index(
        new_frame_index);
      assert(ch_to_reserved_row_to_nonrow_index_bits_to_remapped_src_page_index
               .at(get_channel_for_accessing_tables(channel))
               .at(reserved_row)
               .at(nonrow_index_bits) == orig_frame_index);
    }
  }
  uint64_t map_addr_to_new_frame(const long addr, const long new_frame) {
    return (new_frame << os_page_offset_bits) |
           (addr & ((uint64_t(1) << os_page_offset_bits) - 1));
  }
  long get_line_on_page_bit(const uint line_on_page) {
    return (((long)1) << line_on_page);
  }
  int get_coreid_from_addr(long addr) { return (addr >> coreid_start_bit_pos); }
  int calc_log2(int val) {
    int n = 0;
    while((val >>= 1))
      n++;
    return n;
  }
  int slice_lower_bits(long& addr, int bits) {
    assert(bits < (int)(sizeof(int) * CHAR_BIT));
    int lbits = addr & ((1 << bits) - 1);
    addr >>= bits;
    return lbits;
  }
  int slice_lower_bits_and_track_num_shifted(long& addr, int bits,
                                             int& num_shifted, int level) {
    if(-1 == addr_bits_start_pos[level])
      addr_bits_start_pos[level] = num_shifted;
    num_shifted += bits;
    return slice_lower_bits(addr, bits);
  }
  void clear_lower_bits(long& addr, int bits) { addr >>= bits; }
  long lrand(void) {
    if(sizeof(int) < sizeof(long)) {
      return static_cast<long>(rand()) << (sizeof(int) * 8) | rand();
    }

    return rand();
  }

  void set_req_addr_vec(long addr, vector<int>& addr_vec) {
    // Each transaction size is 2^tx_bits, so first clear the lowest tx_bits
    // bits
    clear_lower_bits(addr, tx_bits);
    int bits_sliced = 0;

    switch(int(type)) {
      case int(Type::ChRaBaRoCo):
        // currently only support RoBaRaCoCh with Scarab, because we only
        // scramble the bits just above the VA page offset bits, and we want
        // channel/rank/bank bits to come from the scrambled bits. Also, if
        // use_rest_of_addr_as_row_addr is on, then we want row addr to come
        // from the most significant bits
        assert(false);

        for(int i = addr_bits.size() - 1; i >= 0; i--) {
          addr_vec[i] = slice_lower_bits_and_track_num_shifted(
            addr, addr_bits[i], bits_sliced, i);
        }
        break;
      case int(Type::RoBaRaCoCh):
        addr_vec[0] = slice_lower_bits_and_track_num_shifted(addr, addr_bits[0],
                                                             bits_sliced, 0);

        addr_vec[addr_bits.size() - 1] = slice_lower_bits_and_track_num_shifted(
          addr, addr_bits[addr_bits.size() - 1], bits_sliced,
          addr_bits.size() - 1);
        // fill out addr_vec for everything up until row
        for(int i = 1; i < int(T::Level::Row); i++) {
          addr_vec[i] = slice_lower_bits_and_track_num_shifted(
            addr, addr_bits[i], bits_sliced, i);
        }
        addr_vec[int(T::Level::Row)] = slice_row_addr(addr, bits_sliced);
        break;
      case int(Type::SkylakeDDR4):
        set_skylakeddr4_addr_vec(addr_vec, addr);
        break;
      default:
        assert(false);
    }
  }

  Type parse_addr_map_type(const Config& configs) {
    if("ChRaBaRoCo" == configs["addr_map_type"]) {
      return Type::ChRaBaRoCo;
    } else if("RoBaRaCoCh" == configs["addr_map_type"]) {
      return Type::RoBaRaCoCh;
    } else if("SkylakeDDR4" == configs["addr_map_type"]) {
      return Type::SkylakeDDR4;
    } else {
      assert(false);
      return Type::MAX;
    }
  }

  RemapPolicy parse_addr_remap_policy(const Config& configs) {
    if("None" == configs["addr_remap_policy"]) {
      return RemapPolicy::None;
    } else if("AlwaysToAlternate" == configs["addr_remap_policy"]) {
      return RemapPolicy::AlwaysToAlternate;
    } else if("Flexible" == configs["addr_remap_policy"]) {
      return RemapPolicy::Flexible;
    } else {
      assert(false);
      return RemapPolicy::MAX;
    }
  }

  CopyMode parse_addr_remap_copy_mode(const Config& configs) {
    if("Free" == configs["addr_remap_copy_mode"]) {
      return CopyMode::Free;
    } else if("Real" == configs["addr_remap_copy_mode"]) {
      return CopyMode::Real;
    } else {
      assert(false);
      return CopyMode::MAX;
    }
  }

  CopyGranularity parse_addr_remap_copy_granularity(const Config& configs) {
    if("Page" == configs["addr_remap_copy_granularity"]) {
      return CopyGranularity::Page;
    } else if("Line" == configs["addr_remap_copy_granularity"]) {
      return CopyGranularity::Line;
    } else {
      assert(false);
      return CopyGranularity::MAX;
    }
  }

  CopyTime parse_addr_remap_copy_time(const Config& configs) {
    if("Whenever" == configs["addr_remap_copy_time"]) {
      return CopyTime::Whenever;
    } else if("Periodic" == configs["addr_remap_copy_time"]) {
      return CopyTime::Periodic;
    } else {
      assert(false);
      return CopyTime::MAX;
    }
  }

  PeriodicCopySelectPolicy parse_addr_remap_periodic_copy_select_policy(
    const Config& configs) {
    if("CoreAccessFrac" == configs["addr_remap_periodic_copy_select_policy"]) {
      return PeriodicCopySelectPolicy::CoreAccessFrac;
    } else if("TotalAccessFrac" ==
              configs["addr_remap_periodic_copy_select_policy"]) {
      return PeriodicCopySelectPolicy::TotalAccessFrac;
    } else if("InverseCoreRowFrac" ==
              configs["addr_remap_periodic_copy_select_policy"]) {
      return PeriodicCopySelectPolicy::InverseCoreRowFrac;
    } else {
      assert(false);
      return PeriodicCopySelectPolicy::MAX;
    }
  }

  PeriodicCopyIntraCoreSelectPolicy
    parse_addr_remap_periodic_copy_intracore_select_policy(
      const Config& configs) {
    if("MostAccesses" ==
       configs["addr_remap_periodic_copy_intracore_select_policy"]) {
      return PeriodicCopyIntraCoreSelectPolicy::MostAccesses;
    } else if("Oldest" ==
              configs["addr_remap_periodic_copy_intracore_select_policy"]) {
      return PeriodicCopyIntraCoreSelectPolicy::Oldest;
    } else {
      assert(false);
      return PeriodicCopyIntraCoreSelectPolicy::MAX;
    }
  }

  PeriodicCopyCandidatesOrg parse_addr_remap_periodic_copy_candidates_org(
    const Config& configs) {
    if("NonrowIndex_FrameFreq" ==
       configs["addr_remap_periodic_copy_candidates_org"]) {
      return PeriodicCopyCandidatesOrg::NonrowIndex_FrameFreq;
    } else if("SeqBatchFreq" ==
              configs["addr_remap_periodic_copy_candidates_org"]) {
      return PeriodicCopyCandidatesOrg::SeqBatchFreq;
    } else {
      assert(false);
      return PeriodicCopyCandidatesOrg::MAX;
    }
  }

  int slice_row_addr(long addr, int& bits_sliced) {
    addr_bits_start_pos[int(T::Level::Row)] = bits_sliced;
    // for the row addr, if use_rest_of_addr_as_row_addr is on, then we use
    // all the remaining phys addr bits in the row address (to make sure two
    // distinct phys addrs never alias to the same data in Ramulator)
    if(use_rest_of_addr_as_row_addr) {
      constexpr uint num_folded_bits = sizeof(int) * CHAR_BIT;
      long           shifted         = (addr >> num_folded_bits);
      return (shifted ^ addr);

    } else {
      return slice_lower_bits_and_track_num_shifted(
        addr, addr_bits[int(T::Level::Row)], bits_sliced, int(T::Level::Row));
    }
  }

  void BaRaBgCo7ChCo2(vector<int>& addr_vec, long addr) { assert(false); }
  void set_skylakeddr4_addr_vec(vector<int>& addr_vec, long addr) {
    assert(false);
  }
  void populate_frames_freelist_for_ch_row(const int channel, const long row,
                                           const int coreid) {
    assert(false);
  }

  int get_channel_from_frame_index(const long frame_index) {
    const long addr = frame_index << os_page_offset_bits;
    std::bitset<sizeof(addr) * CHAR_BIT> addr_bitset(addr >> tx_bits);
    int                                  channel_parity = 0;
    for(auto frame_index_bit_pos : frame_index_channel_xor_bits_pos) {
      channel_parity ^= addr_bitset[frame_index_bit_pos];
    }

    return channel_parity;
  }

  int get_channel_parity_from_nonrow_frame_index(const long frame_index) {
    const long addr = frame_index << os_page_offset_bits;
    std::bitset<sizeof(addr) * CHAR_BIT> addr_bitset(addr >> tx_bits);
    int                                  channel_parity = 0;
    for(auto frame_index_bit_pos : frame_index_nonrow_channel_xor_bits_pos) {
      channel_parity ^= addr_bitset[frame_index_bit_pos];
    }

    return channel_parity;
  }

  bool row_freelist_exhausted(const int channel, const long row) {
    return (
      ch_to_row_to_ch_freebits_parity_to_avail_frames[channel][row][0]
        .empty() &&
      ch_to_row_to_ch_freebits_parity_to_avail_frames[channel][row][1].empty());
  }

  bool line_is_remapped(bool& page_is_remapped, const int orig_channel,
                        const long orig_frame_index,
                        const long line_on_page_bit) {
    const int channel_for_accessing_tables = get_channel_for_accessing_tables(
      orig_channel);
    if(ch_to_page_index_remapping.count(channel_for_accessing_tables)) {
      if(ch_to_page_index_remapping.at(channel_for_accessing_tables)
           .count(orig_frame_index)) {
        page_is_remapped           = true;
        uint64_t copied_lines_mask = ch_to_page_index_remapping
                                       .at(channel_for_accessing_tables)
                                       .at(orig_frame_index)
                                       .second;
        if(copied_lines_mask & line_on_page_bit) {
          return true;
        }
      }
    }

    return false;
  }

  void assert_req_is_unmapped_noncopy_new_access(const Request& req) {
    assert(!req.is_copy);
    assert(!req.is_remapped);
    assert(req.is_first_command);
    assert(is_read_or_write_req(req));
  }

  int get_channel_for_accessing_tables(const int orig_channel) {
    const int channel_for_accessing_tables = channels_share_tables ?
                                               0 :
                                               orig_channel;
    return channel_for_accessing_tables;
  }

  void remap_if_alt_frame_exists(Request& req) {
    assert_req_is_unmapped_noncopy_new_access(req);

    vector<int> orig_addr_vec(addr_bits.size());
    set_req_addr_vec(req.addr, orig_addr_vec);
    int orig_channel = orig_addr_vec[int(T::Level::Channel)];

    long orig_frame_index, line_on_page_bit;
    bool frame_remapped = false;
    get_page_index_offset_bit(req.addr, orig_frame_index, line_on_page_bit);
    bool line_remapped = line_is_remapped(frame_remapped, orig_channel,
                                          orig_frame_index, line_on_page_bit);
    if(line_remapped ||
       (frame_remapped && (req.type == Request::Type::WRITE))) {
      req.addr = map_addr_to_new_frame(
        req.addr, ch_to_page_index_remapping
                    .at(get_channel_for_accessing_tables(orig_channel))
                    .at(orig_frame_index)
                    .first);
      verify_remapped_addr(orig_channel, req.addr, req.orig_addr, req.coreid);
      set_req_addr_vec(req.addr, req.addr_vec);
      if(!channels_share_tables)
        assert(req.addr_vec[int(T::Level::Channel)] == orig_channel);
      req.is_remapped = true;
    }
  }

  void update_ch_to_oldest_unfilled_row(
    const int channel, unordered_map<int, long>& oldest_unfilled_row,
    unordered_map<int, long>& newest_unfilled_row) {
    while(row_freelist_exhausted(channel, oldest_unfilled_row[channel]) &&
          (oldest_unfilled_row[channel] < newest_unfilled_row[channel])) {
      oldest_unfilled_row[channel]++;
      assert(oldest_unfilled_row[channel] <= newest_unfilled_row[channel]);
    }
  }

  long get_free_frame_from_freelist(const int channel, const long row,
                                    const int orig_channel_parity) {
    long new_frame_index =
      ch_to_row_to_ch_freebits_parity_to_avail_frames[channel][row]
                                                     [orig_channel_parity]
                                                       .back();
    ch_to_row_to_ch_freebits_parity_to_avail_frames[channel][row]
                                                   [orig_channel_parity]
                                                     .pop_back();

    return new_frame_index;
  }

  void get_new_free_frame(const int channel, const long frame_index,
                          int coreid) {
    unordered_map<int, long>& oldest_unfilled_row =
      remap_to_partitioned_rows ? core_to_ch_to_oldest_unfilled_row[coreid] :
                                  global_ch_to_oldest_unfilled_row;
    unordered_map<int, long>& newest_unfilled_row =
      remap_to_partitioned_rows ? core_to_ch_to_newest_unfilled_row[coreid] :
                                  global_ch_to_newest_unfilled_row;

    long row                 = oldest_unfilled_row[channel];
    int  orig_channel_parity = get_channel_from_frame_index(frame_index);
    for(; row <= newest_unfilled_row[channel]; row++) {
      if(!ch_to_row_to_ch_freebits_parity_to_avail_frames[channel][row]
                                                         [orig_channel_parity]
                                                           .empty())
        break;
    }

    if(row > newest_unfilled_row[channel]) {
      newest_unfilled_row[channel]++;
      populate_frames_freelist_for_ch_row(channel, newest_unfilled_row[channel],
                                          coreid);
      row = newest_unfilled_row[channel];
    }
    long new_frame_index = get_free_frame_from_freelist(channel, row,
                                                        orig_channel_parity);
    update_ch_to_oldest_unfilled_row(channel, oldest_unfilled_row,
                                     newest_unfilled_row);

    ch_to_page_index_remapping[channel][frame_index] = std::make_pair(
      new_frame_index, 0);
    num_reserved_pages_allocated_by_ch[channel]++;
    num_reserved_pages_allocated_by_core[coreid]++;
    stats_callback(int(StatCallbackType::PAGE_REMAPPED), coreid, 1);
  }

  void remap_and_copy_whenever(const int channel, const long frame_index,
                               const long     line_on_page_bit,
                               const Request& orig_req) {
    if(0 == ch_to_page_index_remapping.count(channel)) {
      ch_to_page_index_remapping[channel] =
        unordered_map<long, std::pair<long, uint64_t>>();
    }

    if(0 == ch_to_page_index_remapping[channel].count(frame_index)) {
      if(num_reserved_pages_allocated_by_core[orig_req.coreid].value() <
         addr_remap_max_allocated_pages_per_core) {
        get_new_free_frame(channel, frame_index, orig_req.coreid);
      } else {
        return;
      }
    }

    // actually copy the line(s)
    uint64_t copied_lines_mask =
      ch_to_page_index_remapping[channel][frame_index].second;

    const long new_frame_index =
      ch_to_page_index_remapping[channel][frame_index].first;
    const long new_addr = map_addr_to_new_frame(orig_req.addr, new_frame_index);
    verify_remapped_addr(channel, new_addr, orig_req.orig_addr,
                         orig_req.coreid);

    switch(remap_copy_mode) {
      case CopyMode::Free:
        if(CopyGranularity::Line == remap_copy_granularity) {
          if(0 == (copied_lines_mask & line_on_page_bit)) {
            ch_to_page_index_remapping[channel][frame_index].second |=
              line_on_page_bit;

            if(orig_req.type == Request::Type::WRITE) {
              if(orig_req.is_remapped) {
                assert(orig_req.addr == new_addr);
              }
              // if the orig req wasn't remapped, we assume we were able to
              // redirect it to the remapped page
            } else if(orig_req.type == Request::Type::READ) {
              num_copy_writes_by_ch[channel]++;
              stats_callback(int(StatCallbackType::PAGE_REMAPPING_COPY_WRITE),
                             orig_req.coreid, 1);
            } else
              assert(false);
          }
        } else if(CopyGranularity::Page == remap_copy_granularity) {
          if(0 == copied_lines_mask) {
            int num_tx_in_frame_ch_slice_log2 = (os_page_offset_bits - tx_bits);
            if(addr_bits[int(T::Level::Channel)] > 0) {
              // make sure some of the channel bits come from the page offset
              // bits, which means each page will be interleaved across the
              // channels
              assert(channel_xor_bits_pos.size() >
                     frame_index_channel_xor_bits_pos.size());
              num_tx_in_frame_ch_slice_log2 -=
                addr_bits[int(T::Level::Channel)];
              assert(num_tx_in_frame_ch_slice_log2 > 0);
            }

            ch_to_page_index_remapping[channel][frame_index].second =
              UINT64_MAX;

            const int num_tx_in_frame_ch_slice =
              (1 << num_tx_in_frame_ch_slice_log2);
            num_copy_reads_by_ch[channel] += num_tx_in_frame_ch_slice;
            num_copy_writes_by_ch[channel] += num_tx_in_frame_ch_slice;
            stats_callback(int(StatCallbackType::PAGE_REMAPPING_COPY_WRITE),
                           orig_req.coreid, num_tx_in_frame_ch_slice);
          }
        } else {
          assert(false);
        }
        break;

      case CopyMode::Real:
        if(CopyGranularity::Line == remap_copy_granularity) {
          if(0 == (copied_lines_mask & line_on_page_bit)) {
            if(orig_req.type == Request::Type::READ) {
              Request copy_req;
              copy_req.is_first_command = true;
              copy_req.addr             = new_addr;
              copy_req.orig_addr        = orig_req.orig_addr;
              copy_req.addr_vec.resize(addr_bits.size());
              set_req_addr_vec(copy_req.addr, copy_req.addr_vec);
              assert(copy_req.addr_vec[int(T::Level::Channel)] == channel);
              copy_req.coreid      = orig_req.coreid;
              copy_req.is_demand   = false;
              copy_req.is_copy     = true;
              copy_req.is_remapped = true;
              copy_req.type        = Request::Type::WRITE;

              if(enqueue_req_to_ctrl(copy_req, copy_req.coreid)) {
                ch_to_page_index_remapping[channel][frame_index].second |=
                  line_on_page_bit;
                num_copy_writes_by_ch[channel]++;
                stats_callback(int(StatCallbackType::PAGE_REMAPPING_COPY_WRITE),
                               orig_req.coreid, 1);
              } else {
                return;
              }
            } else if(orig_req.type == Request::Type::WRITE) {
              ch_to_page_index_remapping[channel][frame_index].second |=
                line_on_page_bit;
              if(orig_req.is_remapped) {
                assert(orig_req.addr == new_addr);
              }
              // if the orig req wasn't remapped, we assume that we were able to
              // redirect it to the remapped page
            } else {
              assert(false);
            }
          }
        } else {
          assert(false);
        }
        break;

      default:
        assert(false);
        break;
    }

    assert(0 != (ch_to_page_index_remapping[channel][frame_index].second &
                 line_on_page_bit));
  }

  long compute_first_reserved_row(const int coreid) {
    const long first_reserved_line = (long(num_cores + coreid)
                                      << coreid_start_bit_pos) >>
                                     tx_bits;
    int total_non_row_bits = 0;
    for(uint lev = 0; lev != addr_bits.size(); lev++) {
      if(lev != uint(T::Level::Row))
        total_non_row_bits += addr_bits[lev];
    }
    const long first_reserved_row = (first_reserved_line >> total_non_row_bits);
    return first_reserved_row;
  }

  void get_page_index_offset_bit(long addr, long& page_index,
                                 long& line_on_page_bit) {
    const uint page_offset  = slice_lower_bits(addr, os_page_offset_bits);
    page_index              = addr;
    const uint line_on_page = page_offset >> 6;
    assert(line_on_page < (sizeof(OsPageInfo().lines_seen) * CHAR_BIT));
    assert(line_on_page < (sizeof(OsPageInfo().lines_written) * CHAR_BIT));
    line_on_page_bit = get_line_on_page_bit(line_on_page);
  }

  bool is_read_or_write_req(const Request& req) {
    return ((req.type == Request::Type::READ) ||
            (req.type == Request::Type::WRITE));
  }

  void update_os_page_info(const Request& req, const int channel) {
    if(!req.is_copy) {
      if(is_read_or_write_req(req)) {
        long page_index;
        long line_on_page_bit;
        get_page_index_offset_bit(req.orig_addr, page_index, line_on_page_bit);

        std::unordered_map<long, OsPageInfo>& os_page_tracking_map =
          os_page_tracking_map_by_ch[channel];

        if(0 == os_page_tracking_map.count(page_index))
          os_page_tracking_map[page_index] = OsPageInfo();

        if(line_on_page_bit & os_page_tracking_map.at(page_index).lines_seen) {
          os_page_tracking_map.at(page_index).reuse_count++;
          stats_callback(int(StatCallbackType::DRAM_ORACLE_REUSE), req.coreid,
                         0 /* TODO: use this to pass retired_inst_window*/);
        }
        if(line_on_page_bit &
           os_page_tracking_map.at(page_index).lines_written) {
          os_page_tracking_map.at(page_index).prev_written_reuse_count++;
          stats_callback(int(StatCallbackType::DRAM_ORACLE_PREV_WRITTEN_REUSE),
                         req.coreid,
                         0 /* TODO: use this to pass retired_inst_window*/);
        }
        os_page_tracking_map.at(page_index).lines_seen |= line_on_page_bit;
        if(req.type == Request::Type::WRITE) {
          os_page_tracking_map.at(page_index).lines_written |= line_on_page_bit;
        }
        os_page_tracking_map.at(page_index).access_count++;

        if(remap_policy != RemapPolicy::None) {
          if(CopyTime::Whenever == remap_copy_time) {
            if(-1 != addr_remap_page_reuse_threshold) {
              if(os_page_tracking_map.at(page_index).reuse_count >
                 addr_remap_page_reuse_threshold) {
                remap_and_copy_whenever(channel, page_index, line_on_page_bit,
                                        req);
              }
            } else if(-1 != addr_remap_page_access_threshold) {
              if(os_page_tracking_map.at(page_index).access_count >
                 addr_remap_page_access_threshold) {
                remap_and_copy_whenever(channel, page_index, line_on_page_bit,
                                        req);
              }
            }
          } else if(CopyTime::Periodic == remap_copy_time) {
            update_remap_candidates_and_interval_accesses(
              channel, req.coreid, req.orig_addr, page_index, line_on_page_bit);
          } else {
            assert(false);
          }
        }
      }
    }
  }

  long get_row_from_addr(const long addr) {
    const long frame_index = (addr >> os_page_offset_bits);
    const long row_address = (frame_index >> log2_num_frames_per_row);
    return row_address;
  }

  int get_nonrow_index_bits_from_page_index(const long page_index) {
    long page_index_non_const = page_index;
    int  nonrow_index_bits    = slice_lower_bits(page_index_non_const,
                                             log2_num_frames_per_row);
    return nonrow_index_bits;
  }

  long gen_frame_from_reserved_row_and_nonrow_index_bits(
    const long reserved_row, const int nonrow_index_bits) {
    const long row_component = (reserved_row << log2_num_frames_per_row);
    const long final_frame   = row_component | nonrow_index_bits;
    return final_frame;
  }

  void update_reserved_row_interval_accesses(const int channel, const long addr,
                                             const long orig_page_index) {
    const int channel_for_accessing_tables = get_channel_for_accessing_tables(
      channel);
    const long remapped_addr = map_addr_to_new_frame(
      addr, ch_to_page_index_remapping.at(channel_for_accessing_tables)
              .at(orig_page_index)
              .first);
    assert(get_coreid_from_addr(remapped_addr) == num_cores);
    const long reserved_row = get_row_from_addr(remapped_addr);
    assert(
      ch_to_reserved_row_to_interval_accesses.at(channel_for_accessing_tables)
        .count(reserved_row));
    if(remap_periodic_copy_candidates_org ==
       PeriodicCopyCandidatesOrg::NonrowIndex_FrameFreq) {
      const int orig_page_index_nonrow_index_bits =
        get_nonrow_index_bits_from_page_index(orig_page_index);
      assert(ch_to_reserved_row_to_nonrow_index_bits_to_remapped_src_page_index
               .at(channel_for_accessing_tables)
               .at(reserved_row)
               .at(orig_page_index_nonrow_index_bits) == orig_page_index);
    }
    ch_to_reserved_row_to_interval_accesses.at(channel_for_accessing_tables)
      .at(reserved_row)++;
  }

  void update_seq_batches_candidate_info(const int channel, const int coreid,
                                         const long orig_page_index) {
    CoreCandidateSeqBatchesInfo& core_seq_batches_info =
      ch_to_core_to_candidate_seq_batches_info
        .at(get_channel_for_accessing_tables(channel))
        .at(coreid);
    assert(!core_seq_batches_info.candidate_seq_batches.empty());

    if(core_seq_batches_info.frame_to_seq_batch_num.count(orig_page_index)) {
      size_t containing_seq_batch_idx =
        core_seq_batches_info.frame_to_seq_batch_num.at(orig_page_index);
      CandidateSeqBatchInfo& seq_batch_info =
        core_seq_batches_info.candidate_seq_batches.at(
          containing_seq_batch_idx);
      assert(seq_batch_info.frames_set.count(orig_page_index));
      assert(!seq_batch_info.already_chosen);
      seq_batch_info.access_count++;
    } else {
      int framebits_ch_xor_parity = get_channel_from_frame_index(
        orig_page_index);
      if(channels_share_tables)
        framebits_ch_xor_parity = 0;

      size_t& insert_ptr =
        core_seq_batches_info.insert_ptr_by_framebits_ch_xor_parity.at(
          framebits_ch_xor_parity);
      CandidateSeqBatchInfo& current_seq_batch_info =
        core_seq_batches_info.candidate_seq_batches.at(insert_ptr);

      assert(current_seq_batch_info.seq_frames.size() ==
             current_seq_batch_info.frames_set.size());
      assert(current_seq_batch_info.seq_frames.size() <
             size_t(num_frames_per_row));
      int num_channels = ch_to_core_to_candidate_seq_batches_info.size();
      if(channels_share_tables)
        num_channels = 1;
      const size_t max_frames_per_parity = size_t(num_frames_per_row) /
                                           num_channels;
      int& parity_seen = current_seq_batch_info.framebits_ch_xor_parity_seen.at(
        framebits_ch_xor_parity);
      assert(size_t(parity_seen) < max_frames_per_parity);
      assert(0 == current_seq_batch_info.frames_set.count(orig_page_index));
      assert(!current_seq_batch_info.already_chosen);

      current_seq_batch_info.seq_frames.push_back(orig_page_index);
      current_seq_batch_info.frames_set.insert(orig_page_index);
      assert(current_seq_batch_info.seq_frames.size() ==
             current_seq_batch_info.frames_set.size());
      current_seq_batch_info.access_count++;
      parity_seen++;

      core_seq_batches_info.frame_to_seq_batch_num[orig_page_index] = size_t(
        insert_ptr);

      if(size_t(parity_seen) == max_frames_per_parity) {
        insert_ptr++;
        assert(insert_ptr <=
               core_seq_batches_info.candidate_seq_batches.size());
      }
      if(insert_ptr == core_seq_batches_info.candidate_seq_batches.size()) {
        core_seq_batches_info.candidate_seq_batches.emplace_back();
      }
    }
  }

  void update_nonrow_index_freq_candidate_info(const int  channel,
                                               const int  coreid,
                                               const long orig_page_index) {
    int nonrow_index_bits = get_nonrow_index_bits_from_page_index(
      orig_page_index);
    unordered_map<long, CandidatePageInfo>& page_index_to_candidate_info_map =
      ch_to_core_to_nonrow_index_bits_to_page_index_to_candidate_info
        .at(get_channel_for_accessing_tables(channel))
        .at(coreid)
        .at(nonrow_index_bits);
    if(page_index_to_candidate_info_map.count(orig_page_index) == 0) {
      page_index_to_candidate_info_map[orig_page_index] = CandidatePageInfo();
    }
    page_index_to_candidate_info_map.at(orig_page_index).access_count++;
  }

  void update_remap_candidates_and_interval_accesses(
    const int channel, const int coreid, const long addr,
    const long orig_page_index, const long line_on_page_bit) {
    ch_to_core_to_interval_accesses[get_channel_for_accessing_tables(channel)]
                                   [coreid]++;

    bool page_remapped = false;
    line_is_remapped(page_remapped, channel, orig_page_index, line_on_page_bit);
    if(page_remapped) {
      update_reserved_row_interval_accesses(channel, addr, orig_page_index);
    } else {
      if(remap_periodic_copy_candidates_org ==
         PeriodicCopyCandidatesOrg::NonrowIndex_FrameFreq) {
        update_nonrow_index_freq_candidate_info(channel, coreid,
                                                orig_page_index);
      } else if(remap_periodic_copy_candidates_org ==
                PeriodicCopyCandidatesOrg::SeqBatchFreq) {
        update_seq_batches_candidate_info(channel, coreid, orig_page_index);
      } else {
        assert(false);
      }
    }
  }

  vector<long> get_txs_that_belong_to_all_channels(const long page_index) {
    const int    num_tx_in_frame_log2 = (os_page_offset_bits - tx_bits);
    const int    num_tx_in_frame      = (1 << num_tx_in_frame_log2);
    const int    tx_size              = (1 << tx_bits);
    vector<long> addrs_that_belong_to_all_channel = vector<long>();

    for(int line_in_frame = 0; line_in_frame < num_tx_in_frame;
        line_in_frame++) {
      long addr = (page_index << os_page_offset_bits) +
                  (line_in_frame * tx_size);
      addrs_that_belong_to_all_channel.push_back(addr);
    }
    return addrs_that_belong_to_all_channel;
  }

  vector<long> get_txs_that_belong_to_channel(const int  channel,
                                              const long page_index) {
    const int    num_tx_in_frame_log2         = (os_page_offset_bits - tx_bits);
    const int    num_tx_in_frame              = (1 << num_tx_in_frame_log2);
    const int    tx_size                      = (1 << tx_bits);
    vector<long> addrs_that_belong_to_channel = vector<long>();
    vector<int>  dummy_addr_vec(addr_bits.size());

    for(int line_in_frame = 0; line_in_frame < num_tx_in_frame;
        line_in_frame++) {
      long addr = (page_index << os_page_offset_bits) +
                  (line_in_frame * tx_size);
      set_req_addr_vec(addr, dummy_addr_vec);
      if(channel == dummy_addr_vec[int(T::Level::Channel)]) {
        addrs_that_belong_to_channel.push_back(addr);
      }
    }
    return addrs_that_belong_to_channel;
  }

  void get_victim(const int channel, int& coreid_of_row_to_be_allocated,
                  long& row_to_allocate) {
    deque<AllocatedRowInfo>& allocated_rows =
      ch_to_allocated_row_info_in_order.at(channel);
    assert(!allocated_rows.empty());

    if(choose_minuse_candidate) {
      long   min_interval_accesses_so_far = LONG_MAX;
      size_t chosen_i                     = allocated_rows.size();
      for(size_t i = 0; i < allocated_rows.size(); i++) {
        const long candidate_victim_row_accesses = get_current_row_accesses(
          channel, allocated_rows.at(i).reserved_row, false);
        if(candidate_victim_row_accesses < min_interval_accesses_so_far) {
          min_interval_accesses_so_far = candidate_victim_row_accesses;
          chosen_i                     = i;
        }
      }
      assert(min_interval_accesses_so_far < LONG_MAX);
      assert(chosen_i < allocated_rows.size());
      row_to_allocate               = allocated_rows.at(chosen_i).reserved_row;
      coreid_of_row_to_be_allocated = allocated_rows.at(chosen_i).coreid;
      allocated_rows.erase(allocated_rows.begin() + chosen_i);
    } else {
      row_to_allocate               = allocated_rows.front().reserved_row;
      coreid_of_row_to_be_allocated = allocated_rows.front().coreid;
      allocated_rows.pop_front();
    }
    assert(valid_core_id(coreid_of_row_to_be_allocated));
  }

  long get_row_to_allocate(const int channel,
                           bool&     allocating_already_empty_row,
                           int&      coreid_of_row_to_be_allocated) {
    long          row_to_allocate;
    vector<long>& free_rows = ch_to_free_rows.at(channel);
    if(free_rows.empty()) {
      get_victim(channel, coreid_of_row_to_be_allocated, row_to_allocate);
      allocating_already_empty_row = false;
    } else {
      row_to_allocate              = free_rows.back();
      allocating_already_empty_row = true;
      free_rows.pop_back();
    }

    return row_to_allocate;
  }

  void reinsert_unreallocated_row(const int  channel,
                                  const long row_being_allocated,
                                  const int  coreid_of_row_being_replaced,
                                  const bool allocating_already_empty_row) {
    if(allocating_already_empty_row) {
      ch_to_free_rows.at(channel).push_back(row_being_allocated);
    } else {
      AllocatedRowInfo allocated_row_info;
      allocated_row_info.reserved_row = row_being_allocated;
      allocated_row_info.coreid       = coreid_of_row_being_replaced;
      ch_to_allocated_row_info_in_order.at(channel).push_back(
        allocated_row_info);
    }
  }

  void add_to_pending_frame_remaps(
    const int            channel, const unordered_map<int, long>&
                         nonrow_index_bits_to_chosen_candidate_page) {
    for(const auto& nonrow_index_bits_chosen_candidate_page :
        nonrow_index_bits_to_chosen_candidate_page) {
      const long chosen_candidate_page =
        nonrow_index_bits_chosen_candidate_page.second;

      unordered_set<long>& pending_frame_remaps = ch_to_pending_frame_remaps.at(
        channel);

      assert(0 == pending_frame_remaps.count(chosen_candidate_page));
      pending_frame_remaps.insert(chosen_candidate_page);
    }
  }

  void finalize_seq_batches_chosen_candidates(
    const int channel, const int chosen_candidate_coreid,
    const unordered_map<int, long>&
      nonrow_index_bits_to_chosen_candidate_page) {
    CoreCandidateSeqBatchesInfo& core_seq_batches_info =
      ch_to_core_to_candidate_seq_batches_info.at(channel).at(
        chosen_candidate_coreid);
    assert(!core_seq_batches_info.candidate_seq_batches.empty());

    int chosen_seq_batch_idx = -1;
    for(const auto& nonrow_index_bits_chosen_candidate_page :
        nonrow_index_bits_to_chosen_candidate_page) {
      const long chosen_candidate_page =
        nonrow_index_bits_chosen_candidate_page.second;

      const size_t expected_idx_in_vec =
        core_seq_batches_info.frame_to_seq_batch_num.at(chosen_candidate_page);

      if(chosen_seq_batch_idx >= 0) {
        assert(size_t(chosen_seq_batch_idx) == expected_idx_in_vec);
      }
      chosen_seq_batch_idx = expected_idx_in_vec;

      assert(
        core_seq_batches_info.candidate_seq_batches.at(chosen_seq_batch_idx)
          .frames_set.count(chosen_candidate_page));
    }

    assert(chosen_seq_batch_idx >= 0);
    const size_t num_chosen_pages =
      nonrow_index_bits_to_chosen_candidate_page.size();
    assert(core_seq_batches_info.candidate_seq_batches.at(chosen_seq_batch_idx)
             .frames_set.size() ==
           core_seq_batches_info.candidate_seq_batches.at(chosen_seq_batch_idx)
             .seq_frames.size());
    assert(core_seq_batches_info.candidate_seq_batches.at(chosen_seq_batch_idx)
             .seq_frames.size() >= num_chosen_pages);
    core_seq_batches_info.candidate_seq_batches.at(chosen_seq_batch_idx)
      .already_chosen = true;
  }

  void finalize_nonrow_index_freq_chosen_candidates(
    const int channel, const int chosen_candidate_coreid,
    const unordered_map<int, long>&
      nonrow_index_bits_to_chosen_candidate_page) {
    unordered_map<int, unordered_map<long, CandidatePageInfo>>&
      nonrow_index_bits_to_page_index_to_candidate_info =
        ch_to_core_to_nonrow_index_bits_to_page_index_to_candidate_info
          .at(channel)
          .at(chosen_candidate_coreid);
    for(const auto& nonrow_index_bits_chosen_candidate_page :
        nonrow_index_bits_to_chosen_candidate_page) {
      const int nonrow_index_bits =
        nonrow_index_bits_chosen_candidate_page.first;
      const long chosen_candidate_page =
        nonrow_index_bits_chosen_candidate_page.second;
      assert(
        nonrow_index_bits_to_page_index_to_candidate_info.at(nonrow_index_bits)
          .count(chosen_candidate_page));
      nonrow_index_bits_to_page_index_to_candidate_info.at(nonrow_index_bits)
        .erase(chosen_candidate_page);
    }
  }

  void update_num_reserved_pages_stat(const int channel, const int coreid,
                                      const size_t delta,
                                      const bool   subtracting) {
    assert(delta > 0);
    assert(delta <= size_t(num_frames_per_row));
    assert(coreid != -1);

    if(subtracting) {
      num_reserved_pages_allocated_by_ch[channel] -= delta;
      assert(num_reserved_pages_allocated_by_ch[channel].value() >= 0);
      num_reserved_pages_allocated_by_core[coreid] -= delta;
      assert(num_reserved_pages_allocated_by_core[coreid].value() >= 0);
      stats_callback(int(StatCallbackType::PAGE_REMAPPED), coreid, -delta);
    } else {
      num_reserved_pages_allocated_by_ch[channel] += delta;
      assert(num_reserved_pages_allocated_by_ch[channel].value() <=
             max_possible_pages_allocated);
      num_reserved_pages_allocated_by_core[coreid] += delta;
      assert(num_reserved_pages_allocated_by_core[coreid].value() <=
             max_possible_pages_allocated);
      stats_callback(int(StatCallbackType::PAGE_REMAPPED), coreid, delta);
    }
  }

  void update_num_reserved_rows_stat(const int coreid, const bool subtracting) {
    if(subtracting) {
      num_reserved_rows_allocated_by_core[coreid]--;
      stats_callback(int(StatCallbackType::ROW_ALLOCATED), coreid, -1);
    } else {
      num_reserved_rows_allocated_by_core[coreid]++;
      stats_callback(int(StatCallbackType::ROW_ALLOCATED), coreid, 1);
    }
  }

  long get_candidate_row_accesses_for_core_seq_batches(
    const int channel, const int core,
    unordered_map<int, long>& nonrow_index_bits_to_chosen_candidate_page,
    const int rowbits_ch_xor_parity, int& batches_before_chosen_one,
    long& chosen_one_access_count) {
    int selected_seq_batch_idx = -1;

    const CoreCandidateSeqBatchesInfo& core_seq_batches_info =
      ch_to_core_to_candidate_seq_batches_info.at(channel).at(core);
    assert(!core_seq_batches_info.candidate_seq_batches.empty());

    long most_accesses_so_far      = 0;
    int  valid_batches_seen_so_far = 0;
    for(size_t i = 0; i < core_seq_batches_info.candidate_seq_batches.size();
        i++) {
      const CandidateSeqBatchInfo& seq_batch_info =
        core_seq_batches_info.candidate_seq_batches.at(i);
      if(!seq_batch_info.already_chosen) {
        if(remap_periodic_copy_intracore_select_policy ==
           PeriodicCopyIntraCoreSelectPolicy::MostAccesses) {
          if(seq_batch_info.access_count > most_accesses_so_far) {
            most_accesses_so_far      = seq_batch_info.access_count;
            selected_seq_batch_idx    = i;
            batches_before_chosen_one = valid_batches_seen_so_far;
            chosen_one_access_count   = most_accesses_so_far;
          }
        } else if(remap_periodic_copy_intracore_select_policy ==
                  PeriodicCopyIntraCoreSelectPolicy::Oldest) {
          most_accesses_so_far      = seq_batch_info.access_count;
          selected_seq_batch_idx    = i;
          batches_before_chosen_one = valid_batches_seen_so_far;
          chosen_one_access_count   = most_accesses_so_far;
          break;
        } else {
          assert(false);
        }
        valid_batches_seen_so_far++;
      }
    }


    if(most_accesses_so_far > 0) {
      assert(selected_seq_batch_idx >= 0);
      const CandidateSeqBatchInfo& seq_batch_info =
        core_seq_batches_info.candidate_seq_batches.at(selected_seq_batch_idx);
      assert(seq_batch_info.seq_frames.size() <= size_t(num_frames_per_row));

      array<vector<long>, 2> chosen_pages_by_framebits_ch_xor_parity;
      for(size_t i = 0; i < seq_batch_info.seq_frames.size(); i++) {
        const long chosen_page = seq_batch_info.seq_frames.at(i);
        assert(seq_batch_info.frames_set.count(chosen_page));
        assert(core_seq_batches_info.frame_to_seq_batch_num.at(chosen_page) ==
               size_t(selected_seq_batch_idx));
        if(0 == ch_to_pending_frame_remaps.at(channel).count(chosen_page)) {
          if(0 == ch_to_page_index_remapping.at(channel).count(chosen_page)) {
            int framebits_ch_xor_parity = get_channel_from_frame_index(
              chosen_page);
            if(channels_share_tables)
              framebits_ch_xor_parity = 0;
            chosen_pages_by_framebits_ch_xor_parity.at(framebits_ch_xor_parity)
              .push_back(chosen_page);
          }
        }
      }

      int num_channels = ch_to_core_to_candidate_seq_batches_info.size();
      if(channels_share_tables)
        num_channels = 1;
      const size_t max_frames_per_parity = size_t(num_frames_per_row) /
                                           num_channels;
      assert(chosen_pages_by_framebits_ch_xor_parity.at(0).size() <=
             max_frames_per_parity);
      assert(chosen_pages_by_framebits_ch_xor_parity.at(1).size() <=
             max_frames_per_parity);
      if(channels_share_tables) {
        assert(0 == chosen_pages_by_framebits_ch_xor_parity.at(1).size());
      }

      for(size_t i = 0; i < size_t(num_frames_per_row); i++) {
        int required_framebits_ch_xor_parity =
          get_channel_parity_from_nonrow_frame_index(i) ^ rowbits_ch_xor_parity;
        if(channels_share_tables) {
          required_framebits_ch_xor_parity = 0;
        }

        vector<long>& vector_with_required_parity =
          chosen_pages_by_framebits_ch_xor_parity.at(
            required_framebits_ch_xor_parity);
        if(!vector_with_required_parity.empty()) {
          nonrow_index_bits_to_chosen_candidate_page[i] =
            vector_with_required_parity.back();
          vector_with_required_parity.pop_back();
        }
      }

      assert(chosen_pages_by_framebits_ch_xor_parity.at(0).empty());
      assert(chosen_pages_by_framebits_ch_xor_parity.at(1).empty());
    }

    return most_accesses_so_far;
  }

  long get_candidate_row_accesses_for_core_nonrow_index_freq(
    const int channel, const int core,
    unordered_map<int, long>& nonrow_index_bits_to_chosen_candidate_page,
    const int                 required_row_channel_xor) {
    long total_candidate_row_accesses = 0;
    const unordered_map<int, unordered_map<long, CandidatePageInfo>>&
      nonrow_index_bits_to_page_index_to_candidate_info =
        ch_to_core_to_nonrow_index_bits_to_page_index_to_candidate_info
          .at(channel)
          .at(core);
    for(auto& nonrow_index_bits_and_accessed_pages_map :
        nonrow_index_bits_to_page_index_to_candidate_info) {
      const int nonrow_index_bits =
        nonrow_index_bits_and_accessed_pages_map.first;
      const unordered_map<long, CandidatePageInfo>& accessed_pages_map =
        nonrow_index_bits_and_accessed_pages_map.second;
      long most_accesses_so_far           = 0;
      long page_with_most_accesses_so_far = 0;
      for(auto& accessed_page : accessed_pages_map) {
        const long page     = accessed_page.first;
        const long accesses = accessed_page.second.access_count;

        const int row_channel_xor_for_page =
          get_row_channel_xor_from_frame_index(page);
        if(0 == ch_to_pending_frame_remaps.at(channel).count(page)) {
          if(0 == ch_to_page_index_remapping.at(channel).count(page)) {
            if(row_channel_xor_bits_pos.empty() ||
               (row_channel_xor_for_page == required_row_channel_xor)) {
              if(accesses > most_accesses_so_far) {
                most_accesses_so_far           = accesses;
                page_with_most_accesses_so_far = page;
              }
            }
          }
        }
      }
      total_candidate_row_accesses += most_accesses_so_far;
      if(most_accesses_so_far > 0) {
        nonrow_index_bits_to_chosen_candidate_page[nonrow_index_bits] =
          page_with_most_accesses_so_far;
      }
    }

    return total_candidate_row_accesses;
  }

  long get_current_row_accesses(const int channel, const long row_to_allocate,
                                const bool row_currently_empty) {
    if(row_currently_empty) {
      return 0;
    } else {
      return ch_to_reserved_row_to_interval_accesses.at(channel).at(
        row_to_allocate);
    }
  }

  double get_row_score(const int channel, const long accesses,
                       const int coreid) {
    long denominator = 0;
    if(-1 == coreid) {
      assert(0 == accesses);
      return 0;
    }

    double numerator = 0;

    if(remap_periodic_copy_select_policy ==
       PeriodicCopySelectPolicy::CoreAccessFrac) {
      numerator   = accesses;
      denominator = ch_to_core_to_interval_accesses.at(channel).at(coreid);
    } else if(remap_periodic_copy_select_policy ==
              PeriodicCopySelectPolicy::TotalAccessFrac) {
      numerator   = accesses;
      denominator = 1;
    } else if(remap_periodic_copy_select_policy ==
              PeriodicCopySelectPolicy::InverseCoreRowFrac) {
      if(accesses > 0) {
        const long num_already_allocated_rows =
          num_reserved_rows_allocated_by_core[coreid].value();
        assert(num_already_allocated_rows <= addr_remap_num_reserved_rows);
        numerator = addr_remap_num_reserved_rows - num_already_allocated_rows;
      } else {
        numerator = 0;
      }
      denominator = 1;
    } else {
      assert(false);
    }

    if(denominator != 0)
      return numerator / denominator;
    else {
      return 0;
    }
  }

  PendingFrameReplacementInfo create_pending_frame_replacement_info(
    const int channel, const int nonrow_index_bits,
    const bool orig_frame_exists, const long orig_frame_index,
    const bool new_frame_exists, const long new_frame_index) {
    PendingFrameReplacementInfo pending_frame_replacement_info;
    pending_frame_replacement_info.nonrow_index_bits      = nonrow_index_bits;
    pending_frame_replacement_info.orig_frame_index_valid = orig_frame_exists;
    pending_frame_replacement_info.orig_frame_index       = orig_frame_index;
    pending_frame_replacement_info.new_frame_index_valid  = new_frame_exists;
    pending_frame_replacement_info.new_frame_index        = new_frame_index;
    if(orig_frame_exists) {
      if(channels_share_tables) {
        pending_frame_replacement_info.pending_writeback_reads =
          get_txs_that_belong_to_all_channels(orig_frame_index);
      } else {
        pending_frame_replacement_info.pending_writeback_reads =
          get_txs_that_belong_to_channel(channel, orig_frame_index);
      }
    }
    if(new_frame_exists) {
      if(channels_share_tables) {
        pending_frame_replacement_info.pending_fill_reads =
          get_txs_that_belong_to_all_channels(new_frame_index);
      } else {
        pending_frame_replacement_info.pending_fill_reads =
          get_txs_that_belong_to_channel(channel, new_frame_index);
      }
      pending_frame_replacement_info.new_frame_new_lines_on_page_mask = 0;
      for(const long fill_addr :
          pending_frame_replacement_info.pending_fill_reads) {
        long page_index, line_on_page_bit;
        get_page_index_offset_bit(fill_addr, page_index, line_on_page_bit);
        assert(new_frame_index == page_index);
        pending_frame_replacement_info.new_frame_new_lines_on_page_mask |=
          line_on_page_bit;
      }
    }
    return pending_frame_replacement_info;
  }

  PendingRowReplacementInfo create_pending_row_replacement_info(
    const int channel, const long row_being_replaced, const int old_coreid,
    const int new_coreid,
    const unordered_map<int, long>&
      nonrow_index_bits_to_chosen_candidate_page) {
    PendingRowReplacementInfo pending_row_replacement_info;
    pending_row_replacement_info.reserved_row = row_being_replaced;
    pending_row_replacement_info.old_coreid   = old_coreid;
    pending_row_replacement_info.new_coreid   = new_coreid;
    pending_row_replacement_info.pending_frame_replacements =
      vector<PendingFrameReplacementInfo>();
    const unordered_map<int, long>&
      existing_nonrow_index_bits_to_remapped_pages_index =
        ch_to_reserved_row_to_nonrow_index_bits_to_remapped_src_page_index
          .at(channel)
          .at(row_being_replaced);
    for(int nonrow_index_bits = 0; nonrow_index_bits < num_frames_per_row;
        nonrow_index_bits++) {
      bool existing_frame_exists = false;
      bool new_frame_exists      = false;
      long existing_frame_index = 0, new_frame_index = 0;

      if(existing_nonrow_index_bits_to_remapped_pages_index.count(
           nonrow_index_bits)) {
        existing_frame_exists = true;
        existing_frame_index =
          existing_nonrow_index_bits_to_remapped_pages_index.at(
            nonrow_index_bits);
      }
      if(nonrow_index_bits_to_chosen_candidate_page.count(nonrow_index_bits)) {
        new_frame_exists = true;
        new_frame_index  = nonrow_index_bits_to_chosen_candidate_page.at(
          nonrow_index_bits);
      }

      if(existing_frame_exists || new_frame_index) {
        pending_row_replacement_info.pending_frame_replacements.push_back(
          create_pending_frame_replacement_info(
            channel, nonrow_index_bits, existing_frame_exists,
            existing_frame_index, new_frame_exists, new_frame_index));
      }
    }
    return pending_row_replacement_info;
  }

  bool valid_core_id(const int core_id) {
    return ((core_id >= 0) && (core_id < num_cores));
  }

  unordered_map<int, long> process_all_candidates(
    const int channel, const double current_row_score, int& core_selected,
    const int required_row_channel_xor, bool& candidates_available) {
    core_selected = -1;
    vector<unordered_map<int, long>> all_cores_chosen_pages =
      vector<unordered_map<int, long>>(num_cores);
    double      best_candidate_score_so_far              = 0;
    vector<int> all_cores_batches_seen_before_chosen_one = vector<int>(
      num_cores, 0);
    vector<long> all_cores_chosen_one_access_count = vector<long>(num_cores, 0);

    for(int core = 0; core < num_cores; core++) {
      all_cores_chosen_pages[core] = unordered_map<int, long>();
      long total_accesses          = 0;
      if(remap_periodic_copy_candidates_org ==
         PeriodicCopyCandidatesOrg::NonrowIndex_FrameFreq) {
        total_accesses = get_candidate_row_accesses_for_core_nonrow_index_freq(
          channel, core, all_cores_chosen_pages[core],
          required_row_channel_xor);
      } else if(remap_periodic_copy_candidates_org ==
                PeriodicCopyCandidatesOrg::SeqBatchFreq) {
        total_accesses = get_candidate_row_accesses_for_core_seq_batches(
          channel, core, all_cores_chosen_pages[core], required_row_channel_xor,
          all_cores_batches_seen_before_chosen_one.at(core),
          all_cores_chosen_one_access_count.at(core));
      } else {
        assert(false);
      }
      if(total_accesses > 0)
        candidates_available = true;
      double candidate_row_score = get_row_score(channel, total_accesses, core);
      if(candidate_row_score > best_candidate_score_so_far) {
        best_candidate_score_so_far = candidate_row_score;
        core_selected               = core;
      }
    }

    if(candidates_available) {
      assert(valid_core_id(core_selected));
    }

    if(best_candidate_score_so_far > current_row_score) {
      stats_callback(
        int(StatCallbackType::ROW_PICKED), core_selected,
        all_cores_batches_seen_before_chosen_one.at(core_selected));
      stats_callback(int(StatCallbackType::ROW_PICKED_ACCESS_COUNT),
                     core_selected,
                     all_cores_chosen_one_access_count.at(core_selected));
      return all_cores_chosen_pages[core_selected];
    } else
      return unordered_map<int, long>();
  }

  int get_row_channel_xor_from_addr_without_tx(
    const long addr_without_tx_bits) {
    std::bitset<sizeof(addr_without_tx_bits) * CHAR_BIT> addr_bitset(
      addr_without_tx_bits);
    int result = 0;
    for(auto row_channel_xor_bit_pos : row_channel_xor_bits_pos) {
      result ^= addr_bitset[row_channel_xor_bit_pos];
    }
    return result;
  }

  int get_row_channel_xor_from_row_addr(const long row_addr) {
    assert(log2_num_frames_per_row > 0);
    assert(os_page_offset_bits > 0);
    assert(tx_bits > 0);
    const long addr_without_tx_bits = (row_addr
                                       << (log2_num_frames_per_row +
                                           os_page_offset_bits - tx_bits));
    return get_row_channel_xor_from_addr_without_tx(addr_without_tx_bits);
  }

  int get_row_channel_xor_from_frame_index(const long frame_index) {
    assert(os_page_offset_bits > 0);
    assert(tx_bits > 0);
    const long addr_without_tx_bits = (frame_index
                                       << (os_page_offset_bits - tx_bits));
    return get_row_channel_xor_from_addr_without_tx(addr_without_tx_bits);
  }

  void clear_interval_accesses_and_candidate_infos(const int channel) {
    for(int core = 0; core < num_cores; core++) {
      ch_to_core_to_interval_accesses.at(channel).at(core) = 0;

      ch_to_core_to_candidate_seq_batches_info[channel][core] =
        CoreCandidateSeqBatchesInfo();

      for(int nonrow_index = 0; nonrow_index < num_frames_per_row;
          nonrow_index++) {
        ch_to_core_to_nonrow_index_bits_to_page_index_to_candidate_info
          [channel][core][nonrow_index] =
            unordered_map<long, CandidatePageInfo>();
      }
    }
    for(auto& reserved_row_interval_accesses :
        ch_to_reserved_row_to_interval_accesses.at(channel)) {
      reserved_row_interval_accesses.second = 0;
    }
  }

  bool pending_frame_replacement_done(
    const PendingFrameReplacementInfo& frame_replacement_info) {
    return (frame_replacement_info.pending_writeback_reads.empty() &&
            frame_replacement_info.pending_writeback_writes.empty() &&
            frame_replacement_info.pending_fill_reads.empty() &&
            frame_replacement_info.pending_fill_writes.empty());
  }

  void undo_frame_remapping(const int channel, const long reserved_row,
                            const int  nonrow_index_bits,
                            const long frame_to_undo) {
    assert(ch_to_page_index_remapping.at(channel).count(frame_to_undo));
    ch_to_page_index_remapping.at(channel).erase(frame_to_undo);
    assert(ch_to_reserved_row_to_nonrow_index_bits_to_remapped_src_page_index
             .at(channel)
             .at(reserved_row)
             .at(nonrow_index_bits) == frame_to_undo);
    ch_to_reserved_row_to_nonrow_index_bits_to_remapped_src_page_index
      .at(channel)
      .at(reserved_row)
      .erase(nonrow_index_bits);
    remove_from_all_associativities(frame_to_undo, channel);
  }

  void remap_frame(const int channel, const long reserved_row,
                   const int nonrow_index_bits, const long frame_to_remap,
                   const uint64_t new_mask) {
    assert(0 == ch_to_page_index_remapping.at(channel).count(frame_to_remap));
    assert(ch_to_pending_frame_remaps.at(channel).count(frame_to_remap));
    const long new_dst_frame =
      gen_frame_from_reserved_row_and_nonrow_index_bits(reserved_row,
                                                        nonrow_index_bits);
    ch_to_page_index_remapping.at(channel)[frame_to_remap] = std::make_pair(
      new_dst_frame, new_mask);
    assert(0 ==
           ch_to_reserved_row_to_nonrow_index_bits_to_remapped_src_page_index
             .at(channel)
             .at(reserved_row)
             .count(nonrow_index_bits));
    ch_to_reserved_row_to_nonrow_index_bits_to_remapped_src_page_index
      .at(channel)
      .at(reserved_row)[nonrow_index_bits] = frame_to_remap;
    ch_to_pending_frame_remaps.at(channel).erase(frame_to_remap);
    insert_into_all_associativities(frame_to_remap, channel);
  }

  Request gen_copy_request(const long orig_addr, const long cache_frame,
                           const bool remap_to_cache_frame, const int coreid,
                           const Request::Type type) {
    Request req;
    req.is_first_command = true;
    req.orig_addr        = orig_addr;
    if(remap_to_cache_frame) {
      req.addr        = map_addr_to_new_frame(orig_addr, cache_frame);
      req.is_remapped = true;
    } else {
      req.addr        = orig_addr;
      req.is_remapped = false;
    }
    req.coreid    = coreid;
    req.is_demand = false;
    req.is_copy   = true;
    req.type      = type;

    req.addr_vec.resize(addr_bits.size());
    set_req_addr_vec(req.addr, req.addr_vec);

    return req;
  }

  bool check_if_transaction_accepted(Request& req) {
    if(CopyMode::Free == remap_copy_mode)
      return true;
    else if(CopyMode::Real == remap_copy_mode) {
      assert(req.is_copy);
      return enqueue_req_to_ctrl(req, req.coreid);
    } else {
      assert(false);
      return false;
    }
  }

  void update_copy_read_write_stats(const int channel, const int coreid,
                                    const Request::Type type) {
    if(Request::Type::READ == type) {
      num_copy_reads_by_ch[channel]++;
      stats_callback(int(StatCallbackType::PAGE_REMAPPING_COPY_READ), coreid,
                     1);
    } else if(Request::Type::WRITE == type) {
      num_copy_writes_by_ch[channel]++;
      stats_callback(int(StatCallbackType::PAGE_REMAPPING_COPY_WRITE), coreid,
                     1);
    } else {
      assert(false);
    }
  }

  void process_pending_transactions(
    const int channel, const bool during_warmup, vector<long>& src_transactions,
    vector<long>& dst_transactions, const bool dst_exists,
    const long cache_frame, const Request::Type src_type,
    const bool map_src_to_cache_frame, const int src_coreid) {
    while(!src_transactions.empty()) {
      const long orig_addr = src_transactions.back();
      assert(get_coreid_from_addr(orig_addr) == src_coreid);
      Request src_request = gen_copy_request(
        orig_addr, cache_frame, map_src_to_cache_frame, src_coreid, src_type);
      if(!channels_share_tables)
        assert(channel == src_request.addr_vec[int(T::Level::Channel)]);
      if(during_warmup || check_if_transaction_accepted(src_request)) {
        update_copy_read_write_stats(channel, src_coreid, src_type);
        src_transactions.pop_back();
        if(dst_exists) {
          dst_transactions.push_back(orig_addr);
        }
      } else {
        return;
      }
    }
  }

  bool process_pending_frame_replacement(
    const int channel, const long reserved_row, const bool during_warmup,
    const int old_coreid, const int new_coreid,
    PendingFrameReplacementInfo& frame_replacement_info) {
    assert(!pending_frame_replacement_done(frame_replacement_info));
    const long cache_frame = gen_frame_from_reserved_row_and_nonrow_index_bits(
      reserved_row, frame_replacement_info.nonrow_index_bits);
    if(frame_replacement_info.orig_frame_index_valid) {
      if(!frame_replacement_info.pending_writeback_reads.empty()) {
        process_pending_transactions(
          channel, during_warmup,
          frame_replacement_info.pending_writeback_reads,
          frame_replacement_info.pending_writeback_writes, true, cache_frame,
          Request::Type::READ, true, old_coreid);
      } else if(!frame_replacement_info.pending_writeback_writes.empty()) {
        vector<long> dummy = vector<long>();
        process_pending_transactions(
          channel, during_warmup,
          frame_replacement_info.pending_writeback_writes, dummy, false,
          cache_frame, Request::Type::WRITE, false, old_coreid);
        assert(dummy.empty());
        if(frame_replacement_info.pending_writeback_writes.empty()) {
          undo_frame_remapping(channel, reserved_row,
                               frame_replacement_info.nonrow_index_bits,
                               frame_replacement_info.orig_frame_index);
        }
      }
    }

    if(frame_replacement_info.new_frame_index_valid) {
      if(!frame_replacement_info.pending_fill_reads.empty()) {
        process_pending_transactions(
          channel, during_warmup, frame_replacement_info.pending_fill_reads,
          frame_replacement_info.pending_fill_writes, true, cache_frame,
          Request::Type::READ, false, new_coreid);
      } else if(!frame_replacement_info.pending_fill_writes.empty()) {
        vector<long> dummy = vector<long>();
        process_pending_transactions(
          channel, during_warmup, frame_replacement_info.pending_fill_writes,
          dummy, false, cache_frame, Request::Type::WRITE, true, new_coreid);
        assert(dummy.empty());
        // only remap frame after for sure old frame remapping has been undone
        if(frame_replacement_info.pending_writeback_reads.empty() &&
           frame_replacement_info.pending_writeback_writes.empty()) {
          if(frame_replacement_info.pending_fill_writes.empty()) {
            remap_frame(
              channel, reserved_row, frame_replacement_info.nonrow_index_bits,
              frame_replacement_info.new_frame_index,
              frame_replacement_info.new_frame_new_lines_on_page_mask);
          }
        }
      }
    }

    return pending_frame_replacement_done(frame_replacement_info);
  }

  bool process_single_pending_row_replacement(
    const int channel, PendingRowReplacementInfo& row_replacement_info) {
    assert(!row_replacement_info.pending_frame_replacements.empty());
    bool can_pop = process_pending_frame_replacement(
      channel, row_replacement_info.reserved_row,
      row_replacement_info.during_warmup, row_replacement_info.old_coreid,
      row_replacement_info.new_coreid,
      row_replacement_info.pending_frame_replacements.back());
    if(can_pop) {
      row_replacement_info.pending_frame_replacements.pop_back();
    }
    return row_replacement_info.pending_frame_replacements.empty();
  }

  void process_pending_row_replacements(const int channel) {
    if((remap_policy == RemapPolicy::None) ||
       (remap_copy_time != CopyTime::Periodic))
      return;

    if(!ch_to_pending_row_replacements.at(channel).empty()) {
      bool row_replacement_done = process_single_pending_row_replacement(
        channel, ch_to_pending_row_replacements.at(channel).back());

      if(row_replacement_done) {
        AllocatedRowInfo allocated_row_info;
        allocated_row_info.coreid =
          ch_to_pending_row_replacements.at(channel).back().new_coreid;
        allocated_row_info.reserved_row =
          ch_to_pending_row_replacements.at(channel).back().reserved_row;
        ch_to_allocated_row_info_in_order.at(channel).push_back(
          allocated_row_info);
        ch_to_pending_row_replacements.at(channel).pop_back();
      }
    } else {
      assert((ch_to_free_rows.at(channel).size() +
              ch_to_allocated_row_info_in_order.at(channel).size()) ==
             uint(addr_remap_num_reserved_rows));
    }
  }

  void do_periodic_remap(const int channel, const int num_rows_to_allocate,
                         const bool during_warmup) {
    assert(CopyTime::Periodic == remap_copy_time);
    for(int rows_allocated = 0; rows_allocated < num_rows_to_allocate;
        rows_allocated++) {
      bool allocating_already_empty_row;
      int  coreid_of_row_being_replaced = -1;
      long row_to_allocate              = get_row_to_allocate(
        channel, allocating_already_empty_row, coreid_of_row_being_replaced);
      long current_row_accesses = get_current_row_accesses(
        channel, row_to_allocate, allocating_already_empty_row);
      double current_row_score = get_row_score(channel, current_row_accesses,
                                               coreid_of_row_being_replaced);
      int    required_row_channel_xor = get_row_channel_xor_from_row_addr(
        row_to_allocate);

      int                      chosen_candidate_coreid = -1;
      bool                     candidates_available    = false;
      unordered_map<int, long> nonrow_index_bits_to_chosen_candidate_page =
        process_all_candidates(channel, current_row_score,
                               chosen_candidate_coreid,
                               required_row_channel_xor, candidates_available);

      if(!nonrow_index_bits_to_chosen_candidate_page.empty()) {
        add_to_pending_frame_remaps(channel,
                                    nonrow_index_bits_to_chosen_candidate_page);
        if(remap_periodic_copy_candidates_org ==
           PeriodicCopyCandidatesOrg::NonrowIndex_FrameFreq) {
          finalize_nonrow_index_freq_chosen_candidates(
            channel, chosen_candidate_coreid,
            nonrow_index_bits_to_chosen_candidate_page);
        } else if(remap_periodic_copy_candidates_org ==
                  PeriodicCopyCandidatesOrg::SeqBatchFreq) {
          finalize_seq_batches_chosen_candidates(
            channel, chosen_candidate_coreid,
            nonrow_index_bits_to_chosen_candidate_page);
        } else {
          assert(false);
        }
        const int new_row_occupancy_bin =
          nonrow_index_bits_to_chosen_candidate_page.size() * 20 /
          num_frames_per_row;

        if(!allocating_already_empty_row) {
          const size_t num_pages_writing_back =
            ch_to_reserved_row_to_nonrow_index_bits_to_remapped_src_page_index
              .at(channel)
              .at(row_to_allocate)
              .size();
          update_num_reserved_pages_stat(channel, coreid_of_row_being_replaced,
                                         num_pages_writing_back, true);
          update_num_reserved_rows_stat(coreid_of_row_being_replaced, true);
          stats_callback(
            int(StatCallbackType::PERIODIC_COPY_REALLOCATED_OCCUPIED_ROW),
            chosen_candidate_coreid, new_row_occupancy_bin);
        } else {
          num_reserved_rows_allocated_by_ch[channel]++;
          assert(num_reserved_rows_allocated_by_ch[channel].value() <=
                 addr_remap_num_reserved_rows);
          stats_callback(
            int(StatCallbackType::PERIODIC_COPY_ALLOCATED_FREE_ROW),
            chosen_candidate_coreid, new_row_occupancy_bin);
        }

        const size_t num_pages_filling =
          nonrow_index_bits_to_chosen_candidate_page.size();
        update_num_reserved_pages_stat(channel, chosen_candidate_coreid,
                                       num_pages_filling, false);
        update_num_reserved_rows_stat(chosen_candidate_coreid, false);

        PendingRowReplacementInfo pending_row_replacement =
          create_pending_row_replacement_info(
            channel, row_to_allocate, coreid_of_row_being_replaced,
            chosen_candidate_coreid,
            nonrow_index_bits_to_chosen_candidate_page);
        pending_row_replacement.during_warmup = during_warmup;
        ch_to_pending_row_replacements.at(channel).push_back(
          pending_row_replacement);
      } else {
        if(candidates_available) {
          assert(valid_core_id(chosen_candidate_coreid));
          stats_callback(
            int(StatCallbackType::PERIODIC_COPY_NO_CHANGE_CANDIDATE_SCORE_LOW),
            chosen_candidate_coreid, 0);
        } else {
          stats_callback(
            int(StatCallbackType::PERIODIC_COPY_NO_CHANGE_CANDIDATE_SCORE_ZERO),
            0, 0);
        }

        reinsert_unreallocated_row(channel, row_to_allocate,
                                   coreid_of_row_being_replaced,
                                   allocating_already_empty_row);
      }
    }

    clear_interval_accesses_and_candidate_infos(channel);
    // TODO: make sure relevant stats are updated, and think about which new
    // stats should be created
    // TODO: think about adding other asserts
  }

  void check_if_threshold_satisfied(int channel, vector<long>& vec, long i,
                                    long seen_count, long desired_count,
                                    VectorStat& threshold_stat,
                                    VectorStat& num_pages_stat, bool& done) {
    if((seen_count > desired_count) && !done) {
      long threshold_candidiate  = vec[i];
      long lost_due_to_threshold = (i + 1) * threshold_candidiate;
      long remaining             = seen_count - lost_due_to_threshold;

      if(remaining > desired_count) {
        threshold_stat[channel] = threshold_candidiate;
        num_pages_stat[channel] = (i + 1);

        done = true;
      }
    }
  }

  void remove_from_shadow_cache(const long page_frame_index, const int channel,
                                const int associativity) {
    const long    cacheindex = get_cacheindex(associativity, page_frame_index);
    vector<long>& pageindex_vector =
      associativity_to_ch_to_cache_index_to_pageindex_vector.at(associativity)
        .at(channel)
        .at(cacheindex);

    vector<long>::iterator itr = find(pageindex_vector.begin(),
                                      pageindex_vector.end(), page_frame_index);
    assert(itr != pageindex_vector.cend());
    pageindex_vector.erase(itr);
  }

  void remove_from_all_associativities(const long page_frame_index,
                                       const int  channel) {
    for(int associativity = 1; associativity <= max_associativity;
        associativity *= 2) {
      remove_from_shadow_cache(page_frame_index, channel, associativity);
    }
  }

  int insert_into_shadow_cache(const long page_frame_index, const int channel,
                               const int associativity) {
    const long    cacheindex = get_cacheindex(associativity, page_frame_index);
    vector<long>& pageindex_vector =
      associativity_to_ch_to_cache_index_to_pageindex_vector.at(associativity)
        .at(channel)
        .at(cacheindex);

    assert(find(pageindex_vector.begin(), pageindex_vector.end(),
                page_frame_index) == pageindex_vector.end());
    const double ratio  = double(pageindex_vector.size()) / associativity;
    int          bucket = (ratio < 1) ? 0 : (ratio - 1) / 0.1;

    pageindex_vector.push_back(page_frame_index);
    if(bucket > 10)
      bucket = 10;
    return bucket;
  }

  void insert_into_all_associativities(const long page_frame_index,
                                       const int  channel) {
    const int direct_mapped_callback_type = int(
      StatCallbackType::SHADOW_CACHE_INSERT_DIRECT_MAPPED);
    int delta_from_direct_mapped = 0;
    for(int associativity = 1; associativity <= max_associativity;
        associativity *= 2) {
      const int bucket   = insert_into_shadow_cache(page_frame_index, channel,
                                                  associativity);
      const int stat_int = direct_mapped_callback_type +
                           delta_from_direct_mapped;
      assert((stat_int >=
              int(StatCallbackType::SHADOW_CACHE_INSERT_DIRECT_MAPPED)) &&
             (stat_int <= int(StatCallbackType::SHADOW_CACHE_INSERT_ASSOC32)));
      stats_callback(
        stat_int, get_coreid_from_addr(page_frame_index << os_page_offset_bits),
        bucket);
      delta_from_direct_mapped++;
    }
  }

  long get_num_sets(const int associativity) {
    return max_possible_pages_allocated / associativity;
  }

  long get_cacheindex(const int associatiity, const long page_frame_index) {
    const long num_sets_mask = get_num_sets(associatiity) - 1;
    return (page_frame_index & num_sets_mask);
  }

  void setup_shadow_page_remappings(const int num_channels) {
    for(int associativity = 1; associativity <= max_associativity;
        associativity *= 2) {
      associativity_to_ch_to_cache_index_to_pageindex_vector[associativity] =
        unordered_map<int, unordered_map<long, vector<long>>>();

      const long num_sets = get_num_sets(associativity);
      for(int channel = 0; channel < num_channels; channel++) {
        associativity_to_ch_to_cache_index_to_pageindex_vector.at(
          associativity)[channel] = unordered_map<long, vector<long>>();
        for(int cache_index = 0; cache_index < num_sets; cache_index++) {
          associativity_to_ch_to_cache_index_to_pageindex_vector
            .at(associativity)
            .at(channel)[cache_index] = vector<long>();
        }
      }
    }
  }

  void setup_for_remap_copy_periodic(int* sz) {
    assert(addr_remap_num_reserved_rows > 0);
    assert(remap_to_partitioned_rows);
    assert(num_frames_per_row > 0);

    max_possible_pages_allocated = addr_remap_num_reserved_rows *
                                   num_frames_per_row;

    setup_shadow_page_remappings(sz[int(T::Level::Channel)]);
    for(int ch = 0; ch < sz[int(T::Level::Channel)]; ch++) {
      ch_to_page_index_remapping[ch] =
        unordered_map<long, std::pair<long, uint64_t>>();
      ch_to_free_rows[ch]                         = vector<long>();
      ch_to_allocated_row_info_in_order[ch]       = deque<AllocatedRowInfo>();
      ch_to_core_to_interval_accesses[ch]         = unordered_map<int, long>();
      ch_to_reserved_row_to_interval_accesses[ch] = unordered_map<long, long>();
      ch_to_reserved_row_to_nonrow_index_bits_to_remapped_src_page_index[ch] =
        unordered_map<long, unordered_map<int, long>>();
      ch_to_core_to_nonrow_index_bits_to_page_index_to_candidate_info[ch] =
        unordered_map<
          int, unordered_map<int, unordered_map<long, CandidatePageInfo>>>();
      ch_to_core_to_candidate_seq_batches_info[ch] =
        unordered_map<int, CoreCandidateSeqBatchesInfo>();
      ch_to_pending_row_replacements[ch] = vector<PendingRowReplacementInfo>();
      ch_to_pending_frame_remaps[ch]     = unordered_set<long>();

      const long first_reserved_row = compute_first_reserved_row(0);
      for(long row_i = 0; row_i < addr_remap_num_reserved_rows; row_i++) {
        const long reserved_row = first_reserved_row + row_i;
        ch_to_free_rows[ch].push_back(reserved_row);
        ch_to_reserved_row_to_interval_accesses[ch][reserved_row] = 0;
        ch_to_reserved_row_to_nonrow_index_bits_to_remapped_src_page_index
          [ch][reserved_row] = unordered_map<int, long>();
      }
      for(int core = 0; core < num_cores; core++) {
        ch_to_core_to_interval_accesses[ch][core] = 0;
        ch_to_core_to_nonrow_index_bits_to_page_index_to_candidate_info
          [ch][core] =
            unordered_map<int, unordered_map<long, CandidatePageInfo>>();
        ch_to_core_to_candidate_seq_batches_info[ch][core] =
          CoreCandidateSeqBatchesInfo();

        for(int nonrow_index = 0; nonrow_index < num_frames_per_row;
            nonrow_index++) {
          ch_to_core_to_nonrow_index_bits_to_page_index_to_candidate_info
            [ch][core][nonrow_index] = unordered_map<long, CandidatePageInfo>();
        }
      }
    }
  }

  void setup_for_remap_copy_whenever(int* sz) {
    for(int ch = 0; ch < sz[int(T::Level::Channel)]; ch++) {
      if(remap_to_partitioned_rows) {
        for(int core = 0; core < num_cores; core++) {
          const long first_reserved_row = compute_first_reserved_row(core);
          core_to_ch_to_oldest_unfilled_row[core][ch] = first_reserved_row;
          core_to_ch_to_newest_unfilled_row[core][ch] = first_reserved_row;
          populate_frames_freelist_for_ch_row(ch, first_reserved_row, core);
        }
      } else {
        const long first_reserved_row        = compute_first_reserved_row(0);
        global_ch_to_oldest_unfilled_row[ch] = first_reserved_row;
        global_ch_to_newest_unfilled_row[ch] = first_reserved_row;
        populate_frames_freelist_for_ch_row(ch, first_reserved_row, 0);
      }
    }
  }

  void compute_os_page_stats(int channel) {
    std::unordered_map<long, OsPageInfo>& os_page_tracking_map =
      os_page_tracking_map_by_ch[channel];

    long              total_reuses              = 0;
    long              total_prev_written_reuses = 0;
    long              total_accesses            = 0;
    std::vector<long> reuses, accesses;
    const ulong       total_pages = os_page_tracking_map.size();
    reuses.reserve(total_pages);
    accesses.reserve(total_pages);

    for(auto page_it = os_page_tracking_map.cbegin();
        page_it != os_page_tracking_map.cend(); page_it++) {
      long reuse_count  = page_it->second.reuse_count;
      long access_count = page_it->second.access_count;
      total_reuses += reuse_count;
      total_prev_written_reuses += page_it->second.prev_written_reuse_count;
      total_accesses += access_count;
      reuses.push_back(reuse_count);
      accesses.push_back(access_count);
    }
    std::sort(reuses.begin(), reuses.end(), std::greater<long>());
    std::sort(accesses.begin(), accesses.end(), std::greater<long>());
    assert(reuses.size() == total_pages);
    assert(accesses.size() == total_pages);

    channel_reuses_by_ch[channel]              = total_reuses;
    channel_prev_written_reuses_by_ch[channel] = total_prev_written_reuses;
    channel_pages_touched_by_ch[channel]       = total_pages;
    const long reuses_50                       = 0.5 * total_reuses;
    const long reuses_80                       = 0.8 * total_reuses;
    const long accesses_50                     = 0.5 * total_accesses;
    const long accesses_80                     = 0.9 * total_accesses;

    {
      long seen_reuses   = 0;
      bool done_50_reuse = false;
      bool done_80_reuse = false;
      for(ulong i = 0; i < reuses.size(); i++) {
        seen_reuses += reuses[i];
        check_if_threshold_satisfied(channel, reuses, i, seen_reuses, reuses_50,
                                     reuse_threshold_at_50_by_ch,
                                     num_pages_threshold_at_50_reuse_by_ch,
                                     done_50_reuse);
        check_if_threshold_satisfied(channel, reuses, i, seen_reuses, reuses_80,
                                     reuse_threshold_at_80_by_ch,
                                     num_pages_threshold_at_80_reuse_by_ch,
                                     done_80_reuse);
        if(done_50_reuse && done_80_reuse)
          break;
      }
    }

    {
      long seen_accesses  = 0;
      bool done_50_access = false;
      bool done_80_access = false;
      for(ulong i = 0; i < accesses.size(); i++) {
        seen_accesses += accesses[i];
        check_if_threshold_satisfied(channel, accesses, i, seen_accesses,
                                     accesses_50, access_threshold_at_50_by_ch,
                                     num_pages_threshold_at_50_access_by_ch,
                                     done_50_access);
        check_if_threshold_satisfied(channel, accesses, i, seen_accesses,
                                     accesses_80, access_threshold_at_80_by_ch,
                                     num_pages_threshold_at_80_access_by_ch,
                                     done_80_access);
        if(done_50_access && done_80_access)
          break;
      }
    }
  }
};  // namespace ramulator

template <>
void Memory<DDR4>::populate_frames_freelist_for_ch_row(const int  channel,
                                                       const long row,
                                                       const int  coreid);

template <>
void Memory<DDR4>::BaRaBgCo7ChCo2(vector<int>& addr_vec, long addr);

template <>
void Memory<DDR4>::set_skylakeddr4_addr_vec(vector<int>& addr_vec, long addr);

} /*namespace ramulator*/

#endif /*__MEMORY_H*/
