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
 * File         : addr_trans.h
 * Author       : HPS Research Group
 * Date         : 10/28/2012
 * Description  : "Fake" virtual to physical address translation. Uses a hash
 *function, and does not maintain page tables. Used to randomize DRAM bank
 *mappings.
 ***************************************************************************************/

#ifndef __ADDR_TRANS_H__
#define __ADDR_TRANS_H__

#include "globals/enum.h"
#include "globals/global_types.h"

/**************************************************************************************/
/* Types */

#define ADDR_TRANSLATION_LIST(elem) \
  elem(NONE) elem(FLIP) elem(RANDOM) elem(PRESERVE_BLP) elem(PRESERVE_STREAM)

DECLARE_ENUM(Addr_Translation, ADDR_TRANSLATION_LIST, ADDR_TRANS_);

/**************************************************************************************/
/* Prototypes */

Addr addr_translate(Addr virt_addr);

#endif  // __ADDR_TRANS_H__
