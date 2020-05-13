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
 * File         : pin_scarab_common_lib.h
 * Author       : HPS Research Group
 * Date         : 10/30/2018
 * Description  :
 ***************************************************************************************/

/* Interface to use PIN Execution Driven frontend as a functional frontend */

#ifndef __PIN_SCARAB_COMMON_LIB_H__
#define __PIN_SCARAB_COMMON_LIB_H__

#include <queue>
#include "../../ctype_pin_inst.h"
#include "../../op_info.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Scarab_To_Pin_Cmd_enum {
  FE_NULL,
  FE_FETCH_OP,
  FE_REDIRECT,
  FE_RECOVER_BEFORE,
  FE_RECOVER_AFTER,
  FE_RETIRE,
  FE_NUM_COMMANDS
} Scarab_To_Pin_Cmd;

struct Scarab_To_Pin_Msg {
  Scarab_To_Pin_Cmd type;
  uint64_t          inst_uid;
  Addr              inst_addr;
} __attribute__((packed));

typedef std::deque<compressed_op> ScarabOpBuffer_type;

Flag is_sentinal_op(compressed_op* op);

extern uint64_t fast_forward_count;

#ifdef __cplusplus
}
#endif

#endif
