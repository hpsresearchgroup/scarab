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
 * File         : thread.c
 * Author       : HPS Research Group
 * Date         : 5/2/2000
 * Description  :
 ***************************************************************************************/

#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"
#include "op_pool.h"

#include "frontend/frontend.h"
#include "thread.h"

#include "core.param.h"
#include "debug/debug.param.h"
#include "general.param.h"


/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_THREAD, ##args)


/**************************************************************************************/
/* Prototypes */

/**************************************************************************************/
/* set_thread_data: */
void set_thread_data(Thread_Data* new_td) {
  td = new_td;
}

/**************************************************************************************/
/* init_thread: */

void init_thread(Thread_Data* td, char* argv[], char* envp[]) {
  set_map_data(&td->map_data);
  init_map(0);
  init_list(&td->seq_op_list, "SEQ_OP_LIST", sizeof(Op*), TRUE);
}


/**************************************************************************************/
/* recover_thread: */

void recover_thread(Thread_Data* td, Addr new_pc, Counter op_num,
                    uns64 inst_uid, Flag remain_wrongpath) {
  recover_seq_op_list(td, op_num);
  if(FETCH_OFF_PATH_OPS) {
    if(remain_wrongpath) {
      frontend_redirect(td->proc_id, inst_uid, new_pc);
    } else {
      frontend_recover(td->proc_id, inst_uid);
    }
    ASSERTM(td->proc_id, new_pc == frontend_next_fetch_addr(td->proc_id),
            "Scarab's recovery addr 0x%llx does not match frontend's recovery "
            "addr 0x%llx\n",
            new_pc, frontend_next_fetch_addr(td->proc_id));
  }
  recover_map();
}


/**************************************************************************************/
/* add_to_seq_op_list: */

void add_to_seq_op_list(Thread_Data* td, Op* op) {
  Op** op_p;
  ASSERT(td->proc_id, op);
  ASSERT(td->proc_id, td->proc_id == op->proc_id);
  ASSERT(td->proc_id, op->op_pool_valid);
  op_p  = dl_list_add_tail(&td->seq_op_list);
  *op_p = op;
  DEBUG(td->proc_id, "Adding to seq op list  op:%s  count:%d\n",
        unsstr64(op->op_num), td->seq_op_list.count);
  ASSERT(td->proc_id, (td->seq_op_list.count < 8193));
}


/**************************************************************************************/
/* remove_from_seq_op_list: */

void remove_from_seq_op_list(Thread_Data* td, Op* op) {
  Op** op_p = dl_list_remove_head(&td->seq_op_list);
  ASSERT(td->proc_id, op_p);
  ASSERT(td->proc_id, td->proc_id == (*op_p)->proc_id);
  ASSERT(td->proc_id, *op_p);
  ASSERT(td->proc_id, (*op_p)->op_pool_valid);
  ASSERTM(td->proc_id, *op_p == op,
          "op_p_num: %s op_num: %s dis_op_p: %s dis_op: %s\n",
          unsstr64((*op_p)->op_num), unsstr64(op->op_num),
          disasm_op((*op_p), TRUE), disasm_op(op, TRUE));
  ASSERT(td->proc_id, (*op_p)->unique_num == op->unique_num);
  DEBUG(td->proc_id, "Removing op from seq op list  op:%s  count:%d\n",
        unsstr64(op->op_num), td->seq_op_list.count);
}


/**************************************************************************************/
/* recover_seq_op_list: */

void recover_seq_op_list(Thread_Data* td, Counter op_num) {
  // Traverse the sequential op list and remove everything younger than the
  // recovering op
  Op** op_p = (Op**)list_start_head_traversal(&td->seq_op_list);
  if(op_p) {
    ASSERT(td->proc_id, *op_p);
    ASSERT(td->proc_id, td->proc_id == (*op_p)->proc_id);
    if((*op_p)->op_num > op_num) {
      ASSERTM(td->proc_id, (*op_p)->op_num == op_num + 1,
              "Oldest in-flight op_num:%lld, recovery op_num:%lld\n",
              (*op_p)->op_num, op_num + 1);
      clear_list(&td->seq_op_list);
    } else {
      for(; op_p; op_p = (Op**)list_next_element(&td->seq_op_list)) {
        ASSERT(td->proc_id, (*op_p)->op_num <= op_num);
        if((*op_p)->op_num == op_num) {
          clip_list_at_current(&td->seq_op_list);
          break;
        }
      }
    }
  }

  DEBUG(td->proc_id, "Recovering seq op list  op:%s  count:%d\n",
        unsstr64(op_num), td->seq_op_list.count);
}


/**************************************************************************************/
/* thread_map: sets the dependencies in the thread Op_Info struct */

void thread_map_op(Op* op) {
  ASSERT(td->proc_id, map_data == &td->map_data);
  ASSERT(td->proc_id, td->proc_id == map_data->proc_id);
  ASSERT(td->proc_id, td->proc_id == op->proc_id);
  /* call map.c map_op, which will use the thread's map data to set
     the dependency information in op->thread_info */
  map_op(op);
}

/**************************************************************************************/
/* thread_map_mem_dep: sets the dependencies in the thread Op_Info struct */

void thread_map_mem_dep(Op* op) {
  ASSERT(td->proc_id, map_data == &td->map_data);
  ASSERT(td->proc_id, td->proc_id == map_data->proc_id);
  ASSERT(td->proc_id, td->proc_id == op->proc_id);
  /* call map.c map_op, which will use the thread's map data to set
     the dependency information in op->thread_info */
  map_mem_dep(op);
}

/**************************************************************************************/
/* remove_next_from_seq_op_list: */

Op* remove_next_from_seq_op_list(Thread_Data* td) {
  Op** op_p = dl_list_remove_head(&td->seq_op_list);
  DEBUG(td->proc_id, "Removing op from seq op list  op:%s  count:%d\n",
        unsstr64((*op_p)->op_num), td->seq_op_list.count);
  return *op_p;
}

/**************************************************************************************/
/* reset_seq_op_list: */

void reset_seq_op_list(Thread_Data* td) {
  // Traverse the sequential op list and remove and free every op
  Op** op_p = (Op**)list_start_head_traversal(&td->seq_op_list);
  for(; op_p; op_p = (Op**)list_next_element(&td->seq_op_list)) {
    ASSERT(td->proc_id, td->proc_id == (*op_p)->proc_id);
    free_op(*op_p);
  }
  clear_list(&td->seq_op_list);


  DEBUG(td->proc_id, "Reseting seq op list   count:%d\n",
        td->seq_op_list.count);
}
