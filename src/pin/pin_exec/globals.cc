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

#include "globals.h"

std::ostream* out = &cerr;

Address_Tracker instrumented_rip_tracker;

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
ADDRINT                   pending_magic_inst        = NOT_MAGIC;
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

// Commandline arguments
bool     heartbeat_enabled;
uint32_t max_buffer_size;
uint64_t start_rip;