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

#if defined(TARGET_IA32E)
#define EXIT_SYSCALL_NUM1 231
#define EXIT_SYSCALL_NUM2 60
#else
#define EXIT_SYSCALL_NUM1 1
#define EXIT_SYSCALL_NUM2 1
#endif

void is_syscall(ADDRINT ip, ADDRINT num, ADDRINT arg0, ADDRINT arg1,
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

void save_context(CONTEXT* ctxt) {
  PIN_SaveContext(ctxt, &checkpoints.get_tail().ctxt);
}

void check_if_region_written_to(ADDRINT write_addr) {
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

void save_mem(ADDRINT write_addr, UINT32 write_size, UINT write_index) {
#ifndef ASSUME_PERFECT
  write_addr = ADDR_MASK(write_addr);

  check_if_region_written_to(write_addr);

  checkpoints.get_tail().mem_state_list[write_index].init(write_addr,
                                                          write_size);
  PIN_SafeCopy(checkpoints.get_tail().mem_state_list[write_index].mem_data_ptr,
               (void*)write_addr, write_size);
#endif
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

void add_right_path_exec_br(CONTEXT* ctxt) {
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

void before_ins_no_mem(CONTEXT* ctxt) {
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

void before_ins_one_mem(CONTEXT* ctxt, ADDRINT write_addr, UINT32 write_size) {
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

void before_ins_multi_mem(CONTEXT*                   ctxt,
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

void redirect(CONTEXT* ctx) {
  std::cout << "Inside redirect analysis\n";
  started = true;
  std::cout << "About to redirect to " << std::hex << start_rip << "\n";
  PIN_SetContextRegval(ctx, REG_INST_PTR, (const UINT8*)(&start_rip));
  PIN_RemoveInstrumentation();
  PIN_ExecuteAt(ctx);
}

void logging(ADDRINT n_eip, ADDRINT curr_eip, BOOL check_next_addr,
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
    if(heartbeat_enabled && !(uid_ctr & 0x7FFFF)) {
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

void check_ret_control_ins(ADDRINT read_addr, UINT32 read_size, CONTEXT* ctxt) {
  read_addr = ADDR_MASK(read_addr);
  if(!fast_forward_count) {
    ASSERTM(0, read_size <= 8,
            "RET pops more than 8 bytes off the stack as ESP: %" PRIx64
            ", size: %u\n",
            (uint64_t)read_addr, read_size);
#ifndef ASSUME_PERFECT
    char  buf[8];
    void* prev_data = buf;
    PIN_SafeCopy(prev_data, (void*)read_addr, read_size);
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

void check_nonret_control_ins(BOOL taken, ADDRINT target_addr) {
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

void check_nonret_control_mem_target(BOOL taken, ADDRINT addr, UINT32 ld_size) {
  addr = ADDR_MASK(addr);
  if(!fast_forward_count) {
    ADDRINT target_addr;
    if(ld_size != 8) {
      target_addr = 0;
    } else {
#ifndef ASSUME_PERFECT
      PIN_SafeCopy(&target_addr, (void*)addr, ld_size);
#endif
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
      *out << "Error: Found Scarab Marker that does not have known code."
           << endl;
  }
}
