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
 * File         : dvfs.h
 * Author       : HPS Research Group
 * Date         : 3/28/2012
 * Description  : DVFS controller
 ***************************************************************************************/

#ifndef __DVFS_H__
#define __DVFS_H__

#include "globals/enum.h"

/**************************************************************************************/
/* Enumerations */

#define DVFS_METRIC_LIST(elem) elem(DELAY) elem(ENERGY) elem(EDP) elem(ED2)

DECLARE_ENUM(DVFS_Metric, DVFS_METRIC_LIST, DVFS_METRIC_);

/**************************************************************************************/
/* Prototypes */

/* Initialization */
void dvfs_init(void);

/* Call at the end of every cycle to make DVFS controller work */
void dvfs_cycle(void);

/* Done */
void dvfs_done(void);

#endif /* #ifndef __DVFS_H__ */
