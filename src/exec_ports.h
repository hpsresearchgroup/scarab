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
 * File         : exec_ports.h
 * Author       : HPS Research Group
 * Date         : 1/9/18
 * Description  : Defines the macros used in the exec_ports.def file
 ***************************************************************************************/

#ifndef __EXEC_PORTS_H__
#define __EXEC_PORTS_H__

#include "table_info.h"

/**************************************************************************************/
/* Type Declarations */
void init_exec_ports(uns8, const char*);

typedef enum Power_FU_Type_enum {
  POWER_FU_ALU,
  POWER_FU_MUL_DIV,
  POWER_FU_FPU
} Power_FU_Type;

Power_FU_Type power_get_fu_type(Op_Type op_type, Flag is_simd);
uns64         get_fu_type(Op_Type op_type, Flag is_simd);

#endif /* #ifndef __EXEC_PORTS_H__ */
