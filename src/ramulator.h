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
 * File         : ramulator.h
 * Author       : SAFARI research group
 * Date         : 6/12/2018
 * Description  : Header file defining an interface to Ramulator
 ***************************************************************************************/

#ifndef __RAMULATOR_H__
#define __RAMULATOR_H__

#include "globals/global_types.h"
#include "memory/memory.h"

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

EXTERNC void ramulator_init();
EXTERNC void ramulator_finish();

EXTERNC int  ramulator_send(Mem_Req* scarab_req);
EXTERNC void ramulator_tick();

EXTERNC int ramulator_get_chip_width();
EXTERNC int ramulator_get_chip_size();
EXTERNC int ramulator_get_num_chips();
EXTERNC int ramulator_get_chip_row_buffer_size();

EXTERNC Mem_Req* ramulator_search_queue(long phys_addr, Mem_Req_Type type);
#undef EXTERNC

#endif  // __RAMULATOR_H__
