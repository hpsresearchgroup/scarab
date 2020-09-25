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

#include "analysis_functions.h"

#include "main_loop.h"

#include "../pin_lib/decoder.h"

#define ENABLE_HYPER_FF_HEARTBEAT
void PIN_FAST_ANALYSIS_CALL docount(UINT32 c) {
  int64_t temp = hyper_fast_forward_count - c;
  // Saturating subtract: temp &= -(temp <= hyper_fast_forward_count);
  hyper_fast_forward_count = temp;

#ifdef ENABLE_HYPER_FF_HEARTBEAT
  total_ff_count += c;
  if(!(total_ff_count & 0x7FFFFFF0)) {
    std::cout << "Hyper FF Heartbeat: inst_count=" << total_ff_count << " ("
              << setprecision(2)
              << (100.0 *
                  (orig_hyper_fast_forward_count - hyper_fast_forward_count)) /
                   orig_hyper_fast_forward_count
              << "%)" << endl;
  }
#endif

  if(hyper_fast_forward_count <= 0) {
    hyper_ff = false;
    std::cout << "Exiting Hyper Fast Forward Mode." << endl;

    if(hyper_fast_forward_delta > 0) {
      fast_forward_count += (hyper_fast_forward_count +
                             hyper_fast_forward_delta);
      if(fast_forward_count > 0) {
        std::cout << "Entering Fast Forward Mode: " << fast_forward_count
                  << " ins remaining" << endl;
      }
    }
    PIN_RemoveInstrumentation();
  }
}

#if defined(TARGET_IA32E)
#define EXIT_SYSCALL_NUM1 231
#define EXIT_SYSCALL_NUM2 60
#else
#define EXIT_SYSCALL_NUM1 1
#define EXIT_SYSCALL_NUM2 1
#endif
void process_syscall(ADDRINT ip, ADDRINT num, ADDRINT arg0, ADDRINT arg1,
                     ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5,
                     CONTEXT* ctxt, bool real_syscall) {
  if(!fast_forward_count) {
    const bool is_exit_syscall = (real_syscall && (EXIT_SYSCALL_NUM1 == num ||
                                                   EXIT_SYSCALL_NUM2 == num));

    main_loop(ctxt, Mem_Writes_Info(), true, is_exit_syscall);
  }
}

void process_instruction_no_mem_write(CONTEXT* ctxt) {
  if(!fast_forward_count) {
    main_loop(ctxt, Mem_Writes_Info(), false, false);
  }
}

void process_instruction_one_mem_write(CONTEXT* ctxt, ADDRINT write_addr,
                                       UINT32 write_size) {
  write_addr = ADDR_MASK(write_addr);
  if(!fast_forward_count) {
    main_loop(ctxt, Mem_Writes_Info(write_addr, write_size), false, false);
  }
}

void process_instruction_multi_mem_write(
  CONTEXT* ctxt, PIN_MULTI_MEM_ACCESS_INFO* mem_access_info, bool is_scatter) {
  if(!fast_forward_count) {
    main_loop(ctxt, Mem_Writes_Info(mem_access_info, ctxt, is_scatter), false,
              false);
  }
}

void enter_wrongpath_nop_mode_if_needed() {
  if(!fast_forward_count && pintool_state.is_on_wrongpath_nop_mode()) {
    wrongpath_nop_mode_main_loop();
  }
}

void change_pintool_control_flow_if_needed(CONTEXT* ctxt) {
  if(pintool_state.should_change_control_flow()) {
    CONTEXT* new_ctxt = pintool_state.get_context_for_changing_control_flow();
    if(pintool_state.should_skip_next_instruction()) {
      fast_forward_count = 2;
    }
    pintool_state.clear_changing_control_flow();
    PIN_ExecuteAt(new_ctxt);
  }
}

void redirect(CONTEXT* ctx) {
  std::cout << "Inside redirect analysis\n";
  started = true;
  std::cout << "About to redirect to " << std::hex << start_rip << "\n";
  PIN_SetContextRegval(ctx, REG_INST_PTR, (const UINT8*)(&start_rip));
  PIN_RemoveInstrumentation();
  PIN_ExecuteAt(ctx);
}

void logging(ADDRINT next_rip, ADDRINT curr_rip, bool check_next_addr,
             bool taken) {
  static bool first = true;
  if(fast_forward_count) {
    if(!(fast_forward_count & 0xFFFFF)) {
      std::cout << "Heartbeat: Fast Forwarding (ins. remain="
                << fast_forward_count << ")" << endl;
    }
    fast_forward_count -= !fast_forward_to_pin_start;
    total_ff_count++;

    if(first && fast_forward_count == 0) {
      first = false;
      std::cout << "Exiting Fast Forward mode: inst_count=" << total_ff_count
                << endl;
    }
  }

  pintool_state.set_next_rip(next_rip);

  if(!fast_forward_count) {
    if(heartbeat_enabled && !(pintool_state.get_curr_inst_uid() & 0x7FFFF)) {
      std::cout << "Heartbeat (uid=" << pintool_state.get_curr_inst_uid() << ")"
                << endl;
    }

    DBG_PRINT(
      pintool_state.get_curr_inst_uid(), dbg_print_start_uid, dbg_print_end_uid,
      "Curr EIP=%" PRIx64 ", next EIP=%" PRIx64 ", Curr uid=%" PRIu64
      ", wrongpath=%d, instrumented=%d\n",
      ADDR_MASK(curr_rip), ADDR_MASK(next_rip),
      pintool_state.get_curr_inst_uid(), pintool_state.is_on_wrongpath(),
      instrumented_rip_tracker.contains(ADDR_MASK(next_rip)));

    if(pintool_state.is_on_wrongpath() && check_next_addr && !taken &&
       !instrumented_rip_tracker.contains(next_rip)) {
      // if we're currently on the wrong path and we somehow
      // about to come across an instruction that was never
      // instrumented, then go in WPNM right away to avoid the
      // possibility of PIN going off and instrumenting wrong
      // path code that might crash PIN
      pintool_state.set_wrongpath_nop_mode(
        WPNM_REASON_NOT_TAKEN_TO_NOT_INSTRUMENTED, next_rip);
    }

    /*
    DBG_PRINT(
      pintool_state.get_curr_inst_uid(), dbg_print_start_uid, dbg_print_end_uid,
      "Curr EIP=%" PRIx64 ", next EIP=%" PRIx64 ", Curr uid=%" PRIu64
      ", wrongpath=%d, wpnm=%d, instrumented=%d\n",
      (uint64_t)curr_eip, (uint64_t)n_eip, pintool_state.get_curr_inst_uid(),
      on_wrongpath, on_wrongpath_nop_mode,
      instrumented_rip_tracker.contains(next_eip));
    */
  }
}

void exception_handler_followup(CONTEXT* ctxt) {
  if(pintool_state.should_insert_dummy_exception_br()) {
    ASSERTM(0, !op_mailbox_full,
            "Expected empty mailbox for rightpath exception op @ %" PRIu64
            ".\n",
            pintool_state.get_curr_inst_uid());

    const auto inst_uid = pintool_state.get_next_inst_uid();

    uint64_t curr_rip = PIN_GetContextReg(ctxt, REG_INST_PTR);
    op_mailbox        = create_dummy_jump(
      pintool_state.get_rightpath_exception_next_rip(), curr_rip);
    op_mailbox.inst_uid = inst_uid;
    op_mailbox_full     = true;

    DBG_PRINT(inst_uid, dbg_print_start_uid, dbg_print_end_uid,
              "Inserting a dummy branch following an exception. Exception RIP: "
              "%lx, Branch RIP: %lx, jumping to %lx\n",
              pintool_state.get_rightpath_exception_rip(),
              pintool_state.get_rightpath_exception_next_rip(), curr_rip);

    checkpoints.append_to_cir_buf();
    checkpoints.get_tail().update(ctxt, inst_uid, false, false, false, curr_rip,
                                  Mem_Writes_Info{}, false);

    pintool_state.clear_rightpath_exception();
  }
}

void check_ret_control_ins(ADDRINT read_addr, UINT32 read_size, CONTEXT* ctxt) {
  if(!fast_forward_count) {
    read_addr = ADDR_MASK(read_addr);
    ASSERTM(0, read_size == 8,
            "RET should pop exactly 8 bytes off the stack. RSP: %" PRIx64
            ", size: %u\n",
            (uint64_t)read_addr, read_size);
    uint64_t target_addr;
    PIN_SafeCopy(&target_addr, (void*)read_addr, read_size);
    target_addr = ADDR_MASK(target_addr);
    pintool_state.set_next_rip(target_addr);
    DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
              dbg_print_end_uid, "Ret Control targetaddr=%" PRIx64 "\n",
              (uint64_t)target_addr);


    if(pintool_state.is_on_wrongpath() &&
       !instrumented_rip_tracker.contains(target_addr)) {
      target_addr = target_addr ? target_addr : 1;
      DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
                dbg_print_end_uid,
                "Entering from ret WPNM targetaddr=%" PRIx64 "\n",
                (uint64_t)target_addr);
      pintool_state.set_wrongpath_nop_mode(
        WPNM_REASON_RETURN_TO_NOT_INSTRUMENTED, target_addr);
    }
  }
}

void check_nonret_control_ins(bool taken, ADDRINT target_addr) {
  if(!fast_forward_count) {
    pintool_state.set_next_rip(target_addr);
    target_addr = ADDR_MASK(target_addr);
    DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
              dbg_print_end_uid, "Non Ret Control targetaddr=%" PRIx64 "\n",
              (uint64_t)target_addr);
    if(pintool_state.is_on_wrongpath() && taken &&
       !instrumented_rip_tracker.contains(target_addr)) {
      target_addr = target_addr ? target_addr : 1;
      DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
                dbg_print_end_uid,
                "Entering from nonret WPNM targetaddr=%" PRIx64 "\n",
                (uint64_t)target_addr);
      pintool_state.set_wrongpath_nop_mode(
        WPNM_REASON_NONRET_CF_TO_NOT_INSTRUMENTED, target_addr);
    }
  }
}

void check_nonret_control_mem_target(bool taken, ADDRINT addr, UINT32 ld_size) {
  if(!fast_forward_count) {
    addr = ADDR_MASK(addr);
    ADDRINT target_addr;
    if(ld_size != 8) {
      target_addr = 0;
    } else {
      PIN_SafeCopy(&target_addr, (void*)addr, ld_size);
    }
    check_nonret_control_ins(taken, target_addr);
  }
}

#define SCARAB_MARKERS_PIN_BEGIN (1)
#define SCARAB_MARKERS_PIN_END (2)
void handle_scarab_marker(ADDRINT op) {
  switch(op) {
    case SCARAB_MARKERS_PIN_BEGIN:
      fast_forward_to_pin_start = (fast_forward_count = 0);
      break;
    case SCARAB_MARKERS_PIN_END:
      fast_forward_to_pin_start = (fast_forward_count = 1);
      break;
    default:
      std::cerr << "Error: Found Scarab Marker that does not have known code."
                << endl;
  }
}