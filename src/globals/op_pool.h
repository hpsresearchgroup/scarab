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
 * File         : op_pool.h
 * Author       : HPS Research Group
 * Date         : 1/28/1998
 * Description  : Header for op_pool.c
 ***************************************************************************************/

#ifndef __OP_POOL_H__
#define __OP_POOL_H__


/**************************************************************************************/
/* Global Variables */

extern Op  invalid_op;
extern uns op_pool_entries;
extern uns op_pool_active_ops;


/**************************************************************************************/
/* Prototypes */

void init_op_pool(void);
void reset_op_pool(void);
Op*  alloc_op(uns proc_id);
void free_op(Op*);
void op_pool_init_op(Op*);
void op_pool_setup_op(uns proc_id, Op* op);

/**************************************************************************************/

#endif /* #ifndef __OP_POOL_H__ */
