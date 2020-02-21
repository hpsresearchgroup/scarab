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
 * File         : stat_mon.h
 * Author       : HPS Research Group
 * Date         : 12/13/2012
 * Description  : Statistic monitor: allows any statistic to be examined at any
 *interval
 ***************************************************************************************/

#ifndef __STAT_MON_H__
#define __STAT_MON_H__

#include "globals/global_types.h"

/**************************************************************************************/
/* Forward Declarations */

struct Stat_Mon_struct;
typedef struct Stat_Mon_struct Stat_Mon;

/**************************************************************************************/
/* Prototypes */

/* Create a stat_monitor from an array of stat indexes */
Stat_Mon* stat_mon_create_from_array(uns* stat_idx_array, uns num);

/* Create a stat_monitor from a range of stat indexes */
Stat_Mon* stat_mon_create_from_range(uns first_stat_idx, uns last_stat_idx);

/* Get the count of a stat since last reset */
Counter stat_mon_get_count(Stat_Mon* stat_mon, uns proc_id, uns stat_idx);

/* Get the value of a stat (for float stats) since last reset */
double stat_mon_get_value(Stat_Mon* stat_mon, uns proc_id, uns stat_idx);

/* Start a new interval in a stat monitor */
void stat_mon_reset(Stat_Mon* stat_mon);

/* Destroy a stat monitor */
void stat_mon_free(Stat_Mon* stat_mon);

#endif  // __STAT_MON_H__
