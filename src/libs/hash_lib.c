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
 * File         : libs/hash_lib.c
 * Author       : HPS Research Group
 * Date         : 9/22/1998
 * Description  : A hash table library.
 ***************************************************************************************/

#include <stdlib.h>
#include "debug/debug_macros.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"

#include "libs/hash_lib.h"
#include "libs/malloc_lib.h"

#include "debug/debug.param.h"


/**************************************************************************************/
/* Macros */

#define DEBUG(args...) _DEBUG(DEBUG_HASH_LIB, ##args)

#define HASH_INDEX(table, key) (((uns)(key)) % (table)->buckets)


/**************************************************************************************/
// Global variables

#define NUM_HASH_TABLE_PRIMES 12
static uns const hash_table_primes[NUM_HASH_TABLE_PRIMES] = {
  1, 5, 11, 23, 47, 101, 211, 401, 811, 1601, 3209, 6373};


/**************************************************************************************/
/* init_hash_table: */

void init_hash_table(Hash_Table* table, const char* name, uns buckets,
                     uns data_size) {
  init_complex_hash_table(table, name, buckets, data_size, NULL);
}

void init_complex_hash_table(Hash_Table* table, const char* name, uns buckets,
                             uns data_size,
                             Flag (*eq_func)(void const*, void const*)) {
  table->name      = strdup(name);
  table->buckets   = buckets;
  table->data_size = data_size;
  table->count     = 0;
  table->entries   = (Hash_Table_Entry**)calloc(buckets,
                                              sizeof(Hash_Table_Entry*));
  table->eq_func   = eq_func;
}


/**************************************************************************************/
/* hash_table_access: access the hash table.  Return the data pointer
   if it hits, NULL otherwise */


void* hash_table_access(Hash_Table const* table, int64 key) {
  // {{{ access hash table using simple key compare
  uns               index  = HASH_INDEX(table, key);
  Hash_Table_Entry* bucket = table->entries[index];
  Hash_Table_Entry* temp;

  for(temp = bucket; temp != NULL; temp = temp->next)
    if(temp->key == key)
      return temp->data;

  return NULL;
  // }}}
}

void* complex_hash_table_access(Hash_Table const* table, int64 key,
                                void const* data) {
  // {{{ access hash table using a complex comparison
  uns               index  = HASH_INDEX(table, key);
  Hash_Table_Entry* bucket = table->entries[index];
  Hash_Table_Entry* temp;

  ASSERT(0, table->eq_func);
  ASSERT(0, data);

  for(temp = bucket; temp != NULL; temp = temp->next)
    if(temp->key == key && table->eq_func(temp->data, data))
      return temp->data;

  return NULL;
  // }}}
}


/**************************************************************************************/
/* hash_table_access_create: access the hash table.  Return the data
   pointer if it hits an existing entry.  Otherwise, allocate a new
   entry and return its data pointer. */

void* hash_table_access_create(Hash_Table* table, int64 key, Flag* new_entry) {
  // {{{ access hash table using simple key compare
  uns               index    = HASH_INDEX(table, key);
  Hash_Table_Entry* bucket   = table->entries[index];
  Hash_Table_Entry* new_hash = NULL;
  Hash_Table_Entry* temp;
  Hash_Table_Entry* prev = NULL;

  *new_entry = FALSE;
  ASSERT(0, index < table->buckets);
  for(temp = bucket; temp != NULL; temp = temp->next) {
    if(temp->key == key)
      return temp->data;
    else
      prev = temp;
  }
  table->count++;
  *new_entry = TRUE;
  new_hash   = (Hash_Table_Entry*)smalloc(sizeof(Hash_Table_Entry));
  ASSERT(0, new_hash);
  new_hash->key  = key;
  new_hash->next = NULL;
  new_hash->data = (void*)smalloc(table->data_size);
  ASSERT(0, new_hash->data);

  if(prev)
    prev->next = new_hash;
  else
    table->entries[index] = new_hash;

  _DEBUGA(0, 0, "smalloc'd %ld bytes for %s (%d entries)\n",
          (unsigned long int)sizeof(Hash_Table_Entry) + table->data_size,
          table->name, table->count);

  return new_hash->data;
  // }}}
}

void* complex_hash_table_access_create(Hash_Table* table, int64 key,
                                       void const* data, Flag* new_entry) {
  // {{{ access hash table using a complex comparison
  uns               index    = HASH_INDEX(table, key);
  Hash_Table_Entry* bucket   = table->entries[index];
  Hash_Table_Entry* new_hash = NULL;
  Hash_Table_Entry* temp;
  Hash_Table_Entry* prev = NULL;

  ASSERT(0, table->eq_func);
  ASSERT(0, data);

  *new_entry = FALSE;
  ASSERT(0, index < table->buckets);
  for(temp = bucket; temp != NULL; temp = temp->next) {
    if(temp->key == key && table->eq_func(temp->data, data))
      return temp->data;
    else
      prev = temp;
  }
  table->count++;
  *new_entry = TRUE;
  new_hash   = (Hash_Table_Entry*)smalloc(sizeof(Hash_Table_Entry));
  ASSERT(0, new_hash);
  new_hash->key  = key;
  new_hash->next = NULL;
  new_hash->data = (void*)smalloc(table->data_size);
  ASSERT(0, new_hash->data);

  if(prev)
    prev->next = new_hash;
  else
    table->entries[index] = new_hash;

  _DEBUGA(0, 0, "smalloc'd %ld bytes for %s (%d entries)\n",
          (unsigned long int)sizeof(Hash_Table_Entry) + table->data_size,
          table->name, table->count);

  return new_hash->data;
  // }}}
}


/**************************************************************************************/
/* hash_table_access_delete: look up an entry and delete it. return
   TRUE if it was found, FALSE otherwise */

Flag hash_table_access_delete(Hash_Table* table, int64 key) {
  // {{{ access hash table using simple key compare
  uns               index  = HASH_INDEX(table, key);
  Hash_Table_Entry* bucket = table->entries[index];
  Hash_Table_Entry* temp;
  Hash_Table_Entry* prev = NULL;

  for(temp = bucket; temp != NULL; temp = temp->next) {
    if(temp->key == key) {
      Hash_Table_Entry* next_ptr = temp->next;
      sfree(table->data_size, temp->data);
      sfree(sizeof(Hash_Table_Entry), temp);
      if(prev)
        prev->next = next_ptr;
      else
        table->entries[index] = next_ptr;
      table->count--;
      ASSERT(0, table->count >= 0);
      return TRUE;
    } else
      prev = temp;
  }

  return FALSE;
  // }}}
}

Flag complex_hash_table_access_delete(Hash_Table* table, int64 key,
                                      void const* data) {
  // {{{ access hash table using a complex comparison
  uns               index  = HASH_INDEX(table, key);
  Hash_Table_Entry* bucket = table->entries[index];
  Hash_Table_Entry* temp;
  Hash_Table_Entry* prev = NULL;

  ASSERT(0, table->eq_func);
  ASSERT(0, data);

  for(temp = bucket; temp != NULL; temp = temp->next) {
    if(temp->key == key && table->eq_func(temp->data, data)) {
      Hash_Table_Entry* next_ptr = temp->next;
      sfree(table->data_size, temp->data);
      sfree(sizeof(Hash_Table_Entry), temp);
      if(prev)
        prev->next = next_ptr;
      else
        table->entries[index] = next_ptr;
      table->count--;
      ASSERT(0, table->count >= 0);
      return TRUE;
    } else
      prev = temp;
  }

  return FALSE;
  // }}}
}


/**************************************************************************************/
/* hash_table_clear: */

void hash_table_clear(Hash_Table* table) {
  Hash_Table_Entry* temp0;
  Hash_Table_Entry* temp1;
  uns               count = 0;
  int               ii;

  for(ii = 0; ii < table->buckets; ii++) {
    temp0 = table->entries[ii];
    while(temp0) {
      temp1 = temp0->next;
      sfree(table->data_size, temp0->data);
      sfree(sizeof(Hash_Table_Entry), temp0);
      temp0 = temp1;
      count++;
    }
    table->entries[ii] = NULL;
  }
  ASSERT(0, count == table->count);
  table->count = 0;
}


/**************************************************************************************/
/* hash_table_flatten: counts up hash table entries and returns a newly
   allocated array of pointers to the data elements (copied from the hash nodes)
 */

void** hash_table_flatten(Hash_Table* table, void** reuse_array) {
  Hash_Table_Entry* temp;
  void**            new_array;
  uns               count = 0;
  int               ii;

  if(table->count == 0)
    return NULL;

  if(reuse_array)
    new_array = reuse_array;
  else {
    new_array = (void**)malloc(sizeof(void*) * table->count);
    ASSERT(0, new_array);
  }

  /* write into the new array */
  count = 0;
  for(ii = 0; ii < table->buckets; ii++) {
    temp = table->entries[ii];
    while(temp) {
      new_array[count++] = temp->data;
      temp               = temp->next;
    }
  }

  ASSERTM(0, count == table->count, "%d %d\n", count, table->count);
  ASSERTM(0, count > 0, "%d %d\n", count, table->count);

  return new_array;
}


/**************************************************************************************/
// hash_table_scan: scans all of the nodes in the hash table and runs
// the scan_fanc on them

void hash_table_scan(Hash_Table* table, void (*scan_func)(void*, void*),
                     void*       arg) {
  int               count = 0;
  Hash_Table_Entry* temp;
  int               ii;

  ASSERT(0, scan_func);

  if(table->count == 0)
    return;

  for(ii = 0; ii < table->buckets; ii++) {
    temp = table->entries[ii];
    while(temp) {
      count++;
      scan_func(temp->data, arg);
      temp = temp->next;
    }
  }
  ASSERT(0, count == table->count);
}


/**************************************************************************************/
// hash_table_rehash: expand or contract the hash table
// WARNING: this hasn't really been tested

void hash_table_rehash(Hash_Table* table, int new_buckets) {
  Hash_Table_Entry** new_entries;
  Hash_Table_Entry** flat_entries;
  Hash_Table_Entry** old_entries = table->entries;
  uns                old_buckets = table->buckets;
  Hash_Table_Entry*  temp;
  uns                ii, jj;

  ASSERT(0, new_buckets >= 0);
  if(new_buckets == 0) {  // use next prime
    new_buckets = table->buckets;
    for(ii = 0; ii < NUM_HASH_TABLE_PRIMES - 1; ii++)
      if(hash_table_primes[ii] == old_buckets) {
        new_buckets = hash_table_primes[ii + 1];
        break;
      }
  }
  if(new_buckets == old_buckets)
    return;

  // flatten hash table into an array of entries
  flat_entries = (Hash_Table_Entry**)malloc(table->count *
                                            sizeof(Hash_Table_Entry*));
  for(ii = 0, jj = 0; ii < old_buckets; ii++)
    for(temp = old_entries[ii]; temp; temp = temp->next)
      flat_entries[jj++] = temp;
  ASSERT(0, jj == table->count);

  // replace old with new and free the old entry array
  new_entries = (Hash_Table_Entry**)calloc(new_buckets,
                                           sizeof(Hash_Table_Entry*));
  ASSERT(0, new_entries);
  ASSERT(0, new_buckets > 0 && new_buckets < 100000);
  table->buckets = new_buckets;
  table->entries = new_entries;
  free(old_entries);

  // insert each element
  for(ii = 0; ii < table->count; ii++) {
    Hash_Table_Entry* temp  = flat_entries[ii];
    uns               index = HASH_INDEX(table, temp->key);
    temp->next              = table->entries[index];
    table->entries[index]   = temp;
  }

  free(flat_entries);
}

/**************************************************************************************/
// hash_table_access_replace: replace the data in an existing entry, or create
// it
//                            if it doesn't exist yet
void hash_table_access_replace(Hash_Table* table, int64 key,
                               void* replacement) {
  // {{{ access hash table using simple key compare
  uns               index    = HASH_INDEX(table, key);
  Hash_Table_Entry* bucket   = table->entries[index];
  Hash_Table_Entry* new_hash = NULL;
  Hash_Table_Entry* temp;
  Hash_Table_Entry* prev      = NULL;
  Flag              new_entry = FALSE;
  UNUSED(new_entry);
  ASSERT(0, replacement);
  ASSERT(0, index < table->buckets);
  for(temp = bucket; temp != NULL; temp = temp->next) {
    if(temp->key == key) {
      /* May not want to free the memory in case there are other valid pointers
         to it. ASSERT(0,temp->data); free(table->data_size, temp->data);
      */
      temp->data = replacement;
      return;
    } else {
      prev = temp;
    }
  }
  table->count++;
  new_entry = TRUE;
  new_hash  = (Hash_Table_Entry*)smalloc(sizeof(Hash_Table_Entry));
  ASSERT(0, new_hash);
  new_hash->key  = key;
  new_hash->next = NULL;
  new_hash->data = replacement;

  if(prev)
    prev->next = new_hash;
  else
    table->entries[index] = new_hash;

  _DEBUGA(0, 0, "smalloc'd %ld bytes for %s (%d entries)\n",
          (unsigned long int)sizeof(Hash_Table_Entry), table->name,
          table->count);
  // }}}
}
