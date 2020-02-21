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

#include "../globals/global_types.h"
#include "../table_info.h"
#include "stdio.h"

FILE* mystdout = stdout;
FILE* mystderr = stderr;
FILE* mystatus = stdout;

Counter  cycle_count  = 0;
Counter  unique_count = 0;
Counter* op_count;
Counter* inst_count;
Counter* unique_count_per_core;
Flag*    trace_read_done;

// const char* PIN_EXEC_DRIVEN_FE_SOCKET = "./temp.socket";
const char* FILE_TAG             = "";
uns         INST_HASH_TABLE_SIZE = 500021;
int         op_type_delays[NUM_OP_TYPES];
