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

#include "exception_handling.h"

#include "instruction_processing.h"
#include "scarab_interface.h"

#include "../pin_lib/decoder.h"

namespace {

bool dummy_handler(THREADID tid, INT32 sig, CONTEXT* ctxt, bool hasHandler,
                   const EXCEPTION_INFO* pExceptInfo, void* v) {
#ifdef DEBUG_PRINT
  ADDRINT curr_eip;
  PIN_GetContextRegval(ctxt, REG_INST_PTR, (UINT8*)&curr_eip);
  DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
            dbg_print_end_uid,
            "dummyhandler curr_uid=%" PRIu64 ", curr_eip=%" PRIx64 ", sig=%d\n",
            pintool_state.get_curr_inst_uid(), (uint64_t)curr_eip, sig);
#endif

  return true;
}

bool propagate_the_signal() {
  return true;
}

bool skip_this_instruction(CONTEXT* ctxt) {
  // TODO: Does this work properly while fast-forwarding?
  // As is, next_rip is probably bogus while fast-forwarding.
  PIN_SetContextReg(ctxt, REG_INST_PTR, pintool_state.get_next_rip());
  return false;
}

void mark_checkpoint_as_unretirable() {
  // mark the checkpoint for the excepting instruction as unretireable
  bool   found_uid = false;
  UINT64 uid       = pintool_state.get_curr_inst_uid() - 1;
  INT64  idx       = checkpoints.get_head_index();
  INT64  n         = checkpoints.get_size();
  while(n > 0) {
    if(checkpoints[idx].uid == uid) {
      found_uid                                 = true;
      checkpoints[idx].unretireable_instruction = true;
      break;
    }

    idx++;
    n--;
  }
  ASSERTM(0, found_uid, "Checkpoint %" PRIu64 " not found. \n", uid);
}

void mark_mailbox_op_as_exception_and_insert_in_buffer() {
  ASSERTM(0, op_mailbox_full,
          "Op mailbox empty when an exception was triggered, uid: %ld.\n",
          pintool_state.get_curr_inst_uid());
  op_mailbox.is_ifetch_barrier = 1;
  op_mailbox.cf_type           = CF_SYS;
  insert_scarab_op_in_buffer(op_mailbox);
  op_mailbox_full = false;
}

bool process_exception_on_wrongpath(CONTEXT* ctxt, INT32 sig) {
  if(sig == SIGFPE || sig == SIGSEGV || sig == SIGBUS) {
    mark_checkpoint_as_unretirable();
    DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
              dbg_print_end_uid, "signalhandler return false\n");
    return skip_this_instruction(ctxt);
  } else {
    mark_mailbox_op_as_exception_and_insert_in_buffer();
    process_instruction_with_exception(ctxt);

    ASSERTM(0, pintool_state.should_change_control_flow(),
            "An exception in the wrongpath should be flushed before getting "
            "committed, uid=%ld.\n",
            pintool_state.get_curr_inst_uid());

    PIN_SaveContext(pintool_state.get_context_for_changing_control_flow(),
                    ctxt);
    if(pintool_state.should_skip_next_instruction()) {
      fast_forward_count = 2;
    }
    pintool_state.clear_changing_control_flow();
    return false;
  }
}

bool process_exception_on_rightpath(CONTEXT* ctxt, INT32 sig, ADDRINT rip,
                                    ADDRINT next_rip) {
  if(sig == SIGFPE || sig == SIGSEGV || sig == SIGILL) {
    process_instruction_with_exception(ctxt);

    if(pintool_state.should_change_control_flow()) {
      // The exception is not to be committed due to a recover/redirect.
      // Update the CONTEXT and suppress the signal.
      PIN_SaveContext(pintool_state.get_context_for_changing_control_flow(),
                      ctxt);
      if(pintool_state.should_skip_next_instruction()) {
        fast_forward_count = 2;
      }
      pintool_state.clear_changing_control_flow();
      return false;
    } else {
      pintool_state.set_rightpath_exception(rip, next_rip);
      return true;
    }
  } else {
    ASSERTM(0, false, "Unhandled rightpath exception\n");
  }
}

ADDRINT get_next_rip() {
  ASSERTM(0, op_mailbox_full,
          "Op mailbox empty when an exception was triggered, uid: %ld.\n",
          pintool_state.get_curr_inst_uid());
  return op_mailbox.instruction_next_addr;
}

bool signal_handler(THREADID tid, INT32 sig, CONTEXT* ctxt, bool hasHandler,
                    const EXCEPTION_INFO* pExceptInfo, void* v) {
  ADDRINT curr_rip = PIN_GetContextReg(ctxt, REG_INST_PTR);
  DBG_PRINT(
    pintool_state.get_curr_inst_uid(), dbg_print_start_uid, dbg_print_end_uid,
    "signalhandler curr_uid=%" PRIu64 ", curr_eip=%" PRIx64 ", sig=%d, wp=%d\n",
    pintool_state.get_curr_inst_uid(), (uint64_t)curr_rip, sig,
    pintool_state.is_on_wrongpath());

  if(fast_forward_count) {
    if(pintool_state.is_on_wrongpath()) {
      return skip_this_instruction(ctxt);
    } else {
      return propagate_the_signal();
    }
  } else {
    if(pintool_state.is_on_wrongpath()) {
      return process_exception_on_wrongpath(ctxt, sig);
    } else {
      ADDRINT next_rip = get_next_rip();
      mark_mailbox_op_as_exception_and_insert_in_buffer();
      return process_exception_on_rightpath(ctxt, sig, curr_rip, next_rip);
    }
  }
}

}  // namespace

void register_signal_handlers() {
  int intercepted_signals[] = {SIGFPE, SIGILL, SIGSEGV, SIGBUS};
  for(int sig : intercepted_signals) {
    PIN_InterceptSignal(sig, signal_handler, 0);
  }

  int pass_through_signals[] = {
    SIGHUP,    SIGINT,  SIGQUIT,  SIGTRAP, SIGIOT,    SIGKILL, SIGUSR1,
    SIGUSR2,   SIGPIPE, SIGALRM,  SIGTERM, SIGSTKFLT, SIGCHLD, SIGCONT,
    SIGSTOP,   SIGTSTP, SIGTTIN,  SIGTTOU, SIGURG,    SIGXCPU, SIGXFSZ,
    SIGVTALRM, SIGPROF, SIGWINCH, SIGIO,   SIGPWR};
  for(int sig : pass_through_signals) {
    PIN_InterceptSignal(sig, dummy_handler, 0);
  }
}
