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
 * File         : l2markv_pref.h
 * Author       : HPS Research Group
 * Date         : 03/17/2004
 * Description  :
 ***************************************************************************************/
#ifndef __L2MARKV_PREF_H__
#define __L2MARKV_PREF_H__

#include "pref_type.h"

typedef struct L2markv_Rec_Struct {
  Addr    last_addr;
  Addr    next_addr;
  Counter time_diff;
  int     next_addr_counter;
  int     last_addr_counter;
  Counter last_access_time;
} L2markv_Rec;

void l2markv_init(void);
void l2markv_pref(Mem_Req_Info* req, int* train_hit, int* pref_req,
                  Addr* req_addr);
int  l2markv_pref_train(Mem_Req_Info* req);
int  l2markv_pref_pred(Mem_Req_Info* req, Addr* req_addr);
void insert_l2markv_pref_req(Addr va, Counter time);
void update_l2markv_pref_req_queue(void);

void l2next_pref(Mem_Req_Info* req);

#endif /* #ifnde __L2MARKV_PREF_H__  */
