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
 * File         : memory/cache_part.h
 * Author       : HPS Research Group
 * Date         : 2/19/2014
 * Description  : Shared cache partitioning mechanisms
 ***************************************************************************************/

#include "globals/enum.h"

/**************************************************************************************/
/* Forward Declarations */

struct Mem_Req_struct;

/**************************************************************************************/
/* Enums */

#define CACHE_PART_METRIC_LIST(elem) \
  elem(GLOBAL_MISS_RATE) elem(MISS_RATE_SUM) elem(GMEAN_PERF)

DECLARE_ENUM(Cache_Part_Metric, CACHE_PART_METRIC_LIST, CACHE_PART_METRIC_);

#define CACHE_PART_SEARCH_LIST(elem) elem(LOOKAHEAD) elem(BRUTE_FORCE)

DECLARE_ENUM(Cache_Part_Search, CACHE_PART_SEARCH_LIST, CACHE_PART_SEARCH_);

/**************************************************************************************/
/* Prototypes */

/* Initialize */
void cache_part_init(void);

/* Report L1 access */
void cache_part_l1_access(struct Mem_Req_struct*);

/* Report L1 access during warmup */
void cache_part_l1_warmup(uns proc_id, Addr addr);

/* Call every cycle */
void cache_part_update(void);
