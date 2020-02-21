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
 * File         : libs/hash_lib.h
 * Author       : HPS Research Group
 * Date         : 9/22/1998
 * Description  :
 ***************************************************************************************/

#ifndef __HASH_LIB_H__
#define __HASH_LIB_H__

#include "globals/global_defs.h"


/**************************************************************************************/
/* Types */

typedef struct Hash_Table_Entry_struct {
  int64                           key;
  void*                           data;
  struct Hash_Table_Entry_struct* next;
} Hash_Table_Entry;

typedef struct Hash_Table_struct {
  char*              name;
  uns                buckets;
  uns                data_size;
  int                count;  // total number of elements in the hash table
  Hash_Table_Entry** entries;
  Flag (*eq_func)(void const* const, void const* const);
} Hash_Table;


/**************************************************************************************/
/* Prototypes */

void  init_hash_table(Hash_Table*, const char*, uns, uns);
void* hash_table_access(Hash_Table const*, int64);
void* hash_table_access_create(Hash_Table*, int64, Flag*);
Flag  hash_table_access_delete(Hash_Table*, int64);

void  init_complex_hash_table(Hash_Table*, const char*, uns, uns,
                              Flag (*)(void const*, void const*));
void* complex_hash_table_access(Hash_Table const*, int64, void const*);
void* complex_hash_table_access_create(Hash_Table*, int64, void const*, Flag*);
Flag  complex_hash_table_access_delete(Hash_Table*, int64, void const*);

void   hash_table_clear(Hash_Table*);
void** hash_table_flatten(Hash_Table*, void**);
void   hash_table_scan(Hash_Table*, void (*)(void*, void*), void*);
void   hash_table_rehash(Hash_Table*, int);

void hash_table_access_replace(Hash_Table*, int64, void*);
/**************************************************************************************/

#endif /* #ifndef __HASH_LIB_H__ */
