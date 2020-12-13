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
 * File         : libs/cache_lib.c
 * Author       : HPS Research Group
 * Date         : 2/6/1998
 * Description  : This is a library of cache functions.
 ***************************************************************************************/

#include <stdlib.h>
#include "debug/debug_macros.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"

#include "core.param.h"
#include "debug/debug.param.h"
#include "general.param.h"
#include "libs/cache_lib_new.h"
#include "memory/memory.param.h"

// DeleteMe
#define ideal_num_entries 256

/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_CACHE_LIB, ##args)


/**************************************************************************************/
/* Static Prototypes */

static inline uns  cache_index(Cache* cache, Addr addr, Addr* tag,
                               Addr* line_addr);
static inline void update_repl_policy(Cache*, Cache_Entry*, uns, uns, Flag);
static inline Cache_Entry* find_repl_entry(Cache*, uns8, uns, uns*);

/* for ideal replacement */
static inline void*        access_unsure_lines(Cache*, uns, Addr, Flag);
static inline Cache_Entry* insert_sure_line(Cache*, uns, Addr);
static inline void         invalidate_unsure_line(Cache*, uns, Addr);

/**************************************************************************************/
/* Global Variables */

char rand_repl_state[31];


/**************************************************************************************/

static inline uns cache_index(Cache* cache, Addr addr, Addr* tag,
                              Addr* line_addr) {
  *line_addr = addr & ~cache->offset_mask;
  *tag       = addr >> cache->shift_bits & cache->tag_mask;
  return addr >> cache->shift_bits & cache->set_mask;
}

uns ext_cache_index(Cache* cache, Addr addr, Addr* tag, Addr* line_addr) {
  return cache_index(cache, addr, tag, line_addr);
}


/**************************************************************************************/
/* init_cache: */

void init_cache(Cache* cache, const char* name, uns cache_size, uns assoc,
                uns line_size, uns data_size, Repl_Policy repl_policy) {
  uns num_lines = cache_size / line_size;
  uns num_sets  = cache_size / line_size / assoc;
  uns ii, jj;

  DEBUG(0, "Initializing cache called '%s'.\n", name);

  /* set the basic parameters */
  strncpy(cache->name, name, MAX_STR_LENGTH);
  cache->data_size   = data_size;
  cache->num_lines   = num_lines;
  cache->assoc       = assoc;
  cache->num_sets    = num_sets;
  cache->line_size   = line_size;
  cache->repl_policy = repl_policy;

  /* set some fields to make indexing quick */
  cache->set_bits    = LOG2(num_sets);
  cache->shift_bits  = LOG2(line_size);               /* use for shift amt. */
  cache->set_mask    = N_BIT_MASK(LOG2(num_sets));    /* use after shifting */
  cache->tag_mask    = ~cache->set_mask;              /* use after shifting */
  cache->offset_mask = N_BIT_MASK(cache->shift_bits); /* use before shifting */

  /* allocate memory for NMRU replacement counters  */
  cache->repl_ctrs = (uns*)calloc(num_sets, sizeof(uns));

  /* allocate memory for all the sets (pointers to line arrays)  */
  cache->entries = (Cache_Entry**)malloc(sizeof(Cache_Entry*) * num_sets);

  /* allocate memory for the unsure lists (if necessary) */
  if(cache->repl_policy == REPL_IDEAL)
    cache->unsure_lists = (List*)malloc(sizeof(List) * num_sets);

  /* allocate memory for all of the lines in each set */
  for(ii = 0; ii < num_sets; ii++) {
    cache->entries[ii] = (Cache_Entry*)malloc(sizeof(Cache_Entry) * assoc);
    /* allocate memory for all of the data elements in each line */
    for(jj = 0; jj < assoc; jj++) {
      cache->entries[ii][jj].valid = FALSE;
      if(data_size) {
        cache->entries[ii][jj].data = (void*)malloc(data_size);
        memset(cache->entries[ii][jj].data, 0, data_size);
      } else
        cache->entries[ii][jj].data = INIT_CACHE_DATA_VALUE;
      if(cache->repl_policy == REPL_SRRIP) {
        cache->entries[ii][jj].rrpv = cache->assoc-1;
      }
    }

    /* initialize the unsure lists (if necessary) */
    if(cache->repl_policy == REPL_IDEAL) {
      char list_name[MAX_STR_LENGTH + 1];
      snprintf(list_name, MAX_STR_LENGTH, "%.*s unsure [%d]",
               MAX_STR_LENGTH - 20, cache->name, ii);  // 21 guaruntees the
                                                       // string will always be
                                                       // smaller than
                                                       // MAX_STR_LENGTH
      init_list(&cache->unsure_lists[ii], list_name, sizeof(Cache_Entry),
                USE_UNSURE_FREE_LISTS);
    }
  }
  cache->num_demand_access = 0;
  cache->last_update       = 0;

  /* For cache partitioning */
  if(cache->repl_policy == REPL_PARTITION) {
    cache->num_ways_allocted_core = (uns*)malloc(sizeof(uns) * NUM_CORES);
    cache->num_ways_occupied_core = (uns*)malloc(sizeof(uns) * NUM_CORES);
    cache->lru_index_core         = (uns*)malloc(sizeof(uns) * NUM_CORES);
    cache->lru_time_core = (Counter*)malloc(sizeof(Counter) * NUM_CORES);
  }

  /* allocate memory for the back-up lists (if necessary) */
  if(cache->repl_policy == REPL_SHADOW_IDEAL) {
    cache->shadow_entries = (Cache_Entry**)malloc(sizeof(Cache_Entry*) *
                                                  num_sets);
    /* allocate memory for all of the lines in each set */
    for(ii = 0; ii < num_sets; ii++) {
      cache->shadow_entries[ii] = (Cache_Entry*)malloc(sizeof(Cache_Entry) *
                                                       assoc);
      /* allocate memory for all of the data elements in each line */
      for(jj = 0; jj < assoc; jj++) {
        cache->shadow_entries[ii][jj].valid = FALSE;
        if(data_size) {
          cache->shadow_entries[ii][jj].data = (void*)malloc(data_size);
          memset(cache->shadow_entries[ii][jj].data, 0, data_size);
        } else
          cache->shadow_entries[ii][jj].data = INIT_CACHE_DATA_VALUE;
      }
    }
  }

  else if(cache->repl_policy == REPL_IDEAL_STORAGE) {
    cache->shadow_entries = (Cache_Entry**)malloc(sizeof(Cache_Entry*) *
                                                  num_sets);
    cache->queue_end      = (uns*)malloc(sizeof(uns) * num_sets);
    /* allocate memory for all of the lines in each set */
    for(ii = 0; ii < num_sets; ii++) {
      cache->shadow_entries[ii] = (Cache_Entry*)malloc(sizeof(Cache_Entry) *
                                                       ideal_num_entries);
      /* allocate memory for all of the data elements in each line */
      for(jj = 0; jj < ideal_num_entries; jj++) {
        cache->shadow_entries[ii][jj].valid = FALSE;
        if(data_size) {
          cache->shadow_entries[ii][jj].data = (void*)malloc(data_size);
          memset(cache->shadow_entries[ii][jj].data, 0, data_size);
        } else
          cache->shadow_entries[ii][jj].data = INIT_CACHE_DATA_VALUE;
      }
      cache->queue_end[ii] = 0;
    }
  }
}

/**************************************************************************************/
/* cache_access: Does a cache lookup based on the address.  Returns a pointer
 * to the cache line data if it is found.  */

void* cache_access(Cache* cache, Addr addr, Addr* line_addr, Flag update_repl) {
  Addr tag;
  uns  set = cache_index(cache, addr, &tag, line_addr);
  uns  ii;

  if(cache->repl_policy == REPL_IDEAL_STORAGE) {
    return access_ideal_storage(cache, set, tag, addr);
  }

  for(ii = 0; ii < cache->assoc; ii++) {
    Cache_Entry* line = &cache->entries[set][ii];

    if(line->valid && line->tag == tag) {
      /* update replacement state if necessary */
      ASSERT(0, line->data);
      DEBUG(0, "Found line in cache '%s' at (set %u, way %u, base 0x%s)\n",
            cache->name, set, ii, hexstr64s(line->base));

      if(update_repl) {
        if(line->pref) {
          line->pref = FALSE;
        }
        cache->num_demand_access++;
        update_repl_policy(cache, line, set, ii, FALSE);
      }

      return line->data;
    }
  }
  /* if it's a miss and we're doing ideal replacement, look in the unsure list
   */
  if(cache->repl_policy == REPL_IDEAL) {
    DEBUG(0, "Checking unsure list '%s' at (set %u)\n", cache->name, set);
    return access_unsure_lines(cache, set, tag, update_repl);
  }

  if(cache->repl_policy == REPL_SHADOW_IDEAL) {
    DEBUG(0, "Checking shadow cache '%s' at (set %u), base 0x%s\n", cache->name,
          set, hexstr64s(addr));
    return access_shadow_lines(cache, set, tag);
  }


  DEBUG(0, "Didn't find line in set %u in cache '%s' base 0x%s\n", set,
        cache->name, hexstr64s(addr));
  return NULL;
}

/**************************************************************************************/
/* cache_insert: returns a pointer to the data section of the new cache line.
   Sets line_addr to the address of the first block of the new line.  Sets
   repl_line_addr to the address of the first block that was replaced

   DON'T call this unless you are sure that the line is not in the
   cache (call after cache_access returned NULL)
*/

void* cache_insert(Cache* cache, uns8 proc_id, Addr addr, Addr* line_addr,
                   Addr* repl_line_addr) {
  return cache_insert_replpos(cache, proc_id, addr, line_addr, repl_line_addr,
                              INSERT_REPL_DEFAULT, FALSE);
}
/**************************************************************************************/
/* cache_insert_replpos: returns a pointer to the data section of the new cache
   line.  Sets line_addr to the address of the first block of the new line.
   Sets repl_line_addr to the address of the first block that was replaced

   DON'T call this unless you are sure that the line is not in the
   cache (call after cache_access returned NULL)
*/

void* cache_insert_replpos(Cache* cache, uns8 proc_id, Addr addr,
                           Addr* line_addr, Addr* repl_line_addr,
                           Cache_Insert_Repl insert_repl_policy,
                           Flag              isPrefetch) {
  Addr         tag;
  uns          repl_index;
  uns          set = cache_index(cache, addr, &tag, line_addr);
  Cache_Entry* new_line;

  if(cache->repl_policy == REPL_IDEAL) {
    new_line        = insert_sure_line(cache, set, tag);
    *repl_line_addr = 0;
  } else {
    new_line = find_repl_entry(cache, proc_id, set, &repl_index);
    /* before insert the data into cache, if the cache has shadow entry */
    /* insert that entry to the shadow cache */
    if((cache->repl_policy == REPL_SHADOW_IDEAL) && new_line->valid)
      shadow_cache_insert(cache, set, new_line->tag, new_line->base);
    if(new_line->valid)  // bug fixed. 4/26/04 if the entry is not valid,
                         // repl_line_addr should be set to 0
      *repl_line_addr = new_line->base;
    else
      *repl_line_addr = 0;
    DEBUG(0,
          "Replacing 2.2f(set %u, way %u, tag 0x%s, base 0x%s) in cache '%s' "
          "with base 0x%s\n",
          set, repl_index, hexstr64s(new_line->tag), hexstr64s(new_line->base),
          cache->name, hexstr64s(*line_addr));
  }

  new_line->proc_id          = proc_id;
  new_line->valid            = TRUE;
  new_line->tag              = tag;
  new_line->base             = *line_addr;
  new_line->last_access_time = sim_time;  // FIXME: this fixes valgrind warnings
                                          // in update_prf_

  new_line->pref = isPrefetch;

  switch(insert_repl_policy) {
    case INSERT_REPL_DEFAULT:
      update_repl_policy(cache, new_line, set, repl_index, TRUE);
      break;
    case INSERT_REPL_LRU:
      new_line->last_access_time = 123;  // Just choose a small number
      break;
    case INSERT_REPL_MRU:
      new_line->last_access_time = sim_time;
      break;
    case INSERT_REPL_MID:  // Insert such that it is Middle(Roughly) of the repl
                           // order
    case INSERT_REPL_LOWQTR:  // Insert such that it is Quarter(Roughly) of the
                              // repl order
    {
      // first form the lru array
      Counter* access = (Counter*)malloc(sizeof(Counter) * cache->assoc);
      int      ii, jj;
      for(ii = 0; ii < cache->assoc; ii++) {
        Cache_Entry* entry = &cache->entries[set][ii];
        if(entry->valid)
          access[ii] = entry->last_access_time;
        else
          access[ii] = 0;
        // Sort
        for(jj = ii - 1; jj >= 0; jj--) {
          if(access[jj + 1] < access[jj]) {  // move data
            Counter temp   = access[jj];
            access[jj]     = access[jj + 1];
            access[jj + 1] = temp;
          } else {
            break;
          }
        }
      }
      if(insert_repl_policy == INSERT_REPL_MID) {
        new_line->last_access_time = access[cache->assoc / 2];
      } else if(insert_repl_policy == INSERT_REPL_LOWQTR) {
        new_line->last_access_time = access[cache->assoc / 4];
      }
      if(new_line->last_access_time == 0)
        new_line->last_access_time = sim_time;
      free(access);
    } break;
    default:
      ASSERT(0, FALSE);  // should never come here
  }
  if(cache->repl_policy == REPL_IDEAL_STORAGE) {
    new_line->last_access_time = cache->assoc;
    /* debug */
    /* insert into the entry also */
    {
      uns          lru_ind  = 0;
      Counter      lru_time = MAX_CTR;
      Cache_Entry* main_line;
      uns          ii;

      /* first cache access */
      for(ii = 0; ii < cache->assoc; ii++) {
        Cache_Entry* line = &cache->entries[set][ii];

        if(line->tag == tag && line->valid) {
          /* update replacement state if necessary */
          ASSERT(0, line->data);
          line->last_access_time = sim_time;
          return new_line->data;
        }
      }
      /* looking for lru */
      for(ii = 0; ii < cache->assoc; ii++) {
        Cache_Entry* entry = &cache->entries[set][ii];
        if(!entry->valid) {
          lru_ind = ii;
          break;
        }
        if(entry->last_access_time < lru_time) {
          lru_ind  = ii;
          lru_time = cache->entries[set][ii].last_access_time;
        }
      }
      main_line                   = &cache->entries[set][lru_ind];
      main_line->valid            = TRUE;
      main_line->tag              = tag;
      main_line->base             = *line_addr;
      main_line->last_access_time = sim_time;
    }
  }
  return new_line->data;
}


/**************************************************************************************/
/* invalidate_line: Does a cache lookup based on the address.  Returns a pointer
   to the cache line data if it is found.  */

void cache_invalidate(Cache* cache, Addr addr, Addr* line_addr) {
  Addr tag;
  uns  set = cache_index(cache, addr, &tag, line_addr);
  uns  ii;

  for(ii = 0; ii < cache->assoc; ii++) {
    Cache_Entry* line = &cache->entries[set][ii];
    if(line->tag == tag && line->valid) {
      line->tag   = 0;
      line->valid = FALSE;
      line->base  = 0;
    }
  }

  if(cache->repl_policy == REPL_IDEAL)
    invalidate_unsure_line(cache, set, tag);
}


/**************************************************************************************/
/* get_next_repl_line:  Return a pointer to the lru item in the cache set */

void* get_next_repl_line(Cache* cache, uns8 proc_id, Addr addr,
                         Addr* repl_line_addr, Flag* valid) {
  Addr         line_tag, line_addr;
  uns          repl_index;
  uns          set_index = cache_index(cache, addr, &line_tag, &line_addr);
  Cache_Entry* new_line  = find_repl_entry(cache, proc_id, set_index,
                                          &repl_index);

  *repl_line_addr = new_line->base;
  *valid          = new_line->valid;
  return new_line->data;
}


/**************************************************************************************/
/* find_repl_entry: Returns the cache lib entry that will be the next to be
   replaced. This call should not change any of the state information. */

Cache_Entry* find_repl_entry(Cache* cache, uns8 proc_id, uns set, uns* way) {
  int ii;
  switch(cache->repl_policy) {
    case REPL_SHADOW_IDEAL:
    case REPL_TRUE_LRU: {
      uns     lru_ind  = 0;
      Counter lru_time = MAX_CTR;
      for(ii = 0; ii < cache->assoc; ii++) {
        Cache_Entry* entry = &cache->entries[set][ii];
        if(!entry->valid) {
          lru_ind = ii;
          break;
        }
        if(entry->last_access_time < lru_time) {
          lru_ind  = ii;
          lru_time = cache->entries[set][ii].last_access_time;
        }
      }
      *way = lru_ind;
      return &cache->entries[set][lru_ind];
    } break;
    case REPL_RANDOM:
    case REPL_NOT_MRU:
    case REPL_ROUND_ROBIN:
    case REPL_LOW_PREF: {
      uns repl_index = cache->repl_ctrs[set];
      for(ii = 0; ii < cache->assoc; ii++) {
        Cache_Entry* entry = &cache->entries[set][ii];
        if(!entry->valid)
          repl_index = ii;
      }
      *way = repl_index;
      return &cache->entries[set][repl_index];
    } break;
    case REPL_IDEAL:
      printf("ERROR: Can't determine entry to be replaced when using ideal "
             "replacement\n");
      ASSERT(0, FALSE);
      break;

    case REPL_IDEAL_STORAGE: {
      printf("ERROR: Check if ideal storage replacement works\n");
      ASSERT(0, FALSE);
      Cache_Entry* new_line = &(
        cache->shadow_entries[set][cache->queue_end[set]]);
      cache->queue_end[set] = (cache->queue_end[set] + 1) % ideal_num_entries;
      return new_line;
    } break;
    case REPL_PARTITION: {
      uns8 way_proc_id;
      uns  lru_ind             = 0;
      uns  total_assigned_ways = 0;

      for(way_proc_id = 0; way_proc_id < NUM_CORES; way_proc_id++) {
        cache->num_ways_occupied_core[way_proc_id] = 0;
        cache->lru_time_core[way_proc_id]          = MAX_CTR;
        ASSERT(way_proc_id, cache->num_ways_allocted_core[way_proc_id]);
        total_assigned_ways += cache->num_ways_allocted_core[way_proc_id];
      }

      ASSERT(proc_id, total_assigned_ways == cache->assoc);

      for(ii = 0; ii < cache->assoc; ii++) {
        Cache_Entry* entry = &cache->entries[set][ii];
        if(!entry->valid) {
          lru_ind = ii;
          *way    = lru_ind;
          return &cache->entries[set][lru_ind];
        }
        cache->num_ways_occupied_core[entry->proc_id]++;
        if(entry->last_access_time < cache->lru_time_core[entry->proc_id]) {
          cache->lru_index_core[entry->proc_id] = ii;
          cache->lru_time_core[entry->proc_id]  = entry->last_access_time;
        }
      }

      // find the core that overoccupies its partition the most
      int max_extra_occ = 0;
      int repl_proc_id  = -1;
      for(way_proc_id = 0; way_proc_id < NUM_CORES; way_proc_id++) {
        if(cache->num_ways_allocted_core[way_proc_id] <
           cache->num_ways_occupied_core[way_proc_id]) {
          int extra_occ = cache->num_ways_occupied_core[way_proc_id] -
                          cache->num_ways_allocted_core[way_proc_id];
          if(extra_occ > max_extra_occ) {
            max_extra_occ = extra_occ;
            repl_proc_id  = way_proc_id;
          }
        }
      }
      int proc_id_extra_occ = cache->num_ways_occupied_core[proc_id] -
                              cache->num_ways_allocted_core[proc_id];
      if(cache->num_ways_allocted_core[proc_id] >
           cache->num_ways_occupied_core[proc_id] ||
         max_extra_occ > proc_id_extra_occ + 1 ||
         ((max_extra_occ > proc_id_extra_occ) &&
          ((proc_id + set) % NUM_CORES > (repl_proc_id + set) % NUM_CORES))) {
        /* the complicated condition above ensures unbiased
           distribution of over-occupancy in case a workload does
           not occupy its allocated way partition */
        ASSERT(0, repl_proc_id >= 0);
        lru_ind = cache->lru_index_core[repl_proc_id];
      } else {
        lru_ind = cache->lru_index_core[proc_id];
      }
      *way = lru_ind;
      return &cache->entries[set][lru_ind];
    }


    default:
      ASSERT(0, FALSE);
  }
}


/**************************************************************************************/
/* update_repl_policy: */

static inline void update_repl_policy(Cache* cache, Cache_Entry* cur_entry,
                                      uns set, uns way, Flag repl) {
  switch(cache->repl_policy) {
    case REPL_IDEAL_STORAGE:
    case REPL_SHADOW_IDEAL:
    case REPL_TRUE_LRU:
    case REPL_PARTITION:
      cur_entry->last_access_time = sim_time;
      break;
    case REPL_RANDOM: {
      char* old_rand_state  = (char*)setstate(rand_repl_state);
      cache->repl_ctrs[set] = rand() % cache->assoc;
      setstate(old_rand_state);
    } break;
    case REPL_NOT_MRU:
      if(way == cache->repl_ctrs[set])
        cache->repl_ctrs[set] = CIRC_INC2(cache->repl_ctrs[set], cache->assoc);
      break;
    case REPL_ROUND_ROBIN:
      cache->repl_ctrs[set] = CIRC_INC2(cache->repl_ctrs[set], cache->assoc);
      break;
    case REPL_IDEAL:
      /* no need to do anything here (nothing changes when we hit on a sure
       * line) */
      /* all unsure hits are handled elsewhere */
      break;
    case REPL_LOW_PREF:
      /* low priority to prefetcher data */
      {
        int     lru_ind  = -1;
        Counter lru_time = MAX_CTR;
        int     ii;
        // cache->repl_ctrs[set] = ....
        for(ii = 0; ii < cache->assoc; ii++) {
          Cache_Entry* entry = &cache->entries[set][ii];
          if(!entry->valid) {
            lru_ind = ii;
            break;
          }
          // compare between prefetcher
          if((entry->last_access_time < lru_time) &&
             cache->entries[set][ii].pref) {
            lru_ind  = ii;
            lru_time = cache->entries[set][ii].last_access_time;
          }
        }
        if(lru_ind == -1) {
          for(ii = 0; ii < cache->assoc; ii++) {
            Cache_Entry* entry = &cache->entries[set][ii];
            if(entry->last_access_time < lru_time) {
              lru_ind  = ii;
              lru_time = cache->entries[set][ii].last_access_time;
            }
          }
        }
        cache->repl_ctrs[set] = lru_ind;
      }
      break;
    case REPL_SRRIP:
      
      break;
    default:
      ASSERT(0, FALSE);
  }
}


/**************************************************************************************/
/* access_unsure_lines: */

static inline void* access_unsure_lines(Cache* cache, uns set, Addr tag,
                                        Flag update_repl) {
  List*        list = &cache->unsure_lists[set];
  Cache_Entry* temp;
  int          ii;

  for(temp = (Cache_Entry*)list_start_head_traversal(list); temp;
      temp = (Cache_Entry*)list_next_element(list)) {
    ASSERT(0, temp->valid);
    if(temp->tag == tag) {
      for(ii = 0; ii < cache->assoc; ii++) {
        if(!cache->entries[set][ii].valid) {
          void* data = cache->entries[set][ii].data;
          memcpy(&cache->entries[set][ii], temp, sizeof(Cache_Entry));
          temp->data = data;
          ASSERT(0, dl_list_remove_current(list) == temp);
          ASSERT(0, ++cache->repl_ctrs[set] <=
                      cache->assoc); /* repl ctr holds the sure count */
          if(cache->repl_ctrs[set] == cache->assoc) {
            for(temp = (Cache_Entry*)list_start_head_traversal(list); temp;
                temp = (Cache_Entry*)list_next_element(list))
              free(temp->data);
            clear_list(&cache->unsure_lists[set]);
          }
          return cache->entries[set][ii].data;
        }
      }
      ASSERT(0, FALSE);
    }
  }
  return NULL;
}


/**************************************************************************************/
/* insert_sure_line: */

static inline Cache_Entry* insert_sure_line(Cache* cache, uns set, Addr tag) {
  List* list = &cache->unsure_lists[set];
  int   ii;
  if(list_get_head(list) || cache->repl_ctrs[set] == cache->assoc) {
    /* if there is an unsure list already, or if we have all sure entries... */
    int count = 0;
    for(ii = 0; ii < cache->assoc; ii++) {
      Cache_Entry* entry = &cache->entries[set][ii];
      if(entry->valid) {
        Cache_Entry* temp = (Cache_Entry*)dl_list_add_tail(list);
        memcpy(temp, entry, sizeof(Cache_Entry));
        temp->data = malloc(sizeof(cache->data_size));
        memcpy(entry->data, temp->data, sizeof(cache->data_size));
        entry->valid = FALSE;
        count++;
      }
    }
    ASSERT(0, count == cache->repl_ctrs[set]);
    cache->repl_ctrs[set] = 1;
    return &cache->entries[set][0];
  } else {
    /* there should be an invalid entry to use */
    for(ii = 0; ii < cache->assoc; ii++) {
      Cache_Entry* entry = &cache->entries[set][ii];
      if(!entry->valid) {
        ASSERT(0, ++cache->repl_ctrs[set] <= cache->assoc);
        return entry;
      }
    }
    ASSERT(0, FALSE);
  }
}


/**************************************************************************************/
/* invalidate_unsure_line: */

static inline void invalidate_unsure_line(Cache* cache, uns set, Addr tag) {
  List*        list = &cache->unsure_lists[set];
  Cache_Entry* temp;
  for(temp = (Cache_Entry*)list_start_head_traversal(list); temp;
      temp = (Cache_Entry*)list_next_element(list)) {
    ASSERT(0, temp->valid);
    if(temp->tag == tag) {
      free(temp->data);
      dl_list_remove_current(list);
      return;
    }
  }
}

void* access_shadow_lines(Cache* cache, uns set, Addr tag) {
  uns     ii;
  uns     lru_ind  = 0;
  Counter lru_time = MAX_CTR;

  for(ii = 0; ii < cache->assoc; ii++) {
    Cache_Entry* line = &cache->shadow_entries[set][ii];

    if(line->tag == tag && line->valid) {
      int jj;
      /* found the entry in the shadow cache */
      ASSERT(0, line->data);
      DEBUG(0,
            "Found line in shadow cache '%s' at (set %u, way %u, base 0x%s)\n",
            cache->name, set, ii, hexstr64s(line->base));

      /* search for the oldest entry */

      for(jj = 0; jj < cache->assoc; jj++) {
        Cache_Entry* entry = &cache->entries[set][jj];
        if(!entry->valid) {
          lru_ind = jj;
          break;
        }
        if(entry->last_access_time < lru_time) {
          lru_ind  = jj;
          lru_time = cache->entries[set][jj].last_access_time;
        }
      }


      if(lru_time < line->last_access_time) {
        Cache_Entry tmp_line;
        DEBUG(0,
              "shadow cache line will be swaped:\ncache->addr:0x%s "
              "cache->lru_time:%lld  shadow_tag:0x%s shadow_insert:%lld \n",
              hexstr64s((cache->entries[set][lru_ind]).tag),
              (uns64)(cache->entries[set][lru_ind]).last_access_time,
              hexstr64s(line->tag), (uns64)line->last_access_time);

        /* shadow entry can be inserted to main cache  */

        tmp_line                       = (cache->entries[set][lru_ind]);
        (cache->entries[set][lru_ind]) = *line;
        *line                          = tmp_line;
        line->last_access_time =
          (cache->entries[set][lru_ind]).last_access_time;
        (cache->entries[set][lru_ind]).last_access_time = sim_time;
        DEBUG(0,
              "shadow cache line is swaped\n cache->addr:0x%s "
              "cache->lru_time:%lld  shadow_tag:0x%s shadow_insert:%lld \n",
              hexstr64s((cache->entries[set][lru_ind]).tag),
              (uns64)(cache->entries[set][lru_ind]).last_access_time,
              hexstr64s(line->tag), (uns64)line->last_access_time);
        /* insert counter later */
        return (line->data);
      } else {
        /* make invalidate for that cache entry */
        DEBUG(0,
              "shadow cache can't find the replacment target: cache_tag:0x%s "
              "lru_time:%lld, insert_time:%lld\n",
              hexstr64s(cache->entries[set][lru_ind].tag), (uns64)lru_time,
              (uns64)line->last_access_time);
        line->valid = FALSE; /* so we don't have 2 copies for the same data */
      }
    }
  }

  DEBUG(0, "Didn't find line in set %u in shadow cache '%s' \n", set,
        cache->name);
  return NULL;
}

void* shadow_cache_insert(Cache* cache, uns set, Addr tag, Addr base) {
  int          ii;
  Cache_Entry* new_line;

  /* within the shadow cache entry, we need to use the true replacement policy
   */

  uns     lru_ind  = 0;
  Counter lru_time = MAX_CTR;
  for(ii = 0; ii < cache->assoc; ii++) {
    Cache_Entry* entry = &cache->shadow_entries[set][ii];
    if(!entry->valid) {
      lru_ind = ii;
      break;
    }
    if(entry->last_access_time < lru_time) {
      lru_ind  = ii;
      lru_time = cache->shadow_entries[set][ii].last_access_time;
    }
  }

  new_line                   = &(cache->shadow_entries[set][lru_ind]);
  new_line->valid            = TRUE;
  new_line->tag              = tag;
  new_line->base             = base;
  new_line->last_access_time = sim_time;
  DEBUG(0,
        "Insert Shadow cache (set %u, way %u, tag 0x%s, base 0x%s) "
        "last_access_time:%lld : sim_time:%lld\n",
        set, lru_ind, hexstr64s(tag), hexstr64s(base),
        (uns64)new_line->last_access_time, (uns64)sim_time);
  return new_line;
}

#define QUEUE_IND(num) (((num) + cache->queue_end[set]) % ideal_num_entries)

void* access_ideal_storage(Cache* cache, uns set, Addr tag, Addr addr) {
  uns          ii;
  uns          valid_start = 0;
  Cache_Entry* new_line;
  int          main_entry_found = FALSE;

  for(ii = 0; ii < cache->assoc; ii++) {
    Cache_Entry* line = &cache->entries[set][ii];
    if(line->tag == tag && line->valid) {
      line->last_access_time = sim_time;
      main_entry_found       = TRUE;
    }
  }

  for(ii = 0; ii < ideal_num_entries; ii++) {
    Cache_Entry* line = &cache->shadow_entries[set][ii];
    if(line->tag == tag && line->valid) {
      uns jj;
      ASSERT(0, line->data);
      if(ii ==
         (cache->queue_end[set] + ideal_num_entries - 1) % ideal_num_entries)
        return line->data;
      for(jj = ((ii - cache->queue_end[set]) % ideal_num_entries);
          jj < ideal_num_entries; jj++) {
        Cache_Entry* cal_line = &cache->shadow_entries[set][QUEUE_IND(jj)];
        if(cal_line->valid) {
          cal_line->last_access_time--;
          DEBUG(0,
                "counter is decreasing. set:%d, queue_end:%u jj:%u ind:%u  "
                "counter:%lld, addr:0x%s\n",
                set, cache->queue_end[set], jj, QUEUE_IND(jj),
                cal_line->last_access_time, hexstr64s(cal_line->base));
          if((cal_line->valid) && (cal_line->last_access_time == 0))
            valid_start = jj;
        }
      }
      for(jj = 0; jj <= valid_start; jj++) {
        Cache_Entry* cal_line = &cache->shadow_entries[set][QUEUE_IND(jj)];
        cal_line->valid       = 0;
        DEBUG(0,
              "Last counter:%u is 0. invalidated ideal storage set:%d, jj:%u "
              "ind:%u counter:%lld, addr:0x%s\n",
              valid_start, set, jj, QUEUE_IND(jj), cal_line->last_access_time,
              hexstr64s(cal_line->base));
      }

      DEBUG(0, "data is found in ideal storage set%u \n", set);

      new_line        = &(cache->shadow_entries[set][cache->queue_end[set]]);
      new_line->valid = TRUE;
      new_line->tag   = tag;
      new_line->base  = addr;
      new_line->last_access_time = cache->assoc;
      cache->queue_end[set] = (cache->queue_end[set] + 1) % ideal_num_entries;
      return (line->data);
    }
  }


  DEBUG(0, "Didn't find line in set %u in ideal_storage cache '%s' \n", set,
        cache->name);
  if(main_entry_found) {
    DEBUG(0, "Only_main set:%u addr:0x%s cycle_time:%lld\n", set,
          hexstr64s(addr), sim_time);
  }
  return NULL;
}


/**************************************************************************************/
/* get_cache_line_addr: */

Addr get_cache_line_addr(Cache* cache, Addr addr) {
  Addr tag;
  Addr line_addr;
  cache_index(cache, addr, &tag, &line_addr);

  return line_addr;
}


/**************************************************************************************/
/* cache_insert_lru: returns a pointer to the data section of the new cache
   line.  Sets line_addr to the address of the first block of the new line.
   Sets repl_line_addr to the address of the first block that was replaced

   DON'T call this unless you are sure that the line is not in the
   cache (call after cache_access returned NULL)

   This function inserts the entry as LRU instead of MRU
*/

void* cache_insert_lru(Cache* cache, uns8 proc_id, Addr addr, Addr* line_addr,
                       Addr* repl_line_addr) {
  Addr         tag;
  uns          repl_index;
  uns          set = cache_index(cache, addr, &tag, line_addr);
  Cache_Entry* new_line;

  if(cache->repl_policy == REPL_IDEAL) {
    new_line        = insert_sure_line(cache, set, tag);
    *repl_line_addr = 0;
  } else {
    new_line = find_repl_entry(cache, proc_id, set, &repl_index);
    /* before insert the data into cache, if the cache has shadow entry */
    /* insert that entry to the shadow cache */
    if((cache->repl_policy == REPL_SHADOW_IDEAL) && new_line->valid)
      shadow_cache_insert(cache, set, new_line->tag, new_line->base);
    if(new_line->valid)  // bug fixed. 4/26/04 if the entry is not valid,
                         // repl_line_addr should be set to 0
      *repl_line_addr = new_line->base;
    else
      *repl_line_addr = 0;
    DEBUG(0,
          "Replacing (set %u, way %u, tag 0x%s, base 0x%s) in cache '%s' with "
          "base 0x%s\n",
          set, repl_index, hexstr64s(new_line->tag), hexstr64s(new_line->base),
          cache->name, hexstr64s(*line_addr));
  }

  new_line->proc_id = proc_id;
  new_line->valid   = TRUE;
  new_line->tag     = tag;
  new_line->base    = *line_addr;
  update_repl_policy(cache, new_line, set, repl_index, TRUE);
  if(cache->repl_policy == REPL_TRUE_LRU)
    new_line->last_access_time = 137;

  if(cache->repl_policy == REPL_IDEAL_STORAGE) {
    new_line->last_access_time = cache->assoc;
    /* debug */
    /* insert into the entry also */
    {
      uns          lru_ind  = 0;
      Counter      lru_time = MAX_CTR;
      Cache_Entry* main_line;
      uns          ii;

      /* first cache access */
      for(ii = 0; ii < cache->assoc; ii++) {
        Cache_Entry* line = &cache->entries[set][ii];

        if(line->tag == tag && line->valid) {
          /* update replacement state if necessary */
          ASSERT(0, line->data);
          line->last_access_time = sim_time;
          return new_line->data;
        }
      }
      /* looking for lru */
      for(ii = 0; ii < cache->assoc; ii++) {
        Cache_Entry* entry = &cache->entries[set][ii];
        if(!entry->valid) {
          lru_ind = ii;
          break;
        }
        if(entry->last_access_time < lru_time) {
          lru_ind  = ii;
          lru_time = cache->entries[set][ii].last_access_time;
        }
      }
      main_line                   = &cache->entries[set][lru_ind];
      main_line->valid            = TRUE;
      main_line->tag              = tag;
      main_line->base             = *line_addr;
      main_line->last_access_time = sim_time;
    }
  }
  return new_line->data;
}

/**************************************************************************************/
/* reset cache: A function that initializes all lines to invalid state*/

void reset_cache(Cache* cache) {
  uns ii, jj;

  for(ii = 0; ii < cache->num_sets; ii++) {
    for(jj = 0; jj < cache->assoc; jj++) {
      cache->entries[ii][jj].valid = FALSE;
    }
  }
}

/**************************************************************************************/
/* cache_find_pos_in_lru_stack: returns the position of a cache line */
/* return -1 : cache miss  */
/*         0 : MRU         */
/*        ...........     */
/*   assoc-1 : LRU         */

int cache_find_pos_in_lru_stack(Cache* cache, uns8 proc_id, Addr addr,
                                Addr* line_addr) {
  Addr         tag;
  uns          set = cache_index(cache, addr, &tag, line_addr);
  uns          ii;
  int          position;
  Cache_Entry* hit_line = NULL;
  Flag         hit      = FALSE;

  for(ii = 0; ii < cache->assoc; ii++) {
    hit_line = &cache->entries[set][ii];

    if(hit_line->valid && hit_line->tag == tag) {
      hit = TRUE;
      break;
    }
  }

  if(!hit)
    return -1;

  ASSERT(0, hit_line);
  ASSERT(0, hit_line->proc_id == proc_id);
  position = 0;
  for(ii = 0; ii < cache->assoc; ii++) {
    Cache_Entry* line = &cache->entries[set][ii];
    if(hit_line->proc_id == line->proc_id &&
       line->last_access_time > hit_line->last_access_time) {
      position++;
    }
  }
  return position;
}


void set_partition_allocate(Cache* cache, uns8 proc_id, uns num_ways) {
  ASSERT(proc_id, cache->repl_policy == REPL_PARTITION);
  ASSERT(proc_id, cache->num_ways_allocted_core);
  cache->num_ways_allocted_core[proc_id] = num_ways;
}


uns get_partition_allocated(Cache* cache, uns8 proc_id) {
  ASSERT(proc_id, cache->repl_policy == REPL_PARTITION);
  ASSERT(proc_id, cache->num_ways_allocted_core);
  return cache->num_ways_allocted_core[proc_id];
}
