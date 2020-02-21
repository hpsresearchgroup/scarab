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
 * File         : debug/pipeview.h
 * Author       : HPS Research Group
 * Date         : 4/24/2012
 * Description  : Pipeline visualization tracing.
 ***************************************************************************************/

#ifndef __PIPEVIEW_H__
#define __PIPEVIEW_H__

#include "globals/global_types.h"

/**************************************************************************************/
/* Forward declarations */

struct Op_struct;

/**************************************************************************************/
/* Prototypes */

/* Initialize pipeline visualization */
void pipeview_init(void);

/* Print an op (when it's freed) */
void pipeview_print_op(struct Op_struct* op);

/* Clean up pipeline visualization */
void pipeview_done(void);

/*************************************************************/

#endif /*  __PIPEVIEW_H__*/
