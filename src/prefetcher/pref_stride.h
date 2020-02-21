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
 * File         : pref_stride.h
 * Author       : HPS Research Group
 * Date         : 1/23/2005
 * Description  :
 ***************************************************************************************/
#ifndef __PREF_STRIDE_H__
#define __PREF_STRIDE_H__

#include "pref_common.h"

#define STRIDE_REGION(x) (x >> (PREF_STRIDE_REGION_BITS))

typedef struct Stride_Region_Table_Entry_Struct {
  Addr tag;
  Flag valid;
  uns  last_access;  // for lru
} Stride_Region_Table_Entry;

typedef struct Stride_Index_Table_Entry_Struct {
  Flag trained;

  // in this mode, strides are captured and essentianlly, it is just being
  // verified
  Flag train_count_mode;

  uns  num_states;
  uns  curr_state;
  Addr last_index;

  int stride[2];
  int s_cnt[2];
  int strans[2];  //== stride12, stride21;

  int recnt;
  int count;

  int  pref_count;
  uns  pref_curr_state;
  Addr pref_last_index;

  Counter pref_sent;
} Stride_Index_Table_Entry;

typedef struct Pref_Stride_Struct {
  HWP_Info* hwp_info;
  // Region table points to index table
  Stride_Region_Table_Entry* region_table;
  // Index table
  Stride_Index_Table_Entry* index_table;
} Pref_Stride;

/*************************************************************/
/* HWP Interface */
void pref_stride_init(HWP* hwp);
void pref_stride_ul1_train(Addr lineAddr, Addr loadPC, Flag ul1_hit);
void pref_stride_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                          uns32 global_hist);
void pref_stride_ul1_hit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                         uns32 global_hist);

/*************************************************************/
/* Misc functions */
void pref_stride_create_newentry(int idx, Addr line_addr, Addr region_tag);

#endif /*  __PREF_STRIDE_H__*/
