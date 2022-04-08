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
* File         : scarab_test_helper.h
* Author       : HPS Research Group
* Date         : 4/8/2022
* Description  :
***************************************************************************************/

#ifndef __SCARAB_TEST_HELPER_H__
#define __SCARAB_TEST_HELPER_H__

extern "C" {
    #include "op.h"
    #include "globals/op_pool.h"
}

inline Op *scarab_test_alloc_op(uns proc_id) {
    Op *op = alloc_op(proc_id);
    op->sched_info = NULL;
    op->table_info = NULL;
    op->inst_info = NULL;
    return op;
}

inline void scarab_test_free_op(Op *op) {
    if (op->op_pool_valid) {
        free_op(op);
    }
}

#endif
