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

Class Cache_entry {
  public:
    uns8 proc_id;
    Flag valid;
    Addr tag;
    Addr base;
    Flag dirty;

    //how should data be represented?
    
    //replacement info should include last_access_time, insertion_time, pf
}

template <typename T> 
Class Cache {
  public:
    String name;

    uns8 data_size; //size used for malloc
    uns8 assoc;
    uns8 num_lines;
    uns8 num_sets;
    uns8 num_lines;
    uns8 shift_amount; 

    uns8 set_bits;

    std::vector<Cache_entry> entries;   
    std::vector<T> data;
}

template <typename T> 
Cache::Cache(String name, uns cache_size, uns assoc, uns line_size, );

template <typename T> 
Cache::~Cache(String name, uns cache_size, uns assoc, uns line_size, uns data_size, );

template <typename T> 
T Cache::insert(uns proc_id, Addr addr);

template <typename T> 
T Cache::invalidate(uns proc_id, Addr addr);

template <typename T> 
T Cache::get_next_repl_line();