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

void undo_mem(const ProcState& undo_state) {
  for(uint i = 0; i < undo_state.num_mem_state; i++) {
    void*  write_addr = (void*)undo_state.mem_state_list[i].mem_addr;
    UINT32 write_size = undo_state.mem_state_list[i].mem_size;
    void*  prev_data  = undo_state.mem_state_list[i].mem_data_ptr;
    PIN_SafeCopy(write_addr, prev_data, write_size);
  }
}

void undo_checkpoints_until_one_after_uid(UINT64 uid) {
  auto idx      = checkpoints.get_tail_index();
  auto head_idx = checkpoints.get_head_index();
  while(!checkpoints.empty() && idx > head_idx) {
    if(checkpoints[idx - 1].uid == uid) {
      return;
    }

    undo_mem(checkpoints[idx]);
    idx = checkpoints.remove_from_cir_buf_tail();
  }
  ASSERTM(0, false, "Checkpoint %" PRIu64 " not found. \n", uid);
}

void undo_checkpoints_until_uid(UINT64 uid) {
  auto idx = checkpoints.get_tail_index();
  while(!checkpoints.empty()) {
    if(checkpoints[idx].uid == uid) {
      return;
    }

    undo_mem(checkpoints[idx]);
    idx = checkpoints.remove_from_cir_buf_tail();
  }
  ASSERTM(0, false, "Checkpoint %" PRIu64 " not found. \n", uid);
}

void record_written_regions(Mem_Writes_Info mem_writes_info) {
  mem_writes_info.for_each_mem([](ADDRINT addr, uint32_t) {
    pageTableEntryStruct* p_entry = NULL;

    bool hitInPageTable = page_table->get_entry(addr, &p_entry);
    if(!hitInPageTable) {
      pageTableStruct* new_page_table = new pageTableStruct();
      update_page_table(new_page_table);

      hitInPageTable = new_page_table->get_entry(addr, &p_entry);
      // some applications like gcc will store to an unmapped address on the
      // right path, so we will not hit

      // check if each new entry has overlapping entries in the old page_table
      for(auto new_entry = new_page_table->entries.begin();
          new_entry != new_page_table->entries.end(); ++new_entry) {
        bool foundOverlapping = false;
        bool allWrittenTo     = true;
        bool allPathsMatch    = true;

        auto overlapping_old_entries = equal_range(
          page_table->entries.begin(), page_table->entries.end(), *new_entry);

        for(auto old_entry = overlapping_old_entries.first;
            old_entry != overlapping_old_entries.second; ++old_entry) {
          foundOverlapping = true;

          allWrittenTo &= old_entry->writtenToOnRightPath;
          allPathsMatch &= (new_entry->path == old_entry->path);
        }

        if(foundOverlapping && allWrittenTo && allPathsMatch) {
          new_entry->writtenToOnRightPath = true;
        }
      }

      delete page_table;
      page_table = new_page_table;
    }
    if(hitInPageTable) {
      p_entry->writtenToOnRightPath = true;
    }
  });
}

void trigger_wrongpath_nop_mode_if_writing_to_new_region(
  Mem_Writes_Info mem_writes_info, uint64_t next_rip) {
  bool writing_to_new_region = false;
  mem_writes_info.for_each_mem(
    [&writing_to_new_region](ADDRINT addr, uint32_t) {
      pageTableEntryStruct* p_entry = NULL;
      if(page_table->get_entry(addr, &p_entry)) {
        if(!p_entry->writtenToOnRightPath) {
          if(p_entry->permissions & 2) {
            writing_to_new_region = true;
          }
        }
      }
    });

  if(writing_to_new_region) {
    pintool_state.set_wrongpath_nop_mode(
      WPNM_REASON_WRONG_PATH_STORE_TO_NEW_REGION, next_rip);
  }
}

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

void do_fe_recover(Scarab_To_Pin_Msg& cmd, CONTEXT* ctxt) {
  DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
            dbg_print_end_uid,
            "recover curr_uid=%" PRIu64 ", target_uid=%" PRIu64 "\n",
            pintool_state.get_curr_inst_uid() - 1, cmd.inst_uid);

  ASSERTM(0, cmd.type == FE_RECOVER_AFTER || cmd.type == FE_RECOVER_BEFORE,
          "Unknown Recover Type (uid=%" PRIu64 ")\n",
          pintool_state.get_curr_inst_uid() - 1);

  scarab_clear_all_buffers();
  undo_checkpoints_until_uid(cmd.inst_uid);

  undo_mem(checkpoints.get_tail());
  if(cmd.type == FE_RECOVER_BEFORE) {
    checkpoints.remove_from_cir_buf_tail();
  }

  const ProcState& checkpoint = checkpoints.get_tail();
  ASSERTM(0, !checkpoint.is_syscall,
          "Unexpected Redirect to current syscall inst @uid=%" PRIu64 "\n",
          checkpoint.uid);

  pintool_state.set_wrongpath(checkpoint.wrongpath);
  ASSERTM(0, !checkpoint.wrongpath_nop_mode,
          "Cannot recover to a dummy checkpoint created in Wrongpath NOP mode");
  pintool_state.set_wrongpath_nop_mode(WPNM_NOT_IN_WPNM, 0);

  bool should_skip_an_instruction_after_recovery = cmd.type == FE_RECOVER_AFTER;
  pintool_state.set_next_state_for_changing_control_flow(
    &checkpoint.ctxt, /*redirect_rip=*/false, 0,
    should_skip_an_instruction_after_recovery);

  /*
  if(!on_wrongpath_nop_mode) {
  } else {
    const auto prev_eip = PIN_GetContextReg(&checkpoints[idx].ctxt,
                                            REG_INST_PTR);
    const auto this_eip = checkpoints[idx].wpnm_eip;
    next_eip = ADDR_MASK(cmd.type == FE_RECOVER_AFTER ? this_eip : prev_eip);
    wpnm_skip_ckp = true;
  }
  */
}

void do_fe_redirect(Scarab_To_Pin_Msg& cmd, CONTEXT* ctxt) {
  DBG_PRINT(
    pintool_state.get_curr_inst_uid(), dbg_print_start_uid, dbg_print_end_uid,
    "redirect curr_uid=%" PRIu64 ", target_uid=%" PRIu64 ", target_eip=%llx\n",
    pintool_state.get_curr_inst_uid(), cmd.inst_uid, cmd.inst_addr);

  scarab_clear_all_buffers();
  undo_checkpoints_until_one_after_uid(cmd.inst_uid);

  pintool_state.set_next_state_for_changing_control_flow(
    &checkpoints.get_tail().ctxt,
    /*redirect=*/true, cmd.inst_addr,
    /*skip_next_instruction=*/false);

  undo_mem(checkpoints.get_tail());
  checkpoints.remove_from_cir_buf_tail();

  const ProcState& checkpoint = checkpoints.get_tail();
  ASSERTM(0, !checkpoint.is_syscall,
          "Unexpected Redirect to current syscall inst @uid=%" PRIu64 "\n",
          checkpoint.uid);
  ASSERTM(0, !checkpoint.wrongpath_nop_mode,
          "Cannot redirect a dummy instruction created in Wrongpath NOP mode");

  // redirect by definition switches to wrongpath
  pintool_state.set_wrongpath(true);

  if(!instrumented_rip_tracker.contains(cmd.inst_addr)) {
    DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
              dbg_print_end_uid,
              "Entering from redirect WPNM targetaddr=%" PRIx64 "\n",
              (uint64_t)cmd.inst_addr);
    pintool_state.set_wrongpath_nop_mode(
      WPNM_REASON_REDIRECT_TO_NOT_INSTRUMENTED, cmd.inst_addr);
    pintool_state.clear_changing_control_flow();
  }
}

void buffer_next_instruction(CONTEXT* ctxt, compressed_op* cop,
                             Mem_Writes_Info mem_writes_info, bool is_syscall,
                             bool is_exit_syscall, bool wrongpath_nop_mode) {
  const auto inst_uid = pintool_state.get_next_inst_uid();
  cop->inst_uid       = inst_uid;

  checkpoints.append_to_cir_buf();

  checkpoints.get_tail().update(
    ctxt, inst_uid, false, pintool_state.is_on_wrongpath(), wrongpath_nop_mode,
    cop->instruction_addr + cop->size, mem_writes_info, is_syscall);

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

  if(pintool_state.is_on_wrongpath()) {
    trigger_wrongpath_nop_mode_if_writing_to_new_region(
      mem_writes_info, cop->instruction_addr + cop->size);
  } else {
    record_written_regions(mem_writes_info);
  }

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
      do_fe_recover(cmd, ctxt);
      return;
    } else if(cmd.type == FE_REDIRECT) {
      do_fe_redirect(cmd, ctxt);
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
              "Scarab sent a fetch command after a syscall was fetched and "
              "before the syscall was retired");
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
  buffer_next_instruction(ctxt, pin_decoder_get_latest_inst(), mem_writes_info,
                          is_syscall, is_exit_syscall, false);

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

void wrongpath_nop_mode_main_loop() {
  CONTEXT         dummy_ctxt;
  Mem_Writes_Info empty_write_info;
  uint64_t        next_rip = pintool_state.get_next_rip();

  while(true) {
    compressed_op dummy_nop = create_dummy_nop(
      next_rip, pintool_state.get_wrongpath_nop_mode_reason());
    buffer_next_instruction(&dummy_ctxt, &dummy_nop, empty_write_info, false,
                            false, true);
    if(scarab_buffer_full()) {
      wait_until_scarab_sends_fetch_op(&dummy_ctxt);
      DBG_PRINT(pintool_state.get_curr_inst_uid(), dbg_print_start_uid,
                dbg_print_end_uid, "============ %d =============\n",
                (int)pintool_state.skip_further_processing());
      if(pintool_state.skip_further_processing()) {
        return;
      }
      scarab_send_buffer();
    }

    ++next_rip;
    if(!next_rip)
      next_rip = 0;
  }
}