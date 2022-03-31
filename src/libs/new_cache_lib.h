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
#include <vector>
#include <string>

class Cache_entry {
  public:
    uns8 proc_id;
    Flag valid;
    Addr tag;
    Addr base;
    Flag dirty;

    //how should data be represented?
    
    //replacement info should include last_access_time, insertion_time, pf
};

typedef enum Repl_Policy_enum {
  REPL_TRUE_LRU,    /* actual least-recently-used replacement */
  REPL_RANDOM,      /* random replacement */
  REPL_NOT_MRU,     /* not-most-recently-used replacement */
  REPL_MRU,
  NUM_REPL
} Repl_Policy;

template <typename T> 
class Cache {
  public:
    String name;

    uns8 data_size; //size used for malloc
    uns8 assoc;
    uns num_lines;
    uns num_sets;
    uns8 shift_amount; 

    uns8 set_bits;
    Repl_Policy_enum repl

    std::vector<Cache_entry> entries;   
    std::vector<T> data;

    Cache(string name, uns cache_size, uns assoc, uns line_size, Repl_Policy_enum repl){
      this->name = name;
      this->assoc = assoc;
      this->line_size = line_size;
      
      this->num_lines = cache_size / line_size;
      this->num_sets  = cache_size / line_size / assoc;
    }
    
    ~Cache();
    
    T insert(uns proc_id, Addr addr);
    
    T invalidate(uns proc_id, Addr addr);
    
    T get_next_repl_line();
};