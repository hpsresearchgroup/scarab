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
#include <utility>


#include "ramulator/Config.h"
#include "ramulator/Request.h"
#include "ramulator/ScarabWrapper.h"

extern "C" {
#include "general.param.h"
#include "globals/assert.h"
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

void stats_callback(int coreid, int type);

deque<pair<long, Mem_Req*>> resp_queue;  // completed read request that need to
                                         // send back to Scarab

map<long, list<Mem_Req*>> inflight_read_reqs;
// map<long, Mem_Req*> inflight_read_reqs;

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

void stats_callback(int coreid, int type) {
  switch(type) {
    case int(StatCallbackType::DRAM_ACT):
      STAT_EVENT(coreid, POWER_DRAM_ACTIVATE);
      break;
    case int(StatCallbackType::DRAM_PRE):
      STAT_EVENT(coreid, POWER_DRAM_PRECHARGE);
      break;
    case int(StatCallbackType::DRAM_READ):
      STAT_EVENT(coreid, POWER_DRAM_READ);
      break;
    case int(StatCallbackType::DRAM_WRITE):
      STAT_EVENT(coreid, POWER_DRAM_WRITE);
      break;
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
  auto it_scarab_req = inflight_read_reqs.find(req.addr);
  if(it_scarab_req != inflight_read_reqs.end() &&
     req.type == Request::Type::READ) {
    DEBUG(scarab_req->proc_id,
          "Ramulator: Duplicate (%s) request to address %llx\n",
          Mem_Req_Type_str(scarab_req->type), scarab_req->addr);
    // Can have duplicate Ifetch and Dfetch requests, but only one of each
    ASSERT(0, it_scarab_req->second.size() <= 1);

    if(req.type == Request::Type::READ)
      inflight_read_reqs[req.addr].push_back(
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
      ASSERTM(0, inflight_read_reqs.find(req.addr) == inflight_read_reqs.end(),
              "ERROR: A read request to the same address shouldn't be sent "
              "multiple times to Ramulator\n");
      // inflight_read_reqs[req.addr] = scarab_req;
      inflight_read_reqs[req.addr].push_back(scarab_req);
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

void enqueue_response(Request& req) {
  // This should only be called by READ requests
  ASSERTM(0, req.type == Request::Type::READ,
          "ERROR: Responses should be sent only for read requests! \n");
  ASSERTM(0, inflight_read_reqs.find(req.addr) != inflight_read_reqs.end(),
          "ERROR: A corresponding Scarab request was not found for the "
          "Ramulator request that read address: %lu\n",
          req.addr);

  auto it_scarab_req = inflight_read_reqs.find(req.addr);
  for(auto req : it_scarab_req->second)
    resp_queue.push_back(make_pair(it_scarab_req->first, req));
  // resp_queue.push_back(make_pair(it_scarab_req->first,
  // it_scarab_req->second));
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

  ramulator_req->addr   = scarab_req->phys_addr;
  ramulator_req->coreid = scarab_req->proc_id;

  ramulator_req->callback = enqueue_response;
}

void ramulator_tick() {
  wrapper->tick();

  if(resp_queue.size() > 0) {
    if(try_completing_request(resp_queue.front().second))
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

Mem_Req* ramulator_search_queue(long phys_addr, Mem_Req_Type type) {
  ASSERTM(
    0,
    (type == MRT_IFETCH) || (type == MRT_DFETCH) || (type == MRT_IPRF) ||
      (type == MRT_DPRF) || (type == MRT_DSTORE) || (type == MRT_MIN_PRIORITY),
    "Ramulator: Cannot search write requests in Ramulator request queue\n");
  auto it_req = inflight_read_reqs.find(phys_addr);

  // Search request queue
  if(it_req != inflight_read_reqs.end()) {
    for(auto req : it_req->second) {
      if((req->type == MRT_IFETCH || req->type == MRT_IPRF) &&
         (type == MRT_IFETCH || type == MRT_IPRF))
        return req;
      else if((req->type == MRT_DFETCH || req->type == MRT_DPRF ||
               req->type == MRT_DSTORE) &&
              (type == MRT_DFETCH || type == MRT_DPRF || type == MRT_DSTORE))
        return req;
    }
  }

  // Search response queue
  for(auto resp : resp_queue) {
    if(resp.first == phys_addr) {
      if((resp.second->type == MRT_IFETCH || resp.second->type == MRT_IPRF) &&
         (type == MRT_IFETCH || type == MRT_IPRF))
        return resp.second;
      else if((resp.second->type == MRT_DFETCH ||
               resp.second->type == MRT_DPRF ||
               resp.second->type == MRT_DSTORE) &&
              (type == MRT_DFETCH || type == MRT_DPRF || type == MRT_DSTORE))
        return resp.second;
    }
  }

  return NULL;
}
