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
 * File         : dvfs/power_pred.h
 * Author       : HPS Research Group
 * Date         : 05/19/2012
 * Description  : Power prediction for DVFS
 ***************************************************************************************/

#ifndef __POWER_PRED_H__
#define __POWER_PRED_H__

#include "globals/global_defs.h"
#include "globals/global_types.h"

/**************************************************************************************/
/* Prototypes */

/* Return predicted normalized power for the provided core and memory
   cycle times, memory access fractions, and the predicted slowdown of each core
 */
double power_pred_norm_power(uns* core_cycle_times, uns memory_cycle_time,
                             double* memory_access_fracs, double* slowdowns);

#endif  // __POWER_PRED_H__
