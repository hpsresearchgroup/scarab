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
 * File         : map.h
 * Author       : HPS Research Group
 * Date         : 2/16/1999
 * Description  :
 ***************************************************************************************/

#ifndef __MAP_H__
#define __MAP_H__

#include "isa/isa_macros.h"
#include "libs/hash_lib.h"
#include "op.h"

/**************************************************************************************/
/* Types */

typedef struct Map_Entry_struct {
  Op*     op;         /* last op to write (invalid when committed) */
  Counter op_num;     /* op number of the last op to write (not cleared, only
                         overwritten) */
  Counter unique_num; /* unique number of the last op to write (not cleared,
                         only overwritten) */
} Map_Entry;

typedef struct Map_Data_struct {
  /* store information about the last op to write each register */
  uns8      proc_id;
  Map_Entry reg_map[NUM_REG_IDS * 2];
  Flag      map_flags[NUM_REG_IDS];

  Map_Entry last_store[2];
  Flag      last_store_flag;

  Hash_Table oracle_mem_hash;

  Wake_Up_Entry* free_list_head;
  uns            wake_up_entries;
  uns            active_wake_up_entries;
} Map_Data;


/**************************************************************************************/
/* External Variables */

extern Map_Data* map_data;


/**************************************************************************************/
/* Prototypes */

Map_Data* set_map_data(Map_Data*);
void      init_map(uns8);
void      recover_map(void);
void      rebuild_offpath_map(void);
void      reset_map(void);
void      map_op(Op*);
void      map_mem_dep(Op*);
void      wake_up_ops(Op*, Dep_Type, void (*)(Op*, Op*, uns8));
void      free_wake_up_list(Op*);
void      add_to_wake_up_lists(Op*, Op_Info*, void (*)(Op*, Op*, uns8));

void add_src_from_op(Op*, Op*, Dep_Type);
void add_src_from_map_entry(Op*, Map_Entry*, Dep_Type);

void simple_wake(Op*, Op*, uns8);
void delete_store_hash_entry(Op*);

void clear_not_rdy_bit(Op*, uns);
Flag test_not_rdy_bit(Op*, uns);
void set_not_rdy_bit(Op*, uns);

/**************************************************************************************/

#endif /* #ifndef __MAP_H__ */
