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
 * File         : freq.h
 * Author       : HPS Research Group
 * Date         : 03/13/2012
 * Description  : Modeling frequency domains
 ***************************************************************************************/

#ifndef __FREQ_H__
#define __FREQ_H__

#include "globals/global_types.h"

/**************************************************************************************/
/* Types */

typedef unsigned int Freq_Domain_Id;

/**************************************************************************************/
/* External variables */

extern Freq_Domain_Id FREQ_DOMAIN_CORES[];
extern Freq_Domain_Id FREQ_DOMAIN_L1;
extern Freq_Domain_Id FREQ_DOMAIN_MEMORY;

/**************************************************************************************/
/* Prototypes */

/* Initialize frequency domains */
void freq_init(void);

/* Is the frequency domain ready to be simulated at this time (is its
   cycle starting at this exact time)? */
Flag freq_is_ready(Freq_Domain_Id id);

/* Advance time to the next earliest time a frequency domain will be
   ready to be simulated */
void freq_advance_time(void);

/* Reset cycle time of each domain to zero but keep the time value. */
void freq_reset_cycle_counts(void);

/* Returns the cycle count in the specified frequency domain */
Counter freq_cycle_count(Freq_Domain_Id id);

/* Returns the current simulation time (in femtoseconds) */
Counter freq_time(void);

/* Returns the future simulation time (in femtoseconds) when the
   specified domain reaches the specified cycle count (without
   changing its frequency) */
Counter freq_future_time(Freq_Domain_Id, Counter cycle_count);

/* Sets the cycle time of the specified frequency domain (takes effect
   on the next cycle of that domain) */
void freq_set_cycle_time(Freq_Domain_Id, uns cycle_time);

/* Returns the current cycle time of the specified frequency domain */
uns freq_get_cycle_time(Freq_Domain_Id id);

/* Convert the cycle count of one domain to the other. If DVFS is
   enabled, this only works if the frequency domains did not change
   frequency during the cycles counted */
Counter freq_convert(Freq_Domain_Id src, Counter src_cycle_count,
                     Freq_Domain_Id dst);

/* Returns the earliest cycle in dst domain that is at or after the
   time specified by the src_cycle_count in src domain */
Counter freq_convert_future_cycle(Freq_Domain_Id src, Counter src_cycle_count,
                                  Freq_Domain_Id dst);

/* Clean up at the end */
void freq_done(void);

#endif /* #ifndef __FREQ_H__ */
