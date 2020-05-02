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
 * File         : mem_req.c
 * Author       : HPS Research Group
 * Date         : 8/14/2013
 * Description  : Memory request
 ***************************************************************************************/

#include "memory/mem_req.h"
#include "core.param.h"
#include "globals/enum.h"

/**************************************************************************************/
/* Enums */

DEFINE_ENUM(Mem_Req_Type, MRT_LIST);
DEFINE_ENUM(Dram_Req_Status, DRAM_REQ_STATUS_LIST);

/**************************************************************************************/
/* Global Variables */

const char* const mem_req_state_names[] = {
  "INV",      "MLC_NEW",     "MLC_WAIT", "MLC_HIT_DONE", "L1_NEW",
  "L1_WAIT",  "L1_HIT_DONE", "BUS_NEW",  "MEM_NEW",      "MEM_SCHEDULED",
  "MEM_WAIT", "BUS_BUSY",    "BUS_WAIT", "MEM_DONE",     "BUS_IN_DONE",
  "FILL_L1",  "FILL_MLC",    "FILL_DONE"};

/**************************************************************************************/
/* mem_req_type_is_demand */

Flag mem_req_type_is_demand(Mem_Req_Type type) {
  return type == MRT_IFETCH || type == MRT_DFETCH || type == MRT_DSTORE;
}

/**************************************************************************************/
/* mem_req_type_is_prefetch */

Flag mem_req_type_is_prefetch(Mem_Req_Type type) {
  return type == MRT_IPRF || type == MRT_DPRF;
}

/**************************************************************************************/
/* mem_req_type_is_stalling */

Flag mem_req_type_is_stalling(Mem_Req_Type type) {
  return type == MRT_IFETCH || type == MRT_DFETCH ||
         (!STORES_DO_NOT_BLOCK_WINDOW && type == MRT_DSTORE);
}
