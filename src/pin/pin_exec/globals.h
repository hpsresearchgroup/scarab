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

#ifndef PIN_EXEC_GLOBALS_H__
#define PIN_EXEC_GLOBALS_H__

#include <iostream>
#include <unordered_map>

#undef UNUSED
#undef WARNING

#include "pin.H"

#undef UNUSED
#undef WARNING

#include "../pin_lib/message_queue_interface_lib.h"
#include "read_mem_map.h"
#include "utils.h"

// Global Variables
extern std::ostream* out;

extern Address_Tracker instrumented_rip_tracker;

const int checkpoints_init_capacity = 512;
extern CirBuf<ProcState, checkpoints_init_capacity> checkpoints;

extern UINT64 uid_ctr;
extern UINT64 dbg_print_start_uid;
extern UINT64 dbg_print_end_uid;
extern UINT64 heartbeat;

extern CONTEXT last_ctxt;
extern ADDRINT next_eip;

extern Client*                   scarab;
extern ScarabOpBuffer_type       scarab_op_buffer;
extern compressed_op             op_mailbox;
extern bool                      op_mailbox_full;
extern bool                      pending_fetch_op;
extern bool                      pending_syscall;
extern bool                      pending_exception;
extern ADDRINT                   pending_magic_inst;
extern bool                      on_wrongpath;
extern bool                      on_wrongpath_nop_mode;
extern Wrongpath_Nop_Mode_Reason wrongpath_nop_mode_reason;
extern bool                      generate_dummy_nops;
extern bool                      wpnm_skip_ckp;
extern bool                      entered_wpnm;
extern bool                      exit_syscall_found;
extern bool                      buffer_sentinel;
extern bool                      started;

extern pageTableStruct* page_table;

// Excpetion handling
extern bool              seen_rightpath_exc_mode;
extern ADDRINT           saved_excp_eip;
extern ADDRINT           saved_excp_next_eip;
extern Scarab_To_Pin_Msg saved_cmd;
extern bool              excp_rewind_msg;
extern bool              found_syscall;
extern bool              excp_ff;

// TODO_b: this name could be better?
extern bool     fast_forward_to_pin_start;
extern uint64_t total_ff_count;
extern bool     hyper_ff;
extern int64_t  hyper_fast_forward_delta;
extern int64_t  hyper_fast_forward_count;
extern int64_t  orig_hyper_fast_forward_count;

// Commandline arguments
extern bool     heartbeat_enabled;
extern uint32_t max_buffer_size;
extern uint64_t start_rip;


#endif  // PIN_EXEC_GLOBALS_H__