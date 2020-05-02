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
 * File         : perf_pred.h
 * Author       : HPS Research Group
 * Date         : 10/20/2011
 * Description  : Performance prediction counters for DVFS. Currently
 *implementing the "leading loads" technique and DRAM critical path estimation.
 ***************************************************************************************/

#ifndef __PERF_PRED_H__
#define __PERF_PRED_H__

#include "globals/enum.h"
#include "memory/mem_req.h"

#define PERF_PRED_MECH_LIST(ELEM) \
  ELEM(LEADING_LOADS)             \
  ELEM(STALL)                     \
  ELEM(CP)                        \
  ELEM(CP_PREF)

DECLARE_ENUM(Perf_Pred_Mech, PERF_PRED_MECH_LIST, PERF_PRED_);

#define PERF_PRED_REQ_LATENCY_MECH_LIST(ELEM) \
  ELEM(REQ_LATENCY)                           \
  ELEM(DRAM_LATENCY)                          \
  ELEM(VIRTUAL_CLOCK)

DECLARE_ENUM(Perf_Pred_Req_Latency_Mech, PERF_PRED_REQ_LATENCY_MECH_LIST,
             PERF_PRED_REQ_LATENCY_MECH_);

/* Initialize before using other functions */
void init_perf_pred(void);

/* Reset state */
void reset_perf_pred(void);

/* Finalize */
void perf_pred_done(void);

/* These three functions must be called in temporal order: */

/* Report that a memory request has started */
void perf_pred_mem_req_start(struct Mem_Req_struct*);

/* Report start and end of a memory request's DRAM latency */
void perf_pred_dram_latency_start(struct Mem_Req_struct*);
void perf_pred_dram_latency_end(struct Mem_Req_struct*);

/* Report that a memory request is done */
void perf_pred_mem_req_done(struct Mem_Req_struct*);

/* Report that an outstanding memory request changed type */
void perf_pred_update_mem_req_type(struct Mem_Req_struct*,
                                   Mem_Req_Type old_type, Flag old_offpath);

/* The functions above must be called in temporal order */

/* Run every cycle */
void perf_pred_cycle(void);

/* Report whether the chip is running this cycle (call every cycle) */
void perf_pred_core_busy(uns proc_id, uns num_fus_busy);

/* Report slack encountered by a request in a DRAM bank */
void perf_pred_slack(Mem_Req* req, Counter constraint, Counter latency,
                     Flag final);

/* Report that a request started that, from this cycle, is affected by
   the off-chip latency */
void perf_pred_off_chip_effect_start(struct Mem_Req_struct*);

/* Report that a request affected by off-chip latency has ended */
void perf_pred_off_chip_effect_end(struct Mem_Req_struct*);

/* Report a lost row buffer hit */
void perf_pred_lost_row_buffer_hit(uns mem_bank);

/* HACK: reset stats */
void perf_pred_reset_stats(void);

/* Call when an interval is done (before perf_pred_slowdown) */
void perf_pred_interval_done(void);

/* Return predicted slowdown for the provided chip and memory cycle
   times */
double perf_pred_slowdown(uns proc_id, Perf_Pred_Mech mech, uns chip_cycle_time,
                          uns memory_cycle_time);

/* LLC Level Parallelism calculation */

/* Report than an Icache/Dcache miss started */
void perf_pred_l0_miss_start(struct Mem_Req_struct*);

/* Report than an Icache/Dcache miss ended */
void perf_pred_l0_miss_end(struct Mem_Req_struct*);

#endif  // __PERF_PRED_H__
