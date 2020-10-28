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
#include <bitset>
#include <cassert>
#include <cmath>
#include <functional>
#include <limits.h>
#include <set>
#include <tuple>
#include <unordered_map>
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
  virtual double clk_ns()                                         = 0;
  virtual void   tick()                                           = 0;
  virtual bool   send(Request& req)                               = 0;
  virtual int    pending_requests()                               = 0;
  virtual void   finish(void)                                     = 0;
  virtual long   page_allocator(long addr, int coreid)            = 0;
  virtual void   record_core(int coreid)                          = 0;
  virtual void   set_high_writeq_watermark(const float watermark) = 0;
  virtual void   set_low_writeq_watermark(const float watermark)  = 0;

  virtual int get_chip_width() = 0;
  virtual int get_chip_size()  = 0;
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

    type                      = parse_addr_map_type(configs);
    remap_policy              = parse_addr_remap_policy(configs);
    remap_copy_mode           = parse_addr_remap_copy_mode(configs);
    remap_copy_granularity    = parse_addr_remap_copy_granularity(configs);
    track_os_pages            = configs.get_config("track_os_page_reuse");
    remap_to_partitioned_rows = configs.get_config(
      "addr_remap_to_partitioned_rows");
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
    num_reserved_rows_allocated_by_ch.init(sz[int(T::Level::Channel)])
      .name("num_reserved_rows_allocated_by_ch")
      .desc("How many total reserved rows were allocated");
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
      vector<int> dummy_addr_vec(addr_bits.size());
      set_req_addr_vec(
        0, dummy_addr_vec);  // just to initialize addr_bits_start_pos

      for(int core = 0; core < num_cores; core++) {
        core_to_ch_to_oldest_unfilled_row[core] = unordered_map<int, long>();
        core_to_ch_to_newest_unfilled_row[core] = unordered_map<int, long>();
      }

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
  }

  ~Memory() {
    for(auto ctrl : ctrls)
      delete ctrl;
    delete spec;
  }

  double clk_ns() { return spec->speed_entry.tCK; }

  int get_chip_width() { return spec->org_entry.dq; }

  int get_chip_size() { return spec->org_entry.size; }

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

    bool is_active = false;
    for(auto ctrl : ctrls) {
      is_active = is_active || ctrl->is_active();
      ctrl->tick();
    }
    if(is_active) {
      ramulator_active_cycles++;
    }
  }

  bool send(Request& req) {
    req.addr_vec.resize(addr_bits.size());
    long      addr   = req.addr;
    const int coreid = req.coreid;
    assert(get_coreid_from_addr(addr) == coreid);

    if(!req.is_remapped && is_read_or_write_req(req)) {
      assert(req.orig_addr == req.addr);
      if(remap_policy == RemapPolicy::AlwaysToAlternate) {
        remap_if_alt_frame_exists(req);
      } else if(remap_policy == RemapPolicy::Flexible) {
        assert(false);
      }
    }

    if(!req.is_remapped)
      set_req_addr_vec(addr, req.addr_vec);

    if(row_always_0 && is_read_or_write_req(req)) {
      req.addr_vec[int(T::Level::Row)] = 0;
    }

    return enqueue_req_to_ctrl(req, coreid);
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

  const int   stride_to_upper_xored_bit        = 4;
  int         addr_remap_page_access_threshold = -1;
  int         addr_remap_page_reuse_threshold  = -1;
  bool        remap_to_partitioned_rows;
  vector<int> channel_xor_bits_pos;
  vector<int> frame_index_channel_xor_bits_pos;

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
        if(req.is_remapped) {
          ++num_reads_to_reserved_rows_by_ch[channel];
          stats_callback(int(StatCallbackType::REMAPPED_DATA_ACCESS),
                         req.coreid, 0);
        }
      }
      if(req.type == Request::Type::WRITE) {
        ++num_write_requests[coreid];
        if(req.is_remapped) {
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

  void assert_remapped_addr_proc_id(const long addr, const int coreid) {
    if(remap_to_partitioned_rows) {
      assert(get_coreid_from_addr(addr) == num_cores + coreid);
    } else {
      assert(get_coreid_from_addr(addr) == num_cores);
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

  bool row_freelist_exhausted(const int channel, const long row) {
    return (
      ch_to_row_to_ch_freebits_parity_to_avail_frames[channel][row][0]
        .empty() &&
      ch_to_row_to_ch_freebits_parity_to_avail_frames[channel][row][1].empty());
  }

  void remap_if_alt_frame_exists(Request& req) {
    assert(!req.is_copy);
    assert(!req.is_remapped);
    assert(req.is_first_command);
    assert(is_read_or_write_req(req));
    vector<int> orig_addr_vec(addr_bits.size());
    set_req_addr_vec(req.addr, orig_addr_vec);

    long orig_frame_index;
    long line_on_page_bit;
    get_page_index_offset_bit(req.addr, orig_frame_index, line_on_page_bit);
    int channel = orig_addr_vec[int(T::Level::Channel)];
    if(ch_to_page_index_remapping.count(channel)) {
      if(ch_to_page_index_remapping.at(channel).count(orig_frame_index)) {
        uint64_t copied_lines_mask =
          ch_to_page_index_remapping.at(channel).at(orig_frame_index).second;
        if((copied_lines_mask & line_on_page_bit) ||
           req.type == Request::Type::WRITE) {
          req.addr = map_addr_to_new_frame(
            req.addr,
            ch_to_page_index_remapping.at(channel).at(orig_frame_index).first);
          assert_remapped_addr_proc_id(req.addr, req.coreid);
          set_req_addr_vec(req.addr, req.addr_vec);
          assert(req.addr_vec[int(T::Level::Channel)] ==
                 orig_addr_vec[int(T::Level::Channel)]);
          req.is_remapped = true;
        }
      }
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
    stats_callback(int(StatCallbackType::PAGE_REMAPPED), coreid, 0);
  }

  void remap_and_copy(const int channel, const long frame_index,
                      const long line_on_page_bit, const Request& orig_req) {
    if(0 == ch_to_page_index_remapping.count(channel)) {
      ch_to_page_index_remapping[channel] =
        unordered_map<long, std::pair<long, uint64_t>>();
    }

    if(0 == ch_to_page_index_remapping[channel].count(frame_index)) {
      get_new_free_frame(channel, frame_index, orig_req.coreid);
    }

    // actually copy the line(s)
    uint64_t copied_lines_mask =
      ch_to_page_index_remapping[channel][frame_index].second;

    const long new_frame_index =
      ch_to_page_index_remapping[channel][frame_index].first;
    const long new_addr = map_addr_to_new_frame(orig_req.addr, new_frame_index);
    assert_remapped_addr_proc_id(new_addr, orig_req.coreid);

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
          if(-1 != addr_remap_page_reuse_threshold) {
            if(os_page_tracking_map.at(page_index).reuse_count >
               addr_remap_page_reuse_threshold) {
              remap_and_copy(channel, page_index, line_on_page_bit, req);
            }
          } else if(-1 != addr_remap_page_access_threshold) {
            if(os_page_tracking_map.at(page_index).access_count >
               addr_remap_page_access_threshold) {
              remap_and_copy(channel, page_index, line_on_page_bit, req);
            }
          }
        }
      }
    }
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
};

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
