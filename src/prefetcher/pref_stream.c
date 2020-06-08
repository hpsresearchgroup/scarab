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
 * File         : pref_stream.c
 * Author       : HPS Research Group
 * Date         : 1/20/2005
 * Description  : Stream Prefetcher
 ***************************************************************************************/

#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"

#include "globals/assert.h"
#include "globals/utils.h"
#include "op.h"

#include "core.param.h"
#include "dcache_stage.h"
#include "debug/debug.param.h"
#include "general.param.h"
#include "libs/cache_lib.h"
#include "libs/hash_lib.h"
#include "libs/list_lib.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "prefetcher//pref_stream.h"
#include "prefetcher//stream.param.h"
#include "prefetcher/l2l1pref.h"
#include "prefetcher/pref.param.h"
#include "prefetcher/pref_common.h"
#include "statistics.h"

/**************************************************************************************/
/* Macros */
#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_STREAM, ##args)
#define DEBUG_PREFACC(proc_id, args...) _DEBUG(proc_id, DEBUG_PREFACC, ##args)

/**************************************************************************************/
/* Global Variables */

extern Dcache_Stage* dc;

/***************************************************************************************/
/* Local Prototypes */

static void collect_stream_stats(const Stream_Buffer* stream);

/**************************************************************************************/
/* stream prefetcher  */
/* prefetch is initiated by dcache miss but the request fills the l1 cache
 * (second level) cache   */
/* each stream has the starting pointer and the ending pointer and those
 * pointers tell whether the dl0 miss is within the boundary (buffer) */
/* stream buffer fetches one by one (end point requests the fetch address  */
/* stream buffer just holds the stream boundary not the data itself and data is
 * stored in the second level cache  */
/* At the beginning we will wait until we see 2 miss addresses */
/* using 2 miss addresses we decide the direction of the stream ( upward or
 * downward) and in the begining fill the half of buffer ( so many request !!!
 * ) */
/* Reference : IBM POWER 4 White paper */

Pref_Stream* pref_stream_core;
Pref_Stream* pref_stream;

void set_pref_stream(Pref_Stream* new_pref_stream) {
  pref_stream = new_pref_stream;
}


void pref_stream_init(HWP* hwp) {
  uns8 proc_id;

  if(!PREF_STREAM_ON) {
    return;
  }

  ASSERTM(0, PREF_REPORT_PREF_MATCH_AS_HIT || PREF_REPORT_PREF_MATCH_AS_MISS,
          "Stream prefetcher must train on demands matching prefetch request "
          "buffers\n");

  hwp->hwp_info->enabled = TRUE;

  pref_stream_core = (Pref_Stream*)malloc(sizeof(Pref_Stream) * NUM_CORES);

  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    pref_stream_core[proc_id].hwp_info = hwp->hwp_info;

    if(PREF_STREAM_PER_CORE_ENABLE) {
      pref_stream_core[proc_id].stream = (Stream_Buffer*)calloc(
        STREAM_BUFFER_N, sizeof(Stream_Buffer));
      memset(pref_stream_core[proc_id].stream, 0,
             STREAM_BUFFER_N * sizeof(Stream_Buffer));
      pref_stream_core[proc_id].train_filter = (Addr*)calloc(TRAIN_FILTER_SIZE,
                                                             sizeof(Addr));
      memset(pref_stream_core[proc_id].train_filter, 0,
             TRAIN_FILTER_SIZE * sizeof(Addr));
      pref_stream_core[proc_id].train_filter_no    = (int*)malloc(sizeof(int));
      *(pref_stream_core[proc_id].train_filter_no) = 0;
    }

    pref_stream_core[proc_id].train_num           = STREAM_TRAIN_NUM;
    pref_stream_core[proc_id].distance            = STREAM_LENGTH;
    pref_stream_core[proc_id].pref_degree_vals[0] = 4;
    pref_stream_core[proc_id].pref_degree_vals[1] = 8;
    pref_stream_core[proc_id].pref_degree_vals[2] = 16;
    pref_stream_core[proc_id].pref_degree_vals[3] = 32;
    pref_stream_core[proc_id].pref_degree_vals[4] = 64;
    pref_stream_core[proc_id].pref_degree_vals[5] = 64;

    pref_stream_core[proc_id].num_tosend         = STREAM_PREFETCH_N;
    pref_stream_core[proc_id].num_tosend_vals[0] = 1;
    pref_stream_core[proc_id].num_tosend_vals[1] = 1;
    pref_stream_core[proc_id].num_tosend_vals[2] = 2;
    pref_stream_core[proc_id].num_tosend_vals[3] = 4;
    pref_stream_core[proc_id].num_tosend_vals[4] = 4;
    pref_stream_core[proc_id].num_tosend_vals[5] = 6;
  }

  if(!PREF_STREAM_PER_CORE_ENABLE) {
    pref_stream_core[0].stream = (Stream_Buffer*)calloc(STREAM_BUFFER_N,
                                                        sizeof(Stream_Buffer));
    memset(pref_stream_core[0].stream, 0,
           STREAM_BUFFER_N * sizeof(Stream_Buffer));
    pref_stream_core[0].train_filter = (Addr*)calloc(TRAIN_FILTER_SIZE,
                                                     sizeof(Addr));
    memset(pref_stream_core[0].train_filter, 0,
           TRAIN_FILTER_SIZE * sizeof(Addr));
    pref_stream_core[0].train_filter_no    = (int*)malloc(sizeof(int));
    *(pref_stream_core[0].train_filter_no) = 0;

    for(proc_id = 1; proc_id < NUM_CORES; proc_id++) {
      pref_stream_core[proc_id].stream       = pref_stream_core[0].stream;
      pref_stream_core[proc_id].train_filter = pref_stream_core[0].train_filter;
      pref_stream_core[proc_id].train_filter_no =
        pref_stream_core[0].train_filter_no;
    }
  }
}

void pref_stream_train(uns8 proc_id, Addr line_addr, Addr load_PC,
                       uns32 global_hist, Flag create) /* line_addr: the first
                                                          address of the cache
                                                          block */
{
  // search the stream buffer
  int  hit_index = -1;
  int  ii;
  int  dis, maxdistance;
  Addr line_index = line_addr >> LOG2(DCACHE_LINE_SIZE);
  /* training filter */

  DEBUG(proc_id, "[DL0MISS:0x%s]ma:0x%7s mi:0x%7s\n", "L1", hexstr64(line_addr),
        hexstr64(line_index));

  if(!pref_stream_train_stream_filter(line_index)) {
    if(PREF_THROTTLE_ON) {
      pref_stream_throttle(proc_id);

      if(PREF_STREAM_ACCPERSTREAM)
        pref_stream_throttle_streams(line_index);
    }

    if(PREF_THROTTLEFB_ON) {
      pref_stream_throttle_fb(proc_id);
    }

    /* search for stream buffer */
    hit_index = pref_stream_train_create_stream_buffer(
      proc_id, line_addr, TRUE, create,
      0);  // so we create on dcache misses also?

    if(hit_index ==
       -1) /* we do not have a trained buffer, nor did we create it */
      return;

    Stream_Buffer* stream = &pref_stream->stream[hit_index];
    pref_stream_addto_train_stream_filter(line_index);
    ASSERT(proc_id, proc_id == stream->proc_id);

    if(stream->trained) {
      stream->lru = cycle_count;  // update lru
      STAT_EVENT(0, HIT_TRAIN_STREAM);
      stream->pause = SAT_DEC(stream->pause, 0);
      if(stream->pause > 0)
        return;

      /* hit the stream_buffer, request the prefetch */
      uns num_tosend = pref_stream->num_tosend;
      if(stream->buffer_full)
        num_tosend = MAX2(STREAM_FULL_N, num_tosend);

      for(ii = 0; ii < num_tosend; ii++) {
        if((stream->sp == line_index) &&
           (stream->buffer_full)) {  // when is buffer_full set to FALSE except
                                     // for buffer creation?
          // stream prefetch is requesting  enough far ahead
          // stop prefetch and wait until miss address is within the buffer area
          stream->pause = STREAM_FULL_N;
          return;
        }

        ASSERT(proc_id, proc_id == stream->ep >> (58 - LOG2(DCACHE_LINE_SIZE)));
        // IBM traces: some wrap over becaseu of too small or too large
        // addresses
        if(proc_id !=
           (stream->ep + stream->dir) >> (58 - LOG2(DCACHE_LINE_SIZE))) {
          stream->valid = FALSE;
          return;
        }

        if(PREF_HFILTER_ON &&
           pref_hfilter_pred_useless(proc_id, stream->ep + stream->dir, load_PC,
                                     global_hist)) {
          // do not send the prefetch
        } else {
          Addr line_index = stream->ep + stream->dir;
          uns  distance   = stream->dir > 0 ? line_index - stream->sp :
                                           stream->sp - line_index;
          if(!pref_addto_ul1req_queue_set(
               proc_id, line_index, pref_stream->hwp_info->id, distance,
               load_PC, global_hist, stream->buffer_full))
            return;
        }


        stream->ep  = stream->ep + stream->dir;
        dis         = stream->ep - stream->sp;
        maxdistance = (PREF_STREAM_ACCPERSTREAM ? stream->length :
                                                  pref_stream->distance);
        if(((stream->dir == 1) && (dis > maxdistance)) ||
           ((stream->dir == -1) && (dis < -maxdistance))) {
          stream->buffer_full = TRUE;
          stream->sp          = stream->sp + stream->dir;
        }

        if(REMOVE_REDUNDANT_STREAM)
          pref_stream_remove_redundant_stream(hit_index);

        DEBUG(proc_id,
              "[InQ:0x%s]ma:0x%7s mi:0x%7s d:%2d ri:0x%7s, sp:0x%7s ep:0x%7s\n",
              "L1", hexstr64(line_addr), hexstr64(line_index), stream->dir,
              hexstr64(stream->ep + stream->dir), hexstr64(stream->sp),
              hexstr64(stream->ep));
      }
    } else
      STAT_EVENT_ALL(MISS_TRAIN_STREAM);
  }
}

void pref_stream_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                          uns32 global_hist) {
  set_pref_stream(&pref_stream_core[proc_id]);

  pref_stream_train(proc_id, lineAddr, loadPC, global_hist, TRUE);
}

void pref_stream_ul1_hit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                         uns32 global_hist) {
  set_pref_stream(&pref_stream_core[proc_id]);

  pref_stream_train(proc_id, lineAddr, loadPC, global_hist, FALSE);
}

int pref_stream_train_create_stream_buffer(uns8 proc_id, Addr line_addr,
                                           Flag train, Flag create,
                                           int extra_dis) {
  int  ii;
  int  dir;
  int  lru_index     = -1;
  Flag found_closeby = FALSE;
  Addr line_index    = line_addr >> LOG2(DCACHE_LINE_SIZE);

  ASSERTM(proc_id, extra_dis == 0 || (!train && !create),
          "extra_dis should not be used when altering prefetcher state\n");

  // First check for a trained buffer
  for(ii = 0; ii < STREAM_BUFFER_N; ii++) {
    Stream_Buffer* stream = &pref_stream->stream[ii];
    if(stream->valid && stream->trained) {
      if(((stream->sp <= line_index) &&
          (stream->ep + extra_dis >= line_index) && (stream->dir == 1)) ||
         ((stream->sp >= line_index) &&
          (stream->ep - extra_dis <= line_index) && (stream->dir == -1))) {
        // found a trained buffer
        ASSERT(proc_id, proc_id == stream->proc_id);
        if(train)
          stream->train_hit++;
        return ii;
      }
    }
  }

  if(train || create) {
    for(ii = 0; ii < STREAM_BUFFER_N; ii++) {
      Stream_Buffer* stream = &pref_stream->stream[ii];
      if(stream->valid && !stream->trained) {
        if((stream->sp <= (line_index + STREAM_TRAIN_LENGTH)) &&
           (stream->sp >= (line_index - STREAM_TRAIN_LENGTH))) {
          ASSERT(proc_id, proc_id == stream->proc_id);

          if(train) {  // do these only if we are training
            // decide the train dir
            if(stream->sp > line_index)
              dir = -1;
            else
              dir = 1;
            stream->train_hit++;
            if(stream->train_hit > pref_stream->train_num) {
              stream->trained     = TRUE;
              stream->start_vline = stream->sp;
              stream->ep          = (dir > 0) ?
                             line_index + STREAM_START_DIS :
                             line_index - STREAM_START_DIS;  // BUG: 57
              // check for address space overflow
              if(get_proc_id_from_cmp_addr(
                   stream->ep << LOG2(DCACHE_LINE_SIZE)) != proc_id) {
                stream->valid = FALSE;
                return -1;
              }
              stream->dir = dir;
              DEBUG(proc_id,
                    "stream  trained stream_index:%3d sp %7s ep %7s dir %2d "
                    "miss_index %7d\n",
                    ii, hexstr64(stream->sp), hexstr64(stream->ep), stream->dir,
                    (int)line_index);
            }
          }

          return ii;
        }
      }
    }

    if(!create || found_closeby)
      return -1;
  }

  if(create) {
    // search for invalid buffer
    for(ii = 0; ii < STREAM_BUFFER_N; ii++) {
      if(!pref_stream->stream[ii].valid) {
        lru_index = ii;
        break;
      }
    }

    // search for oldest buffer

    if(lru_index == -1) {
      lru_index = 0;

      for(ii = 0; ii < STREAM_BUFFER_N; ii++) {
        if(pref_stream->stream[ii].lru < pref_stream->stream[lru_index].lru) {
          lru_index = ii;
        }
      }
      STAT_EVENT(0, REPLACE_OLD_STREAM);
      collect_stream_stats(&pref_stream->stream[lru_index]);
      if(PREF_STREAM_PER_CORE_ENABLE) {
        uns8 proc_id2 = pref_stream->stream[lru_index].sp >>
                        (58 - LOG2(DCACHE_LINE_SIZE));
        ASSERT(proc_id, proc_id == proc_id2);
      }
    }

    // create new train buffer
    Stream_Buffer* lru_stream = &pref_stream->stream[lru_index];
    lru_stream->proc_id       = proc_id;
    lru_stream->lru           = cycle_count;
    lru_stream->valid         = TRUE;
    lru_stream->sp            = line_index;
    lru_stream->ep            = line_index;
    lru_stream->train_hit     = 1;
    lru_stream->trained       = FALSE;
    lru_stream->buffer_full   = FALSE;
    lru_stream->pause         = 0;

    lru_stream->length      = STREAM_LENGTH;
    lru_stream->pref_issued = 0;
    lru_stream->pref_useful = 0;

    STAT_EVENT_ALL(STREAM_TRAIN_CREATE);
    STAT_EVENT(proc_id, CORE_STREAM_TRAIN_CREATE);
    DEBUG(proc_id,
          "create new stream : stream_no :%3d, line_index %7s sp = %7s\n",
          lru_index, hexstr64(line_index), hexstr64(lru_stream->sp));
    return lru_index;
  }

  return -1;
}

void collect_stream_stats(const Stream_Buffer* stream) {
  uns len;
  if(stream->dir == 0 || !stream->trained) {
    len = 0;
  } else if(stream->dir == 1) {
    len = stream->ep - stream->start_vline + 1;
  } else {
    len = stream->start_vline - stream->ep + 1;
  }

  if(len != 0) {
    uns8 proc_id = stream->sp >> (58 - LOG2(DCACHE_LINE_SIZE));
    STAT_EVENT(proc_id, CORE_STREAM_LENGTH_0 + MIN2(len / 10, 10));
    INC_STAT_EVENT(proc_id, CORE_CUM_STREAM_LENGTH_0 + MIN2(len / 10, 10), len);
    STAT_EVENT(proc_id,
               CORE_STREAM_TRAIN_HITS_0 + MIN2(stream->train_hit / 10, 10));
    INC_STAT_EVENT(
      proc_id, CORE_CUM_STREAM_TRAIN_HITS_0 + MIN2(stream->train_hit / 10, 10),
      stream->train_hit);
  }
}

// IGNORE
void pref_stream_throttle(uns8 proc_id) {
  int   dyn_shift = 0;
  float acc       = pref_get_accuracy(proc_id, pref_stream->hwp_info->id);

  if(acc != 1.0) {
    if(acc > PREF_ACC_THRESH_1) {
      dyn_shift += 2;
    } else if(acc > PREF_ACC_THRESH_2) {
      dyn_shift += 1;
    } else if(acc > PREF_ACC_THRESH_3) {
      dyn_shift = 0;
    } else if(acc > PREF_ACC_THRESH_4) {
      dyn_shift = dyn_shift - 1;
    } else {
      dyn_shift = dyn_shift - 2;
    }
  }
  if(acc == 1.0) {
    pref_stream->distance = 64;
    STAT_EVENT(0, PREF_DISTANCE_4);
  } else {
    if(dyn_shift >= 2) {
      pref_stream->distance = 128;
      STAT_EVENT(0, PREF_DISTANCE_5);
    } else if(dyn_shift == 1) {
      pref_stream->distance = 64;
      STAT_EVENT(0, PREF_DISTANCE_4);
    } else if(dyn_shift == 0) {
      pref_stream->distance = 32;
      STAT_EVENT(0, PREF_DISTANCE_3);
    } else if(dyn_shift == -1) {
      pref_stream->distance = 16;
      STAT_EVENT(0, PREF_DISTANCE_2);
    } else if(dyn_shift <= -2) {
      pref_stream->distance = 5;
      STAT_EVENT(0, PREF_DISTANCE_1);
    }
  }
}

////////////////////////////////////////////////////////////////////////
// Rest Used when throttling for each stream separately - NON FUNCTIONAL
Flag pref_stream_train_stream_filter(Addr line_index) {
  int ii;
  for(ii = 0; ii < TRAIN_FILTER_SIZE; ii++) {
    if(pref_stream->train_filter[ii] == line_index) {
      return TRUE;
    }
  }
  return FALSE;
}

void pref_stream_addto_train_stream_filter(Addr line_index) {
  pref_stream->train_filter[((*(pref_stream->train_filter_no))++) %
                            TRAIN_FILTER_SIZE] = line_index;
}


void pref_stream_remove_redundant_stream(int hit_index) {
  int            ii;
  Stream_Buffer* hit_stream = &pref_stream->stream[hit_index];

  for(ii = 0; ii < STREAM_BUFFER_N; ii++) {
    Stream_Buffer* stream = &pref_stream->stream[ii];
    if(ii == hit_index || !stream->valid)
      continue;
    if((stream->ep < hit_stream->ep && stream->ep > hit_stream->sp) ||
       (stream->sp < hit_stream->ep && stream->sp > hit_stream->sp)) {
      stream->valid = FALSE;
      STAT_EVENT(0, REMOVE_REDUNDANT_STREAM_STAT);
      DEBUG(
        0,
        "stream[%d] sp:0x%s ep:0x%s is removed by stream[%d] sp:0x%s ep:0x%s\n",
        ii, hexstr64(stream->sp), hexstr64(stream->ep), hit_index,
        hexstr64(hit_stream->sp), hexstr64(hit_stream->ep));
    }
  }
}

float pref_stream_acc_getacc(int index, float pref_acc) {
  float acc = pref_stream->stream[index].pref_issued > 40 ?
                ((float)pref_stream->stream[index].pref_useful) /
                  ((float)pref_stream->stream[index].pref_issued) :
                pref_acc;
  return acc;
}

void pref_stream_acc_ul1_useful(Addr line_index) {
  uns ii;
  if(!PREF_STREAM_ON) {
    return;
  }
  for(ii = 0; ii < STREAM_BUFFER_N; ii++) {
    Stream_Buffer* stream = &pref_stream->stream[ii];
    if(stream->valid && stream->trained) {
      if(((stream->start_vline <= line_index) && (stream->ep >= line_index) &&
          (stream->dir == 1)) ||
         ((stream->start_vline >= line_index) && (stream->ep <= line_index) &&
          (stream->dir == -1))) {
        // found a trained buffer
        stream->pref_useful += 1;
      }
    }
  }
}

void pref_stream_acc_ul1_issued(Addr line_index) {
  uns ii;
  for(ii = 0; ii < STREAM_BUFFER_N; ii++) {
    Stream_Buffer* stream = &pref_stream->stream[ii];
    if(stream->valid && stream->trained) {
      if(((stream->start_vline <= line_index) && (stream->ep >= line_index) &&
          (stream->dir == 1)) ||
         ((stream->start_vline >= line_index) && (stream->ep <= line_index) &&
          (stream->dir == -1))) {
        // found a trained buffer
        stream->pref_issued += 1;
      }
    }
  }
}

// IGNORE
void pref_stream_throttle_streams(Addr line_index) {
  int ii;
  for(ii = 0; ii < STREAM_BUFFER_N; ii++) {
    Stream_Buffer* stream = &pref_stream->stream[ii];
    if(stream->valid && stream->trained) {
      if(((stream->ep - PREF_ACC_DISTANCE_10 <= line_index) &&
          (stream->ep >= line_index) && (stream->dir == 1)) ||
         ((stream->ep + PREF_ACC_DISTANCE_10 >= line_index) &&
          (stream->ep <= line_index) && (stream->dir == -1))) {
        // found a trained buffer
        pref_stream_throttle_stream(ii);
        return;
      }
    }
  }
}


// IGNORE
/*
   throttle_stream_pf -> reset the stream length and train length for this
   stream buffer
*/
void pref_stream_throttle_stream(int index) {
  // FIXME: why is this here?
}

void pref_stream_per_core_done(uns proc_id) {
  set_pref_stream(&pref_stream_core[PREF_STREAM_PER_CORE_ENABLE ? proc_id : 0]);

  for(uns ii = 0; ii < STREAM_BUFFER_N; ii++) {
    Stream_Buffer* stream = &pref_stream->stream[ii];
    if(PREF_STREAM_PER_CORE_ENABLE ||
       (stream->sp >> (58 - LOG2(DCACHE_LINE_SIZE))) == proc_id) {
      collect_stream_stats(stream);
    }
  }
}

void pref_stream_throttle_fb(uns8 proc_id) {
  if(PREF_DHAL) {  // on pref_dhal, we update the dyn_degree based on sent pref
    pref_stream->distance = pref_stream->hwp_info->dyn_degree_core[proc_id];
  } else {
    pref_get_degfb(proc_id, pref_stream->hwp_info->id);
    ASSERTM(0,
            pref_stream->hwp_info->dyn_degree_core[proc_id] >= 0 &&
              pref_stream->hwp_info->dyn_degree_core[proc_id] <= PREF_MAX_DEGFB,
            "Degree: %i\n", pref_stream->hwp_info->dyn_degree_core[proc_id]);
    pref_stream->distance =
      pref_stream
        ->pref_degree_vals[pref_stream->hwp_info->dyn_degree_core[proc_id]];
    pref_stream->num_tosend =
      pref_stream
        ->num_tosend_vals[pref_stream->hwp_info->dyn_degree_core[proc_id]];
  }
}

Flag pref_stream_bw_prefetchable(uns proc_id, Addr line_addr) {
  set_pref_stream(&pref_stream_core[proc_id]);

  int idx = pref_stream_train_create_stream_buffer(proc_id, line_addr, FALSE,
                                                   FALSE, 0);
  if(idx == -1)
    return FALSE;

  Stream_Buffer* stream = &pref_stream->stream[idx];
  return stream->buffer_full;
}
