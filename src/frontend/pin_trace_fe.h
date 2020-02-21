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

/***************************************************************************************
 * File         : frontend/pin_trace_fe.h
 * Author       : HSP Research Group
 * Date         :
 * Description  :
 ***************************************************************************************/

#ifndef __PIN_TRACE_FE_H__
#define __PIN_TRACE_FE_H__

#include "globals/global_types.h"

/**************************************************************************************/
/* Forward Declarations */

struct Trace_Uop_struct;
typedef struct Trace_Uop_struct Trace_Uop;
struct Op_struct;

/**************************************************************************************/
/* Prototypes */

void trace_init(void);

/* Implementing the frontend interface */
Addr trace_next_fetch_addr(uns proc_id);
Flag trace_can_fetch_op(uns proc_id);
void trace_fetch_op(uns proc_id, Op* op);
void trace_redirect(uns proc_id, uns64 inst_uid, Addr fetch_addr);
void trace_recover(uns proc_id, uns64 inst_uid);
void trace_retire(uns proc_id, uns64 inst_uid);

/* For restarting of traces */
void trace_done(void);
void trace_close_trace_file(uns proc_id);
void trace_setup(uns proc_id);

#endif
