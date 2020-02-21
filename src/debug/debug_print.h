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
 * File         : debug/debug_print.h
 * Author       : HPS Research Group
 * Date         : 10/15/1997
 * Description  : Header for the debug_print functions in debug/debug_print.c
 ***************************************************************************************/

#ifndef __DEBUG_PRINT_H__
#define __DEBUG_PRINT_H__

#include <stdio.h>
#include "globals/global_types.h"


/**************************************************************************************/
/* External Variables */

extern const char* const icache_state_names[];
extern const char* const tcache_state_names[];
extern const char* const cf_type_names[];
extern const char* const sm_state_names[];


/**************************************************************************************/
/* Prototypes */

void  print_op(Op*);
void  print_func_op(Op*);
void  print_short_op_array(FILE*, Op* [], uns);
void  print_op_array(FILE*, Op* [], uns, uns);
void  print_open_op_array(FILE*, Op* [], uns, uns);
void  print_open_op_array_end(FILE*, uns);
void  print_op_field(FILE*, Op*, uns);
void  print_field_tail(FILE*, uns);
void  print_field_head(FILE*, uns);
char* disasm_op(Op*, Flag wide);
char* disasm_reg(uns);


/**************************************************************************************/

#endif /* #ifndef __DEBUG_PRINT_H__ */
