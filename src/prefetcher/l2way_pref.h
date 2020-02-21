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
 * File         : l2way_pref.h
 * Author       : HPS Research Group
 * Date         : 03/11/2004
 * Description  :
 ***************************************************************************************/
#ifndef __L2WAY_PREF_H__
#define __L2WAY_PREF_H__

#include "globals/global_types.h"
#include "pref_type.h"

// way predictor training structure
typedef struct L2way_Rec_Struct {
  uns     last_way;
  uns     pred_way;
  uns     counter;
  Counter last_access_time;
} L2way_Rec;

typedef struct L2set_Rec_Struct {
  Addr last_addr;
  Addr pred_addr;
} L2set_Rec;

typedef struct L1pref_Req_Struct {
  Flag    valid;
  Addr    va;
  Counter time;
  Counter rdy_cycle;
} L1pref_Req;


void l2way_init(void);
void l2way_pref(Mem_Req_Info* req);
void l2way_pref_train(Mem_Req_Info* req);
void l2way_pref_pred(Mem_Req_Info* req);
void insert_l2way_pref_req(Addr va, Counter time);
void update_l2way_pref_req_queue(void);
#endif /* #ifndef __L2WAY_PREF_H__ */
