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
 * File         : stream_pref.h
 * Author       : HPS Research Group
 * Date         : 10/24/2002
 * Description  :
 ***************************************************************************************/
#ifndef __STREAM_PREF_H__
#define __STREAM_PREF_H__

#include "globals/global_types.h"

/**************************************************************************************/
/* Forward Declarations */

struct Mem_Req_struct;
struct Pref_Mem_Req_struct;
struct Stream_Buffer_struct;

/**************************************************************************************/
/* Types */

typedef struct Stream_HWP_Struct {
  // stream HWP
  Stream_Buffer* stream;
  Stream_Buffer* l2hit_stream;
  /* prefetch req queues */
  Pref_Mem_Req* pref_req_queue;
  Pref_Mem_Req* l2hit_pref_req_queue;
  Pref_Mem_Req* l2hit_l2send_req_queue;
} Stream_HWP;

void stream_dl0_miss(Addr line_addr);
void stream_ul1_miss(Mem_Req* req);
void update_pref_queue(void);
void init_stream_HWP(void);
int  train_create_stream_buffer(uns proc_id, Addr line_index, Flag train,
                                Flag create);
Flag train_stream_filter(Addr line_index);
Flag pref_req_queue_filter(Addr line_addr);

void l2_hit_stream_pref(Addr line_addr, Flag hit);
Flag train_l2hit_stream_filter(Addr line_index);
void l2hit_stream_req(Addr line_index, Flag hit);
int  train_l2hit_stream_buffer(Addr line_index, Flag hit);
void stream_dl0_hit_train(Addr line_addr);


#endif /*  __STREAM_PREF_H__*/
