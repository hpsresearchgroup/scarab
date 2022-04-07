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
}

#include "stage_data.h"

#include <algorithm>

StageData::StageData(uns proc_id_, std::string name_, int32_t stage_width_) :
    proc_id(proc_id_), name(name_), op_count(0), ops(stage_width_, NULL)
{ }

void StageData::insert(Op *op) {
    //ASSERT(proc_id, op);
    //ASSERT(proc_id, ops[op_count] == nullptr);
    //ASSERT(proc_id, op_count < ops.size());
    ops[op_count] = op;
    op_count += 1;
}

void StageData::reset() {
    op_count = 0;
    std::fill(ops.begin(), ops.end(), nullptr);
}

void StageData::recover() {}

void StageData::debug() {}
