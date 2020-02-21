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
 * File         : pref_ghb.h
 * Author       : HPS Research Group
 * Date         : 11/16/2004
 * Description  :
 ***************************************************************************************/
#ifndef __PREF_GHB_H__
#define __PREF_GHB_H__

#include "pref_common.h"

#define CZONE_TAG(x) (x >> (PREF_GHB_CZONE_BITS))

typedef struct GHB_Index_Table_Entry_Struct {
  Addr czone_tag;
  Flag valid;
  uns  ghb_ptr;      // ptr to last entry in ghb with same czone
  uns  last_access;  // for lru
} GHB_Index_Table_Entry;

typedef struct GHB_Entry_Struct {
  Addr miss_index;
  int  ghb_ptr;          // -1 == invalid
  int  ghb_reverse_ptr;  // -1 == invalid
  int  idx_reverse_ptr;
} GHB_Entry;

typedef struct Pref_GHB_Struct {
  HWP_Info* hwp_info;

  // Index table
  GHB_Index_Table_Entry* index_table;
  // GHB
  GHB_Entry* ghb_buffer;

  int ghb_tail;
  int ghb_head;

  int  deltab_size;
  int* delta_buffer;

  uns pref_degree;

  uns pref_degree_vals[5];
} Pref_GHB;

/*************************************************************/
/* HWP Interface */
void set_pref_ghb(Pref_GHB* new_ghb_hwp);
void pref_ghb_init(HWP* hwp);
void pref_ghb_ul1_train(uns8 proc_id, Addr lineAddr, Addr loadPC, Flag ul1_hit);
void pref_ghb_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                       uns32 global_hist);
void pref_ghb_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                          uns32 global_hist);

/*************************************************************/
/* Misc functions */
void pref_ghb_create_newentry(int idx, Addr line_addr, Addr czone_tag,
                              int old_ptr);

void pref_ghb_throttle(void);
void pref_ghb_throttle_fb(void);

#endif /*  __PREF_GHB_H__*/
