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
 * File         : libs/new_cache_lib.h
 * Author       : HPS Research Group
 * Date         : 3/10/2022
 * Description  : This is a library of cache functions.
 ***************************************************************************************/

#include "globals/global_defs.h"
#include "libs/list_lib.h"
#include "libs/cache_lib/repl.h"
#include "globals/utils.h"
#include <vector>
#include <string>

class Cache_entry {
  public:
    uns8 proc_id;
    Flag valid;
    Addr tag;
    Addr base;
    Flag dirty;

    //replacement info should include last_access_time, insertion_time, pf
};

template <typename T> 
class Cache {
  public:
    String name;

    //uns8 data_size; //size used for malloc
    uns8 assoc;
    uns num_lines;
    uns num_sets;
    uns8 shift_amount; 
    Addr set_mask;    /* mask applied after shifting to get the index */
    Addr tag_mask;    /* mask used to get the tag after shifting */
    Addr offset_mask; /* mask used to get the line offset */

    uns8 set_bits;
    Repl_Policy_enum repl;

    std::vector<Cache_entry> entries;   
    std::vector<T> data;
    std::vector<uns> repl_set;

    Counter num_demand_access;
    Counter last_update; /* last update cycle */

    Cache(std::string name, uns cache_size, uns assoc, uns line_size, Repl_Policy_enum repl) :
    {
      this->name = name;
      this->assoc = assoc;
      this->line_size = line_size;
      
      this->num_lines = cache_size / line_size;
      this->num_sets  = cache_size / line_size / assoc;

      this->set_bits = LOG2(num_sets);
      this->shift_amount = LOG2(line_size);
      this->set_mask = N_BIT_MASK(LOG2(num_sets));
      this->tag_mask = ~this->set_mask;
      this->offset_mask = N_BIT_MASK(cache->shift_bits);



      entries.resize(num_lines);
      data.resize(num_lines);
      repl_set.resize(assoc);

      num_demand_access = 0;
      last_update = 0;
    }
    
    ~Cache(); //do i need this?
    
    inline uns cache_index(Addr addr) {
      return addr >> cache->shift_bits & cache->set_mask;
    }

    inline Addr cache_tag(Addr addr) {
      return addr >> this->shift_bits & this->tag_mask;
    }

    inline Addr cache_line_addr(Addr addr) {
      return addr & ~this->offset_mask;
    }

    T* access(uns proc_id, Addr addr){
      uns index = search(proc_id, addr);
      if(index != 0xFFFFFFFF) {
        if(entries[index]->pref) {
          line->pref = FALSE;
        }
        cache->num_demand_access++;
        //TODO: add the replacement policy update
        return data[index]; 
      }
      return NULL; 
    }

    T* probe(uns proc_id, Addr addr){
      uns index = search(proc_id, addr);
      if(index != 0xFFFFFFFF)  
        return data[index]; 
      }
      return NULL; 
    }

    //searches the cache for the line, returns the index into data and entries vector if found,
    //returns 0xFFFFFFFF if not found
    uns search(uns proc_id, Addr addr){
      Addr tag = cache_tag (addr);
      uns  set = cache_index(addr);
      uns  ii;

      for(ii = 0; ii < cache->assoc; ii++) {
        Cache_Entry line = entries[set*assoc + ii];

        if(line.valid && line.tag == tag) {
          /* update replacement state if necessary */
          ASSERT(proc_id, line->data);
          if(update_repl) {
          }
          ASSERT(porc_id, ~(set*assoc + ii));
          return set*assoc + ii;
        }
      }
      DEBUG(proc_id, "Didn't find line in set %u in cache '%s' base 0x%s\n", set,
            this->name, hexstr64s(addr));
      return 0xFFFFFFFF;
    }
    
    T insert(uns proc_id, Addr addr){
      Addr tag = cache_tag (addr);
      Addr line_addr = cache_line_addr(addr);
      uns  set = cache_index(addr);

      uns base = set * assoc;
      for(uns ii = 0; ii < assoc; i++){
        repl_set[ii] = base + ii;
      }
      
      uns new_line_index = find_next_repl_index(repl_set);

      entries[new_line_index].valid = true;
      entries[new_line_index].tag = tag;
      entries[new_line_index].proc_id = proc_id;
      entries[new_line_index].base = line_addr;
      
      return NULL;
    }
    
    T invalidate(uns proc_id, Addr addr){
      uns index = search(proc_id, addr);
      if(index != 0xFFFFFFFF){
        entries[index].tag = 0;  
        entries[index].valid= FALSE;  
        entries[index].base= FALSE;  
      }
      return NULL;
    }
    
    T get_next_repl_line(){

    }
};