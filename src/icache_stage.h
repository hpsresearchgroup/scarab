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
 * File         : icache_stage.h
 * Author       : HPS Research Group
 * Date         : 12/7/1998
 * Description  :
 ***************************************************************************************/

#ifndef __ICACHE_STAGE_H__
#define __ICACHE_STAGE_H__

#include "globals/global_types.h"
#include "libs/cache_lib.h"
#include "stage_data.h"

/**************************************************************************************/
/* Forward Declarations */

struct Inst_Info_struct;
struct Mem_Req_struct;
struct Pb_Data_struct;

/**************************************************************************************/
/* Types */

/* name strings are in debug/debug_print.c */
typedef enum Icache_State_enum {
  IC_FETCH,
  IC_REFETCH,
  IC_FILL,
  IC_WAIT_FOR_MISS,
  IC_WAIT_FOR_REDIRECT,
  IC_WAIT_FOR_EMPTY_ROB,
  IC_WAIT_FOR_TIMER,
} Icache_State;

typedef struct Icache_Stage_struct {
  uns8       proc_id;
  Stage_Data sd; /* stage interface data */

  Icache_State state; /* state that the ICACHE is in */
  Icache_State
    next_state; /* state that the ICACHE is going to be in next cycle */

  Counter inst_count; /* instruction counter used to number ops (global counter
                         is for retired ops) */
  Inst_Info** line;   /* pointer to current line on a hit */
  Addr        line_addr;       /* address of the last cache line hit */
  Addr        fetch_addr;      /* address fetched */
  Addr        next_fetch_addr; /* address to fetch */
  Flag        off_path;        /* is the icache fetching on the correct path? */
  Flag back_on_path; /* did a recovery happen to put the machine back on path?
                      */

  Counter rdy_cycle; /* cycle that the henry icache will return data (only used
                        in henry model) */
  Counter timer_cycle; /* cycle that the icache stall timer will have elapsed
                          and the icache can fetch again */

  Cache icache;           /* the cache storage structure (caches Inst_Info *) */
  Cache icache_line_info; /* contains info about the icache lines */
  Cache
       pref_icache; /* Prefetcher cache storage structure (caches Inst_Info *) */
  char rand_wb_state[31]; /* State of random number generator for random
                             writeback */
} Icache_Stage;

typedef struct Icache_Data_struct {
  Flag fetched_by_offpath; /* fetched by an off_path op? */
  Addr offpath_op_addr;    /* PC of the off path op that fetched this line */
  Counter
       offpath_op_unique; /* unique of the off path op that fetched this line */
  uns  read_count[2];
  Flag HW_prefetch;

  Counter fetch_cycle;
  Counter onpath_use_cycle;
} Icache_Data;


/**************************************************************************************/
/* Global Variables */

Pb_Data* ic_pb_data;  // cmp cne is fine for cmp now assuming homogeneous cmp
// But decided to use array for future use


/**************************************************************************************/
/* External Variables */

extern Icache_Stage* ic;

/**************************************************************************************/
/* Prototypes */

/* vanilla hps model */
void set_icache_stage(Icache_Stage*);
void init_icache_trace(void);
void set_pb_data(Pb_Data*);
void init_icache_stage(uns8, const char*);
void reset_icache_stage(void);
void reset_all_ops_icache_stage(void);
void recover_icache_stage(void);
void redirect_icache_stage(void);
void debug_icache_stage(void);
void update_icache_stage(void);
Flag icache_fill_line(Mem_Req*);
void wp_process_icache_hit(Icache_Data* line, Addr fetch_addr);
void wp_process_icache_fill(Icache_Data* line, Mem_Req* req);
Flag icache_off_path(void);

/**************************************************************************************/

#endif /* #ifndef __ICACHE_STAGE_H__ */
