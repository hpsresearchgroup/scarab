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

#ifndef PIN_EXEC_EXCEPTION_HANDLING_H__
#define PIN_EXEC_EXCEPTION_HANDLING_H__

#include "globals.h"

void register_signal_handlers();

bool dummy_handler(THREADID tid, INT32 sig, CONTEXT* ctxt, bool hasHandler,
                   const EXCEPTION_INFO* pExceptInfo, void* v);

bool signal_handler(THREADID tid, INT32 sig, CONTEXT* ctxt, bool hasHandler,
                    const EXCEPTION_INFO* pExceptInfo, void* v);

// Main loop for rightpath execptions: any context change is delayed until we
// reach the execption handler
bool excp_main_loop(int sig);

#endif  // PIN_EXEC_EXCEPTION_HANDLING_H__