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

#include "main_loop.h"
#include "scarab_interface.h"

void register_signal_handlers() {
  // Intercept signals to see exceptions
  PIN_InterceptSignal(SIGFPE, signal_handler, 0);
  PIN_InterceptSignal(SIGILL, signal_handler, 0);
  PIN_InterceptSignal(SIGSEGV, signal_handler, 0);
  PIN_InterceptSignal(SIGBUS, signal_handler, 0);

  PIN_InterceptSignal(SIGHUP, dummy_handler, 0);
  PIN_InterceptSignal(SIGINT, dummy_handler, 0);
  PIN_InterceptSignal(SIGQUIT, dummy_handler, 0);
  PIN_InterceptSignal(SIGTRAP, dummy_handler, 0);
  PIN_InterceptSignal(SIGIOT, dummy_handler, 0);
  PIN_InterceptSignal(SIGKILL, dummy_handler, 0);
  PIN_InterceptSignal(SIGUSR1, dummy_handler, 0);
  PIN_InterceptSignal(SIGUSR2, dummy_handler, 0);
  PIN_InterceptSignal(SIGPIPE, dummy_handler, 0);
  PIN_InterceptSignal(SIGALRM, dummy_handler, 0);
  PIN_InterceptSignal(SIGTERM, dummy_handler, 0);
  PIN_InterceptSignal(SIGSTKFLT, dummy_handler, 0);
  PIN_InterceptSignal(SIGCHLD, dummy_handler, 0);
  PIN_InterceptSignal(SIGCONT, dummy_handler, 0);
  PIN_InterceptSignal(SIGSTOP, dummy_handler, 0);
  PIN_InterceptSignal(SIGTSTP, dummy_handler, 0);
  PIN_InterceptSignal(SIGTTIN, dummy_handler, 0);
  PIN_InterceptSignal(SIGTTOU, dummy_handler, 0);
  PIN_InterceptSignal(SIGURG, dummy_handler, 0);
  PIN_InterceptSignal(SIGXCPU, dummy_handler, 0);
  PIN_InterceptSignal(SIGXFSZ, dummy_handler, 0);
  PIN_InterceptSignal(SIGVTALRM, dummy_handler, 0);
  PIN_InterceptSignal(SIGPROF, dummy_handler, 0);
  PIN_InterceptSignal(SIGWINCH, dummy_handler, 0);
  PIN_InterceptSignal(SIGIO, dummy_handler, 0);
  PIN_InterceptSignal(SIGPWR, dummy_handler, 0);
}

bool dummy_handler(THREADID tid, INT32 sig, CONTEXT* ctxt, bool hasHandler,
                   const EXCEPTION_INFO* pExceptInfo, void* v) {
#ifdef DEBUG_PRINT
  ADDRINT curr_eip;
  PIN_GetContextRegval(ctxt, REG_INST_PTR, (UINT8*)&curr_eip);
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "dummyhandler curr_uid=%" PRIu64 ", curr_eip=%" PRIx64 ", sig=%d\n",
            uid_ctr, (uint64_t)curr_eip, sig);
#endif

  return true;
}

bool signal_handler(THREADID tid, INT32 sig, CONTEXT* ctxt, bool hasHandler,
                    const EXCEPTION_INFO* pExceptInfo, void* v) {
  ADDRINT curr_eip;
  PIN_GetContextRegval(ctxt, REG_INST_PTR, (UINT8*)&curr_eip);
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "signalhandler curr_uid=%" PRIu64 ", curr_eip=%" PRIx64
            ", sig=%d, wp=%d\n",
            uid_ctr, (uint64_t)curr_eip, sig, on_wrongpath);
  if(!fast_forward_count || on_wrongpath) {
    if(on_wrongpath) {
      if(sig == SIGFPE || sig == SIGSEGV || sig == SIGBUS) {
        PIN_SetContextRegval(ctxt, REG_INST_PTR, (const UINT8*)(&next_eip));

        // mark the checkpoint for the excepting instruction as unretireable
        INT64 idx = checkpoints.get_head_index();
        INT64 n   = checkpoints.get_size();

        bool   found_uid = false;
        UINT64 uid       = uid_ctr - 1;

        if(!fast_forward_count) {
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

        DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
                  "signalhandler return false\n");
        return false;
      } else if(sig == SIGILL) {
        DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
                  "Fail to detect ILLOP at %" PRIx64 "\n", curr_eip);
        pending_syscall = true;  // Treat illegal ops as syscalls (fetch barrier
                                 // behavior)
        pending_exception = true;

        seen_rightpath_exc_mode = false;
        return excp_main_loop(sig);

        ASSERTM(0, false,
                "Reached end of wrong excp after retiring all instructions\n");

        return true;  // Throw the exception to PIN process to terminate it
      } else {
        return true;  // Throw the exception to PIN process to terminate it
      }
    } else {  // On Right Path
      if(sig == SIGFPE || sig == SIGSEGV || sig == SIGILL) {
        // The mailbox op is currently the offending op, it needs to be treated
        // as syscall
        // The main loop should upgrade the mailbox op to be buffered for
        // sending to scarab
        pending_syscall = true;  // Treat illegal ops as syscalls (fetch barrier
                                 // behavior)
        pending_exception = true;

        saved_excp_eip      = curr_eip;
        saved_excp_next_eip = next_eip;
        DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
                  "Found rightpath excp at %" PRIx64 "\n", curr_eip);

        // Before starting the handler, wait for scarab to retire all ops.
        // If it turns out scarab wait to redirect/recover, then this is
        // actually on the wrong path!

        return excp_main_loop(sig);

        ASSERTM(0, false,
                "Reached end of rightpath excp without retiring all "
                "instructions\n");

        return true;

      } else {
        ASSERTM(0, false, "Unhandled rightpath exception\n");
        return true;  // Throw the exception to PIN process to terminate it
      }
    }
  }

  // Handles the case when fast forward for recover cause an excption
  if(excp_ff && on_wrongpath) {
    PIN_SetContextRegval(ctxt, REG_INST_PTR, (const UINT8*)(&next_eip));
    return false;
  } else {
    fprintf(stderr, "PIN: Found exception sig=%d on rightpath\n", sig);
    return true;
  }
}

bool excp_main_loop(int sig) {
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "excp main loop next_eip=%" PRIx64 "\n", (uint64_t)next_eip);

  bool buffer_ready                    = false;
  bool syscall_has_been_sent_to_scarab = false;
  bool have_consumed_op                = false;
  bool need_scarab_cmd                 = false;

  while(true) {
    Scarab_To_Pin_Msg cmd;
    cmd.type = FE_NULL;

    if(need_scarab_cmd) {
      cmd = get_scarab_cmd();
    }
    saved_cmd = cmd;

    if(cmd.type == FE_RECOVER_BEFORE || cmd.type == FE_RECOVER_AFTER) {
      seen_rightpath_exc_mode = false;
      excp_rewind_msg         = true;
      return false;
    } else if(cmd.type == FE_REDIRECT) {
      seen_rightpath_exc_mode = false;
      excp_rewind_msg         = true;
      return false;
    } else if(cmd.type == FE_RETIRE) {
      if(do_fe_retire(cmd)) {
        DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
                  "term retire in exec\n");
        excp_rewind_msg   = false;
        pending_exception = false;
        pending_syscall   = false;
        fprintf(stderr, "PIN: Found exception sig=%d on rightpath\n", sig);
        seen_rightpath_exc_mode = true;
        return true;
      }
      // Once all instruction upto the excpeting instructions are retired, take
      // the exception
      if(cmd.inst_uid + 1 == uid_ctr && !on_wrongpath) {
        DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
                  "execute rightpath exception handler\n");
        fprintf(stderr, "PIN: Found exception sig=%d on rightpath\n", sig);
        excp_rewind_msg         = false;
        pending_exception       = false;
        pending_syscall         = false;
        seen_rightpath_exc_mode = true;
        return true;
      }
    } else if(cmd.type == FE_FETCH_OP) {
      do_fe_fetch_op(syscall_has_been_sent_to_scarab);
    } else if(cmd.type == FE_NULL) {
      // If a syscall causes an exception, it has already been sent to scarab
      if(found_syscall)
        have_consumed_op = true;
      else
        do_fe_null(have_consumed_op);
    }
    // We always have a scarab command
    buffer_ready               = scarab_buffer_full() || pending_syscall;
    bool send_buffer_to_scarab = buffer_ready && pending_fetch_op &&
                                 have_consumed_op;
    if(send_buffer_to_scarab) {
      scarab_send_buffer();
      pending_fetch_op  = false;
      pending_exception = false;
      pending_syscall   = false;
    }

    if(have_consumed_op)
      need_scarab_cmd = true;
  }
}
