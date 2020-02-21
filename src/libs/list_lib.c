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
 * File         : libs/list_lib.c
 * Author       : HPS Research Group
 * Date         : 12/14/1998
 * Description  :
 ***************************************************************************************/

#include "debug/debug_macros.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"

#include "libs/list_lib.h"
#include "libs/malloc_lib.h"

#include "debug/debug.param.h"


/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_LIST_LIB, ##args)
#define DEBUGU(proc_id, args...) _DEBUGU(proc_id, DEBUG_LIST_LIB, ##args)
#define FREE_LIST_ALLOC_SIZE 8
#define VERIFY_LIST_COUNTS FALSE


/**************************************************************************************/
/* Prototypes */

static inline List_Entry* get_list_entry(List*);
static inline void        free_list_entry(List*, List_Entry*);
static inline void        verify_list_counts(List*);


/**************************************************************************************/
/* init_list: */

void init_list(List* list, char name[], uns data_size, Flag use_free_list) {
  DEBUGU(0, "Initializing list called '%s'.\n", name);

  /* set the basic parameters */
  list->name          = strdup(name);
  list->data_size     = data_size;
  list->head          = NULL;
  list->tail          = NULL;
  list->free          = NULL;
  list->count         = 0;
  list->use_free_list = use_free_list;

  list->free_count  = 0;
  list->total_count = 0;
}


/**************************************************************************************/
/* clear_list: */

void clear_list(List* list) {
  DEBUG(0, "Clearing list '%s'.\n", list->name);
  if(list->tail) {
    if(list->use_free_list) {
      list->tail->next = list->free;
      list->free       = list->head;

      list->free_count += list->count;
    } else {
      List_Entry *temp0, *temp1;
      for(temp0 = list->head; temp0 != NULL; temp0 = temp1) {
        temp1 = temp0->next;
        free(temp0);
      }
    }
    list->head  = NULL;
    list->tail  = NULL;
    list->count = 0;
  } else
    ASSERT(0, list->count == 0);

  verify_list_counts(list);
}


/**************************************************************************************/
/* clip_list_at_current: */

void clip_list_at_current(List* list) {
  DEBUG(0, "Clipping list '%s'.\n", list->name);
  ASSERT(0, list);
  ASSERT(0, list->current);
  if(list->current->next) {
    if(list->use_free_list) {
      list->tail->next    = list->free;
      list->free          = list->current->next;
      list->current->next = NULL;

      list->free_count += list->count - (list->place + 1);
    } else {
      List_Entry *temp0, *temp1;
      for(temp0 = list->current->next; temp0 != NULL; temp0 = temp1) {
        temp1 = temp0->next;
        free(temp0);
      }
    }
    list->tail       = list->current;
    list->tail->next = NULL;
    list->count      = list->place + 1;
  }
  verify_list_counts(list);
}


/**************************************************************************************/
/* alloc_list_entry: */

static inline List_Entry* get_list_entry(List* list) {
  List_Entry *temp, *rval;
  uns         ii, size;

  if(list->use_free_list) {
    if(list->free) {
      temp       = list->free;
      list->free = temp->next;
      list->free_count--;
      return temp;
    }

    size = sizeof(List_Entry) + list->data_size - sizeof(char);

    ASSERT(0, FREE_LIST_ALLOC_SIZE > 1);
    rval = (List_Entry*)malloc(size * FREE_LIST_ALLOC_SIZE);
    ASSERT(0, rval);
    temp       = (List_Entry*)((char*)rval + size);
    list->free = temp;
    list->total_count += FREE_LIST_ALLOC_SIZE;
    list->free_count += FREE_LIST_ALLOC_SIZE - 1;

    for(ii = 0; ii < FREE_LIST_ALLOC_SIZE - 2; ii++) {
      char* temp2;
      temp->next = (List_Entry*)((char*)temp + size);
      temp2      = (char*)temp +
              size;  // work-around: stupid c++ can't handle my code
      temp = (List_Entry*)temp2;
    }
    temp->next = NULL;
  } else {
    size = sizeof(List_Entry) + list->data_size - sizeof(char);
    rval = (List_Entry*)malloc(size);
    ASSERT(0, rval);
    list->total_count++;
  }

  return rval;
}


/**************************************************************************************/
/* free_list_entry: */

static inline void free_list_entry(List* list, List_Entry* entry) {
  if(list->use_free_list) {
    entry->next = list->free;
    list->free  = entry;
    list->free_count++;
  } else {
    free(entry);
    list->total_count--;
  }
  list->count--;
  verify_list_counts(list);
}


/**************************************************************************************/
/* sl_list_add_tail: */

void* sl_list_add_tail(List* list) {
  List_Entry* temp = get_list_entry(list);

  DEBUG(0, "Adding to list '%s' at tail.\n", list->name);

  temp->next = NULL;

  if(list->count == 0) {
    list->head = temp;
    list->tail = temp;
  } else {
    list->tail->next = temp;
    list->tail       = temp;
  }
  list->count++;
  DEBUG(0, "%d %d %d\n", list->count, list->free_count, list->total_count);
  verify_list_counts(list);
  return &temp->data;
}


/**************************************************************************************/
/* dl_list_add_tail: */

void* dl_list_add_tail(List* list) {
  List_Entry* temp = get_list_entry(list);

  DEBUG(0, "Adding to list '%s' at tail.\n", list->name);

  temp->next = NULL;
  temp->prev = NULL;

  if(list->count == 0) {
    temp->prev = NULL;
    list->head = temp;
    list->tail = temp;
  } else {
    temp->prev       = list->tail;
    list->tail->next = temp;
    list->tail       = temp;
  }
  list->count++;
  DEBUG(0, "%d %d %d\n", list->count, list->free_count, list->total_count);
  verify_list_counts(list);
  return &temp->data;
}


/**************************************************************************************/
/* sl_list_add_tail: */

void* sl_list_add_head(List* list) {
  List_Entry* temp = get_list_entry(list);

  DEBUG(0, "Adding to list '%s' at head.\n", list->name);

  temp->next = list->head;
  list->head = temp;

  if(list->count == 0)
    list->tail = temp;

  list->count++;
  DEBUG(0, "%d %d %d\n", list->count, list->free_count, list->total_count);
  verify_list_counts(list);
  return &temp->data;
}


/**************************************************************************************/
/* dl_list_add_head: */

void* dl_list_add_head(List* list) {
  List_Entry* temp = get_list_entry(list);

  DEBUG(0, "Adding to list '%s' at head.\n", list->name);

  temp->next = list->head;
  temp->prev = NULL;

  if(list->count == 0)
    list->tail = temp;
  else
    list->head->prev = temp;

  list->head = temp;

  list->count++;
  DEBUG(0, "%d %d %d\n", list->count, list->free_count, list->total_count);
  verify_list_counts(list);
  return &temp->data;
}


/**************************************************************************************/
/* sl_list_remove_head: */

void* sl_list_remove_head(List* list) {
  void*       temp;
  List_Entry* free;

  DEBUG(0, "Removing head of list '%s'.\n", list->name);

  if(list->head) {
    temp = &list->head->data;
    free = list->head;
    if(list->tail == list->head) {
      list->head = NULL;
      list->tail = NULL;
    } else
      list->head = list->head->next;
    free_list_entry(list, free);
  } else {
    verify_list_counts(list);
    return NULL;
  }

  DEBUG(0, "%d %d %d\n", list->count, list->free_count, list->total_count);
  verify_list_counts(list);
  return temp;
}


/**************************************************************************************/
/* dl_list_remove_head: */

void* dl_list_remove_head(List* list) {
  void*       temp;
  List_Entry* free;

  DEBUG(0, "Removing head of list '%s'.\n", list->name);

  if(list->head) {
    temp = &list->head->data;
    free = list->head;
    if(list->tail == list->head) {
      list->head = NULL;
      list->tail = NULL;
    } else {
      list->head       = list->head->next;
      list->head->prev = NULL;
    }
    free_list_entry(list, free);
  } else {
    verify_list_counts(list);
    return NULL;
  }

  DEBUG(0, "%d %d %d\n", list->count, list->free_count, list->total_count);
  verify_list_counts(list);
  return temp;
}

/**************************************************************************************/
/* dl_list_remove_tail: */

void* dl_list_remove_tail(List* list) {
  void*       temp;
  List_Entry* free;

  DEBUG(0, "Removing tail of list '%s'.\n", list->name);

  if(list->tail) {
    temp = &list->tail->data;
    free = list->tail;
    if(list->tail == list->head) {
      list->head = NULL;
      list->tail = NULL;
    } else {
      list->tail       = list->tail->prev;
      list->tail->next = NULL;
    }
    free_list_entry(list, free);
  } else {
    verify_list_counts(list);
    return NULL;
  }

  DEBUG(0, "%d %d %d\n", list->count, list->free_count, list->total_count);
  verify_list_counts(list);
  return temp;
}


/**************************************************************************************/
/* dl_list_remove_current: */

void* dl_list_remove_current(List* list) {
  void*       temp;
  List_Entry *free, *next, *prev;

  DEBUG(0, "Removing current of list '%s'.\n", list->name);

  ASSERT(0, list->current);
  next = list->current->next;
  prev = list->current->prev;
  free = list->current;
  temp = &list->current->data;

  if(next && prev) {
    next->prev    = prev;
    prev->next    = next;
    list->current = prev;
  } else if(next && !prev) {
    ASSERT(0, list->head == list->current);
    list->head    = next;
    next->prev    = NULL;
    list->current = NULL;
  } else if(!next && prev) {
    ASSERT(0, list->tail == list->current);
    list->tail    = prev;
    list->current = prev;
    prev->next    = NULL;
  } else {
    ASSERT(0, list->head == list->current);
    ASSERT(0, list->tail == list->current);
    list->head    = NULL;
    list->tail    = NULL;
    list->current = NULL;
  }

  free_list_entry(list, free);

  DEBUG(0, "%d %d %d\n", list->count, list->free_count, list->total_count);
  verify_list_counts(list);
  return temp;
}


/**************************************************************************************/
/* sl_list_add_after_current: */

void* sl_list_add_after_current(List* list) {
  List_Entry* temp = get_list_entry(list);

  DEBUG(0, "Adding after current of list '%s'.\n", list->name);

  if(!list->current)  // assume that it should go at the tail
    return sl_list_add_tail(list);

  temp->next          = list->current->next;
  list->current->next = temp;
  if(list->tail == list->current)
    list->tail = temp;
  list->count++;

  DEBUG(0, "%d %d %d\n", list->count, list->free_count, list->total_count);
  verify_list_counts(list);
  return &temp->data;
}


/**************************************************************************************/
/* dl_list_add_after_current: */

void* dl_list_add_after_current(List* list) {
  List_Entry* temp = get_list_entry(list);

  DEBUG(0, "Adding after current of list '%s'.\n", list->name);

  if(!list->current)  // assume that it should go at the tail
    return dl_list_add_tail(list);

  temp->next = list->current->next;
  temp->prev = list->current;
  if(list->tail == list->current)
    list->tail = temp;
  else
    list->current->next->prev = temp;
  list->current->next = temp;
  list->count++;

  DEBUG(0, "%d %d %d\n", list->count, list->free_count, list->total_count);
  verify_list_counts(list);
  return &temp->data;
}


/**************************************************************************************/
/* list_get_head: */

void* list_get_head(List* list) {
  ASSERT(0, list);
  return list->head ? &list->head->data : NULL;
}


/**************************************************************************************/
/* list_get_tail: */

void* list_get_tail(List* list) {
  ASSERT(0, list);
  return list->tail ? &list->tail->data : NULL;
}


/**************************************************************************************/
/* list_get_current: */

void* list_get_current(List* list) {
  ASSERT(0, list);
  return list->current ? &list->current->data : NULL;
}


/**************************************************************************************/
/* list_start_head_traversal: */

void* list_start_head_traversal(List* list) {
  ASSERT(0, list);
  list->current = list->head;
  list->place   = 0;
  return list->current ? &list->current->data : NULL;
}


/**************************************************************************************/
/* list_start_tail_traversal: */

void* list_start_tail_traversal(List* list) {
  ASSERT(0, list);
  list->current = list->tail;
  list->place   = list->count - 1;
  return list->current ? &list->current->data : NULL;
}


/**************************************************************************************/
/* list_next_element: */

void* list_next_element(List* list) {
  ASSERT(0, list);
  list->current = list->current ? list->current->next : list->head;
  list->place++;
  return list->current ? &list->current->data : NULL;
}


/**************************************************************************************/
/* list_prev_element: */

void* list_prev_element(List* list) {
  ASSERT(0, list);
  list->current = list->current ? list->current->prev : list->tail;
  list->place--;
  return list->current ? &list->current->data : NULL;
}


/**************************************************************************************/
// list_at_head:

Flag list_at_head(List* list) {
  ASSERT(0, list);
  return list->current == list->head;
}


/**************************************************************************************/
// list_at_tail:

Flag list_at_tail(List* list) {
  ASSERT(0, list);
  return list->current == list->tail;
}


/**************************************************************************************/
// list_flatten:

void** list_flatten(List* list) {
  void**      new_array;
  List_Entry* cur;
  int         ii;

  ASSERT(0, list);

  cur       = list->head;
  new_array = (void**)malloc(sizeof(void*) * list->count);
  for(ii = 0; ii < list->count; ii++) {
    ASSERT(0, cur);
    new_array[ii] = &cur->data;
    cur           = cur->next;
  }

  return new_array;
}


/**************************************************************************************/
/* verify_list_counts: */

static inline void verify_list_counts(List* list) {
#if VERIFY_LIST_COUNTS
  List_Entry* temp;
  uns         count       = 0;
  uns         free_count  = 0;
  uns         total_count = 0;

  for(temp = list->head; temp; temp = temp->next) {
    count++;
    total_count++;
  }

  for(temp = list->free; temp; temp = temp->next) {
    free_count++;
    total_count++;
  }

  ASSERT(0, count + free_count == total_count);
  ASSERT(0, list->count + list->free_count == list->total_count);
  ASSERTM(0, count == list->count, "%d %d\n", count, list->count);
  ASSERTM(0, free_count == list->free_count, "%d %d\n", free_count,
          list->free_count);
  ASSERT(0, total_count == list->total_count);
#endif
}


/**************************************************************************************/
/* list_get_count: */

int list_get_count(List* list) {
  ASSERT(0, list);
  return list->count;
}
