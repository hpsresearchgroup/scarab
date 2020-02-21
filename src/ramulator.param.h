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
 * File         : ramulator.param.h
 * Author       : SAFARI research group
 * Date         : 6/12/2018
 * Description  :
 ***************************************************************************************/

#ifndef __RAMULATOR_PARAM_H__
#define __RAMULATOR_PARAM_H__

#include "globals/global_types.h"


/**************************************************************************************/
/* extern all of the variables defined in ramulator.param.def */

#define DEF_PARAM(name, variable, type, func, def, const) \
  extern const type variable;
#include "ramulator.param.def"
#undef DEF_PARAM


/**************************************************************************************/

#endif /* #ifndef __RAMULATOR_PARAM_H__ */
