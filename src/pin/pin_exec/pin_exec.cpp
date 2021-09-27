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

#include <algorithm>
#include <cinttypes>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <syscall.h>
#include <unordered_map>

#undef UNUSED
#undef WARNING

#include "pin.H"

#undef UNUSED
#undef WARNING

#include "analysis_functions.h"
#include "exception_handling.h"
#include "globals.h"
#include "read_mem_map.h"
#include "scarab_interface.h"
#include "utils.h"

// scarab files
#include "../pin_lib/decoder.h"
#include "../pin_lib/message_queue_interface_lib.h"
#include "../pin_lib/pin_scarab_common_lib.h"

/* ===================================================================== */

/* ===================================================================== */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "",
                            "specify file name for pintool output");

KNOB<string> KnobSocketPath(KNOB_MODE_WRITEONCE, "pintool", "socket_path",
                            "./pin_exec_driven_fe_socket.temp",
                            "specify socket path to communicate with Scarab");

KNOB<UINT32> KnobCoreId(KNOB_MODE_WRITEONCE, "pintool", "core_id", "0",
                        "The ID of the Scarab core to connect to");

KNOB<UINT32> KnobMaxBufferSize(
  KNOB_MODE_WRITEONCE, "pintool", "max_buffer_size", "8",
  "pintool buffers up to (max_buffer_size-2) instructions for sending");

KNOB<UINT64> KnobHyperFastForwardCount(
  KNOB_MODE_WRITEONCE, "pintool", "hyper_fast_forward_count", "0",
  "pin quickly skips close to hyper_ffc instructions");
KNOB<UINT64> KnobFastForwardCount(
  KNOB_MODE_WRITEONCE, "pintool", "fast_forward_count", "0",
  "After skipping hyper_ffc, pin skips exactly (ffc-1) instructions");
// TODO_b: why is this UINT64 instead of bool?
KNOB<UINT64> KnobFastForwardToStartInst(
  KNOB_MODE_WRITEONCE, "pintool", "fast_forward_to_start_inst", "0",
  "Pin skips instructions until start instruction is found");
KNOB<bool>   KnobHeartbeatEnabled(KNOB_MODE_WRITEONCE, "pintool", "heartbeat",
                                "false",
                                "Periodically output heartbeat messages");
KNOB<UINT64> KnobDebugPrintStartUid(
  KNOB_MODE_WRITEONCE, "pintool", "debug_print_start_uid", "0",
  "Start printing debug prints at this UID (inclusive)");
KNOB<UINT64> KnobDebugPrintEndUid(KNOB_MODE_WRITEONCE, "pintool",
                                  "debug_print_end_uid", "18446744073709551615",
                                  "Stop printing debug prints after this UID");
KNOB<UINT64> KnobStartRip(KNOB_MODE_WRITEONCE, "pintool", "rip", "0",
                          "the starting rip of the program");

/* ===================================================================== */
/* ===================================================================== */

namespace {

INT32 Usage() {
  cerr << "Pintool based exec frontend for scarab simulator" << endl << endl;
  cerr << KNOB_BASE::StringKnobSummary() << endl;
  return -1;
}

void insert_logging(const INS& ins) {
  if(INS_Category(ins) == XED_CATEGORY_COND_BR) {
    INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(logging), IARG_ADDRINT,
                   INS_NextAddress(ins), IARG_ADDRINT, INS_Address(ins),
                   IARG_BOOL, INS_HasFallThrough(ins), IARG_BRANCH_TAKEN,
                   IARG_END);
  } else {
    INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(logging), IARG_ADDRINT,
                   INS_NextAddress(ins), IARG_ADDRINT, INS_Address(ins),
                   IARG_BOOL, INS_HasFallThrough(ins), IARG_BOOL, false,
                   IARG_END);
  }
}

void insert_check_for_magic_instructions(const INS& ins) {
  if(INS_IsXchg(ins) && INS_OperandReg(ins, 0) == REG_GCX &&
     INS_OperandReg(ins, 1) == REG_GCX) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)handle_scarab_marker,
                   IARG_REG_VALUE, REG_RCX, IARG_END);
  }
}

void insert_processing_for_syscalls(const INS& ins) {
  INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(process_syscall), IARG_INST_PTR,
                 IARG_SYSCALL_NUMBER, IARG_SYSARG_VALUE, 0, IARG_SYSARG_VALUE,
                 1, IARG_SYSARG_VALUE, 2, IARG_SYSARG_VALUE, 3,
                 IARG_SYSARG_VALUE, 4, IARG_SYSARG_VALUE, 5, IARG_CONTEXT,
                 IARG_BOOL, INS_IsSyscall(ins), IARG_END);
}

void insert_checks_for_control_flow(const INS& ins) {
  if(INS_IsRet(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(check_ret_control_ins),
                   IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_CONTEXT,
                   IARG_END);
  } else if(INS_IsBranchOrCall(ins)) {
    if(INS_IsDirectBranchOrCall(ins)) {
      if(INS_Category(ins) == XED_CATEGORY_COND_BR) {
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(check_nonret_control_ins),
                       IARG_BRANCH_TAKEN, IARG_ADDRINT,
                       INS_DirectBranchOrCallTargetAddress(ins), IARG_END);
      } else {
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(check_nonret_control_ins),
                       IARG_BOOL, true, IARG_ADDRINT,
                       INS_DirectBranchOrCallTargetAddress(ins), IARG_END);
      }
    } else if(INS_IsMemoryRead(ins)) {
      INS_InsertCall(ins, IPOINT_BEFORE,
                     AFUNPTR(check_nonret_control_mem_target), IARG_BOOL, true,
                     IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
    } else if(INS_MaxNumRRegs(ins) > 0) {
      INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(check_nonret_control_ins),
                     IARG_BOOL, true, IARG_REG_VALUE, INS_RegR(ins, 0),
                     IARG_END);
    } else {
      // Force WPNM
      INS_InsertCall(ins, IPOINT_BEFORE,
                     AFUNPTR(check_nonret_control_mem_target), IARG_BOOL, true,
                     IARG_ADDRINT, 0, IARG_UINT32, 0, IARG_END);
    }
  }
}

void insert_processing_for_nonsyscall_instructions(const INS& ins) {
  if(!INS_IsMemoryWrite(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)before_ins_no_mem, IARG_CONTEXT,
                   IARG_END);
  } else {
    if(INS_hasKnownMemorySize(ins)) {
      // Single memory op
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)before_ins_one_mem,
                     IARG_CONTEXT, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                     IARG_END);
    } else {
      // Multiple memory ops
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)before_ins_multi_mem,
                     IARG_CONTEXT, IARG_MULTI_MEMORYACCESS_EA, IARG_BOOL,
                     INS_IsVscatter(ins), IARG_END);
    }
  }
}

void instrumentation_func_per_trace(TRACE trace, void* v) {
#ifdef DEBUG_PRINT
  stringstream instructions_ss;

  for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
    for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
      instructions_ss << "0x" << hex << INS_Address(ins) << endl;
    }
  }

  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "Instrumenting Trace at address 0x%p. Instructions:\n%s\n",
            (void*)TRACE_Address(trace), instructions_ss.str().c_str());
#endif

  // used to be IPOINT_ANYWHERE
  if(hyper_ff) {
    for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
      BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)docount,
                     IARG_FAST_ANALYSIS_CALL, IARG_UINT32, BBL_NumIns(bbl),
                     IARG_END);
    }
  }
}


void instrumentation_func_per_instruction(INS ins, void* v) {
  if(!started) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)redirect, IARG_CONTEXT,
                   IARG_END);
  } else {
    if(!hyper_ff) {
      instrumented_rip_tracker.insert(INS_Address(ins));

      DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
                "Instrument from Instruction() eip=%" PRIx64 "\n",
                (uint64_t)INS_Address(ins));

      insert_logging(ins);
      insert_check_for_magic_instructions(ins);

      // Inserting functions to create a compressed op
      pin_decoder_insert_analysis_functions(ins);

      xed_decoded_inst_t* xed_ins = INS_XedDec(ins);
      if(INS_IsSyscall(ins) || is_ifetch_barrier(xed_ins)) {
        insert_processing_for_syscalls(ins);
      } else {
        insert_checks_for_control_flow(ins);
        insert_processing_for_nonsyscall_instructions(ins);
      }

#ifdef DEBUG_PRINT
      stringstream ss;
      if(INS_IsDirectBranchOrCall(ins)) {
        ss << "0x" << hex << INS_DirectBranchOrCallTargetAddress(ins);
      } else {
        ss << "(not a direct branch or call)";
      }

      DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
                "Leaving Instrument from Instruction() eip=%" PRIx64
                ", %s, direct target: %s\n",
                (uint64_t)INS_Address(ins), INS_Mnemonic(ins).c_str(),
                ss.str().c_str());
#endif
    }
  }
}

}  // namespace

void Fini(INT32 code, void* v) {
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "Fini reached, app exit code=%d\n.", code);
  *out << "End of program reached, disconnect from Scarab.\n" << endl;
  scarab->disconnect();
  *out << "Pintool Fini Reached.\n" << endl;
}

int main(int argc, char* argv[]) {
#if DEBUG_PRINT
  setbuf(stdout, NULL);
#endif

  // Read memmap for process
  page_table = new pageTableStruct();
  update_page_table(page_table);

  if(PIN_Init(argc, argv)) {
    return Usage();
  }
  // if no start EIP was specified, then we don't need to redirect,
  // and so we have "started"
  started   = (0 == KnobStartRip.Value());
  start_rip = KnobStartRip.Value();

  heartbeat_enabled = KnobHeartbeatEnabled.Value();
  max_buffer_size   = KnobMaxBufferSize.Value();

  fast_forward_count = KnobFastForwardCount.Value();
  fast_forward_to_pin_start =
    (fast_forward_count = KnobFastForwardToStartInst.Value());
  hyper_fast_forward_count = KnobHyperFastForwardCount.Value() -
                             hyper_fast_forward_delta;
  orig_hyper_fast_forward_count = KnobHyperFastForwardCount.Value();

  dbg_print_start_uid = KnobDebugPrintStartUid.Value();
  dbg_print_end_uid   = KnobDebugPrintEndUid.Value();

  register_signal_handlers();

  hyper_ff = false;
  if(hyper_fast_forward_count > 0) {
    hyper_ff = true;
    *out << "Entering Hyper Fast Forward Mode: " << hyper_fast_forward_count
         << " ins remaining" << endl;
  } else if(fast_forward_count > 0) {
    if(fast_forward_to_pin_start) {
      *out << "Entering Fast Forward Mode: looking for start instruction"
           << endl;
    } else {
      *out << "Entering Fast Forward Mode: " << fast_forward_count
           << " ins remaining" << endl;
    }
  }

  max_buffer_size = KnobMaxBufferSize.Value();
  string fileName = KnobOutputFile.Value();

  if(!fileName.empty()) {
    out = new std::ofstream(fileName.c_str());
  }

  pin_decoder_init(true, out);

  // Register function to be called to instrument traces
  TRACE_AddInstrumentFunction(instrumentation_func_per_trace, 0);
  INS_AddInstrumentFunction(instrumentation_func_per_instruction, 0);

  // Register function to be called when the application exits
  PIN_AddFiniFunction(Fini, 0);

  scarab = new Client(KnobSocketPath, KnobCoreId);

  // Start the program, never returns
  PIN_StartProgram();
  return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
