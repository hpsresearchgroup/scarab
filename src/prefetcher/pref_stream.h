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
 * File         : pref_stream.h
 * Author       : HPS Research Group
 * Date         : 1/20/2005
 * Description  : Stream Prefetcher
 ***************************************************************************************/
#ifndef __PREF_STREAM_H__
#define __PREF_STREAM_H__

#include "globals/global_types.h"

/**************************************************************************************/
/* Forward Declarations */

struct HWP_struct;
struct HWP_Info_struct;

/**************************************************************************************/
/* Types */

// typedef in globals/global_types.h
struct Stream_Buffer_struct {
  uns8 proc_id;
  Addr load_pc[4];
  Addr line_index;
  Addr sp;
  Addr ep;
  Addr start_vline;
  int  dir;
  int  lru;
  Flag valid;
  Flag buffer_full;
  Flag trained;
  uns  pause;  // number of stream demands remaining before next prefetch burst
               // can be sent
  int train_hit;
  uns length;  // Now with the pref accuracy, we can dynamically tune the length
  uns pref_issued;
  uns pref_useful;
};

// stream HWP
typedef struct Pref_Stream_struct {
  HWP_Info* hwp_info;

  // WATCHOUT These are shared by cores or duplicated based on
  // PREF_STREAM_PER_CORE_ENABLE
  Stream_Buffer* stream;
  Addr*          train_filter;
  int*           train_filter_no;
  ////////////////////////////////////////////////

  uns train_num;  // With pref accuracy, dynamically tune the train length
  uns distance;
  uns pref_degree_vals[10];

  uns num_tosend;
  uns num_tosend_vals[10];

} Pref_Stream;

void set_pref_stream(Pref_Stream*);

void pref_stream_init(HWP* hwp);
void pref_stream_per_core_done(uns proc_id);

void pref_stream_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                          uns32 global_hist);
void pref_stream_ul1_hit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                         uns32 global_hist);
void pref_stream_train(uns8 proc_id, Addr lineAddr, Addr loadPC,
                       uns32 global_hist, Flag create);

int  pref_stream_train_create_stream_buffer(uns8 proc_id, Addr line_index,
                                            Flag train, Flag create,
                                            int extra_dis);
Flag pref_stream_train_stream_filter(Addr line_index);

void pref_stream_addto_train_stream_filter(Addr line_index);

Flag pref_stream_req_queue_filter(Addr line_addr);

void pref_stream_remove_redundant_stream(int hit_index);

Flag pref_stream_bw_prefetchable(uns proc_id, Addr line_addr);

// Used when throttling using the overall accuracy numbers
void pref_stream_throttle(uns8 proc_id);

void pref_stream_throttle_fb(uns8 proc_id);

/////////////////////////////////////////////////
// Used when throttling for each stream separately
// NON FUNCTIONAL CURRENTLY
void pref_stream_throttle_streams(Addr line_index);
void pref_stream_throttle_stream(int index);
// Again - Use ONLY when throttling per stream
float pref_stream_acc_getacc(int index, float pref_acc);
void  pref_stream_acc_ul1_useful(Addr line_index);
void  pref_stream_acc_ul1_issued(Addr line_index);
///////////////////////////////////////////////////

#endif /*  __PREF_STREAM_H__*/
