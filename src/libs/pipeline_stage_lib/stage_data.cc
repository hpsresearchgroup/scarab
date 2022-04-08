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
* File         : stage_data.cc
* Author       : HPS Research Group
* Date         : 4/6/2022
* Description  :
***************************************************************************************/

extern "C" {
    #include "globals/global_defs.h"
    #include "globals/global_types.h"
    #include "globals/global_vars.h"
    #include "globals/assert.h"
    #include "globals/op_pool.h"

    #include "debug/debug_macros.h"
    #include "debug/debug_print.h"
}

#include "stage_data.h"

#include <algorithm>

StageData::StageData(uns proc_id_, std::string name_, int32_t stage_width_) :
    proc_id(proc_id_), name(name_), num_ops(0), ops(stage_width_, NULL)
{ }

void StageData::insert(Op *op) {
    ASSERT(proc_id, op);
    ASSERT(proc_id, ops[num_ops] == nullptr);
    ASSERT(proc_id, num_ops < ops.size());
    ops[num_ops] = op;
    num_ops += 1;
}

void StageData::reset() {
    num_ops = 0;
    std::fill(ops.begin(), ops.end(), nullptr);
}

bool StageData::flush_op(Op *op, Counter recovery_op_num) {
    return op->op_num > recovery_op_num;
}

void StageData::recover(Counter recovery_op_num) {
    num_ops = 0;
    for (auto& op : ops) {
        if (op) {
            if (flush_op(op, recovery_op_num)) {
                free_op(op);
            } else {
                insert(op);
            }
        }
    }
    std::fill(ops.begin() + num_ops, ops.end(), nullptr);
}

void StageData::debug() const {
    DPRINTF("# %-10s  num_ops:%d\n", name.c_str(), num_ops);
    print_op_array(GLOBAL_DEBUG_STREAM, (Op **) &ops[0], ops.size(), num_ops);
}
