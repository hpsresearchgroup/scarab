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

#ifndef __CACHE_CPP_H__
#define __CACHE_CPP_H__

class Cache_entry {
  public:
  uns8 proc_id;
  Flag valid;
  Addr tag;
  Addr base;
  Flag dirty;
  Flag pref;
};

template <typename T> 
class Cache_access_result {
  public:
  Flag hit;
  Addr access_addr;
  Addr line_addr;
  T data;
  Cache_address cache_addr;

  Cache_access_result() :
    hit(false) {
  }
};

template <typename T> 
class Cache_cpp {
  public:
    std::string name;

    uns8 assoc;
    uns line_size;
    uns num_lines;
    uns num_sets;
    uns8 shift_amount; 
    Addr set_mask;    /// mask applied after shifting to get the index
    Addr tag_mask;    /// mask used to get the tag after shifting
    Addr offset_mask; /// mask used to get the line offset

    uns8 set_bits;

    std::vector<std::vector<Cache_entry>> entries;   
    std::vector<std::vector<T>> data;
    std::vector<Cache_address> repl_set;

    Counter num_demand_access;
    Counter last_update; /* last update cycle */

    repl_class repl; 

    Cache_cpp(std::string name, uns cache_size, uns assoc, uns line_size, Repl_Policy_CPP_enum repl_policy) :
      name(name), assoc(assoc), line_size(line_size), num_lines(cache_size/line_size), 
      num_sets(cache_size/line_size/assoc), 
      entries(num_sets), data(num_sets), repl_set(assoc),
      num_demand_access(0), last_update(0),
      repl(repl_policy, num_sets, assoc){
      this->set_bits = LOG2(num_sets);
      this->shift_amount = LOG2(line_size);
      this->set_mask = N_BIT_MASK(LOG2(num_sets));
      this->tag_mask = ~this->set_mask;
      this->offset_mask = N_BIT_MASK(shift_amount);

      for(uns ii = 0; ii < num_sets; ii++) {
        entries[ii].resize(assoc);
        data[ii].resize(assoc);
      }
    }
    
    inline uns cache_index(Addr addr) {
      return addr >> this->shift_amount & this->set_mask;
    }

    inline Addr cache_tag(Addr addr) {
      return addr >> this->shift_amount & this->tag_mask;
    }

    inline Addr cache_line_addr(Addr addr) {
      return addr & ~this->offset_mask;
    }

    Cache_access_result<T> access(uns proc_id, Addr addr) {
      Cache_address cache_addr = search(proc_id, addr);
      Cache_access_result<T> ret;
      if(cache_addr.valid) {
        if(entries[cache_addr.set][cache_addr.way].pref) {
          entries[cache_addr.set][cache_addr.way].pref = FALSE;
        }
        this->num_demand_access++;
        ret.hit = true;
        ret.access_addr = addr;
        ret.line_addr = cache_line_addr(addr);
        ret.data = data[cache_addr.set][cache_addr.way];
        ret.cache_addr = cache_addr;
        repl.access(cache_addr);
      }
      return ret; 
    }

    Cache_access_result<T> probe(uns proc_id, Addr addr){
      Cache_address cache_addr = search(proc_id, addr);
      Cache_access_result<T> ret;
      if(cache_addr.valid) {
        ret.hit = true;
        ret.access_addr = addr;
        ret.line_addr = cache_line_addr(addr);
        ret.data = data[cache_addr.set][cache_addr.way];
        ret.cache_addr = cache_addr;
      }
      return ret; 
    }

    Cache_address search(uns proc_id, Addr addr){
      Addr tag = cache_tag (addr);
      uns  set = cache_index(addr);
      uns  ii;
      Cache_address ret;
      ret.valid = 0;

      for(ii = 0; ii < assoc; ii++) {
        auto line = entries[set][ii];

        if(line.valid && line.tag == tag) {
          ret.valid = true;
          ret.set = set;
          ret.way = ii;
          return ret;
        }
      }
      //DEBUG(proc_id, "Didn't find line in set %u in cache '%s' base 0x%s\n", set,
      //      this->name, hexstr64s(addr));
      return ret;
    }
    
    Cache_access_result<T> insert(uns proc_id, Addr addr, Flag is_prefetch, T new_data){
      Addr tag = cache_tag (addr);
      Addr line_addr = cache_line_addr(addr);
      uns  set = cache_index(addr);

      for(uns ii = 0; ii < assoc; ii++){
        repl_set[ii].valid = true;
        repl_set[ii].set = set;
        repl_set[ii].way = ii;
      }
      
      Cache_address new_line_addr = repl.get_next_repl(repl_set);

      Cache_access_result<T> ret;
      ret.hit = entries[new_line_addr.set][new_line_addr.way].valid;
      ret.line_addr = entries[new_line_addr.set][new_line_addr.way].tag;
      ret.access_addr = addr;
      ret.data = data[new_line_addr.set][new_line_addr.way];
      ret.cache_addr = new_line_addr;

      entries[new_line_addr.set][new_line_addr.way].valid = true;
      entries[new_line_addr.set][new_line_addr.way].tag = tag;
      entries[new_line_addr.set][new_line_addr.way].proc_id = proc_id;
      entries[new_line_addr.set][new_line_addr.way].base = line_addr;

      repl.insert(new_line_addr, proc_id, is_prefetch);

      data[new_line_addr.set][new_line_addr.way] = new_data;
      return ret;
    }
    
    Cache_access_result<T> invalidate(uns proc_id, Addr addr){
      Cache_address pos = search(proc_id, addr);
      Cache_access_result<T> ret;
      if(pos.valid){
        entries[pos.set][pos.way].tag = 0;  
        entries[pos.set][pos.way].valid= FALSE;  
        entries[pos.set][pos.way].base= FALSE;  
        repl.invalidate(pos);
        ret.hit = true;
        ret.access_addr = addr;
        ret.line_addr = cache_line_addr(addr);
        ret.data = data[pos.set][pos.way];
        ret.cache_addr = pos;
      }
      return ret; 
    }
    
    T get_next_repl_line(uns proc_id, Addr addr){
      Addr tag = cache_tag (addr);
      Addr line_addr = cache_line_addr(addr);
      uns  set = cache_index(addr);

      for(uns ii = 0; ii < assoc; ii++){
        repl_set[ii].valid = true;
        repl_set[ii].set = set;
        repl_set[ii].way = ii;
      }
      
      Cache_address new_line_index = repl.get_next_repl(repl_set);
      return data[new_line_index.set][new_line_index.way];
    }
}; //class

#endif  // __CACHE_CPP_H__