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
 * File         : frontend/pin_trace_fe.c
 * Author       : HPS Research Group
 * Date         : 10/28/2006
 * Description  :
 ***************************************************************************************/
#include "debug/debug.param.h"
#include "debug/debug_macros.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"

#include "bp/bp.h"
#include "statistics.h"

#include "./pin/pin_lib/uop_generator.h"
#include "bp/bp.param.h"
#include "ctype_pin_inst.h"
#include "frontend/pin_trace_fe.h"
#include "frontend/pin_trace_read.h"
#include "isa/isa.h"

/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_TRACE_READ, ##args)


/**************************************************************************************/
/* Global Variables */

static char* trace_files[MAX_NUM_PROCS];

ctype_pin_inst* next_pi;

/**************************************************************************************/
/* trace_init() */

void trace_init() {
  ASSERTM(0, !FETCH_OFF_PATH_OPS,
          "Trace frontend does not support wrong path. Turn off "
          "FETCH_OFF_PATH_OPS\n");

  uop_generator_init(NUM_CORES);

  next_pi = (ctype_pin_inst*)malloc(NUM_CORES * sizeof(ctype_pin_inst));

  pin_trace_file_pointer_init(NUM_CORES);

  /* temp variable needed for easy initialization syntax */
  char* tmp_trace_files[MAX_NUM_PROCS] = {
    CBP_TRACE_R0,  CBP_TRACE_R1,  CBP_TRACE_R2,  CBP_TRACE_R3,  CBP_TRACE_R4,
    CBP_TRACE_R5,  CBP_TRACE_R6,  CBP_TRACE_R7,  CBP_TRACE_R8,  CBP_TRACE_R9,
    CBP_TRACE_R10, CBP_TRACE_R11, CBP_TRACE_R12, CBP_TRACE_R13, CBP_TRACE_R14,
    CBP_TRACE_R15, CBP_TRACE_R16, CBP_TRACE_R17, CBP_TRACE_R18, CBP_TRACE_R19,
    CBP_TRACE_R20, CBP_TRACE_R21, CBP_TRACE_R22, CBP_TRACE_R23, CBP_TRACE_R24,
    CBP_TRACE_R25, CBP_TRACE_R26, CBP_TRACE_R27, CBP_TRACE_R28, CBP_TRACE_R29,
    CBP_TRACE_R30, CBP_TRACE_R31, CBP_TRACE_R32, CBP_TRACE_R33, CBP_TRACE_R34,
    CBP_TRACE_R35, CBP_TRACE_R36, CBP_TRACE_R37, CBP_TRACE_R38, CBP_TRACE_R39,
    CBP_TRACE_R40, CBP_TRACE_R41, CBP_TRACE_R42, CBP_TRACE_R43, CBP_TRACE_R44,
    CBP_TRACE_R45, CBP_TRACE_R46, CBP_TRACE_R47, CBP_TRACE_R48, CBP_TRACE_R49,
    CBP_TRACE_R50, CBP_TRACE_R51, CBP_TRACE_R52, CBP_TRACE_R53, CBP_TRACE_R54,
    CBP_TRACE_R55, CBP_TRACE_R56, CBP_TRACE_R57, CBP_TRACE_R58, CBP_TRACE_R59,
    CBP_TRACE_R60, CBP_TRACE_R61, CBP_TRACE_R62, CBP_TRACE_R63,
  };
  if(DUMB_CORE_ON) {
    // avoid errors by specifying a trace known to be good
    tmp_trace_files[DUMB_CORE] = tmp_trace_files[0];
  }
  for(uns proc_id = 0; proc_id < MAX_NUM_PROCS; proc_id++) {
    trace_files[proc_id] = tmp_trace_files[proc_id];
  }
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    trace_setup(proc_id);
  }
}

void trace_setup(uns proc_id) {
  pin_trace_open(proc_id, trace_files[proc_id]);
  pin_trace_read(proc_id, &next_pi[proc_id]);
}

/**************************************************************************************/
/* trace_next_fetch_addr */

Addr trace_next_fetch_addr(uns proc_id) {
  return convert_to_cmp_addr(proc_id, next_pi[proc_id].instruction_addr);
}

/**************************************************************************************/
/* trace done */

void trace_done() {
  uns proc_id;
  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    pin_trace_close(proc_id);
  }
}

void trace_close_trace_file(uns proc_id) {
  pin_trace_close(proc_id);
}

Flag trace_can_fetch_op(uns proc_id) {
  return !(uop_generator_get_eom(proc_id) && trace_read_done[proc_id]);
}

void trace_fetch_op(uns proc_id, Op* op) {
  if(uop_generator_get_bom(proc_id)) {
    ASSERT(proc_id, !trace_read_done[proc_id] && !reached_exit[proc_id]);
    uop_generator_get_uop(proc_id, op, &next_pi[proc_id]);
  } else {
    uop_generator_get_uop(proc_id, op, NULL);
  }

  if(uop_generator_get_eom(proc_id)) {
    int success = pin_trace_read(proc_id, &next_pi[proc_id]);
    if(!success) {
      trace_read_done[proc_id] = TRUE;
      reached_exit[proc_id]    = TRUE;
      /* this flag is supposed to be set in uop_generator_get_uop() but there
       * is a circular dependency on trace_read_done to be set. So, we set
       * op->exit here. */
      op->exit = TRUE;
    }
  }
}

void trace_redirect(uns proc_id, uns64 inst_uid, Addr fetch_addr) {
  FATAL_ERROR(proc_id, "Trace frontend does not support wrong path. Turn off "
                       "FETCH_OFF_PATH_OPS\n");
}

void trace_recover(uns proc_id, uns64 inst_uid) {
  FATAL_ERROR(proc_id, "Trace frontend does not support wrong path. Turn off "
                       "FETCH_OFF_PATH_OPS\n");
}

void trace_retire(uns proc_id, uns64 inst_uid) {
  // Trace frontend does not need to communicate to PIN which instruction are
  // retired.
}
