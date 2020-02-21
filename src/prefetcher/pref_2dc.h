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
 * File         : pref_2dc.h
 * Author       : HPS Research Group
 * Date         : 11/16/2004
 * Description  :
 ***************************************************************************************/
#ifndef __PREF_2DC_H__
#define __PREF_2DC_H__

typedef enum Pref_2DC_HashFunc_Enum {
  PREF_2DC_HASH_FUNC_DEFAULT,
} Pref_2DC_HashFunc;

typedef struct Pref_2DC_Cache_Data_struct {
  int delta;
} Pref_2DC_Cache_Data;

typedef struct Pref_2DC_Region_Struct {
  int deltaA, deltaB, deltaC;
} Pref_2DC_Region;

typedef struct Pref_2DC_Struct {
  HWP_Info* hwp_info;

  // 2DC Cache
  Cache cache;
  uns   cache_index_bits;

  uns               pref_degree;
  Addr              last_access;
  Addr              last_loadPC;
  Pref_2DC_HashFunc hash_func;
  Pref_2DC_Region*  regions;
} Pref_2DC;

/*************************************************************/
/* HWP Interface */
void pref_2dc_init(HWP* hwp);
void pref_2dc_ul1_train(Addr lineAddr, Addr loadPC, Flag ul1_hit);
void pref_2dc_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                       uns32 global_hist);
void pref_2dc_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                          uns32 global_hist);

/*************************************************************/
/* Misc functions */
void pref_2dc_throttle(void);
Addr pref_2dc_hash(Addr lineIndex, Addr loadPC, int deltaA, int deltaB);

#endif /*  __PREF_2DC_H__*/
