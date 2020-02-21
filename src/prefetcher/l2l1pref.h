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
 * File         : l2l1pref.h
 * Author       : HPS Research Group
 * Date         : 03/24/2004
 * Description  : L2 to L1 prefetcher support functions
 ***************************************************************************************/
#ifndef __L2L1PREF_H__
#define __L2L1PREF_H__

#include "dcache_stage.h"
#include "globals/global_types.h"
#include "l2l1pref.param.h"
#include "l2markv_pref.h"
#include "l2way_pref.h"
#include "pref_type.h"

/**************************************************************************************/
/* Global vars */
/**************************************************************************************/
/* Local vars */

/**************************************************************************************/
/* Prototypes */


typedef struct L2_hit_ip_stat_Struct {
  Counter hit_count;
  Counter last_cycle;
  int     delta[9];
} L2_hit_ip_stat_entry;


void init_prefetch(void); /* need to move somewhere for generatic prefetcher
                             file */
void         l2l1_init(void);
void         l2l1pref_mem(Mem_Req* req);
void         l2l1pref_mem_process(Mem_Req_Info* req);
void         l2l1pref_dcache(Addr line_addr, Op* op);
Dcache_Data* dc_pref_cache_access(Op* op);
void         dc_pref_cache_insert(Addr addr);
Flag         dc_pref_cache_fill_line(Mem_Req*);
void         ideal_l2l1_prefetcher(Op* op);
void         l2l1_done(void);

/*stat */
void hps_hit_stat(Mem_Req* req);
void hps_miss_stat(Mem_Req* req);
void dc_miss_stat(Op* op);

/**************************************************************************************/

#endif /* ifndef __L2L1PREF_H__  */
