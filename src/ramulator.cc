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
 * File         : ramulator.cc
 * Author       : SAFARI research group
 * Date         : 6/12/2018
 * Description  : Defines an interface to Ramulator
 ***************************************************************************************/

#include <deque>
#include <list>
#include <map>


#include "ramulator/Config.h"
#include "ramulator/Request.h"
#include "ramulator/ScarabWrapper.h"

extern "C" {
#include "addr_trans.h"
#include "general.param.h"
#include "globals/assert.h"
#include "globals/utils.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "ramulator.h"
#include "ramulator.param.h"
#include "statistics.h"
}

/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_MEMORY, ##args)

/**************************************************************************************/


using namespace ramulator;

ScarabWrapper* wrapper = NULL;
Config*        configs = NULL;

void to_ramulator_req(const Mem_Req* scarab_req, Request* ramulator_req);
void init_configs();
bool try_completing_request(Mem_Req* req);
void enqueue_response(Request& req);

void stats_callback(int type, uns proc_id, int bucket_index);

deque<Mem_Req*> resp_queue;  // completed read request that need to send back to
                             // Scarab
map<long, list<Mem_Req*>> inflight_read_reqs;

void ramulator_init() {
  ASSERTM(0, ICACHE_LINE_SIZE == DCACHE_LINE_SIZE,
          "Ramulator"
          "integration currently support only equal instruction and"
          "data cache line sizes! Currently, ICACHE_LINE_SIZE=%d, "
          "DCACHE_LINE_SIZE=%d \n",
          ICACHE_LINE_SIZE, DCACHE_LINE_SIZE);

  configs = new Config();
  init_configs();

  wrapper = new ScarabWrapper(*configs, DCACHE_LINE_SIZE, &stats_callback);

  DPRINTF("Initialized Ramulator. \n");
}

void ramulator_finish() {
  wrapper->finish();

  delete wrapper;
  delete configs;
}

uns get_point_in_sim_bucket(uns proc_id) {
  Counter cur_inst_count      = inst_count[proc_id];
  uns     point_in_sim_bucket = cur_inst_count / (inst_limit[proc_id] / 10);
  return point_in_sim_bucket;
}

void stat_periodic_copy_row_occupancy(const uns proc_id,
                                      const int occupancy_bucket_index) {
  assert(ALL_CORES_PERIODIC_COPY_ALLOCATED_ROW_OCCUPANCY_0_5_EXCL +
           MIN2(occupancy_bucket_index, 19) >=
         ALL_CORES_PERIODIC_COPY_ALLOCATED_ROW_OCCUPANCY_0_5_EXCL);
  assert(ALL_CORES_PERIODIC_COPY_ALLOCATED_ROW_OCCUPANCY_0_5_EXCL +
           MIN2(occupancy_bucket_index, 19) <=
         ALL_CORES_PERIODIC_COPY_ALLOCATED_ROW_OCCUPANCY_95_100_INCL);
  STAT_EVENT_ALL(ALL_CORES_PERIODIC_COPY_ALLOCATED_ROW_OCCUPANCY_0_5_EXCL +
                 MIN2(occupancy_bucket_index, 19));

  assert(PER_CORE_PERIODIC_COPY_ALLOCATED_ROW_OCCUPANCY_0_5_EXCL +
           MIN2(occupancy_bucket_index, 19) >=
         PER_CORE_PERIODIC_COPY_ALLOCATED_ROW_OCCUPANCY_0_5_EXCL);
  assert(PER_CORE_PERIODIC_COPY_ALLOCATED_ROW_OCCUPANCY_0_5_EXCL +
           MIN2(occupancy_bucket_index, 19) <=
         PER_CORE_PERIODIC_COPY_ALLOCATED_ROW_OCCUPANCY_95_100_INCL);
  STAT_EVENT(proc_id, PER_CORE_PERIODIC_COPY_ALLOCATED_ROW_OCCUPANCY_0_5_EXCL +
                        MIN2(occupancy_bucket_index, 19));
}

void stats_callback(int type, uns proc_id, int bucket_index) {
  int stat_index;

  uns point_in_sim_bucket = get_point_in_sim_bucket(proc_id);

  switch(type) {
    case int(StatCallbackType::DRAM_ACT):
      STAT_EVENT(proc_id, POWER_DRAM_ACTIVATE);
      break;
    case int(StatCallbackType::DRAM_PRE):
      STAT_EVENT(proc_id, POWER_DRAM_PRECHARGE);
      break;
    case int(StatCallbackType::DRAM_READ):
      STAT_EVENT(proc_id, POWER_DRAM_READ);
      STAT_EVENT_ALL(ALL_CORES_DRAM_ACCESS);
      STAT_EVENT(proc_id, PER_CORE_DRAM_ACCESS);
      STAT_EVENT_ALL(ALL_CORES_DRAM_READ);
      STAT_EVENT(proc_id, PER_CORE_DRAM_READ);
      break;
    case int(StatCallbackType::DRAM_WRITE):
      STAT_EVENT(proc_id, POWER_DRAM_WRITE);
      STAT_EVENT_ALL(ALL_CORES_DRAM_ACCESS);
      STAT_EVENT(proc_id, PER_CORE_DRAM_ACCESS);
      STAT_EVENT_ALL(ALL_CORES_DRAM_WRITE);
      STAT_EVENT(proc_id, PER_CORE_DRAM_WRITE);
      break;
    case int(StatCallbackType::DEMAND_COL_REUSE):
      assert(bucket_index >= -1 && bucket_index <= 127);
      stat_index = (-1 == bucket_index) ? -1 : (bucket_index / 4);
      assert(((ALL_CORES_DRAM_DEMAND_COL_REUSE_DIST_0_3 + stat_index) >=
              ALL_CORES_DRAM_DEMAND_COL_REUSE_DIST_INF) &&
             ((ALL_CORES_DRAM_DEMAND_COL_REUSE_DIST_0_3 + stat_index) <=
              ALL_CORES_DRAM_DEMAND_COL_REUSE_DIST_124_127));
      STAT_EVENT_ALL(ALL_CORES_DRAM_DEMAND_COL_REUSE_DIST_0_3 + stat_index);
      assert(((PER_CORE_DRAM_DEMAND_COL_REUSE_DIST_0_3 + stat_index) >=
              PER_CORE_DRAM_DEMAND_COL_REUSE_DIST_INF) &&
             ((PER_CORE_DRAM_DEMAND_COL_REUSE_DIST_0_3 + stat_index) <=
              PER_CORE_DRAM_DEMAND_COL_REUSE_DIST_124_127));
      STAT_EVENT(proc_id, PER_CORE_DRAM_DEMAND_COL_REUSE_DIST_0_3 + stat_index);

    case int(StatCallbackType::NONDEMAND_COL_REUSE):
      assert(bucket_index >= -1 && bucket_index <= 127);
      stat_index = (-1 == bucket_index) ? -1 : (bucket_index / 4);
      assert(((ALL_CORES_DRAM_ALL_COL_REUSE_DIST_0_3 + stat_index) >=
              ALL_CORES_DRAM_ALL_COL_REUSE_DIST_INF) &&
             ((ALL_CORES_DRAM_ALL_COL_REUSE_DIST_0_3 + stat_index) <=
              ALL_CORES_DRAM_ALL_COL_REUSE_DIST_124_127));
      STAT_EVENT_ALL(ALL_CORES_DRAM_ALL_COL_REUSE_DIST_0_3 + stat_index);
      assert(((PER_CORE_DRAM_ALL_COL_REUSE_DIST_0_3 + stat_index) >=
              PER_CORE_DRAM_ALL_COL_REUSE_DIST_INF) &&
             ((PER_CORE_DRAM_ALL_COL_REUSE_DIST_0_3 + stat_index) <=
              PER_CORE_DRAM_ALL_COL_REUSE_DIST_124_127));
      STAT_EVENT(proc_id, PER_CORE_DRAM_ALL_COL_REUSE_DIST_0_3 + stat_index);
      break;

    case int(StatCallbackType::DEMAND_ROW_REUSE):
      assert(bucket_index >= -1 && bucket_index <= ((int)RAMULATOR_ROWS - 1));
      if(-1 == bucket_index) {
        stat_index = -1;
      } else {
        if(bucket_index < 256) {
          stat_index = bucket_index;
        } else {
          stat_index = MIN2(270, 256 + ((bucket_index - 256) / 128));
        }
      }

      assert(((ALL_CORES_DRAM_DEMAND_ROW_REUSE_DIST_0 + stat_index) >=
              ALL_CORES_DRAM_DEMAND_ROW_REUSE_DIST_INF) &&
             ((ALL_CORES_DRAM_DEMAND_ROW_REUSE_DIST_0 + stat_index) <=
              ALL_CORES_DRAM_DEMAND_ROW_REUSE_DIST_2048_MORE));
      STAT_EVENT_ALL(ALL_CORES_DRAM_DEMAND_ROW_REUSE_DIST_0 + stat_index);

      assert(((PER_CORE_DRAM_DEMAND_ROW_REUSE_DIST_0 + stat_index) >=
              PER_CORE_DRAM_DEMAND_ROW_REUSE_DIST_INF) &&
             ((PER_CORE_DRAM_DEMAND_ROW_REUSE_DIST_0 + stat_index) <=
              PER_CORE_DRAM_DEMAND_ROW_REUSE_DIST_2048_MORE));
      STAT_EVENT(proc_id, PER_CORE_DRAM_DEMAND_ROW_REUSE_DIST_0 + stat_index);

    case int(StatCallbackType::NONDEMAND_ROW_REUSE):
      assert(bucket_index >= -1 && bucket_index <= ((int)RAMULATOR_ROWS - 1));
      if(-1 == bucket_index) {
        stat_index = -1;
      } else {
        if(bucket_index < 256) {
          stat_index = bucket_index;
        } else {
          stat_index = MIN2(270, 256 + ((bucket_index - 256) / 128));
        }
      }

      assert(((ALL_CORES_DRAM_ALL_ROW_REUSE_DIST_0 + stat_index) >=
              ALL_CORES_DRAM_ALL_ROW_REUSE_DIST_INF) &&
             ((ALL_CORES_DRAM_ALL_ROW_REUSE_DIST_0 + stat_index) <=
              ALL_CORES_DRAM_ALL_ROW_REUSE_DIST_2048_MORE));
      STAT_EVENT_ALL(ALL_CORES_DRAM_ALL_ROW_REUSE_DIST_0 + stat_index);

      assert(((PER_CORE_DRAM_ALL_ROW_REUSE_DIST_0 + stat_index) >=
              PER_CORE_DRAM_ALL_ROW_REUSE_DIST_INF) &&
             ((PER_CORE_DRAM_ALL_ROW_REUSE_DIST_0 + stat_index) <=
              PER_CORE_DRAM_ALL_ROW_REUSE_DIST_2048_MORE));
      STAT_EVENT(proc_id, PER_CORE_DRAM_ALL_ROW_REUSE_DIST_0 + stat_index);
      break;

    case int(StatCallbackType::ROW_REUSE_TIME): {
      int all_cores_index = ALL_CORES_DRAM_ROW_REUSE_TIME_ROW_HIT +
                            bucket_index;
      assert((all_cores_index >= ALL_CORES_DRAM_ROW_REUSE_TIME_INF) &&
             (all_cores_index <= ALL_CORES_DRAM_ROW_REUSE_TIME_MORE));
      STAT_EVENT_ALL(all_cores_index);

      int per_core_index = PER_CORE_DRAM_ROW_REUSE_TIME_ROW_HIT + bucket_index;
      assert((per_core_index >= PER_CORE_DRAM_ROW_REUSE_TIME_INF) &&
             (per_core_index <= PER_CORE_DRAM_ROW_REUSE_TIME_MORE));
      STAT_EVENT(proc_id, per_core_index);
      break;
    }

    case int(StatCallbackType::PAGE_REMAPPED):
      INC_STAT_EVENT_ALL(ALL_CORES_PAGE_REMAPPED, bucket_index);
      INC_STAT_EVENT(proc_id, PER_CORE_PAGE_REMAPPED, bucket_index);
      break;

    case int(StatCallbackType::PAGE_REMAPPING_COPY_READ):
      // using bucket_index to convey how many copies reads were performed
      // for Free Page copies, might copy multiple lines at a time
      INC_STAT_EVENT_ALL(ALL_CORES_REMAP_COPY_READ, bucket_index);
      INC_STAT_EVENT(proc_id, PER_CORE_REMAP_COPY_READ, bucket_index);

      STAT_EVENT(proc_id, PER_CORE_REMAP_COPY_READ_POINT_IN_SIM_0_9 +
                            MIN2(point_in_sim_bucket, 10));

      break;

    case int(StatCallbackType::PAGE_REMAPPING_COPY_WRITE):
      // using bucket_index to convey how many copies writes were performed
      // for Free Page copies, might copy multiple lines at a time
      INC_STAT_EVENT_ALL(ALL_CORES_REMAP_COPY_WRITE, bucket_index);
      INC_STAT_EVENT(proc_id, PER_CORE_REMAP_COPY_WRITE, bucket_index);

      STAT_EVENT(proc_id, PER_CORE_REMAP_COPY_WRITE_POINT_IN_SIM_0_9 +
                            MIN2(point_in_sim_bucket, 10));

      break;

    case int(StatCallbackType::REMAPPED_DATA_ACCESS):
      STAT_EVENT_ALL(ALL_CORES_REMAPPED_DATA_ACCESS);
      STAT_EVENT(proc_id, PER_CORE_REMAPPED_DATA_ACCESS);

      STAT_EVENT(proc_id, PER_CORE_REMAPPED_DATA_ACCESS_POINT_IN_SIM_0_9 +
                            MIN2(point_in_sim_bucket, 10));

      break;

    case int(StatCallbackType::DRAM_ORACLE_REUSE):
      STAT_EVENT_ALL(ALL_CORES_DRAM_ORACLE_REUSE);
      STAT_EVENT(proc_id, PER_CORE_DRAM_ORACLE_REUSE);
      break;

    case int(StatCallbackType::DRAM_ORACLE_PREV_WRITTEN_REUSE):
      STAT_EVENT_ALL(ALL_CORES_DRAM_ORACLE_PREV_WRITTEN_REUSE);
      STAT_EVENT(proc_id, PER_CORE_DRAM_ORACLE_PREV_WRITTEN_REUSE);
      break;

    case int(StatCallbackType::ROW_ALLOCATED):
      INC_STAT_EVENT_ALL(ALL_CORES_ROWS_ALLOCATED, bucket_index);
      INC_STAT_EVENT(proc_id, PER_CORE_ROWS_ALLOCATED, bucket_index);
      break;

    case int(StatCallbackType::PERIODIC_COPY_ALLOCATED_FREE_ROW):
      STAT_EVENT_ALL(ALL_CORES_PERIODIC_COPY_RESULT_ALLOCATED_FREE_ROW);
      STAT_EVENT(proc_id, PER_CORE_PERIODIC_COPY_RESULT_ALLOCATED_FREE_ROW);

      stat_periodic_copy_row_occupancy(proc_id, bucket_index);
      break;

    case int(StatCallbackType::PERIODIC_COPY_REALLOCATED_OCCUPIED_ROW):
      STAT_EVENT_ALL(ALL_CORES_PERIODIC_COPY_RESULT_REALLOCATED_OCCUPIED_ROW);
      STAT_EVENT(proc_id,
                 PER_CORE_PERIODIC_COPY_RESULT_REALLOCATED_OCCUPIED_ROW);

      stat_periodic_copy_row_occupancy(proc_id, bucket_index);
      break;

    case int(StatCallbackType::PERIODIC_COPY_NO_CHANGE_CANDIDATE_SCORE_LOW):
      STAT_EVENT_ALL(
        ALL_CORES_PERIODIC_COPY_RESULT_NO_CHANGE_CANDIDATE_SCORE_LOW);
      STAT_EVENT(proc_id,
                 PER_CORE_PERIODIC_COPY_RESULT_NO_CHANGE_CANDIDATE_SCORE_LOW);
      break;

    case int(StatCallbackType::PERIODIC_COPY_NO_CHANGE_CANDIDATE_SCORE_ZERO):
      STAT_EVENT_ALL(
        ALL_CORES_PERIODIC_COPY_RESULT_NO_CHANGE_CANDIDATE_SCORE_ZERO);

      break;

    default:
      assert(false);
  }
}

void init_configs() {
  configs->set_core_num(NUM_CORES);  // This has to be 1. We do not instantiate
                                     // CPU cores in Ramulator when using it
                                     // with Scarab as opposed to when using
                                     // Ramulator standalone. So, this should
                                     // not have any effect other than printing
                                     // "per-core" statistics in the output
                                     // file.

  configs->add("standard", RAMULATOR_STANDARD);
  configs->add("speed", RAMULATOR_SPEED);
  configs->add("org", RAMULATOR_ORG);

  configs->add("channels", to_string(RAMULATOR_CHANNELS));
  configs->add("ranks", to_string(RAMULATOR_RANKS));
  configs->add("bank_groups", to_string(RAMULATOR_BANKGROUPS));
  configs->add("banks", to_string(RAMULATOR_BANKS));
  configs->add("rows", to_string(RAMULATOR_ROWS));
  configs->add("columns", to_string(RAMULATOR_COLS));

  configs->add("chip_width", to_string(RAMULATOR_CHIP_WIDTH));
  configs->add("channel_width", to_string(BUS_WIDTH_IN_BYTES * 8));

  configs->add("record_cmd_trace", RAMULATOR_REC_CMD_TRACE);
  configs->add("print_cmd_trace", RAMULATOR_PRINT_CMD_TRACE);
  configs->add("use_rest_of_addr_as_row_addr",
               RAMULATOR_USE_REST_OF_ADDR_AS_ROW_ADDR);
  configs->add("track_col_reuse_distance", RAMULATOR_TRACK_COL_REUSE_DISTANCE);
  configs->add("track_row_reuse_distance", RAMULATOR_TRACK_ROW_REUSE_DISTANCE);
  configs->add("track_os_page_reuse", RAMULATOR_TRACK_OS_PAGE_REUSE);
  configs->add("row_always_0", RAMULATOR_ROW_ALWAYS_0);
  configs->add("addr_map_type", RAMULATOR_ADDR_MAP_TYPE);
  configs->add("addr_remap_policy", RAMULATOR_ADDR_REMAP_POLICY);
  configs->add("addr_remap_copy_mode", RAMULATOR_ADDR_REMAP_COPY_MODE);
  configs->add("addr_remap_copy_granularity",
               RAMULATOR_ADDR_REMAP_COPY_GRANULARITY);
  configs->add("addr_remap_copy_time", RAMULATOR_ADDR_REMAP_COPY_TIME);
  configs->add("addr_remap_periodic_copy_select_policy",
               RAMULATOR_ADDR_REMAP_PERIODIC_COPY_SELECT_POLICY);
  configs->add("addr_remap_periodic_copy_intracore_select_policy",
               RAMULATOR_ADDR_REMAP_PERIODIC_COPY_INTRACORE_SELECT_POLICY);
  configs->add("addr_remap_periodic_copy_candidates_org",
               RAMULATOR_ADDR_REMAP_PERIODIC_COPY_CANDIDATES_ORG);
  configs->add("addr_remap_page_access_threshold",
               to_string(RAMULATOR_ADDR_REMAP_PAGE_ACCESS_THRESHOLD));
  configs->add("addr_remap_page_reuse_threshold",
               to_string(RAMULATOR_ADDR_REMAP_PAGE_REUSE_THRESHOLD));
  configs->add("addr_remap_max_per_core_limit_mb",
               to_string(RAMULATOR_ADDR_REMAP_MAX_PER_CORE_LIMIT_MB));
  configs->add("addr_remap_num_reserved_rows",
               to_string(RAMULATOR_ADDR_REMAP_NUM_RESERVED_ROWS));
  configs->add(
    "addr_remap_dram_cycles_between_periodic_copy",
    to_string(RAMULATOR_ADDR_REMAP_DRAM_CYCLES_BETWEEN_PERIODIC_COPY));
  configs->add("addr_remap_to_partitioned_rows",
               RAMULATOR_ADDR_REMAP_TO_PARTITIONED_ROWS);

  configs->add("scheduling_policy", RAMULATOR_SCHEDULING_POLICY);
  configs->add("readq_entries", to_string(RAMULATOR_READQ_ENTRIES));
  configs->add("writeq_entries", to_string(RAMULATOR_WRITEQ_ENTRIES));
  configs->add("output_dir", OUTPUT_DIR);

  // TODO: make these optional and use the preset values specified by
  // RAMULATOR_SPEED for timings that are not explicitly provided in
  // ramulator.param.def
  configs->add("tCK", to_string(RAMULATOR_TCK));
  configs->add("tCL", to_string(RAMULATOR_TCL));
  configs->add("tCCD", to_string(RAMULATOR_TCCD));
  configs->add("tCCDS", to_string(RAMULATOR_TCCDS));
  configs->add("tCCDL", to_string(RAMULATOR_TCCDL));
  configs->add("tCWL", to_string(RAMULATOR_TCWL));
  configs->add("tBL", to_string(RAMULATOR_TBL));
  configs->add("tWTR", to_string(RAMULATOR_TWTR));
  configs->add("tWTRS", to_string(RAMULATOR_TWTRS));
  configs->add("tWTRL", to_string(RAMULATOR_TWTRL));
  configs->add("tRP", to_string(RAMULATOR_TRP));
  configs->add("tRPpb", to_string(RAMULATOR_TRPpb));
  configs->add("tRPab", to_string(RAMULATOR_TRPab));
  configs->add("tRCD", to_string(RAMULATOR_TRCD));
  configs->add("tRCDR", to_string(RAMULATOR_TRCDR));
  configs->add("tRCDW", to_string(RAMULATOR_TRCDW));
  configs->add("tRAS", to_string(RAMULATOR_TRAS));
}


int ramulator_send(Mem_Req* scarab_req) {
  Request req;

  to_ramulator_req(scarab_req, &req);

  // printf("Ramulator: Received a (%s) request to address %llu\n",
  // Mem_Req_Type_str(scarab_req->type), scarab_req->addr);

  // does inflight_read_reqs have the proc_id in the req?
  assert(req.orig_addr == req.addr);
  auto it_scarab_req = inflight_read_reqs.find(req.orig_addr);
  if(it_scarab_req != inflight_read_reqs.end()) {
    DEBUG(scarab_req->proc_id,
          "Ramulator: Duplicate (%s) request to address %llx\n",
          Mem_Req_Type_str(scarab_req->type), scarab_req->addr);

    if(req.type == Request::Type::READ)
      inflight_read_reqs[req.orig_addr].push_back(
        scarab_req);  // save it as an inflight request so later it will be
                      // moved to the resp_queue at the same time with the older
                      // request
    scarab_req->mem_queue_cycle = cycle_count;
    return true;  // a request to the same address is already issued
  }

  bool is_sent = wrapper->send(req);

  if(is_sent) {
    STAT_EVENT(scarab_req->proc_id, POWER_MEMORY_CTRL_ACCESS);

    if(req.type == Request::Type::READ) {
      ASSERTM(
        0, inflight_read_reqs.find(req.orig_addr) == inflight_read_reqs.end(),
        "ERROR: A read request to the same address shouldn't be sent "
        "multiple times to Ramulator\n");
      inflight_read_reqs[req.orig_addr].push_back(scarab_req);
      STAT_EVENT(scarab_req->proc_id, POWER_MEMORY_CTRL_READ);
    } else if(req.type == Request::Type::WRITE) {
      STAT_EVENT(scarab_req->proc_id, POWER_MEMORY_CTRL_WRITE);
    }

    scarab_req->mem_queue_cycle = cycle_count;
  }

  if(is_sent) {
    DEBUG(scarab_req->proc_id, "Ramulator: The request has been enqueued.\n");
  } else {
    DEBUG(scarab_req->proc_id,
          "Ramulator: The request has been rejected. Queue full?\n");
  }

  return (int)is_sent;
}

void warmup_setup_scarab_req(Mem_Req& scarab_req, uns proc_id, Addr addr,
                             Flag is_writeback) {
  scarab_req.proc_id = proc_id;
  scarab_req.state   = MRS_MEM_NEW;
  if(is_writeback)
    scarab_req.type = MRT_WB;
  else
    scarab_req.type = MRT_DFETCH;
  scarab_req.phys_addr = addr_translate(addr);
}

void warmup_llc_miss(uns proc_id, Addr addr, Flag is_writeback) {
  Mem_Req scarab_req;
  warmup_setup_scarab_req(scarab_req, proc_id, addr, is_writeback);
  Request req;
  to_ramulator_req(&scarab_req, &req);
  wrapper->warmup_process_req(req);
}

void warmup_do_periodic_copy(const int num_rows_to_remap) {
  wrapper->warmup_perform_periodic_copy(num_rows_to_remap);
}

void enqueue_response(Request& req) {
  // This should only be called by READ requests
  ASSERTM(0, req.type == Request::Type::READ,
          "ERROR: Responses should be sent only for read requests! \n");
  ASSERTM(0, inflight_read_reqs.find(req.orig_addr) != inflight_read_reqs.end(),
          "ERROR: A corresponding Scarab request was not found for the "
          "Ramulator request that read address: %lu\n",
          req.orig_addr);

  auto it_scarab_req = inflight_read_reqs.find(req.orig_addr);
  for(auto req : it_scarab_req->second)
    resp_queue.push_back(req);
  // resp_queue.push_back(it_scarab_req->second);
  it_scarab_req->second.clear();
  inflight_read_reqs.erase(it_scarab_req);
}

bool try_completing_request(Mem_Req* req) {
  if((unsigned int)mem->l1fill_queue.entry_count < MEM_L1_FILL_QUEUE_ENTRIES) {
    DEBUG(req->proc_id,
          "Ramulator: Completing a (%s) request to address %llx\n",
          Mem_Req_Type_str(req->type), req->addr);

    mem_complete_bus_in_access(
      req, 0 /*mem->mem_queue.base[ii].priority*/);  // TODO_hasan: how do we
                                                     // need to set the
                                                     // priority?

    // remove from mem queue - how do we handle this now?
    // mem_queue_removal_count++;
    // l1fill_queue_insertion_count++;
    // mem->mem_queue.base[ii].priority =
    // Mem_Req_Priority_Offset[MRT_MIN_PRIORITY];
    // memview_memqueue(MEMVIEW_MEMQUEUE_DEPART, req);

    // if (MEM_MEM_QUEUE_PARTITION_ENABLE) {
    //    ASSERT(0, mem->mem_queue_entry_count_bank[req->mem_flat_bank] > 0);
    //    mem->mem_queue_entry_count_bank[req->mem_flat_bank]--;
    //}

    return true;
  }

  return false;
}

void to_ramulator_req(const Mem_Req* scarab_req, Request* ramulator_req) {
  ASSERTM(scarab_req->proc_id, scarab_req->state == MRS_MEM_NEW,
          "A"
          " request in state %d cannot be issued to Ramulator\n",
          scarab_req->state);

  // only MRT_WB should result in a DRAM write. A plain store miss should still
  // result in a DRAM read
  if(scarab_req->type == MRT_WB)
    ramulator_req->type = Request::Type::WRITE;
  else if(scarab_req->type == MRT_DFETCH || scarab_req->type == MRT_DSTORE ||
          scarab_req->type == MRT_IFETCH || scarab_req->type == MRT_IPRF ||
          scarab_req->type == MRT_DPRF)
    ramulator_req->type = Request::Type::READ;
  else
    ASSERTM(scarab_req->proc_id, false,
            "Ramulator: Currently unsupported Scarab request type: %d\n",
            scarab_req->type);

  ramulator_req->addr      = scarab_req->phys_addr;
  ramulator_req->orig_addr = scarab_req->phys_addr;
  ramulator_req->coreid    = scarab_req->proc_id;
  ramulator_req->is_demand = mem_req_type_is_demand(scarab_req->type);

  ramulator_req->callback = enqueue_response;
}

void ramulator_tick() {
  wrapper->tick();

  if(resp_queue.size() > 0) {
    if(try_completing_request(resp_queue.front()))
      resp_queue.pop_front();
  }
}

int ramulator_get_chip_width() {
  return wrapper->get_chip_width();
}

int ramulator_get_chip_size() {
  return wrapper->get_chip_size();
}

int ramulator_get_num_chips() {
  return wrapper->get_num_chips();
}

int ramulator_get_chip_row_buffer_size() {
  return wrapper->get_chip_row_buffer_size();
}

// Mem_Req* ramulator_search_queue(Addr addr, Mem_Req_Type type) {
//
//    // TODO: assert read request
//    ASSERTM(proc_id, (type == MRT_IFETCH) || (type == MRT_DFETCH) ||
//            (type == MRT_IPRF) || (type == MRT_DPRF), "Ramulator: Cannot
//            search write requests in Ramulator's request queue\n");
//
//    auto it_req = inflight_read_reqs.find(addr)
//
//    if(it_req != inflight_read_reqs.end())
//        return it_req->second;
//
//    return NULL;
//}
