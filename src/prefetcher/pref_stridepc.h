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
 * File         : pref_stridepc.h
 * Author       : HPS Research Group
 * Date         : 1/23/2005
 * Description  : Stride Prefetcher - Based on load's PC address
 ***************************************************************************************/
#ifndef __PREF_STRIDEPC_H__
#define __PREF_STRIDEPC_H__

#include "pref_common.h"

typedef struct StridePC_Table_Entry_Struct {
  Flag trained;
  Flag valid;

  Addr last_addr;
  Addr load_addr;
  Addr start_index;
  Addr pref_last_index;
  int  stride;

  Counter train_num;
  Counter pref_sent;
  Counter last_access;  // for lru
} StridePC_Table_Entry;

typedef struct Pref_StridePC_Struct {
  HWP_Info*             hwp_info;
  StridePC_Table_Entry* stride_table;
} Pref_StridePC;

/*************************************************************/
/* HWP Interface */
void set_pref_stridepc(Pref_StridePC* new_stridepc);
void pref_stridepc_init(HWP* hwp);
void pref_stridepc_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                            uns32 global_hist);
void pref_stridepc_ul1_hit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                           uns32 global_hist);
void pref_stridepc_ul1_train(uns8 proc_id, Addr lineAddr, Addr loadPC,
                             Flag ul1_hit);


/*************************************************************/
/* Misc functions */

#endif /*  __PREF_STRIDEPC_H__*/
