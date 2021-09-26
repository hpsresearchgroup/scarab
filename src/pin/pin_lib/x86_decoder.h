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

#ifndef __X86_DECODER_H__
#define __X86_DECODER_H__

#include "assert.h"

#include "../../ctype_pin_inst.h"
#include "../../table_info.h"
#include "pin_api_to_xed.h"
#include "pin_scarab_common_lib.h"
#include "x87_stack_delta.h"

#include <ostream>
#include <unordered_map>

// Global static instructions map
typedef std::unordered_map<ADDRINT, ctype_pin_inst*> inst_info_map;
typedef inst_info_map::iterator                      inst_info_map_p;

/**************************** Public Functions ********************************/

void init_x86_decoder(std::ostream* err_ostream);
void fill_in_basic_info(ctype_pin_inst* info, const xed_decoded_inst_t* ins);
uint32_t add_dependency_info(ctype_pin_inst*           info,
                             const xed_decoded_inst_t* ins);
void     fill_in_simd_info(ctype_pin_inst* info, const xed_decoded_inst_t* ins,
                           uint32_t max_op_width);
void     apply_x87_bug_workaround(ctype_pin_inst*           info,
                                  const xed_decoded_inst_t* ins);
void     fill_in_cf_info(ctype_pin_inst* info, const xed_decoded_inst_t* ins);

void print_err_if_invalid(ctype_pin_inst* info, const xed_decoded_inst_t* ins);

uint8_t is_ifetch_barrier(const xed_decoded_inst_t* ins);

#endif  //__X86_DECODER_H__
