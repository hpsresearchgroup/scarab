/* Copyright 2020 University of California Santa Cruz
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
 * File         : frontend/memtrace_fe.cc
 * Author       : Heiner Litz
 * Date         : 05/15/2020
 * Description  : Frontend to simulate traces in memtrace format
 ***************************************************************************************/

#include "debug/debug.param.h"
#include "debug/debug_macros.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"

#include "bp/bp.h"
#include "bp/bp.param.h"
#include "ctype_pin_inst.h"
#include "frontend/memtrace/memtrace_fe.h"
#include "isa/isa.h"
#include "pin/pin_lib/uop_generator.h"
#include "pin/pin_lib/x86_decoder.h"
#include "statistics.h"

#define DR_DO_NOT_DEFINE_int64

#include "frontend/memtrace/memtrace_trace_reader_memtrace.h"

/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_TRACE_READ, ##args)

//#define PRINT_INSTRUCTION_INFO
/**************************************************************************************/
/* Global Variables */

char*           trace_files[MAX_NUM_PROCS];
TraceReader*    trace_readers[MAX_NUM_PROCS];
ctype_pin_inst* next_pi;
uint64_t        ins_id    = 0;
uint64_t        prior_tid = 0;
uint64_t        prior_pid = 0;

/**************************************************************************************/
/* Private Functions */

void fill_in_dynamic_info(ctype_pin_inst* info, const InstInfo* insi) {
  uint8_t ld = 0;
  uint8_t st = 0;

  // Note: should be overwritten for a taken control flow instruction
  info->instruction_addr      = insi->pc;
  info->instruction_next_addr = insi->target;
  info->actually_taken        = insi->taken;
  info->branch_target         = insi->target;
  info->inst_uid              = ins_id;

#ifdef PRINT_INSTRUCTION_INFO
  std::cout << std::hex << info->instruction_addr << " Next "
            << info->instruction_next_addr << " size " << (uint32_t)info->size
            << " taken " << (uint32_t)info->actually_taken << " target "
            << info->branch_target << " pid " << insi->pid << " tid "
            << insi->tid << " asm "
            << std::string(
                 xed_iclass_enum_t2str(xed_decoded_inst_get_iclass(insi->ins)))
            << " uid " << std::dec << info->inst_uid << std::endl;
#endif

  if(xed_decoded_inst_get_iclass(insi->ins) == XED_ICLASS_RET_FAR ||
     xed_decoded_inst_get_iclass(insi->ins) == XED_ICLASS_RET_NEAR)
    info->actually_taken = 1;

  for(uint8_t op = 0;
      op < xed_decoded_inst_number_of_memory_operands(insi->ins); op++) {
    // predicated true ld/st are handled just as regular ld/st
    if(xed_decoded_inst_mem_read(insi->ins, op) && !insi->mem_used[op]) {
      // Handle predicated stores specially?
      info->ld_vaddr[ld++] = insi->mem_addr[op];
    } else if(xed_decoded_inst_mem_read(insi->ins, op)) {
      info->ld_vaddr[ld++] = insi->mem_addr[op];
    }
    if(xed_decoded_inst_mem_written(insi->ins, op) && !insi->mem_used[op]) {
      // Handle predicated stores specially?
      info->st_vaddr[st++] = insi->mem_addr[op];
    } else if(xed_decoded_inst_mem_written(insi->ins, op)) {
      info->st_vaddr[st++] = insi->mem_addr[op];
    }
  }
}

int ffwd(const xed_decoded_inst_t* ins) {
  if(!FAST_FORWARD) {
    return 0;
  }
  if(XED_INS_Opcode(ins) == XED_ICLASS_XCHG &&
     XED_INS_OperandReg(ins, 0) == XED_REG_RCX &&
     XED_INS_OperandReg(ins, 1) == XED_REG_RCX) {
    return 0;
  }
  if(ins_id == FAST_FORWARD_TRACE_INS) {
    return 0;
  }
  return 1;
}

int roi(const xed_decoded_inst_t* ins) {
  if(XED_INS_Opcode(ins) == XED_ICLASS_XCHG &&
     XED_INS_OperandReg(ins, 0) == XED_REG_RCX &&
     XED_INS_OperandReg(ins, 1) == XED_REG_RCX) {
    return 1;
  }
  return 0;
}

int memtrace_trace_read(int proc_id, ctype_pin_inst* next_pi) {
  InstInfo* insi;

  do {
    insi = const_cast<InstInfo*>(trace_readers[proc_id]->nextInstruction());
    ins_id++;
    if(!insi->valid) {
      insi = const_cast<InstInfo*>(trace_readers[proc_id]->nextInstruction());
      ins_id++;
      return 0;  // end of trace
    }
  } while(insi->pid != prior_pid || insi->tid != prior_tid);

  memset(next_pi, 0, sizeof(ctype_pin_inst));
  fill_in_dynamic_info(next_pi, insi);
  fill_in_basic_info(next_pi, insi->ins);
  uint32_t max_op_width = add_dependency_info(next_pi, insi->ins);
  fill_in_simd_info(next_pi, insi->ins, max_op_width);
  apply_x87_bug_workaround(next_pi, insi->ins);
  fill_in_cf_info(next_pi, insi->ins);
  print_err_if_invalid(next_pi, insi->ins);

  // End of ROI
  if(roi(insi->ins))
    return 0;

  return 1;
}


/**************************************************************************************/
/* trace_init() */

void memtrace_init(void) {
  /*ASSERTM(0, !FETCH_OFF_PATH_OPS,
          "Trace frontend does not support wrong path. Turn off "
          "FETCH_OFF_PATH_OPS\n");
  */

  uop_generator_init(NUM_CORES);
  init_x86_decoder(nullptr);
  init_x87_stack_delta();

  next_pi = (ctype_pin_inst*)malloc(NUM_CORES * sizeof(ctype_pin_inst));

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
    memtrace_setup(proc_id);
  }
}

void memtrace_setup(uns proc_id) {
  std::string path(trace_files[proc_id]);
  std::string trace(path);
  std::string binaries(MEMTRACE_MODULES_LOG);

  trace_readers[proc_id] = new TraceReaderMemtrace(trace, binaries, 1);

  // FFWD
  const InstInfo* insi = trace_readers[proc_id]->nextInstruction();

  if(FAST_FORWARD) {
    std::cout << "Enter fast forward " << ins_id << std::endl;
  }

  while(!insi->valid || ffwd(insi->ins)) {
    insi = trace_readers[proc_id]->nextInstruction();
    ins_id++;
    if((ins_id % 10000000) == 0)
      std::cout << "Fast forwarded " << ins_id << " instructions." << std::endl;
  }

  if(FAST_FORWARD) {
    std::cout << "Exit fast forward " << ins_id << std::endl;
  }

  prior_pid = insi->pid;
  prior_tid = insi->tid;
  assert(prior_tid);
  assert(prior_pid);
  memtrace_trace_read(proc_id, &next_pi[proc_id]);
}

/**************************************************************************************/
/* trace_next_fetch_addr */

Addr memtrace_next_fetch_addr(uns proc_id) {
  return next_pi[proc_id].instruction_addr;
}

/**************************************************************************************/
/* trace done */

void memtrace_done() {
  uns proc_id;
  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    // delete trace_readers[proc_id];
  }
  printf("done\n");
}

void memtrace_close_trace_file(uns proc_id) {
  // delete trace_readers[proc_id];
  printf("close\n");
}

Flag memtrace_can_fetch_op(uns proc_id) {
  assert(proc_id == 0);
  return !(uop_generator_get_eom(proc_id) && trace_read_done[proc_id]);
}

void memtrace_fetch_op(uns proc_id, Op* op) {
  if(uop_generator_get_bom(proc_id)) {
    // ASSERT(proc_id, !trace_read_done[proc_id] && !reached_exit[proc_id]);
    uop_generator_get_uop(proc_id, op, &next_pi[proc_id]);
  } else {
    uop_generator_get_uop(proc_id, op, NULL);
  }

  if(uop_generator_get_eom(proc_id)) {
    int        success = memtrace_trace_read(proc_id, &next_pi[proc_id]);
    static int ins     = 0;
    ins++;
    if(!success) {
      trace_read_done[proc_id] = TRUE;
      reached_exit[proc_id]    = TRUE;
      std::cout << "Reached end of trace" << std::endl;
    }
  }
}

void memtrace_redirect(uns proc_id, uns64 inst_uid, Addr fetch_addr) {
  assert(0);
  // FATAL_ERROR(proc_id, "Trace frontend does not support wrong path. Turn off
  // "
  //                     "FETCH_OFF_PATH_OPS\n");
}

void memtrace_recover(uns proc_id, uns64 inst_uid) {
  assert(0);
  // FATAL_ERROR(proc_id, "Trace frontend does not support wrong path. Turn off
  // "
  //                     "FETCH_OFF_PATH_OPS\n");
}

void memtrace_retire(uns proc_id, uns64 inst_uid) {
  // Trace frontend does not need to communicate to PIN which instruction are
  // retired.
}
