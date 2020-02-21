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
 * File         : scarab_to_pin_interface.h
 * Author       : HPS Research Group
 * Date         : 10/02/2018
 * Description  :
 ***************************************************************************************/
#include "globals/global_types.h"
#include "op_info.h"
#include "stdint.h"

/* Interface to use PIN Execution Driven frontend as a functional frontend */

#ifndef __PIN_EXEC_DRIVEN_FE_H__
#define __PIN_EXEC_DRIVEN_FE_H__

#ifdef __cplusplus
extern "C" {
#endif

struct Op_struct;

/* Initialize the PIN execution driven frontend */
void pin_exec_driven_init(uns num_cores);
void pin_exec_driven_done(Flag* retired_exit);

/* Can an op be fetched for proc_id? */
Flag pin_exec_driven_can_fetch_op(uns proc_id);

/* Next instruction fetch address */
Addr pin_exec_driven_next_fetch_addr(uns proc_id);

/* Get an op from pin_exec_driven */
void pin_exec_driven_fetch_op(uns proc_id, struct Op_struct* op);

/* Redirect pin_exec_driven (down the wrong path) */
void pin_exec_driven_redirect(uns proc_id, uns64 inst_uid, Addr fetch_addr);

/* Recover pin_exec_driven (restart the right path) */
void pin_exec_driven_recover(uns proc_id, uns64 inst_uid);

/* Retire instruction at unique op id */
void pin_exec_driven_retire(uns proc_id, uns64 inst_uid);

#ifdef __cplusplus
}
#endif

#endif  // __PIN_EXEC_DRIVEN_FE_H__
