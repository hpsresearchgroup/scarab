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

#include "main_loop.h"

#include "scarab_interface.h"

#include "../pin_lib/decoder.h"

namespace {

/*
void undo_mem(const ProcState& undo_state) {
  for(uint i = 0; i < undo_state.num_mem_state; i++) {
    void*  write_addr = (void*)undo_state.mem_state_list[i].mem_addr;
    UINT32 write_size = undo_state.mem_state_list[i].mem_size;
    void*  prev_data  = undo_state.mem_state_list[i].mem_data_ptr;
    PIN_SafeCopy(write_addr, prev_data, write_size);
  }
}
*/

/*
void undo_checkpoints_past_uid(UINT64 uid) {
  INT64 idx = checkpoints.get_tail_index();
  while(!checkpoints.empty()) {
    if(checkpoints[idx].uid == uid) {
      return;
    }

    undo_mem(checkpoints[idx]);
    idx = checkpoints.remove_from_cir_buf_tail();
  }
  ASSERTM(0, false, "Checkpoint %" PRIu64 " not found. \n", uid);
}
*/

/*
void recover_to_past_checkpoint(UINT64 uid, bool is_redirect_recover,
                                bool enter_ff) {
  undo_checkpoints_past_uid(uid);
  INT64 idx = checkpoints.get_tail_index();

  scarab_clear_all_buffers();

  while(!checkpoints.empty()) {
    if(checkpoints[idx].uid == uid) {
      // Resumes execution at the saved context

      ADDRINT this_eip;

      on_wrongpath          = checkpoints[idx].wrongpath;
      on_wrongpath_nop_mode = checkpoints[idx].wrongpath_nop_mode;
      generate_dummy_nops   = (generate_dummy_nops && on_wrongpath_nop_mode);
      if(is_redirect_recover) {
        return;
      } else {
        undo_mem(checkpoints[idx]);
        PIN_SaveContext(&(checkpoints[idx].ctxt), &last_ctxt);

        if(!on_wrongpath_nop_mode) {
          if(enter_ff) {
            excp_ff = true;
            fast_forward_count += 2;
            // pin skips (ffc - 1) instructions
          } else {
            idx = checkpoints.remove_from_cir_buf_tail();
          }
          PIN_ExecuteAt(&last_ctxt);
          ASSERTM(0, false, "PIN_ExecuteAt did not redirect execution.\n");
        } else {
          ADDRINT prev_eip;
          PIN_GetContextRegval(&last_ctxt, REG_INST_PTR, (UINT8*)&prev_eip);
          this_eip = checkpoints[idx].wpnm_eip;
          next_eip = ADDR_MASK(enter_ff ? this_eip : prev_eip);
          return;
        }
      }
      ASSERTM(0, false,
              "Recover: found uid %" PRIu64 ", but eip not changed.\n", uid);
    }

    undo_mem(checkpoints[idx]);

    if(is_redirect_recover && (checkpoints[idx].uid == (uid + 1))) {
      PIN_SaveContext(&(checkpoints[idx].ctxt), &last_ctxt);
    }

    idx = checkpoints.remove_from_cir_buf_tail();
  }
  ASSERTM(0, false, "Checkpoint %" PRIu64 " not found. \n", uid);
}
*/

/*
void redirect_to_inst(ADDRINT inst_addr, CONTEXT* ctxt, UINT64 uid) {
scarab_clear_all_buffers();
undo_checkpoints_past_uid(cmd.inst_uid);
PIN_SetContextRegval(&last_ctxt, REG_INST_PTR, (const UINT8*)(&inst_addr));

on_wrongpath = true;  // redirect ALWAYS sets to wrongpath==true
if(on_wrongpath && !instrumented_rip_tracker.contains(inst_addr)) {
  on_wrongpath_nop_mode     = true;
  wrongpath_nop_mode_reason = WPNM_REASON_REDIRECT_TO_NOT_INSTRUMENTED;
  DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
dbg_print_end_uid, "Entering from redirect WPNM targetaddr=%" PRIx64 "\n",
            (uint64_t)inst_addr);
}
if(on_wrongpath_nop_mode) {
  next_eip = ADDR_MASK(inst_addr);
} else {
  PIN_ExecuteAt(&last_ctxt);
  ASSERTM(0, false, "PIN_ExecuteAt did not redirect execution.\n");
}
}
*/


// Retire all instrution upto and including uid
void retire_older_checkpoints(UINT64 uid) {
  INT64 idx       = checkpoints.get_head_index();
  bool  found_uid = false;
  while(!checkpoints.empty()) {
    if(checkpoints[idx].uid <= uid) {
      ASSERTM(0, !checkpoints[idx].wrongpath,
              "Tried to retire wrongpath op %" PRIu64 ".\n", uid);
      if(checkpoints[idx].unretireable_instruction) {
        ADDRINT eip;
        PIN_GetContextRegval(&(checkpoints[idx].ctxt), REG_INST_PTR,
                             (UINT8*)&eip);
        ASSERTM(0, false,
                "Exception by program caused at address 0x%" PRIx64 "\n",
                (uint64_t)eip);
      }
      if(checkpoints[idx].uid == uid) {
        found_uid = true;
      }
      idx = checkpoints.remove_from_cir_buf_head();
    } else {
      break;
    }
  }
  ASSERTM(0, found_uid, "Checkpoint %" PRIu64 " not found. \n", uid);
}

/*
bool do_fe_recover(Scarab_To_Pin_Msg& cmd, CONTEXT* ctxt) {
return false;
DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
dbg_print_end_uid, "recover curr_uid=%" PRIu64 ", target_uid=%" PRIu64 "\n",
pintool_state.get_curr_inst_uid(), cmd.inst_uid); if(pending_syscall &&
cmd.inst_uid == (pintool_state.get_curr_inst_uid() - 1)) { ASSERTM(0, false,
          "Unexpected Recover to current syscall inst @uid=%" PRIu64 "\n",
          pintool_state.get_curr_inst_uid() - 1);
}
seen_rightpath_exc_mode = false;
pending_syscall         = false;
pending_exception       = false;
buffer_sentinel         = false;
ASSERTM(0, cmd.type == FE_RECOVER_AFTER || cmd.type == FE_RECOVER_BEFORE,
        "Unknown Recover Type (uid=%" PRIu64 ")\n",
pintool_state.get_curr_inst_uid() - 1);

scarab_clear_all_buffers();
undo_checkpoints_past_uid(cmd.inst_uid);

INT64 idx             = checkpoints.get_tail_index();
on_wrongpath          = checkpoints[idx].wrongpath;
on_wrongpath_nop_mode = checkpoints[idx].wrongpath_nop_mode;
generate_dummy_nops   = (generate_dummy_nops && on_wrongpath_nop_mode);

if(!on_wrongpath_nop_mode) {
  if(cmd.type == FE_RECOVER_AFTER) {
    excp_ff = true;
    fast_forward_count += 2;
    // pin skips (ffc - 1) instructions
  } else {
    undo_mem(checkpoints[idx]);
    idx = checkpoints.remove_from_cir_buf_tail();
  }
  pintool_state.set_next_state_for_changing_control_flow(
    &checkpoints[idx].ctxt, false, 0);
} else {
  const auto prev_eip = PIN_GetContextReg(&checkpoints[idx].ctxt,
                                          REG_INST_PTR);
  const auto this_eip = checkpoints[idx].wpnm_eip;
  next_eip = ADDR_MASK(cmd.type == FE_RECOVER_AFTER ? this_eip : prev_eip);
  wpnm_skip_ckp = true;
}
}
*/

/*
bool do_fe_redirect(Scarab_To_Pin_Msg& cmd, CONTEXT* ctxt) {
return true;
DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
dbg_print_end_uid, "redirect curr_uid=%" PRIu64 ", target_uid=%" PRIu64
          ", target_eip=%llx\n",
          pintool_state.get_curr_inst_uid(), cmd.inst_uid, cmd.inst_addr);
if(pending_syscall && cmd.inst_uid == (pintool_state.get_curr_inst_uid() - 1)) {
  ASSERTM(0, false,
          "Unexpected Redirect to current syscall inst @uid=%" PRIu64 "\n",
          pintool_state.get_curr_inst_uid() - 1);
}
seen_rightpath_exc_mode = false;
pending_syscall         = false;
pending_exception       = false;
buffer_sentinel         = false;
redirect_to_inst(cmd.inst_addr, ctxt, cmd.inst_uid);
if(on_wrongpath_nop_mode) {
  if(entered_wpnm) {
    wpnm_skip_ckp = true;
  }
} else {
  ASSERTM(0, false,
          "Redirect cmd did not change execution (uid=%" PRIu64 ")\n",
          pintool_state.get_curr_inst_uid());
}
return on_wrongpath_nop_mode;
}
*/

void buffer_next_instruction(CONTEXT* ctxt, Mem_Writes_Info mem_writes_info,
                             bool is_syscall, bool is_exit_syscall) {
  const auto inst_uid = pintool_state.get_next_inst_uid();

  auto cop = pin_decoder_get_latest_inst();
  cop->inst_uid = inst_uid;

  checkpoints.append_to_cir_buf();
  bool on_wrongpath_deleteme          = false;
  bool on_wrongpath_nop_mode_deleteme = false;

  checkpoints.get_tail().update(
    ctxt, inst_uid, false, on_wrongpath_deleteme, on_wrongpath_nop_mode_deleteme,
    cop->instruction_addr + cop->size, mem_writes_info);

  DBG_PRINT(inst_uid, dbg_print_start_uid, dbg_print_end_uid,
            "fenull curr_uid=%" PRIu64 "\n", inst_uid);

  // hack to get SYSENTER (which is only used for 32bit
  // binaries) to work. Basically, because of the way
  // SYSENTER is used by Linux, control will return to
  // the user code 11 bytes after the SYSENTER
  // instruction, even though SYSENTER is only a 2 byte
  // instruction...
  if(strcmp(cop->pin_iclass, "SYSENTER") == 0) {
    cop->size = 11;
  }

  DBG_PRINT(inst_uid, dbg_print_start_uid, dbg_print_end_uid,
            "consuming instruction eip:%" PRIx64
            ", opcode:%s, ifetch_barrier:%u, cf_type:%u, op_type:%u, "
            "num_ld: %u, num_st: %u, exit: %u, ins_size: %u\n",
            cop->instruction_addr, cop->pin_iclass, cop->is_ifetch_barrier,
            cop->cf_type, cop->op_type, cop->num_ld, cop->num_st, cop->exit,
            cop->size);


  if(op_mailbox_full) {
    op_mailbox.instruction_next_addr = cop->instruction_addr;
    if(op_mailbox.cf_type && op_mailbox.actually_taken) {
      op_mailbox.branch_target = cop->instruction_addr;
    }
    insert_scarab_op_in_buffer(op_mailbox);
    op_mailbox_full = false;
  }

  if(is_syscall) {
    // bypass mailbox
    if(is_exit_syscall) {
      cop->exit = 1;
    }
    insert_scarab_op_in_buffer(*cop);
    if(is_exit_syscall) {
      compressed_op sentinel_cop = create_sentinel();
      insert_scarab_op_in_buffer(sentinel_cop);
    }
  } else {
    op_mailbox      = *cop;
    op_mailbox_full = true;
  }
}

template <typename F>
void process_scarab_cmds_until(CONTEXT* ctxt, F&& condition_func) {
  while(true) {
    Scarab_To_Pin_Msg cmd = get_scarab_cmd();
    if(cmd.type == FE_RECOVER_BEFORE || cmd.type == FE_RECOVER_AFTER) {
      // do_fe_recover(cmd, ctxt);
      return;
    } else if(cmd.type == FE_REDIRECT) {
      // do_fe_redirect(cmd, ctxt);
      return;
    } else if(cmd.type == FE_RETIRE) {
      do_fe_retire(cmd);
    } else if(cmd.type == FE_FETCH_OP) {
      do_fe_fetch_op();
    } else {
      ASSERTM(0, false, "Uncorecorgnized Scarab Command Type: %d", cmd.type);
    }

    if(condition_func(cmd))
      return;
  }
}

void wait_until_scarab_sends_fetch_op(CONTEXT* ctxt) {
  process_scarab_cmds_until(
    ctxt, [](const Scarab_To_Pin_Msg& cmd) { return cmd.type == FE_FETCH_OP; });
}

void wait_until_scarab_retires_the_syscall(CONTEXT* ctxt) {
  process_scarab_cmds_until(ctxt, [](const Scarab_To_Pin_Msg& cmd) {
    if(cmd.type == FE_FETCH_OP) {
      ASSERTM(0, false,
              "Scarab sent a fetch command before retiring the syscall in the "
              "pipelin");
    }
    return cmd.type == FE_RETIRE && checkpoints.empty();
  });
}


}  // namespace

void do_fe_retire(Scarab_To_Pin_Msg& cmd) {
  DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
            dbg_print_end_uid,
            "retire curr_uid=%" PRIu64 ", target_uid=%" PRIu64 ", addr=%llx\n",
            pintool_state.get_curr_inst_uid(), cmd.inst_uid, cmd.inst_addr);
  if(cmd.inst_addr) {
    // A non-zero inst address in a retire command signifies to early exit the
    // app.
    PIN_ExitApplication(0);
    ASSERTM(0, false,
            "PIN could not safely terminate app upon scarab request.\n");
  }
  retire_older_checkpoints(cmd.inst_uid);
}

void do_fe_fetch_op() {
  DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
            dbg_print_end_uid, "fetch curr_uid=%" PRIu64 "\n",
            pintool_state.get_curr_inst_uid());
}

// Communicates with scarab, and performs the requested actions
void main_loop(CONTEXT* ctxt, Mem_Writes_Info mem_writes_info, bool is_syscall,
               bool is_exit_syscall) {
  DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
            dbg_print_end_uid, "main loop next_eip=%" PRIx64 "\n",
            (uint64_t)next_eip);

  buffer_next_instruction(ctxt, mem_writes_info, is_syscall, is_exit_syscall);

  if(scarab_buffer_full() || is_syscall) {
    wait_until_scarab_sends_fetch_op(ctxt);
    if(pintool_state.skip_further_processing()) {
      return;
    }

    scarab_send_buffer();

    if(is_syscall) {
      wait_until_scarab_retires_the_syscall(ctxt);
    }
  }
}