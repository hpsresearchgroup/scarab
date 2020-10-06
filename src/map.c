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
 * File         : map.c
 * Author       : HPS Research Group
 * Date         : 2/16/1999
 * Description  :
 ***************************************************************************************/

#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"

#include "map.h"
#include "model.h"
#include "thread.h"

#include "core.param.h"
#include "debug/debug.param.h"
#include "memory/memory.param.h"

#include "cmp_model.h"
#include "libs/hash_lib.h"
#include "statistics.h"

/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_MAP, ##args)
#define DEBUGU(proc_id, args...) _DEBUGU(proc_id, DEBUG_MAP, ##args)

#define WAKE_UP_ENTRIES_INC 256 /* default 256 */
#define MEM_ADDR_SRC \
  0 /* address for memory instructions calculated off source 0 */

#define MEM_MAP_ENTRY_SIZE_LOG 3
#define MEM_MAP_ENTRY_SIZE (1 << MEM_MAP_ENTRY_SIZE_LOG)
#define MEM_MAP_BYTE_IN_ENTRY(va) ((va) & (MEM_MAP_ENTRY_SIZE - 1))
#define MEM_MAP_ENTRY_ADDR(va) ((va) & ~(Addr)(MEM_MAP_ENTRY_SIZE - 1))

#define MEM_MAP_KEY(va) ((va) >> MEM_MAP_ENTRY_SIZE_LOG)
#define MEM_MAP_BYTE_INDEX(byte, off_path) \
  ((byte) + ((off_path) ? MEM_MAP_ENTRY_SIZE : 0))

/**************************************************************************************/
/* Types */

typedef struct Mem_Map_Entry_struct {
  Op* op[2 * MEM_MAP_ENTRY_SIZE]; /* last op to write (invalid when committed),
                                   * first half onpath, second half offpath*/
  uns flag_mask;                  /* offpath flags, one per byte */
  uns store_mask;                 /* shows position of all distinct stores
                                   * supplying a partial value to this map entry */
} Mem_Map_Entry;

/* Data structure for easy traversal of memory map hash given an
   access with an address and size */
typedef struct Mem_Map_Traversal_struct {
  Addr entry_addr; /* Entry address, can be used by caller */
  Addr first_entry_addr;
  Addr last_entry_addr;
  uns  byte; /* Byte within entry, can be used by caller */
  uns  last_byte;
  uns  first_entry_first_byte;
  uns  last_entry_last_byte;
} Mem_Map_Traversal;

/**************************************************************************************/
/* External variables */

extern Op invalid_op;


/**************************************************************************************/
/* Global Variables */

Map_Data* map_data = NULL;

const char* const dep_type_names[NUM_DEP_TYPES] = {
  "REG_DATA",
  "MEM_ADDR",
  "MEM_DATA",
};

/**************************************************************************************/
/* Static prototypes */

static inline void read_reg_map(Op*);
static inline void read_store_map(Op*);
static inline void update_map(Op*);

static inline void expand_wake_up_entries(void);
static inline void update_store_hash(Op* op);
static inline Op*  add_store_deps(Op* op);
static inline void update_map_entry(Op* op, Map_Entry* map_entry);
static inline void recover_mem_map_entry(void* hash_entry, void* arg);

/* memory map hash traversal */
static inline void mem_map_entry_traversal_init(Mem_Map_Traversal* traversal,
                                                Addr va, uns size);
static inline Flag mem_map_entry_traversal_done(Mem_Map_Traversal* traversal);
static inline void mem_map_entry_traversal_next(Mem_Map_Traversal* traversal);
static inline void mem_map_byte_traversal_init(Mem_Map_Traversal* traversal);
static inline Flag mem_map_byte_traversal_done(Mem_Map_Traversal* traversal);
static inline void mem_map_byte_traversal_next(Mem_Map_Traversal* traversal);

/**************************************************************************************/
/* set_map_data: */

Map_Data* set_map_data(Map_Data* new_map_data) {
  Map_Data* old_map_data = map_data;
  map_data               = new_map_data;
  return old_map_data;
}


/**************************************************************************************/
/* init_map: */

void init_map(uns8 proc_id) {
  uns ii;

  ASSERT(proc_id, map_data == &td->map_data);
  memset(map_data, 0, sizeof(Map_Data));
  map_data->proc_id = proc_id;

  /* Initialize the register "last write" map */
  for(ii = 0; ii < NUM_REG_IDS * 2; ii++) {
    map_data->reg_map[ii].op     = &invalid_op;
    map_data->reg_map[ii].op_num = 0;
  }
  for(ii = 0; ii < NUM_REG_IDS; ii++)
    map_data->map_flags[ii] = FALSE;

  map_data->last_store[0].op     = &invalid_op;
  map_data->last_store[0].op_num = 0;
  map_data->last_store[1].op     = &invalid_op;
  map_data->last_store[1].op_num = 0;

  /* Allocate the wake_up_entry pool. */
  expand_wake_up_entries();

  /* Initialize the memory dependence hash table. The number of
     buckets matters since we scan all entries (and all buckets) on
     branch misprediction recovery, so we want to keep the number of
     buckets large enough to avoid collisions but small enough to
     keep the scan fast. Since the number of entries is roughly at
     most the number of in-flight stores, we set the number of
     buckets to the size of instruction window. */
  init_hash_table(&map_data->oracle_mem_hash, "oracle mem dependence map",
                  NODE_TABLE_SIZE, sizeof(Mem_Map_Entry));
}


/**************************************************************************************/
/* recover_map: quick recover back to on path state */

void recover_map() {
  uns ii;
  DEBUG(map_data->proc_id, "Recovering register map\n");
  for(ii = 0; ii < NUM_REG_IDS; ii++)
    map_data->map_flags[ii] = FALSE;
  map_data->last_store_flag = FALSE;
  hash_table_scan(&map_data->oracle_mem_hash, recover_mem_map_entry, NULL);
  rebuild_offpath_map();
}

/**************************************************************************************/
/* recover_mem_map_entry: */

void recover_mem_map_entry(void* hash_entry, void* arg) {
  Mem_Map_Entry* entry = (Mem_Map_Entry*)hash_entry;
  entry->flag_mask     = 0;
}

/**************************************************************************************/
/* rebuild_offpath_map: rebuild the offpath half of map structures
   using the sequential op list from a Thread. Make sure you recover
   the seq_op_list first */

void rebuild_offpath_map() {
  DEBUGU(map_data->proc_id, "Rebuilding map\n");

  ASSERT(map_data->proc_id, map_data->proc_id == td->proc_id);

  /* First find the oldest offpath op */
  Op** op_p = (Op**)list_start_head_traversal(&td->seq_op_list);
  while(op_p && !(*op_p)->off_path) {
    op_p = (Op**)list_next_element(&td->seq_op_list);
  }

  /* rebuild the map starting with the first offpath op */
  for(; op_p; op_p = (Op**)list_next_element(&td->seq_op_list)) {
    update_map(*op_p);
    if((*op_p)->table_info->mem_type == MEM_ST) {
      update_store_hash(*op_p);
    }
  }
}


/**************************************************************************************/
/* expand_wake_up_pool: */

static inline void expand_wake_up_entries() {
  Wake_Up_Entry* new_pool = (Wake_Up_Entry*)calloc(WAKE_UP_ENTRIES_INC,
                                                   sizeof(Wake_Up_Entry));
  uns            ii;

  DEBUGU(map_data->proc_id, "Expanding wake up pool to size %d\n",
         (map_data->wake_up_entries + WAKE_UP_ENTRIES_INC));
  for(ii = 0; ii < WAKE_UP_ENTRIES_INC - 1; ii++)
    new_pool[ii].next = &new_pool[ii + 1];
  new_pool[ii].next        = map_data->free_list_head;
  map_data->free_list_head = &new_pool[0];
  map_data->wake_up_entries += WAKE_UP_ENTRIES_INC;
  ASSERT(map_data->proc_id,
         map_data->wake_up_entries <= WAKE_UP_ENTRIES_INC * 128);
}


/**************************************************************************************/
/* map_op: involves two things: setting up the src array in op_info
   and updating the current map state based on the op's output values.
   note that this function does nothing for memory dependencies.  you
   must call map_mem_dep after oracle_exec to properly handle them. */

void map_op(Op* op) {
  ASSERT(map_data->proc_id, op);
  ASSERT(map_data->proc_id, map_data->proc_id == op->proc_id);

  read_reg_map(op);   /* set reg sources */
  read_store_map(op); /* set addr dependency on last store */
  update_map(op);     /* update reg and last store maps */
}

/**************************************************************************************/
/* read_reg_map: read and set srcs based on registers */

static inline void read_reg_map(Op* op) {
  uns ii;
  for(ii = 0; ii < op->table_info->num_src_regs; ii++) {
    uns        id        = op->inst_info->srcs[ii].id;
    uns        ind       = id << 1 | map_data->map_flags[id];
    Map_Entry* map_entry = &map_data->reg_map[ind];

    DEBUG(map_data->proc_id,
          "Reading map  op_num:%s  off_path:%d  id:%d  flag:%d  ind:%d\n",
          unsstr64(op->op_num), op->off_path, id, map_data->map_flags[id], ind);

    add_src_from_map_entry(op, map_entry, REG_DATA_DEP);
    /* address predictor is called if op is a load & this is first mem op reg
     * read for this reg instance */
  }
}


/**************************************************************************************/
/* read_store_map: used to make mem ops dependent on the last store
   (no speculative loads) */

static inline void read_store_map(Op* op) {
  if(!MEM_OBEY_STORE_DEP || MEM_OOO_STORES)
    return;

  if(op->table_info->mem_type) {
    uns        ind       = map_data->last_store_flag;
    Map_Entry* map_entry = &map_data->last_store[ind];

    DEBUG(map_data->proc_id,
          "Reading store map  op_num:%s  off_path:%d  flag:%d  ind:%d\n",
          unsstr64(op->op_num), op->off_path, map_data->last_store_flag, ind);
    add_src_from_map_entry(op, map_entry, MEM_ADDR_DEP);
  }
}


/**************************************************************************************/
/* update_map: write the register map and the last store map if necessary */

static inline void update_map(Op* op) {
  int ii;

  ASSERT(map_data->proc_id, map_data->proc_id == op->proc_id);
  /* update the register map if the op produces a value */
  for(ii = 0; ii < op->table_info->num_dest_regs; ii++) {
    uns        id        = op->inst_info->dests[ii].id;
    uns        ind       = id << 1 | op->off_path;
    Map_Entry* map_entry = &map_data->reg_map[ind];

    DEBUG(map_data->proc_id,
          "Writing map  op_num:%s  off_path:%d  id:%d  flag:%d  ind:%d\n",
          unsstr64(op->op_num), op->off_path, id, map_data->map_flags[id], ind);

    map_entry->op           = op;
    map_entry->op_num       = op->op_num;
    map_entry->unique_num   = op->unique_num;
    map_data->map_flags[id] = op->off_path;
  }

  /* update the map if the op is a store */
  if(op->table_info->mem_type == MEM_ST) {
    uns        ind       = op->off_path;
    Map_Entry* map_entry = &map_data->last_store[ind];

    DEBUG(map_data->proc_id,
          "Writing store map  op_num:%s  off_path:%d  flag:%d  ind:%d\n",
          unsstr64(op->op_num), op->off_path, map_data->last_store_flag, ind);
    map_entry->op             = op;
    map_entry->op_num         = op->op_num;
    map_entry->unique_num     = op->unique_num;
    map_data->last_store_flag = op->off_path;
  }
}


/**************************************************************************************/
/* update_map_entry */

inline void update_map_entry(Op* op, Map_Entry* map_entry) {
  ASSERT(map_data->proc_id, op);
  ASSERT(map_data->proc_id, map_entry);
  ASSERT(map_data->proc_id, map_entry->op);
  map_entry->op         = op;
  map_entry->op_num     = op->op_num;
  map_entry->unique_num = op->unique_num;
}


/**************************************************************************************/
/* map_mem_dep */

void map_mem_dep(Op* op) {
  if(!MEM_OBEY_STORE_DEP)
    return;
  if(op->table_info->mem_type == MEM_ST)
    update_store_hash(op);
  if(op->table_info->mem_type == MEM_LD)
    add_store_deps(op);
}

/**************************************************************************************/
/* map_mem_*_traversal_*: these functions traverse the memory map
   entries and bytes within those entries given an access with an
   address and size. */

static inline void mem_map_entry_traversal_init(Mem_Map_Traversal* traversal,
                                                Addr va, uns size) {
  Addr last_va = ADDR_PLUS_OFFSET(va, size - 1);  // last byte in access
  traversal->first_entry_addr       = MEM_MAP_ENTRY_ADDR(va);
  traversal->last_entry_addr        = MEM_MAP_ENTRY_ADDR(last_va);
  traversal->entry_addr             = traversal->first_entry_addr;
  traversal->first_entry_first_byte = MEM_MAP_BYTE_IN_ENTRY(va);
  traversal->last_entry_last_byte   = MEM_MAP_BYTE_IN_ENTRY(last_va);

  if(0 == size) {
    // special case - if the size is 0, we shouldn't do a traversal at all, so
    // force the traversal to be "done"
    traversal->entry_addr = ADDR_PLUS_OFFSET(traversal->last_entry_addr,
                                             MEM_MAP_ENTRY_SIZE);
    ASSERT(get_proc_id_from_cmp_addr(va),
           mem_map_entry_traversal_done(traversal));
  }
}

static inline Flag mem_map_entry_traversal_done(Mem_Map_Traversal* traversal) {
  return traversal->entry_addr ==
         ADDR_PLUS_OFFSET(traversal->last_entry_addr, MEM_MAP_ENTRY_SIZE);
}

static inline void mem_map_entry_traversal_next(Mem_Map_Traversal* traversal) {
  traversal->entry_addr = ADDR_PLUS_OFFSET(traversal->entry_addr,
                                           MEM_MAP_ENTRY_SIZE);
}

static inline void mem_map_byte_traversal_init(Mem_Map_Traversal* traversal) {
  traversal->byte = traversal->entry_addr == traversal->first_entry_addr ?
                      traversal->first_entry_first_byte :
                      0;
  traversal->last_byte = traversal->entry_addr == traversal->last_entry_addr ?
                           traversal->last_entry_last_byte :
                           MEM_MAP_ENTRY_SIZE - 1;
  ASSERT(0, traversal->byte <= traversal->last_byte);
}

static inline Flag mem_map_byte_traversal_done(Mem_Map_Traversal* traversal) {
  return traversal->byte > traversal->last_byte;
}

static inline void mem_map_byte_traversal_next(Mem_Map_Traversal* traversal) {
  traversal->byte++;
}

/**************************************************************************************/
/* delete_store_hash_entry */

void delete_store_hash_entry(Op* op) {
  Addr              va = op->oracle_info.va;
  Mem_Map_Traversal traversal;

  ASSERT(map_data->proc_id, map_data->proc_id == op->proc_id);

  /* Iterate through each entry that was written to by the op */
  for(mem_map_entry_traversal_init(&traversal, va, op->oracle_info.mem_size);
      !mem_map_entry_traversal_done(&traversal);
      mem_map_entry_traversal_next(&traversal)) {
    Mem_Map_Entry* mem_map_p = (Mem_Map_Entry*)hash_table_access(
      &map_data->oracle_mem_hash, MEM_MAP_KEY(traversal.entry_addr));

    if(!mem_map_p)
      continue;

    /* Iterate through each byte written to by the op (within this entry) */
    for(mem_map_byte_traversal_init(&traversal);
        !mem_map_byte_traversal_done(&traversal);
        mem_map_byte_traversal_next(&traversal)) {
      uns ind = MEM_MAP_BYTE_INDEX(traversal.byte, op->off_path);
      if(TESTBIT(mem_map_p->store_mask, ind) && mem_map_p->op[ind] == op) {
        CLRBIT(mem_map_p->store_mask, ind);
      }
    }
    if(!mem_map_p->store_mask) {
      hash_table_access_delete(&map_data->oracle_mem_hash,
                               MEM_MAP_KEY(traversal.entry_addr));
    }
  }
}

/**************************************************************************************/
/* add_store_deps: */

static inline Op* add_store_deps(Op* op) {
  Addr              va            = op->oracle_info.va;
  Op*               last_src_op   = NULL;
  uns               orig_num_srcs = op->oracle_info.num_srcs;
  Mem_Map_Traversal traversal;

  ASSERT(map_data->proc_id, map_data->proc_id == op->proc_id);

  /* Iterate through each entry that is read by the op */
  for(mem_map_entry_traversal_init(&traversal, va, op->oracle_info.mem_size);
      !mem_map_entry_traversal_done(&traversal);
      mem_map_entry_traversal_next(&traversal)) {
    Mem_Map_Entry* mem_map_p = (Mem_Map_Entry*)hash_table_access(
      &map_data->oracle_mem_hash, MEM_MAP_KEY(traversal.entry_addr));

    if(!mem_map_p)
      continue;

    /* Iterate through each byte read by the op (within this entry) */
    for(mem_map_byte_traversal_init(&traversal);
        !mem_map_byte_traversal_done(&traversal);
        mem_map_byte_traversal_next(&traversal)) {
      uns ind = MEM_MAP_BYTE_INDEX(
        traversal.byte, TESTBIT(mem_map_p->flag_mask, traversal.byte));
      if(!TESTBIT(mem_map_p->store_mask, ind))
        continue; /* ensure mem_map_p info is valid */
      Op* src_op = mem_map_p->op[ind];
      ASSERTM(op->proc_id,
              BYTE_OVERLAP(src_op->oracle_info.va, src_op->oracle_info.mem_size,
                           va, op->oracle_info.mem_size),
              "%d@0x%08x and %d@0x%08x\n", src_op->oracle_info.mem_size,
              (uns32)src_op->oracle_info.va, op->oracle_info.mem_size,
              (uns32)va);
      if(MEM_OOO_STORES && !src_op->marked) {
        add_src_from_op(op, src_op, MEM_DATA_DEP);
        src_op->marked = TRUE;  // mark op to avoid adding duplicate sources
        STAT_EVENT(op->proc_id, FORWARDED_LD);
      }
      if(!last_src_op ||
         last_src_op->op_num <
           src_op->op_num) { /* take latest store dependency only */
        last_src_op = src_op;
      }
    }
  }

  if(!last_src_op) {
    STAT_EVENT(op->proc_id, LD_NO_FORWARD);
    return NULL; /* No dependency found */
  }

  ASSERT(op->proc_id, last_src_op->op_num < op->op_num || op->off_path);
  if(MEM_OOO_STORES) {
    /* unmark all ops we marked earlier */
    for(uns ii = orig_num_srcs; ii < op->oracle_info.num_srcs; ii++) {
      ASSERT(op->proc_id, op->oracle_info.src_info[ii].op->marked);
      op->oracle_info.src_info[ii].op->marked = FALSE;
    }
  } else {
    add_src_from_op(op, last_src_op, MEM_DATA_DEP);
    STAT_EVENT(op->proc_id, FORWARDED_LD);
  }
  return last_src_op;
}


/**************************************************************************************/
/* update_store_hash: */

static inline void update_store_hash(Op* op) {
  Mem_Map_Entry*    mem_map_p;
  Addr              va = op->oracle_info.va;
  Mem_Map_Traversal traversal;

  ASSERT(map_data->proc_id, map_data->proc_id == op->proc_id);

  /* Iterate through each entry that was written to by the op */
  for(mem_map_entry_traversal_init(&traversal, va, op->oracle_info.mem_size);
      !mem_map_entry_traversal_done(&traversal);
      mem_map_entry_traversal_next(&traversal)) {
    Flag new_entry = FALSE;
    mem_map_p      = (Mem_Map_Entry*)hash_table_access_create(
      &map_data->oracle_mem_hash, MEM_MAP_KEY(traversal.entry_addr),
      &new_entry);

    if(new_entry) {
      mem_map_p->flag_mask  = 0;
      mem_map_p->store_mask = 0;
    }

    /* Iterate through each byte written to by the op (within this entry) */
    for(mem_map_byte_traversal_init(&traversal);
        !mem_map_byte_traversal_done(&traversal);
        mem_map_byte_traversal_next(&traversal)) {
      DEFBIT(mem_map_p->flag_mask, traversal.byte, op->off_path);
      uns ind = MEM_MAP_BYTE_INDEX(traversal.byte, op->off_path);
      SETBIT(mem_map_p->store_mask, ind);
      mem_map_p->op[ind] = op;
    }
  }
}

/**************************************************************************************/
/* wake_up_ops: */

void wake_up_ops(Op* op, Dep_Type type, void (*wake_action)(Op*, Op*, uns8)) {
  Wake_Up_Entry* temp;


  _DEBUG(op->proc_id, DEBUG_REPLAY,
         "Waking up ops from src_op:%s unique:%s type:%s\n",
         unsstr64(op->op_num), unsstr64(op->unique_num), dep_type_names[type]);
  ASSERTM(op->proc_id, !op->wake_up_signaled[type] || op->replay,
          "op_num:%s op:%s off:%d\n", unsstr64(op->op_num), disasm_op(op, TRUE),
          op->off_path);

  ASSERT(op->proc_id, wake_action);
  for(temp = op->wake_up_head; temp; temp = temp->next) {
    Op*     dep_op         = temp->op;
    Counter dep_unique_num = temp->unique_num;

    ASSERT(op->proc_id, dep_op);

    if(temp->dep_type != type)
      continue;
    /* if the stored unique num is not the same as the op pool entry, the op has
           been reclaimed and the wake up should be ignored */
    if(dep_op->unique_num == dep_unique_num && dep_op->op_pool_valid) {
      ASSERTM(op->proc_id, op->proc_id == dep_op->proc_id,
              "dep_op proc_id: %u, valid: %u\n", dep_op->proc_id,
              dep_op->op_pool_valid);
      if(test_not_rdy_bit(dep_op, temp->rdy_bit)) {
        DEBUG(dep_op->proc_id, "Waking up  op_num:%s\n",
              unsstr64(dep_op->op_num));

        ASSERTM(dep_op->proc_id, test_not_rdy_bit(dep_op, temp->rdy_bit),
                "dep_op_num:%s  not_rdy_vector:%x\n", unsstr64(dep_op->op_num),
                dep_op->srcs_not_rdy_vector);

        /* unset the not ready bit for this source */
        clear_not_rdy_bit(dep_op, temp->rdy_bit);

        /* call the wake action function */
        wake_action(op, dep_op, temp->rdy_bit);
      }
    }
  }
  op->wake_up_signaled[type] = TRUE;
}

/**************************************************************************************/
/* add to wake up lists */

void add_to_wake_up_lists(Op* op, Op_Info* op_info,
                          void (*wake_action)(Op*, Op*, uns8)) {
  uns  ii;
  Flag dep_on_in_window_store = FALSE;
  UNUSED(dep_on_in_window_store);

  ASSERT(map_data->proc_id, op);
  ASSERT(map_data->proc_id, op_info);
  ASSERT(map_data->proc_id, op->proc_id == map_data->proc_id);

  for(ii = 0; ii < op_info->num_srcs; ii++) {
    Src_Info* src_info = &op_info->src_info[ii];
    Op*       src_op   = src_info->op;

    if((OBEY_REG_DEP || src_info->type != REG_DATA_DEP) &&
       src_op->op_pool_valid &&
       src_op->unique_num ==
         src_info->unique_num) {  // CMP proc_id comparison here because it
                                  // happens that an op object can be reused
                                  // with same unique number but different
                                  // proc_id -> One global unique num and unique
                                  // num per core separately.


      /* make sure the source op is still in the machine */
      /* add to the src op's wake up list regardless of whether
             it has already produced a result or not */
      Wake_Up_Entry* wake;

      ASSERTM(
        op->proc_id, op->proc_id == src_op->proc_id,
        "op num: %llu fetch: %llu, src_op num: %llu unique: %llu fetch: %llu\n",
        op->op_num, op->fetch_cycle, src_op->op_num, src_op->unique_num,
        src_op->fetch_cycle);

      if(map_data->free_list_head == NULL) {
        ASSERT(map_data->proc_id,
               map_data->active_wake_up_entries == map_data->wake_up_entries);
        expand_wake_up_entries();
      }

      if(src_info->type == MEM_DATA_DEP)
        dep_on_in_window_store = TRUE;

      wake = map_data->free_list_head;
      map_data->active_wake_up_entries++;
      map_data->free_list_head = wake->next;

      wake->op         = op;
      wake->unique_num = op->unique_num;
      wake->dep_type   = src_info->type;
      wake->rdy_bit    = ii;
      wake->next       = NULL;

      if(src_op->wake_up_tail == NULL) {
        src_op->wake_up_head  = wake;
        src_op->wake_up_tail  = wake;
        src_op->wake_up_count = 1;
      } else {
        ASSERT(map_data->proc_id, src_op->wake_up_head);
        src_op->wake_up_tail->next = wake;
        src_op->wake_up_tail       = wake;
        src_op->wake_up_count++;
      }

      if(TRACK_L1_MISS_DEPS) {
        // An op can occupy multiple entries in the wakeup list of another op
        if(src_op->engine_info.l1_miss &&
           !src_op->engine_info.l1_miss_satisfied)
          op->engine_info.dep_on_l1_miss = TRUE;

        if(src_op->engine_info.dep_on_l1_miss)
          op->engine_info.dep_on_l1_miss = TRUE;
      }

      if(src_op->wake_up_signaled[src_info->type]) {
        clear_not_rdy_bit(op, ii);
        wake_action(src_op, op, ii);
      }

      DEBUG(op->proc_id,
            "Added to wake up list  op_num:%s  src_op_num:%s type:%s\n",
            unsstr64(op->op_num), unsstr64(src_op->op_num),
            dep_type_names[src_info->type]);
    } else {
      /* the src op must have retired already  */
      src_info->op = &invalid_op;
      clear_not_rdy_bit(op, ii);
    }
  }
}


/**************************************************************************************/
/* free_wake_up_list: */

void free_wake_up_list(Op* op) {
  ASSERT(map_data->proc_id, op);
  ASSERT(map_data->proc_id, op->proc_id == map_data->proc_id);

  if(op->wake_up_tail) {
    ASSERT(map_data->proc_id, op->wake_up_head);
    DEBUG(map_data->proc_id, "Freeing wake up list for op_num:%s\n",
          unsstr64(op->op_num));
    op->wake_up_tail->next   = map_data->free_list_head;
    map_data->free_list_head = op->wake_up_head;
    map_data->active_wake_up_entries -= op->wake_up_count;
    ASSERT(map_data->proc_id, map_data->active_wake_up_entries >= 0);
    op->wake_up_head = NULL;
    op->wake_up_tail = NULL;
  } else {
    DEBUG(map_data->proc_id, "No wake up list for op_num:%s\n",
          unsstr64(op->op_num));
  }
}


/**************************************************************************************/
/* add_src_from_op: . */

void add_src_from_op(Op* op, Op* src_op, Dep_Type type) {
  uns       src_num = op->oracle_info.num_srcs++;
  Src_Info* info    = &op->oracle_info.src_info[src_num];

  ASSERT(map_data->proc_id, op);
  ASSERT(map_data->proc_id, src_op);
  ASSERT(map_data->proc_id, map_data->proc_id == op->proc_id);
  ASSERT(op->proc_id, op->proc_id == src_op->proc_id);
  ASSERT(op->proc_id, type < NUM_DEP_TYPES);
  ASSERTM(op->proc_id, src_num < MAX_DEPS, "src_num: %i\n", src_num);
  ASSERTM(op->proc_id, src_op->op_num < op->op_num, "op:%s  src_op:%s\n",
          unsstr64(op->op_num), unsstr64(src_op->op_num));

  info->type       = type;
  info->op         = src_op;
  info->op_num     = src_op->op_num;
  info->unique_num = src_op->unique_num;

  /* for memory dependencies, derived_from_prog_input incremented in track_addr
   */
  set_not_rdy_bit(op, src_num);
  if(type == MEM_DATA_DEP) {
    ASSERT(op->proc_id, src_op->table_info->mem_type == MEM_ST &&
                          op->table_info->mem_type == MEM_LD);
  }
  DEBUG(map_data->proc_id, "Added dep op_num:%s  src_op_num:%s  src_num:%d\n",
        unsstr64(op->op_num), unsstr64(src_op->op_num), src_num);
}


/**************************************************************************************/
/* add_src_from_map_entry: set the src_info array */

void add_src_from_map_entry(Op* op, Map_Entry* map_entry, Dep_Type type) {
  uns       src_num = op->oracle_info.num_srcs++;
  Src_Info* info    = &op->oracle_info.src_info[src_num];

  ASSERT(map_data->proc_id, op);
  ASSERT(map_data->proc_id, map_data->proc_id == op->proc_id);
  ASSERT(map_data->proc_id, map_entry);
  ASSERTM(map_data->proc_id, map_entry->op,
          "sop_off_path: %u, op: %p, op_num: %llu, unique_num: %llu\n",
          op->off_path, map_entry->op, map_entry->op_num,
          map_entry->unique_num);
  ASSERT(map_data->proc_id, type < NUM_DEP_TYPES);
  ASSERTM(map_data->proc_id, src_num < MAX_DEPS,
          "op_num: %llu, op_type %u, src_num: %u\n", op->op_num,
          op->table_info->op_type, src_num);
  ASSERTM(map_data->proc_id, map_entry->op_num < op->op_num,
          "op:%s  src_op:%s\n", unsstr64(op->op_num),
          unsstr64(map_entry->op->op_num));

  info->type       = type;
  info->op         = map_entry->op;
  info->op_num     = map_entry->op_num;
  info->unique_num = map_entry->unique_num;

  /* always start with the not ready bit set */
  set_not_rdy_bit(op, src_num);
  DEBUG(map_data->proc_id, "Added dep  op_num:%s  src_op_num:%s  src_num:%d\n",
        unsstr64(op->op_num), unsstr64(map_entry->op_num), src_num);
}


/**************************************************************************************/
/* clear_not_rdy_bit: */

void clear_not_rdy_bit(Op* op, uns bit) {
  ASSERT(map_data->proc_id, op);
  ASSERT(map_data->proc_id, bit < op->oracle_info.num_srcs);
  DEBUG(map_data->proc_id, "Clearing not rdy bit  op_num:%s  bit:%d\n",
        unsstr64(op->op_num), bit);
  op->srcs_not_rdy_vector &= ~(0x1 << bit);
}


/**************************************************************************************/
/* set_not_rdy_bit: */

void set_not_rdy_bit(Op* op, uns bit) {
  ASSERT(map_data->proc_id, op);
  ASSERT(map_data->proc_id, bit < op->oracle_info.num_srcs);
  /*  this message gets annoying
      DEBUG("Setting not rdy bit  op_num:%s  bit:%d\n", unsstr64(op->op_num),
     bit);
  */
  op->srcs_not_rdy_vector |= (0x1 << bit);
}


/**************************************************************************************/
/* test_not_rdy_bit: */

Flag test_not_rdy_bit(Op* op, uns bit) {
  ASSERT(map_data->proc_id, op);
  ASSERT(map_data->proc_id, bit < op->oracle_info.num_srcs);
  return (op->srcs_not_rdy_vector & (0x1 << bit)) > 0;
}


/**************************************************************************************/
/* simple_wake: wake dest_op based on src_op based only on src_op's exec_cycle
 */

void simple_wake(Op* src_op, Op* dep_op, uns8 rdy_bit) {
  ASSERT(src_op->proc_id, src_op->proc_id == dep_op->proc_id);
  ASSERT(src_op->proc_id, src_op && src_op != &invalid_op);
  ASSERT(src_op->proc_id, dep_op && dep_op != &invalid_op);
  dep_op->rdy_cycle = MAX2(dep_op->rdy_cycle, src_op->wake_cycle);
  if(dep_op->srcs_not_rdy_vector == 0)
    dep_op->state = dep_op->rdy_cycle == cycle_count + 1 ? OS_READY :
                                                           OS_WAIT_FWD;
}

/**************************************************************************************/
/* reset_map: */

void reset_map() {
  uns ii;

  ASSERT(map_data->proc_id, map_data == &td->map_data);

  /* initialize the register "last write" map */
  for(ii = 0; ii < NUM_REG_IDS * 2; ii++) {
    map_data->reg_map[ii].op     = &invalid_op;
    map_data->reg_map[ii].op_num = 0;
  }
  for(ii = 0; ii < NUM_REG_IDS; ii++)
    map_data->map_flags[ii] = FALSE;

  map_data->last_store[0].op     = &invalid_op;
  map_data->last_store[0].op_num = 0;
  map_data->last_store[1].op     = &invalid_op;
  map_data->last_store[1].op_num = 0;
}
