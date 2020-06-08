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

#ifndef PIN_EXEC_ANALYSIS_FUNCTIONS_H__
#define PIN_EXEC_ANALYSIS_FUNCTIONS_H__

#include "globals.h"

void PIN_FAST_ANALYSIS_CALL docount(UINT32 c);

void process_syscall(ADDRINT ip, ADDRINT num, ADDRINT arg0, ADDRINT arg1,
                     ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5,
                     CONTEXT* ctxt, bool real_syscall);

void save_context(CONTEXT* ctxt);

void check_if_region_written_to(ADDRINT write_addr);

void save_mem(ADDRINT write_addr, UINT32 write_size, UINT write_index);

void undo_mem(const ProcState& undo_state);

void finish_before_ins_all(CONTEXT* ctxt, bool from_syscall);

void add_right_path_exec_br(CONTEXT* ctxt);

void before_ins_no_mem(CONTEXT* ctxt);

void before_ins_one_mem(CONTEXT* ctxt, ADDRINT write_addr, UINT32 write_size);

void before_ins_multi_mem(CONTEXT*                   ctxt,
                          PIN_MULTI_MEM_ACCESS_INFO* mem_access_info_from_pin,
                          bool                       is_scatter);

void redirect(CONTEXT* ctx);

void logging(ADDRINT n_eip, ADDRINT curr_eip, BOOL check_next_addr, BOOL taken);

void check_ret_control_ins(ADDRINT read_addr, UINT32 read_size, CONTEXT* ctxt);

void check_nonret_control_ins(BOOL taken, ADDRINT target_addr);

void check_nonret_control_mem_target(BOOL taken, ADDRINT addr, UINT32 ld_size);

void handle_scarab_marker(ADDRINT op);

#endif  // PIN_EXEC_ANALYSIS_FUNCTIONS_H__