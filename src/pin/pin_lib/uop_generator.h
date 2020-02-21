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
 ** File         : uop_generator.h
 ** Author       : HPS Research Group
 ** Date         : 1/3/2018
 ** Description  : API to extract uops out of ctype_pin_inst_struct instances.
 ****************************************************************************************/

#ifndef __UOP_GENERATOR_H__
#define __UOP_GENERATOR_H__

#include "../../globals/global_types.h"

/**************************************************************************************/
/* Prototypes */

#ifdef __cplusplus
extern "C" {
#endif

void uop_generator_init(uint32_t num_cores);
Flag uop_generator_extract_op(uns proc_id, Op* op, compressed_op* cop);

void uop_generator_get_uop(uns proc_id, Op* op, compressed_op* inst);
Flag uop_generator_get_bom(uns proc_id);  // Called before
                                          // uop_generator_get_uop.
Flag uop_generator_get_eom(uns proc_id);  // Called after uop_generator_get_uop.
void uop_generator_recover(uns8 proc_id);

#ifdef __cplusplus
}
#endif

#endif  //__UOP_GENERATOR_H__
