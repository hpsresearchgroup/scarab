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
 * File         : libs/list_lib.h
 * Author       : HPS Research Group
 * Date         : 12/14/1998
 * Description  :
 ***************************************************************************************/

#ifndef __LIST_LIB_H__
#define __LIST_LIB_H__

#include "globals/global_defs.h"
#include "globals/global_types.h"


/**************************************************************************************/
/* Types */

typedef struct List_Entry_struct {
  struct List_Entry_struct *next, *prev;
  char                      data; /* placeholder, don't put any
       fields after this or you
       will be screwed */
} List_Entry;


typedef struct List_struct {
  char* name;      /* name of the linked list */
  uns   data_size; /* size of the data elements in the list */

  List_Entry *head, *tail; /* pointers to the head and tail of the list */
  List_Entry* current;     /* pointer to the current element (for traversals) */
  List_Entry* free;        /* pointer to the free entry pool for the list */
  int         count;       /* count of elements in the list */
  int         place;       /* place of 'current' in the list (starts at 0) */
  Flag use_free_list;      /* whether or not to use free list or free memory on
                              removal */

  int free_count;
  int total_count;
} List;


/**************************************************************************************/
/* Prototypes */

void  init_list(List*, char[], uns, Flag);
void  clear_list(List*);
void  clip_list_at_current(List*);
void* sl_list_add_tail(List*);
void* dl_list_add_tail(List*);
void* sl_list_add_head(List*);
void* dl_list_add_head(List*);
void* sl_list_remove_head(List*);
void* dl_list_remove_head(List*);
void* dl_list_remove_tail(List*);
void* dl_list_remove_current(List*);
void* sl_list_add_after_current(List*);
void* dl_list_add_after_current(List*);
void* list_get_head(List*);
void* list_get_tail(List*);
void* list_get_current(List*);

void* list_start_head_traversal(List*);
void* list_start_tail_traversal(List*);
void* list_next_element(List*);
void* list_prev_element(List*);

Flag list_at_head(List*);
Flag list_at_tail(List*);

void** list_flatten(List*);
int    list_get_count(List*);


/**************************************************************************************/

#endif /* #ifndef __LIST_LIB_H__ */
