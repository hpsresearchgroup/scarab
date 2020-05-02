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
 * File         : memview.c
 * Author       : HPS Research Group
 * Date         : 8/7/2013
 * Description  : Tracing memory-related events for visualization.
 ***************************************************************************************/

#include "debug/memview.h"
#include "core.param.h"
#include "exec_ports.h"
#include "freq.h"
#include "general.param.h"
#include "globals/assert.h"
#include "globals/global_types.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "ramulator.param.h"
#include "trigger.h"

/**************************************************************************************/
/* Enums */

DEFINE_ENUM(Memview_Dram_Event, MEMVIEW_DRAM_EVENT_LIST);
DEFINE_ENUM(Memview_Memqueue_Event, MEMVIEW_MEMQUEUE_EVENT_LIST);
DEFINE_ENUM(Memview_Note_Type, MEMVIEW_NOTE_TYPE_LIST);

/**************************************************************************************/
/* Types */

typedef struct Bank_Info_struct {
  uns pos;
} Bank_Info;

typedef struct Proc_Info_struct {
  Counter last_stalled_event_time;
  Counter last_mem_blocked_event_time;
  Counter last_fus_change_time;
  Counter last_memqueue_change_time;
  Flag    stalled;
  Flag    mem_blocked;
  uns     fus_busy;
  uns     num_reqs_by_type[MRT_NUM_ELEMS];
} Proc_Info;

/**************************************************************************************/
/* Global Variables */

FILE*         trace;
Bank_Info*    bank_infos;
Proc_Info*    proc_infos;
Trigger*      start_trigger;
Mem_Req_Type* req_types;

/**************************************************************************************/
/* Local Prototypes */

static void trace_memqueue_state(uns proc_id, Counter begin, Counter end,
                                 uns* num_reqs_by_type);
static void trace_core_state(uns proc_id, Counter begin, Counter end,
                             const char* state);
static void trace_fus_busy(uns proc_id, Counter begin, Counter end,
                           uns fus_busy);

/**************************************************************************************/
/* memview_init */

void memview_init(void) {
  if(!MEMVIEW) {
    trace = NULL;
    return;
  }

  const char* memview_filename = MEMVIEW_FILE;
  trace                        = fopen(memview_filename, "w");
  ASSERTM(0, trace, "Could not open %s\n", memview_filename);

  bank_infos = calloc(RAMULATOR_CHANNELS * RAMULATOR_BANKS, sizeof(Bank_Info));
  proc_infos = calloc(NUM_CORES, sizeof(Proc_Info));
  req_types  = malloc(
    (MEM_REQ_BUFFER_ENTRIES * (PRIVATE_MSHR_ON ? NUM_CORES : 1)) *
    sizeof(Mem_Req_Type));

  // Timing simulation start time might not be zero due to warmup
  // (which has to update time to maintain cache LRU information_
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    Proc_Info* info                   = &proc_infos[proc_id];
    info->last_stalled_event_time     = freq_time();
    info->last_mem_blocked_event_time = freq_time();
    info->last_fus_change_time        = freq_time();
    info->last_memqueue_change_time   = freq_time();
  }

  start_trigger = trigger_create("MEMVIEW START TRIGGER", MEMVIEW_START,
                                 TRIGGER_ONCE);

#define MEMVIEW_PARAM_PRINT(param) fprintf(trace, "%-20s %3d\n", #param, param)
  MEMVIEW_PARAM_PRINT(NUM_CORES);
  MEMVIEW_PARAM_PRINT(NUM_FUS);
  // MEMVIEW_PARAM_PRINT(MEMORY_CYCLE_TIME);
  MEMVIEW_PARAM_PRINT(RAMULATOR_TCK);
  MEMVIEW_PARAM_PRINT(RAMULATOR_CHANNELS);
  MEMVIEW_PARAM_PRINT(RAMULATOR_BANKS);
  MEMVIEW_PARAM_PRINT(MEM_REQ_BUFFER_ENTRIES);
  MEMVIEW_PARAM_PRINT(MEMVIEW_NOTE_NUM_ELEMS);
#undef MEMVIEW_PARAM_PRINT

  fprintf(trace, "\n");  // empty line indicates end of param section
}

/**************************************************************************************/
/* memview_dram */

void memview_dram(Memview_Dram_Event event, Mem_Req* req, uns flat_bank_id,
                  Counter start, Counter end) {
  if(!MEMVIEW)
    return;
  if(!trigger_on(start_trigger))
    return;

  Bank_Info* bank_info = &bank_infos[flat_bank_id];
  fprintf(trace, "%8s %10s %20lld %20lld %2d %10lld %3d %2d %2d\n", "DRAM",
          Memview_Dram_Event_str(event), start, end, req ? req->proc_id : -1,
          req ? req->unique_num : -1, req ? req->id : -1, flat_bank_id,
          bank_info->pos);
  if(event == MEMVIEW_DRAM_COLUMN) {
    bank_info->pos = (bank_info->pos + 1) % 3;
  }
}

void memview_dram_crit_path(const char* from_type_str, uns from_index,
                            const char* to_type_str, uns to_index,
                            Counter start, Counter end) {
  if(!MEMVIEW)
    return;
  if(!trigger_on(start_trigger))
    return;

  fprintf(trace, "%8s %10s %20lld %20lld  %s[%d]->%s[%d]\n", "DRAM",
          "CRIT_PATH", start, end, from_type_str, from_index, to_type_str,
          to_index);
}

void trace_memqueue_state(uns proc_id, Counter begin, Counter end,
                          uns* num_reqs_by_type) {
  fprintf(trace, "%8s %10s %20lld %20lld %2d", "MEMQUEUE", "DURATION", begin,
          end, proc_id);
  for(Mem_Req_Type type = 0; type < MRT_NUM_ELEMS; type++) {
    fprintf(trace, " %2d", num_reqs_by_type[type]);
  }
  fprintf(trace, "\n");
}

/**************************************************************************************/
/* memview_memqueue_depart */

void memview_memqueue(Memview_Memqueue_Event event, Mem_Req* req) {
  if(!MEMVIEW)
    return;

  ASSERT(0, req);
  Proc_Info* proc_info = &proc_infos[req->proc_id];
  if(trigger_on(start_trigger)) {
    trace_memqueue_state(req->proc_id, proc_info->last_memqueue_change_time,
                         freq_time(), proc_info->num_reqs_by_type);
  }
  proc_info->last_memqueue_change_time = freq_time();
  if(event == MEMVIEW_MEMQUEUE_ARRIVE) {
    ASSERT(req->proc_id,
           proc_info->num_reqs_by_type[req->type] < MEM_REQ_BUFFER_ENTRIES);
    proc_info->num_reqs_by_type[req->type]++;
    req_types[req->id] = req->type;
  } else {
    ASSERT(req->proc_id, proc_info->num_reqs_by_type[req->type] > 0);
    ASSERT(req->proc_id, req_types[req->id] == req->type);
    proc_info->num_reqs_by_type[req->type]--;
  }
}

/**************************************************************************************/
/* memview_req_changed_type */

void memview_req_changed_type(struct Mem_Req_struct* req) {
  if(!MEMVIEW)
    return;
  // if (req->queue != &mem->mem_queue) return;
  ASSERTM(0, FALSE,
          "We don't have mem_queue anymore. How can we make sure that req is a "
          "memory request?\n");
  ASSERT(0, req);
  Proc_Info*   proc_info = &proc_infos[req->proc_id];
  Mem_Req_Type old_type  = req_types[req->id];
  if(req->type == old_type)
    return;  // same type, no change
  ASSERT(req->proc_id, proc_info->num_reqs_by_type[old_type] > 0);
  proc_info->num_reqs_by_type[old_type]--;
  req_types[req->id] = req->type;
  ASSERT(req->proc_id,
         proc_info->num_reqs_by_type[req->type] < MEM_REQ_BUFFER_ENTRIES);
  proc_info->num_reqs_by_type[req->type]++;
  if(trigger_on(start_trigger)) {
    trace_memqueue_state(req->proc_id, proc_info->last_memqueue_change_time,
                         freq_time(), proc_info->num_reqs_by_type);
  }
}

/**************************************************************************************/
/* memview_l1 */

void memview_l1(Mem_Req* req) {
  if(!MEMVIEW)
    return;
  if(!trigger_on(start_trigger))
    return;

  ASSERT(0, req);
  fprintf(trace, "%8s %10s %20lld %20lld %2d\n", "LLC", "ACCESS", freq_time(),
          (freq_time() + freq_get_cycle_time(FREQ_DOMAIN_L1) * L1_CYCLES),
          req->proc_id);
}

/**************************************************************************************/
/* memview_core_stall */

void memview_core_stall(uns proc_id, Flag stalled, Flag mem_blocked) {
  if(!MEMVIEW)
    return;

  Proc_Info* proc_info = &proc_infos[proc_id];

  if(proc_info->stalled != stalled) {
    if(trigger_on(start_trigger)) {
      trace_core_state(proc_id, proc_info->last_stalled_event_time, freq_time(),
                       proc_info->stalled ? "STALL" : "COMPUTE");
    }
    proc_info->last_stalled_event_time = freq_time();
    proc_info->stalled                 = stalled;
  }
  if(proc_info->mem_blocked != mem_blocked) {
    if(trigger_on(start_trigger)) {
      trace_core_state(proc_id, proc_info->last_mem_blocked_event_time,
                       freq_time(),
                       proc_info->mem_blocked ? "MEM_BLOCK" : "MEM_AVAIL");
    }
    proc_info->last_mem_blocked_event_time = freq_time();
    proc_info->mem_blocked                 = mem_blocked;
  }
}

/**************************************************************************************/
/* trace_core_state */

void trace_core_state(uns proc_id, Counter begin, Counter end,
                      const char* state) {
  fprintf(trace, "%8s %10s %20lld %20lld %2d\n", "CORE", state, begin, end,
          proc_id);
}

/**************************************************************************************/
/* memview_fus_busy */

void memview_fus_busy(uns proc_id, uns fus_busy) {
  if(!MEMVIEW)
    return;

  Proc_Info* proc_info = &proc_infos[proc_id];

  if(proc_info->fus_busy != fus_busy) {
    if(trigger_on(start_trigger)) {
      trace_fus_busy(proc_id, proc_info->last_fus_change_time, freq_time(),
                     proc_info->fus_busy);
    }
    proc_info->last_fus_change_time = freq_time();
    proc_info->fus_busy             = fus_busy;
  }
}

/**************************************************************************************/
/* trace_fus_busy */

void trace_fus_busy(uns proc_id, Counter begin, Counter end, uns fus_busy) {
  fprintf(trace, "%8s %10s %20lld %20lld %2d %2d\n", "CORE", "FUS_BUSY", begin,
          end, proc_id, fus_busy);
}

/**************************************************************************************/
/* memview_done */

void memview_done(void) {
  if(!MEMVIEW)
    return;
  if(trigger_on(start_trigger)) {
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      Proc_Info* proc_info = &proc_infos[proc_id];
      trace_core_state(proc_id, proc_info->last_stalled_event_time, freq_time(),
                       proc_info->stalled ? "STALL" : "COMPUTE");
      trace_core_state(proc_id, proc_info->last_mem_blocked_event_time,
                       freq_time(),
                       proc_info->mem_blocked ? "MEM_BLOCK" : "MEM_AVAIL");
      trace_fus_busy(proc_id, proc_info->last_fus_change_time, freq_time(),
                     proc_info->fus_busy);
      trace_memqueue_state(proc_id, proc_info->last_memqueue_change_time,
                           freq_time(), proc_info->num_reqs_by_type);
    }
  }
  fclose(trace);
}

/**************************************************************************************/
/* memview_note */

void memview_note(Memview_Note_Type type, const char* str) {
  if(!MEMVIEW)
    return;
  if(trigger_on(start_trigger)) {
    fprintf(trace, "%8s %10d %20lld %20lld %2d %s\n", "NOTE", type, freq_time(),
            0ULL, 0, str);
  }
}

/*************************************************************/
