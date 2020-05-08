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

#ifndef __DECODER_H__
#define __DECODER_H__

#undef UNUSED   // there is a name conflict between PIN and Scarab
#undef WARNING  // there is a name conflict between PIN and Scarab

#include "pin.H"

#undef UNUSED   // there is a name conflict between PIN and Scarab
#undef WARNING  // there is a name conflict between PIN and Scarab
#include "../../ctype_pin_inst.h"
#include "../../table_info.h"
#include "gather_scatter_addresses.h"

void pin_decoder_init(bool translate_x87_regs, std::ostream* err_ostream);

void pin_decoder_insert_analysis_functions(const INS& ins);

ctype_pin_inst* pin_decoder_get_latest_inst();

void pin_decoder_print_unknown_opcodes();

uint8_t is_ifetch_barrier(const INS& ins);

ctype_pin_inst create_sentinel();
ctype_pin_inst create_dummy_jump(uint64_t eip, uint64_t tgt);
ctype_pin_inst create_dummy_nop(uint64_t eip, Wrongpath_Nop_Mode_Reason reason);

#endif  // __DECODER_H__
