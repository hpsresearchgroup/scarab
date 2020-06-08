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
 * File         : dumb_model.c
 * Author       : HPS Research Group
 * Date         : 7/23/2013
 * Description  : Model that drives the memory system with randomly
 *                generated memory requests (no core modeling)
 ***************************************************************************************/

#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/assert.h"
#include "globals/global_vars.h"
#include "statistics.h"

#include "dumb_model.h"
#include "freq.h"
#include "model.h"
#include "sim.h"

#include <unistd.h>

#include "debug/debug.param.h"
#include "general.param.h"
#include "memory/memory.param.h"


/**************************************************************************************/
/* Types */

typedef struct Proc_Info_struct {
  uns avg_req_distance;  // average number of cycles between reqs
  uns avg_row_hits;  // average number of row hits for every row open (incl. 1st
                     // conflict)
  uns  mlp;          // maximum number of outstanding reqs
  Addr last_addr;    // addr of last req (for retrying and generating row hits)
  uns  reqs_out;     // number of outstanding reqs
  Flag retry;        // couldn't send last mem req, keep retrying
  Flag dumb;         // is this core actually dumb
} Proc_Info;

/**************************************************************************************/
/* Local prototypes */

static Flag dumb_req_done(Mem_Req* req);

/**************************************************************************************/
/* Global variables */

static Proc_Info* infos;
static Counter    req_num;
static uns64      page_num_mask;

/**************************************************************************************/
/* dumb_init */

void dumb_init(uns mode) {
  if(mode != WARMUP_MODE)
    return;
  req_num = 0;
  ASSERT(0, is_power_of_2(MEMORY_INTERLEAVE_FACTOR));
  page_num_mask = ~(uns64)(MEMORY_INTERLEAVE_FACTOR - 1);
  infos         = calloc(NUM_CORES, sizeof(Proc_Info));
  if(DUMB_MODEL_RANDOMIZE_DISTANCE) {
    int seed = getpid();
    MESSAGEU(0, "Seed: %d\n", seed);
    srand(seed);
  }
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Proc_Info* info        = &infos[proc_id];
    info->dumb             = SIM_MODEL == DUMB_MODEL || proc_id != DUMB_CORE;
    info->avg_req_distance = DUMB_MODEL_AVG_REQ_DISTANCE;
    if(DUMB_MODEL_RANDOMIZE_DISTANCE) {
      info->avg_req_distance = (rand() % 200) + 40;
      MESSAGEU(proc_id, "Distance: %d\n", info->avg_req_distance);
    } else if(DUMB_MODEL_AVG_REQ_DISTANCE_PER_CORE) {
      int* distances = malloc(NUM_CORES * sizeof(uns));
      uns  num_elems = parse_int_array(
        distances, DUMB_MODEL_AVG_REQ_DISTANCE_PER_CORE, NUM_CORES);
      ASSERT(0, num_elems == NUM_CORES);
      for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
        infos[proc_id].avg_req_distance = distances[proc_id];
      }
    }
    info->avg_row_hits = DUMB_MODEL_AVG_ROW_HITS;
    if(DUMB_MODEL_AVG_ROW_HITS_PER_CORE) {
      int* elems     = malloc(NUM_CORES * sizeof(uns));
      uns  num_elems = parse_int_array(elems, DUMB_MODEL_AVG_ROW_HITS_PER_CORE,
                                      NUM_CORES);
      ASSERT(0, num_elems == NUM_CORES);
      for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
        infos[proc_id].avg_row_hits = elems[proc_id];
      }
    }
    info->mlp = DUMB_MODEL_MLP;
    if(DUMB_MODEL_MLP_PER_CORE) {
      int* elems     = malloc(NUM_CORES * sizeof(uns));
      uns  num_elems = parse_int_array(elems, DUMB_MODEL_AVG_ROW_HITS_PER_CORE,
                                      NUM_CORES);
      ASSERT(0, num_elems == NUM_CORES);
      for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
        infos[proc_id].mlp = elems[proc_id];
      }
    }
    info->last_addr = convert_to_cmp_addr(proc_id, 0);
  }
  if(SIM_MODEL == DUMB_MODEL) {
    // Only dumb model is running, initialize required subset of
    // microarchitecture model
    freq_init();
    set_memory(&dumb_model.memory);
    init_memory();
  } else {
    ASSERT(0, DUMB_CORE_ON);
    ASSERTM(0, DUMB_CORE != 0, "Core 0 cannot be dumb\n");
    // Core 0 is hard-coded in Scarab in some places that may break if DUMB_CORE
    // == 0
  }
}


/**************************************************************************************/
/* dumb_reset: */

void dumb_reset() {
  reset_memory();
}

/**************************************************************************************/
/* dumb_cycle: */

Flag dumb_req_done(Mem_Req* req) {
  uns        proc_id = req->proc_id;
  Proc_Info* info    = &infos[proc_id];
  ASSERT(proc_id, info->reqs_out >= req->req_count);
  info->reqs_out -= req->req_count;
  INC_STAT_EVENT(proc_id, NODE_INST_COUNT, req->req_count);
  inst_count[proc_id] += req->req_count;
  if(SIM_MODEL == DUMB_MODEL && !sim_done[proc_id] && INST_LIMIT &&
     inst_count[proc_id] >= inst_limit[proc_id]) {
    retired_exit[proc_id] = TRUE;
  }
  return TRUE;
}

/**************************************************************************************/
/* dumb_cycle: */

void dumb_cycle() {
  if(!freq_is_ready(FREQ_DOMAIN_L1))
    return;
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    if(SIM_MODEL != DUMB_MODEL && proc_id != DUMB_CORE)
      continue;
    STAT_EVENT(proc_id, NODE_CYCLE);
    Proc_Info* info     = &infos[proc_id];
    Flag       send_req = info->retry || (info->reqs_out < info->mlp &&
                                    (rand() % info->avg_req_distance) == 0);
    if(info->retry || info->reqs_out == info->mlp) {
      STAT_EVENT(proc_id, FULL_WINDOW_STALL);
    }
    if(send_req) {
      ASSERT(proc_id, info->reqs_out < info->mlp);
      Addr addr;
      if(info->retry) {
        addr = info->last_addr;
      } else {
        addr = convert_to_cmp_addr(proc_id, rand() * L1_LINE_SIZE);
        if(rand() % info->avg_row_hits != 0) {
          // make row hit
          uns64 page_num    = info->last_addr & page_num_mask;
          uns64 page_offset = addr & ~page_num_mask;
          addr              = page_num | page_offset;
        }
      }
      ASSERT(proc_id, get_proc_id_from_cmp_addr(addr) == proc_id);
      info->last_addr    = addr;
      Counter unique_num = SIM_MODEL == DUMB_MODEL ? req_num : unique_count;
      Flag sent = new_mem_req(MRT_DFETCH, proc_id, addr, L1_LINE_SIZE, 0, NULL,
                              dumb_req_done, unique_num, NULL);
      info->retry = !sent;
      if(sent) {
        req_num++;
        info->reqs_out++;
        if(SIM_MODEL != DUMB_MODEL)
          unique_count++;
      }
    }
  }
  if(SIM_MODEL == DUMB_MODEL)
    update_memory();
}

/**************************************************************************************/
/* dumb_debug: */

void dumb_debug() {
  debug_memory();
}

/**************************************************************************************/
/* dumb_done: */

void dumb_done() {
  if(SIM_MODEL != DUMB_MODEL)
    return;

  finalize_memory();
}
