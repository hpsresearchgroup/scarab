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
 * File         : power_intf.c
 * Author       : HPS Research Group
 * Date         : 04/21/2012
 * Description  : Interface to the combined McPAT/CACTI power model.
 ***************************************************************************************/

#ifndef __POWER_INTF_H__
#define __POWER_INTF_H__

#include "globals/enum.h"

/**************************************************************************************/
/* Types */

#define POWER_DOMAIN_LIST(elem)                                      \
  elem(CORE_0) elem(CORE_1) elem(CORE_2) elem(CORE_3) elem(CORE_4)   \
    elem(CORE_5) elem(CORE_6) elem(CORE_7) elem(UNCORE) elem(MEMORY) \
      elem(OTHER)

DECLARE_ENUM(Power_Domain, POWER_DOMAIN_LIST, POWER_DOMAIN_);

#define POWER_RESULT_LIST(elem)                                             \
  elem(TOTAL) elem(DYNAMIC) elem(PEAK_DYNAMIC) elem(STATIC)                 \
    elem(SUBTHR_LEAKAGE) elem(GATE_LEAKAGE) elem(VOLTAGE) elem(MIN_VOLTAGE) \
      elem(FREQUENCY)

DECLARE_ENUM(Power_Result, POWER_RESULT_LIST, POWER_RESULT_);

/**************************************************************************************/
/* Prototypes */

/* Initialize power model */
void power_intf_init(void);

/* Calculate power based on POWER_* stats (taking into account only
   the difference in the stats from the last time this function was
   called) */
void power_intf_calc(void);

/* Return the specified power result for the specified domain */
double power_intf_result(Power_Domain domain, Power_Result result);

/* Clean up */
void power_intf_done(void);

#endif  // __POWER_INTF_H__
