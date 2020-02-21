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
 * File         : frontend.h
 * Author       : HPS Research Group
 * Date         : 10/26/2011
 * Description  : Interface for an external frontend.
 ***************************************************************************************/
#ifndef __FRONTEND_H__
#define __FRONTEND_H__

#include "globals/global_types.h"

/*************************************************************/
/* External frontend interface */

struct Op_struct;

/* Initialize the external frontend to run application specified by
   argv (expect argv to end with NULL) */
void frontend_init(void);

void frontend_done(Flag* retired_exit);

/* Get next instruction fetch address */
Addr frontend_next_fetch_addr(uns proc_id);

/* Can we get an op from the frontend (is process proc_id running?) */
Flag frontend_can_fetch_op(uns proc_id);

/* Get an op from the frontend */
void frontend_fetch_op(uns proc_id, struct Op_struct* op);

/* Redirect the front end (down the wrong path) */
void frontend_redirect(uns proc_id, uns64 inst_uid, Addr fetch_addr);

/* Recover the front end (restart the right path) */
void frontend_recover(uns proc_id, uns64 inst_uid);

/* Let the frontend know that this instruction is retired) */
void frontend_retire(uns proc_id, uns64 inst_uid);

/*************************************************************/

#endif /*  __FRONTEND_H__*/
