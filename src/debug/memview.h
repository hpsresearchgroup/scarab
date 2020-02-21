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
 * File         : memview.h
 * Author       : HPS Research Group
 * Date         : 8/7/2013
 * Description  : Tracing memory-related events for visualization.
 ***************************************************************************************/

#ifndef __MEMVIEW_H__
#define __MEMVIEW_H__

#include "globals/enum.h"
#include "globals/global_types.h"

/**************************************************************************************/
/* Forward Declarations */

struct Mem_Req_struct;

/**************************************************************************************/
/* Types */

#define MEMVIEW_DRAM_EVENT_LIST(elem)                               \
  elem(IDLE) elem(PRECHARGE) elem(ACTIVATE) elem(COLUMN) elem(READ) \
    elem(WRITE) elem(BUS) elem(REFRESH)

DECLARE_ENUM(Memview_Dram_Event, MEMVIEW_DRAM_EVENT_LIST, MEMVIEW_DRAM_);

#define MEMVIEW_MEMQUEUE_EVENT_LIST(elem) elem(ARRIVE) elem(DEPART)

DECLARE_ENUM(Memview_Memqueue_Event, MEMVIEW_MEMQUEUE_EVENT_LIST,
             MEMVIEW_MEMQUEUE_);

#define MEMVIEW_NOTE_TYPE_LIST(elem) \
  elem(GENERAL) elem(DRAM_MODE) elem(DRAM_BATCH) elem(DRAM_UNBLOCK)

DECLARE_ENUM(Memview_Note_Type, MEMVIEW_NOTE_TYPE_LIST, MEMVIEW_NOTE_);

/**************************************************************************************/
/* Prototypes */

/* Initialize tracing */
void memview_init(void);

/* Record DRAM event */
void memview_dram(Memview_Dram_Event event, struct Mem_Req_struct* req,
                  uns flat_bank_id, Counter start, Counter end);

/* Record a potential segment of the critical path through DRAM reqs */
void memview_dram_crit_path(const char* from_type_str, uns from_index,
                            const char* to_type_str, uns to_index,
                            Counter start, Counter end);

/* Record mem queue departure */
void memview_memqueue(Memview_Memqueue_Event event, struct Mem_Req_struct* req);

/* Record type change in memory request */
void memview_req_changed_type(struct Mem_Req_struct* req);

/* Record L1 access */
void memview_l1(struct Mem_Req_struct* req);

/* Record core stall */
void memview_core_stall(uns proc_id, Flag stalled, Flag mem_blocked);

/* Record FUs busy */
void memview_fus_busy(uns proc_id, uns fus_busy);

/* Record a note (this can be used for rare but informative events,
   such as status of periodically updated mechanisms) */
void memview_note(Memview_Note_Type type, const char* str);

/* Clean up */
void memview_done(void);

/*************************************************************/

#endif /*  __MEMVIEW_H__*/
