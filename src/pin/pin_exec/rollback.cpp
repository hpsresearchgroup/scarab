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

#include "pin_fe_globals.h"
#include "read_mem_map.h"
#include "rollback_structs.h"

// scarab files
#include "../pin_lib/decoder.h"
#include "../pin_lib/message_queue_interface_lib.h"
#include "../pin_lib/pin_scarab_common_lib.h"

#define ADDR_MASK(x) ((x)&0x0000FFFFFFFFFFFFULL)

using std::unordered_map;

/* ===================================================================== */
/* Globals */
std::ostream* out = &cerr;

unordered_map<ADDRINT, bool> instrumented_eips;

const int                                    checkpoints_init_capacity = 512;
CirBuf<ProcState, checkpoints_init_capacity> checkpoints =
  CirBuf<ProcState, checkpoints_init_capacity>();

UINT64 uid_ctr = 0;
UINT64 dbg_print_start_uid;
UINT64 dbg_print_end_uid;
UINT64 heartbeat = 0;

CONTEXT last_ctxt;
ADDRINT next_eip;

Client*                   scarab;
ScarabOpBuffer_type       scarab_op_buffer;
compressed_op             op_mailbox;
bool                      op_mailbox_full           = false;
bool                      pending_fetch_op          = false;
bool                      pending_syscall           = false;
bool                      pending_exception         = false;
bool                      on_wrongpath              = false;
bool                      on_wrongpath_nop_mode     = false;
Wrongpath_Nop_Mode_Reason wrongpath_nop_mode_reason = WPNM_NOT_IN_WPNM;
bool                      generate_dummy_nops       = false;
bool                      wpnm_skip_ckp             = false;
bool                      entered_wpnm              = false;
bool                      exit_syscall_found        = false;
bool                      buffer_sentinel           = false;
bool                      started                   = false;

pageTableStruct* page_table;

// Excpetion handling
bool              seen_rightpath_exc_mode = false;
ADDRINT           saved_excp_eip          = 0;
ADDRINT           saved_excp_next_eip     = 0;
Scarab_To_Pin_Msg saved_cmd;
bool              excp_rewind_msg = false;
bool              found_syscall   = false;
bool              excp_ff         = false;

// TODO_b: this name could be better?
bool     fast_forward_to_pin_start = false;
uint64_t total_ff_count            = 0;
bool     hyper_ff                  = false;
int64_t  hyper_fast_forward_delta  = 1000000;
int64_t  hyper_fast_forward_count;
int64_t  orig_hyper_fast_forward_count;

// Test variables
UINT64  test_recover_uid;
ADDRINT saved_eip;

/* ===================================================================== */
/* Prototypes */
VOID main_loop(CONTEXT* ctxt);
VOID retire_older_checkpoints(UINT64 uid);
VOID recover_to_past_checkpoint(UINT64 uid, bool is_redirect_recover,
                                bool enter_ff);
VOID redirect_to_inst(ADDRINT inst_addr, CONTEXT* ctxt, UINT64 uid);
VOID HandleScarabMarkers(THREADID tid, ADDRINT op);

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
  KNOB_MODE_WRITEONCE, "pintool", "max_buffer_size", "32",
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
KNOB<UINT64> KnobStartEip(KNOB_MODE_WRITEONCE, "pintool", "rip", "0",
                          "the starting rip of the program");

/* ===================================================================== */
/* ===================================================================== */

INT32 Usage() {
  cerr << "Pintool based exec frontend for scarab simulator" << endl << endl;
  cerr << KNOB_BASE::StringKnobSummary() << endl;
  return -1;
}

VOID save_context(CONTEXT* ctxt) {
  PIN_SaveContext(ctxt, &checkpoints.get_tail().ctxt);
}

VOID check_if_region_written_to(ADDRINT write_addr) {
  pageTableEntryStruct* p_entry = NULL;
  if(on_wrongpath) {
    if(page_table->get_entry(write_addr, &p_entry)) {
      if(!p_entry->writtenToOnRightPath) {
        if(p_entry->permissions & 2) {
          // here is where we detect it's a wrong path
          // write to a previously unwritten region
          on_wrongpath_nop_mode = true;
          wrongpath_nop_mode_reason =
            WPNM_REASON_WRONG_PATH_STORE_TO_NEW_REGION;
        }
      }
    }
  } else {
    bool hitInPageTable = page_table->get_entry(write_addr, &p_entry);
    if(!hitInPageTable) {
      pageTableStruct* new_page_table = new pageTableStruct();
      update_page_table(new_page_table);

      hitInPageTable = new_page_table->get_entry(write_addr, &p_entry);
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
  }
}

/* ===================================================================== */

VOID save_mem(ADDRINT write_addr, UINT32 write_size, UINT write_index) {
#ifndef ASSUME_PERFECT
  write_addr = ADDR_MASK(write_addr);

  check_if_region_written_to(write_addr);

  checkpoints.get_tail().mem_state_list[write_index].init(write_addr,
                                                          write_size);
  PIN_SafeCopy(checkpoints.get_tail().mem_state_list[write_index].mem_data_ptr,
               (VOID*)write_addr, write_size);
#endif
}

/* ===================================================================== */

VOID undo_mem(const ProcState& undo_state) {
  for(uint i = 0; i < undo_state.num_mem_state; i++) {
    VOID*  write_addr = (VOID*)undo_state.mem_state_list[i].mem_addr;
    UINT32 write_size = undo_state.mem_state_list[i].mem_size;
    VOID*  prev_data  = undo_state.mem_state_list[i].mem_data_ptr;
    PIN_SafeCopy(write_addr, prev_data, write_size);
  }
}

void finish_before_ins_all(CONTEXT* ctxt, bool from_syscall) {
  entered_wpnm = false;
  while(on_wrongpath_nop_mode) {
    entered_wpnm = true;
    DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
              "WPNM Curr uid=%" PRIu64 ", wrongpath=%d\n", uid_ctr,
              on_wrongpath);
    generate_dummy_nops = true;
    main_loop(ctxt);
    if(!wpnm_skip_ckp) {
      next_eip = ADDR_MASK(next_eip);

      checkpoints.append_to_cir_buf();
      checkpoints.get_tail().init(uid_ctr, false, on_wrongpath,
                                  on_wrongpath_nop_mode, next_eip, 0);
      uid_ctr++;
      save_context(ctxt);
      PIN_SetContextRegval(&(checkpoints.get_tail().ctxt), REG_INST_PTR,
                           (const UINT8*)(&next_eip));
      next_eip = ADDR_MASK(next_eip + 1);
    }
    wpnm_skip_ckp = false;
  }
  if(entered_wpnm) {
    ASSERTM(0, false, "Entered WPNM, but did not recover (uid=%" PRIu64 ")\n",
            uid_ctr);
  }
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "Falling into application\n");
}

/* ===================================================================== */

VOID add_right_path_exec_br(CONTEXT* ctxt) {
  // Create dummy jmp
  ADDRINT eip;
  PIN_GetContextRegval(ctxt, REG_INST_PTR, (UINT8*)&eip);
  compressed_op cop = create_dummy_jump(saved_excp_next_eip, eip);
  cop.inst_uid      = uid_ctr;
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "Prev EIPs %lx, %lx\n", saved_excp_eip, saved_excp_next_eip);
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "At EIP %" PRIx64 "\n", eip);

  // Mailbox will be empty as we clear it before a rightpath excpetion
  ASSERTM(0, !op_mailbox_full,
          "Expected empty mailbox for rightpath exception op @ %" PRIu64 ".\n",
          uid_ctr);

  // Insert in mailbox
  op_mailbox      = cop;
  op_mailbox_full = true;

  // Save checkpoint
  checkpoints.append_to_cir_buf();
  checkpoints.get_tail().init(uid_ctr, false, on_wrongpath,
                              on_wrongpath_nop_mode, next_eip, 0);
  uid_ctr++;
#ifndef ASSUME_PERFECT
  save_context(ctxt);
#endif
}

/* ===================================================================== */

VOID before_ins_no_mem(CONTEXT* ctxt) {
  if(!fast_forward_count) {
    if(seen_rightpath_exc_mode) {
      add_right_path_exec_br(ctxt);
      seen_rightpath_exc_mode = false;
      saved_excp_eip          = 0;
    }

    main_loop(ctxt);

    next_eip = ADDR_MASK(next_eip);

    checkpoints.append_to_cir_buf();
    checkpoints.get_tail().init(uid_ctr, false, on_wrongpath,
                                on_wrongpath_nop_mode, next_eip, 0);
    uid_ctr++;
#ifndef ASSUME_PERFECT
    save_context(ctxt);
#endif
    finish_before_ins_all(ctxt, false);
  }
}

/* ===================================================================== */

VOID before_ins_one_mem(CONTEXT* ctxt, ADDRINT write_addr, UINT32 write_size) {
  write_addr = ADDR_MASK(write_addr);
  if(!fast_forward_count) {
    if(seen_rightpath_exc_mode) {
      add_right_path_exec_br(ctxt);
      seen_rightpath_exc_mode = false;
      saved_excp_eip          = 0;
    }

    main_loop(ctxt);

    next_eip = ADDR_MASK(next_eip);

    checkpoints.append_to_cir_buf();
    checkpoints.get_tail().init(uid_ctr, false, on_wrongpath,
                                on_wrongpath_nop_mode, next_eip, 1);
    uid_ctr++;
#ifndef ASSUME_PERFECT
    save_context(ctxt);
#endif
    save_mem(write_addr, write_size, 0);
    finish_before_ins_all(ctxt, false);
  }
}

/* ===================================================================== */

VOID before_ins_multi_mem(CONTEXT*                   ctxt,
                          PIN_MULTI_MEM_ACCESS_INFO* mem_access_info) {
  if(!fast_forward_count) {
    if(seen_rightpath_exc_mode) {
      add_right_path_exec_br(ctxt);
      seen_rightpath_exc_mode = false;
      saved_excp_eip          = 0;
    }

    main_loop(ctxt);

    next_eip = ADDR_MASK(next_eip);

    UINT32 numMemOps = mem_access_info->numberOfMemops;

    checkpoints.append_to_cir_buf();
    checkpoints.get_tail().init(uid_ctr, false, on_wrongpath,
                                on_wrongpath_nop_mode, next_eip, numMemOps);
    uid_ctr++;
#ifndef ASSUME_PERFECT
    save_context(ctxt);
#endif
    for(UINT32 i = 0; i < numMemOps; i++) {
      ADDRINT write_addr = ADDR_MASK(mem_access_info->memop[i].memoryAddress);
      UINT32  write_size = mem_access_info->memop[i].bytesAccessed;

      save_mem(write_addr, write_size, i);
    }
    finish_before_ins_all(ctxt, false);
  }
}

/* ===================================================================== */

VOID is_syscall(ADDRINT ip, ADDRINT num, ADDRINT arg0, ADDRINT arg1,
                ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5,
                CONTEXT* ctxt, bool real_syscall) {
  if(!fast_forward_count) {
    pending_syscall = true;
    if(real_syscall &&
       ((EXIT_SYSCALL_NUM1 == num) || (EXIT_SYSCALL_NUM2 == num))) {
      exit_syscall_found = true;
    }

    next_eip = ADDR_MASK(next_eip);

    checkpoints.append_to_cir_buf();
    checkpoints.get_tail().init(uid_ctr, false, on_wrongpath,
                                on_wrongpath_nop_mode, next_eip, 0);
#ifndef ASSUME_PERFECT
    save_context(ctxt);
#endif
    // Because syscalls uniquely are sent to scarab BEFORE their execution,
    // we do NOT update the global uid_ctr until the syscall compressed_op
    // is actually created and buffered for sending.

    main_loop(ctxt);
    finish_before_ins_all(ctxt, true);
  }
}

Scarab_To_Pin_Msg get_scarab_cmd() {
  Scarab_To_Pin_Msg cmd;

  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "START: Receiving from Scarab\n");
  cmd = scarab->receive<Scarab_To_Pin_Msg>();
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "END: %d Received from Scarab\n", cmd.type);

  return cmd;
}

void insert_scarab_op_in_buffer(compressed_op& cop) {
  scarab_op_buffer.push_back(cop);
}

bool scarab_buffer_full() {
  return scarab_op_buffer.size() > (KnobMaxBufferSize - 2);
  // Two spots are always reserved in the buffer just in case the
  // exit syscall and sentinel nullop are
  // the last two elements of a packet sent to Scarab.
}

void scarab_send_buffer() {
  Message<ScarabOpBuffer_type> message = scarab_op_buffer;
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "START: Sending message to Scarab.\n");
  scarab->send(message);
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "END: Sending message to Scarab.\n");
  scarab_op_buffer.clear();
}

void scarab_clear_all_buffers() {
  scarab_op_buffer.clear();
  op_mailbox_full = false;
}

/* ===================================================================== */

bool do_fe_recover(Scarab_To_Pin_Msg& cmd, CONTEXT* ctxt) {
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "recover curr_uid=%" PRIu64 ", target_uid=%" PRIu64 "\n", uid_ctr,
            cmd.inst_uid);
  if(pending_syscall && cmd.inst_uid == (uid_ctr - 1)) {
    ASSERTM(0, false,
            "Unexpected Recover to current syscall inst @uid=%" PRIu64 "\n",
            uid_ctr - 1);
  }
  seen_rightpath_exc_mode = false;
  pending_syscall         = false;
  pending_exception       = false;
  buffer_sentinel         = false;
  bool enter_ff           = false;
  if(cmd.type == FE_RECOVER_BEFORE) {
    enter_ff = false;
  } else if(cmd.type == FE_RECOVER_AFTER) {
    enter_ff = true;
  } else {
    ASSERTM(0, false, "Unknown Recover Type (uid=%" PRIu64 ")\n", uid_ctr - 1);
  }
  PIN_SaveContext(ctxt, &last_ctxt);
  recover_to_past_checkpoint(cmd.inst_uid, false, enter_ff);
  if(on_wrongpath_nop_mode) {
    wpnm_skip_ckp = true;
  } else {
    ASSERTM(0, false,
            "Recover cmd did not change execution (uid=%" PRIu64 ")\n",
            uid_ctr);
  }
  return on_wrongpath_nop_mode;
}

bool do_fe_redirect(Scarab_To_Pin_Msg& cmd, CONTEXT* ctxt) {
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "redirect curr_uid=%" PRIu64 ", target_uid=%" PRIu64
            ", target_eip=%llx\n",
            uid_ctr, cmd.inst_uid, cmd.inst_addr);
  if(pending_syscall && cmd.inst_uid == (uid_ctr - 1)) {
    ASSERTM(0, false,
            "Unexpected Redirect to current syscall inst @uid=%" PRIu64 "\n",
            uid_ctr - 1);
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
            uid_ctr);
  }
  return on_wrongpath_nop_mode;
}

bool do_fe_retire(Scarab_To_Pin_Msg& cmd) {
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "retire curr_uid=%" PRIu64 ", target_uid=%" PRIu64 ", addr=%llx\n",
            uid_ctr, cmd.inst_uid, cmd.inst_addr);
  if(cmd.inst_addr) {
    // A non-zero inst address in a retire command signifies to early exit the
    // app.
    PIN_ExitApplication(0);
    ASSERTM(0, false,
            "PIN could not safely terminate app upon scarab request.\n");
  }
  retire_older_checkpoints(cmd.inst_uid);
  return buffer_sentinel && checkpoints.empty();  // PIN recvd termination
                                                  // retire
}

VOID do_fe_fetch_op(bool& syscall_has_been_sent_to_scarab) {
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "fetch curr_uid=%" PRIu64 "\n", uid_ctr);
  pending_fetch_op = true;

  if(syscall_has_been_sent_to_scarab) {
    // syscall is finished when we recieve the first FETCH_OP after sending the
    // syscall to scarab
    ASSERTM(0, checkpoints.empty(),
            "Scarab has not retired all instructions since receiving a Sys "
            "Call (uid=%" PRIu64 ")\n",
            uid_ctr - 1);
    pending_syscall   = false;
    pending_exception = false;
  }
}

VOID do_fe_null(bool& have_consumed_op) {
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "fenull curr_uid=%" PRIu64 "\n", uid_ctr);
  if(!have_consumed_op) {
    compressed_op* cop;
    compressed_op  dummy_nop;
    if(!pending_exception) {
      if(generate_dummy_nops) {
        next_eip  = ADDR_MASK(next_eip);
        dummy_nop = create_dummy_nop((uint64_t)next_eip,
                                     wrongpath_nop_mode_reason);
        cop       = &dummy_nop;
      } else {
        cop = pin_decoder_get_latest_inst();
        // hack to get SYSENTER (which is only used for 32bit
        // binaries) to work. Basically, because of the way
        // SYSENTER is used by Linux, control will return to
        // the user code 11 bytes after the SYSENTER
        // instruction, even though SYSENTER is only a 2 byte
        // instruction...
        if(strcmp(cop->pin_iclass, "SYSENTER") == 0) {
          cop->size = 11;
        }
        DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
                  "consuming instruction eip:%" PRIx64
                  ", opcode:%s, ifetch_barrier:%u, cf_type:%u, op_type:%u, "
                  "num_ld: %u, num_st: %u, exit: %u, ins_size: %u\n",
                  cop->instruction_addr, cop->pin_iclass,
                  cop->is_ifetch_barrier, cop->cf_type, cop->op_type,
                  cop->num_ld, cop->num_st, cop->exit, cop->size);
      }
      cop->inst_uid = uid_ctr;
      if(pending_syscall && exit_syscall_found) {
        exit_syscall_found = false;
        buffer_sentinel    = true;
      }
    } else {
      ASSERTM(0, !generate_dummy_nops,
              "Dummy NOP generated exception @uid %" PRIu64 ".\n", uid_ctr);
      ASSERTM(0, op_mailbox_full,
              "Expected full mailbox for exc @ %" PRIu64 ".\n", uid_ctr);
      cop                    = &op_mailbox;
      op_mailbox_full        = false;
      cop->cf_type           = CF_SYS;
      cop->is_ifetch_barrier = 1;
      // cop->fake_inst = 1;
      cop->instruction_next_addr = saved_excp_next_eip;
    }

    if(op_mailbox_full) {
      op_mailbox.instruction_next_addr = cop->instruction_addr;
      if(op_mailbox.cf_type && op_mailbox.actually_taken) {
        op_mailbox.branch_target = cop->instruction_addr;
      }
      insert_scarab_op_in_buffer(op_mailbox);
      op_mailbox_full = false;
    }

    if(pending_syscall) {
      // bypass mailbox
      if(buffer_sentinel) {
        cop->exit = 1;
      }
      insert_scarab_op_in_buffer(*cop);
      if(buffer_sentinel) {
        compressed_op sentinel_cop = create_sentinel();
        insert_scarab_op_in_buffer(sentinel_cop);
      }
      if(!pending_exception) {
        // only syscalls create checkpoints BEFORE being sent,
        // so we update uid_ctr here to prep. for next instruction
        uid_ctr++;
      }
    } else {
      op_mailbox      = *cop;
      op_mailbox_full = true;
    }
    have_consumed_op = true;
  }
}

// Communicates with scarab, and performs the requested actions
VOID main_loop(CONTEXT* ctxt) {
  bool buffer_ready                    = false;
  bool syscall_has_been_sent_to_scarab = false;
  bool have_consumed_op                = false;
  bool need_scarab_cmd                 = false;

  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "main loop next_eip=%" PRIx64 "\n", (uint64_t)next_eip);

  while(true) {
    Scarab_To_Pin_Msg cmd;
    cmd.type = FE_NULL;
    if(excp_rewind_msg) {
      excp_rewind_msg = false;
      cmd             = saved_cmd;
    } else if(need_scarab_cmd) {
      cmd = get_scarab_cmd();
    }

    if(cmd.type == FE_RECOVER_BEFORE || cmd.type == FE_RECOVER_AFTER) {
      if(do_fe_recover(cmd, ctxt)) {
        break;
      }
    } else if(cmd.type == FE_REDIRECT) {
      if(do_fe_redirect(cmd, ctxt)) {
        break;
      }
    } else if(cmd.type == FE_RETIRE) {
      if(do_fe_retire(cmd)) {
        break;
      }
    } else if(cmd.type == FE_FETCH_OP) {
      do_fe_fetch_op(syscall_has_been_sent_to_scarab);
    } else if(cmd.type == FE_NULL) {
      do_fe_null(have_consumed_op);
    }

    buffer_ready               = scarab_buffer_full() || pending_syscall;
    bool send_buffer_to_scarab = buffer_ready && pending_fetch_op &&
                                 have_consumed_op;
    if(send_buffer_to_scarab) {
      scarab_send_buffer();
      pending_fetch_op                = false;
      syscall_has_been_sent_to_scarab = pending_syscall;
    }

    if(have_consumed_op && !pending_syscall && !scarab_buffer_full())
      break;

    if(have_consumed_op)
      need_scarab_cmd = true;
  }
}


/* ===================================================================== */

VOID redirect_to_inst(ADDRINT inst_addr, CONTEXT* ctxt, UINT64 uid) {
  PIN_SaveContext(ctxt, &last_ctxt);
  recover_to_past_checkpoint(uid, true, false);
  scarab_clear_all_buffers();
  PIN_SetContextRegval(&last_ctxt, REG_INST_PTR, (const UINT8*)(&inst_addr));

  on_wrongpath = true;  // redirect ALWAYS sets to wrongpath==true
  if(on_wrongpath && instrumented_eips.count(inst_addr) == 0) {
    on_wrongpath_nop_mode     = true;
    wrongpath_nop_mode_reason = WPNM_REASON_REDIRECT_TO_NOT_INSTRUMENTED;
    DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
              "Entering from redirect WPNM targetaddr=%" PRIx64 "\n",
              (uint64_t)inst_addr);
  }
  if(on_wrongpath_nop_mode) {
    next_eip = ADDR_MASK(inst_addr);
  } else {
    PIN_ExecuteAt(&last_ctxt);
    ASSERTM(0, false, "PIN_ExecuteAt did not redirect execution.\n");
  }
}

/* ===================================================================== */

VOID recover_to_past_checkpoint(UINT64 uid, bool is_redirect_recover,
                                bool enter_ff) {
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

/* ===================================================================== */

// Retire all instrution upto and including uid
VOID retire_older_checkpoints(UINT64 uid) {
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

VOID check_ret_control_ins(ADDRINT read_addr, UINT32 read_size, CONTEXT* ctxt) {
  read_addr = ADDR_MASK(read_addr);
  if(!fast_forward_count) {
    ASSERTM(0, read_size <= 8,
            "RET pops more than 8 bytes off the stack as ESP: %" PRIx64
            ", size: %u\n",
            (uint64_t)read_addr, read_size);
#ifndef ASSUME_PERFECT
    char  buf[8];
    VOID* prev_data = buf;
    PIN_SafeCopy(prev_data, (VOID*)read_addr, read_size);
    ADDRINT target_addr = *((UINT64*)prev_data);
    DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
              "Ret Control targetaddr=%" PRIx64 "\n", (uint64_t)target_addr);

    if(on_wrongpath && (instrumented_eips.count(target_addr) == 0)) {
      DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
                "Entering from ret WPNM targetaddr=%" PRIx64 "\n",
                (uint64_t)target_addr);
      on_wrongpath_nop_mode     = true;
      wrongpath_nop_mode_reason = WPNM_REASON_RETURN_TO_NOT_INSTRUMENTED;
      target_addr = ADDR_MASK(target_addr);  // 48 bit canonical VA
      if(!target_addr) {
        next_eip = 1;
      } else {
        next_eip = ADDR_MASK(target_addr);
      }
    }
#endif
  }
}

VOID check_nonret_control_ins(BOOL taken, ADDRINT target_addr) {
  target_addr = ADDR_MASK(target_addr);
  if(!fast_forward_count) {
    DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
              "Non Ret Control targetaddr=%" PRIx64 "\n",
              (uint64_t)target_addr);
    if(on_wrongpath && taken && (instrumented_eips.count(target_addr) == 0)) {
      DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
                "Entering from nonret WPNM targetaddr=%" PRIx64 "\n",
                (uint64_t)target_addr);
      on_wrongpath_nop_mode     = true;
      wrongpath_nop_mode_reason = WPNM_REASON_NONRET_CF_TO_NOT_INSTRUMENTED;
      target_addr = ADDR_MASK(target_addr);  // 48 bit canonical VA
      if(!target_addr) {
        next_eip = 1;
      } else {
        next_eip = ADDR_MASK(target_addr);
      }
    }
  }
}

VOID check_nonret_control_mem_target(BOOL taken, ADDRINT addr, UINT32 ld_size) {
  addr = ADDR_MASK(addr);
  if(!fast_forward_count) {
    ADDRINT target_addr;
    if(ld_size != 8) {
      target_addr = 0;
    } else {
#ifndef ASSUME_PERFECT
      PIN_SafeCopy(&target_addr, (VOID*)addr, ld_size);
#endif
    }
    check_nonret_control_ins(taken, target_addr);
  }
}

VOID logging(ADDRINT n_eip, ADDRINT curr_eip, BOOL check_next_addr,
             BOOL taken) {
  static bool first = true;
  if(fast_forward_count) {
    if(!(fast_forward_count & 0xFFFFF)) {
      *out << "Heartbeat: Fast Forwarding (ins. remain=" << fast_forward_count
           << ")" << endl;
    }
    fast_forward_count -= !fast_forward_to_pin_start;
    total_ff_count++;

    if(first && fast_forward_count == 0) {
      first = false;
      *out << "Exiting Fast Forward mode: inst_count=" << total_ff_count
           << endl;
    }
  }

  n_eip    = ADDR_MASK(n_eip);
  next_eip = ADDR_MASK(n_eip);

  if(!fast_forward_count) {
    if(KnobHeartbeatEnabled && !(uid_ctr & 0x7FFFF)) {
      *out << "Heartbeat (uid=" << uid_ctr << ")" << endl;
    }

    if(on_wrongpath && check_next_addr && !taken &&
       (0 == instrumented_eips.count(n_eip))) {
      // if we're currently on the wrong path and we somehow
      // about to come across an instruction that was never
      // instrumented, then go in WPNM right away to avoid the
      // possibility of PIN going off and instrumenting wrong
      // path code that might crash PIN
      on_wrongpath_nop_mode     = true;
      wrongpath_nop_mode_reason = WPNM_REASON_NOT_TAKEN_TO_NOT_INSTRUMENTED;
    }

    DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
              "Curr EIP=%" PRIx64 ", next EIP=%" PRIx64 ", Curr uid=%" PRIu64
              ", wrongpath=%d, wpnm=%d, instrumented=%zu\n",
              (uint64_t)curr_eip, (uint64_t)n_eip, uid_ctr, on_wrongpath,
              on_wrongpath_nop_mode, instrumented_eips.count(next_eip));
  }
}

#define SCARAB_MARKERS_PIN_BEGIN (1)
#define SCARAB_MARKERS_PIN_END (2)
VOID HandleScarabMarker(THREADID tid, ADDRINT op) {
  switch(op) {
    case SCARAB_MARKERS_PIN_BEGIN:
      fast_forward_to_pin_start = (fast_forward_count = 0);
      break;
    case SCARAB_MARKERS_PIN_END:
      fast_forward_to_pin_start = (fast_forward_count = 1);
      break;
    default:
      *out << "Error: Found Scarab Marker that does not have known code."
           << endl;
  }
}

void debug_analysis(uint32_t number) {
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "debug_analysis=%u\n", number);
}

VOID redirect(CONTEXT* ctx) {
  std::cout << "Inside redirect analysis\n";
  started     = true;
  ADDRINT rip = KnobStartEip.Value();
  std::cout << "About to redirect to " << std::hex << KnobStartEip.Value()
            << "\n";
  PIN_SetContextRegval(ctx, REG_INST_PTR, (const UINT8*)(&rip));
  PIN_RemoveInstrumentation();
  PIN_ExecuteAt(ctx);
}

/* ===================================================================== */

VOID Instruction(INS ins, VOID* v) {
  if(!started) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)redirect, IARG_CONTEXT,
                   IARG_END);
  } else {
    if(!hyper_ff) {
      instrumented_eips.insert(
        std::make_pair<ADDRINT, bool>(INS_Address(ins), true));

      DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
                "Instrument from Instruction() eip=%" PRIx64 "\n",
                (uint64_t)INS_Address(ins));
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

      if(INS_IsXchg(ins) && INS_OperandReg(ins, 0) == REG_GCX &&
         INS_OperandReg(ins, 1) == REG_GCX) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)HandleScarabMarker,
                       IARG_THREAD_ID, IARG_REG_VALUE, REG_ECX, IARG_END);
      }

      // Inserting functions to create a compressed op
      pin_decoder_insert_analysis_functions(ins);

      if(INS_IsSyscall(ins) || is_ifetch_barrier(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(is_syscall), IARG_INST_PTR,
                       IARG_SYSCALL_NUMBER, IARG_SYSARG_VALUE, 0,
                       IARG_SYSARG_VALUE, 1, IARG_SYSARG_VALUE, 2,
                       IARG_SYSARG_VALUE, 3, IARG_SYSARG_VALUE, 4,
                       IARG_SYSARG_VALUE, 5, IARG_CONTEXT, IARG_BOOL,
                       INS_IsSyscall(ins), IARG_END);
      } else {
        if(INS_IsRet(ins)) {
          INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(check_ret_control_ins),
                         IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_CONTEXT,
                         IARG_END);
        } else if(INS_IsBranchOrCall(ins)) {
          if(INS_IsDirectBranchOrCall(ins)) {
            if(INS_Category(ins) == XED_CATEGORY_COND_BR) {
              INS_InsertCall(
                ins, IPOINT_BEFORE, AFUNPTR(check_nonret_control_ins),
                IARG_BRANCH_TAKEN, IARG_ADDRINT,
                INS_DirectBranchOrCallTargetAddress(ins), IARG_END);
            } else {
              INS_InsertCall(
                ins, IPOINT_BEFORE, AFUNPTR(check_nonret_control_ins),
                IARG_BOOL, true, IARG_ADDRINT,
                INS_DirectBranchOrCallTargetAddress(ins), IARG_END);
            }
          } else if(INS_IsMemoryRead(ins)) {
            INS_InsertCall(ins, IPOINT_BEFORE,
                           AFUNPTR(check_nonret_control_mem_target), IARG_BOOL,
                           true, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
                           IARG_END);
          } else if(INS_MaxNumRRegs(ins) > 0) {
            INS_InsertCall(ins, IPOINT_BEFORE,
                           AFUNPTR(check_nonret_control_ins), IARG_BOOL, true,
                           IARG_REG_VALUE, INS_RegR(ins, 0), IARG_END);
          } else {
            // Force WPNM
            INS_InsertCall(ins, IPOINT_BEFORE,
                           AFUNPTR(check_nonret_control_mem_target), IARG_BOOL,
                           true, IARG_ADDRINT, 0, IARG_UINT32, 0, IARG_END);
          }
        }

        bool changes_mem = INS_IsMemoryWrite(ins);
        if(!changes_mem) {
          INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)before_ins_no_mem,
                         IARG_CONTEXT, IARG_END);
        } else {
          if(INS_hasKnownMemorySize(ins)) {
            // Single memory op
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)before_ins_one_mem,
                           IARG_CONTEXT, IARG_MEMORYWRITE_EA,
                           IARG_MEMORYWRITE_SIZE, IARG_END);
          } else {
            // Multiple memory ops
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)before_ins_multi_mem,
                           IARG_CONTEXT, IARG_MULTI_MEMORYACCESS_EA, IARG_END);
          }
        }
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

#define ENABLE_HYPER_FF_HEARTBEAT
VOID PIN_FAST_ANALYSIS_CALL docount(UINT32 c) {
  int64_t temp = hyper_fast_forward_count - c;
  // Saturating subtract: temp &= -(temp <= hyper_fast_forward_count);
  hyper_fast_forward_count = temp;

#ifdef ENABLE_HYPER_FF_HEARTBEAT
  total_ff_count += c;
  if(!(total_ff_count & 0x7FFFFFF0)) {
    *out << "Hyper FF Heartbeat: inst_count=" << total_ff_count << " ("
         << setprecision(2)
         << (100.0 *
             (orig_hyper_fast_forward_count - hyper_fast_forward_count)) /
              orig_hyper_fast_forward_count
         << "%)" << endl;
  }
#endif

  if(hyper_fast_forward_count <= 0) {
    hyper_ff = false;
    *out << "Exiting Hyper Fast Forward Mode." << endl;

    if(hyper_fast_forward_delta > 0) {
      fast_forward_count += (hyper_fast_forward_count +
                             hyper_fast_forward_delta);
      if(fast_forward_count > 0) {
        *out << "Entering Fast Forward Mode: " << fast_forward_count
             << " ins remaining" << endl;
      }
    }
    PIN_RemoveInstrumentation();
  }
}

VOID Trace(TRACE trace, VOID* v) {
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

VOID Fini(INT32 code, VOID* v) {
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "Fini reached, app exit code=%d\n.", code);
  *out << "End of program reached, disconnect from Scarab.\n" << endl;
  scarab->disconnect();
  *out << "Pintool Fini Reached.\n" << endl;
}


/* ===================================================================== */
/* ===================================================================== */

BOOL dummy_handler(THREADID tid, INT32 sig, CONTEXT* ctxt, BOOL hasHandler,
                   const EXCEPTION_INFO* pExceptInfo, VOID* v) {
#ifdef DEBUG_PRINT
  ADDRINT curr_eip;
  PIN_GetContextRegval(ctxt, REG_INST_PTR, (UINT8*)&curr_eip);
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "dummyhandler curr_uid=%" PRIu64 ", curr_eip=%" PRIx64 ", sig=%d\n",
            uid_ctr, (uint64_t)curr_eip, sig);
#endif

  return true;
}

// Main loop for rightpath execptions: any context change is delayed until we
// reach the execption handler
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

BOOL signal_handler(THREADID tid, INT32 sig, CONTEXT* ctxt, BOOL hasHandler,
                    const EXCEPTION_INFO* pExceptInfo, VOID* v) {
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


/* ===================================================================== */
/* ===================================================================== */

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
  started = (0 == KnobStartEip.Value());

  fast_forward_count = KnobFastForwardCount.Value();
  fast_forward_to_pin_start =
    (fast_forward_count = KnobFastForwardToStartInst.Value());
  hyper_fast_forward_count = KnobHyperFastForwardCount.Value() -
                             hyper_fast_forward_delta;
  orig_hyper_fast_forward_count = KnobHyperFastForwardCount.Value();

  dbg_print_start_uid = KnobDebugPrintStartUid.Value();
  dbg_print_end_uid   = KnobDebugPrintEndUid.Value();

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

  string fileName = KnobOutputFile.Value();

  if(!fileName.empty()) {
    out = new std::ofstream(fileName.c_str());
  }

  pin_decoder_init(true, out);

  // Intercept signals to see exceptions
  PIN_InterceptSignal(SIGFPE, signal_handler, 0);
  PIN_InterceptSignal(SIGILL, signal_handler, 0);
  PIN_InterceptSignal(SIGSEGV, signal_handler, 0);
  PIN_InterceptSignal(SIGBUS, signal_handler, 0);

  PIN_InterceptSignal(SIGHUP, dummy_handler, 0);
  PIN_InterceptSignal(SIGINT, dummy_handler, 0);
  PIN_InterceptSignal(SIGQUIT, dummy_handler, 0);
  // PIN_InterceptSignal(SIGILL   , dummy_handler, 0);
  PIN_InterceptSignal(SIGTRAP, dummy_handler, 0);
  PIN_InterceptSignal(SIGIOT, dummy_handler, 0);
  // PIN_InterceptSignal(SIGBUS   , dummy_handler, 0);
  // PIN_InterceptSignal(SIGFPE   , dummy_handler, 0);
  PIN_InterceptSignal(SIGKILL, dummy_handler, 0);
  PIN_InterceptSignal(SIGUSR1, dummy_handler, 0);
  // PIN_InterceptSignal(SIGSEGV  , dummy_handler, 0);
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

  // Register function to be called to instrument traces
  TRACE_AddInstrumentFunction(Trace, 0);
  INS_AddInstrumentFunction(Instruction, 0);

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
