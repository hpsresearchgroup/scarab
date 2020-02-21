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
 * File         : thread.h
 * Author       : HPS Research Group
 * Date         : 5/2/2000
 * Description  :
 ***************************************************************************************/

#ifndef __THREAD_H__
#define __THREAD_H__

#include "globals/global_types.h"
#include "libs/list_lib.h"
#include "map.h"


/**************************************************************************************/
/* Types */

////////////////////////////////////////////////////////////////////////////////////////
// Pipeline Gating
typedef struct Thread_Info_struct {
  /* related to fetch throttle */
  /* this one should be under one big header file */
  int low_conf_count;
  int fetch_throttle_adjust;
  // int fetch_throttle_br_th_adjust;
  int     fetch_br_count;
  Op*     last_bp_miss_op;
  Counter corrpred_counter;
  Counter mispred_counter;
  Counter last_l1_miss_time;
} Thread_Info;

////////////////////////////////////////////////////////////////////////////////////////


typedef struct Thread_struct {
  uns8     proc_id;
  Addr     inst_addr;
  Map_Data map_data;
  List     seq_op_list;
  ///////////////////////////////////////////////////
  // Pipeline Gating
  Thread_Info td_info;
  ///////////////////////////////////////////////////
} Thread_Data;


/**************************************************************************************/
/* External variables */

extern Thread_Data* td; /* here for now, variable declared in sim.c */
/* if we ever go MT, this will turn into an array */


/**************************************************************************************/
/* Prototypes */

void set_thread_data(Thread_Data*);
void init_thread(Thread_Data*, char* [], char* []);
void recover_thread(Thread_Data*, Addr, Counter, uns64, Flag);
void add_to_seq_op_list(Thread_Data*, Op*);
void remove_from_seq_op_list(Thread_Data*, Op*);
void thread_map_op(Op*);
void thread_map_mem_dep(Op*);
void recover_seq_op_list(Thread_Data*, Counter);
Op*  remove_next_from_seq_op_list(Thread_Data*);
void reset_seq_op_list(Thread_Data*);

/**************************************************************************************/

#endif /* #ifndef __THREAD_H__ */
