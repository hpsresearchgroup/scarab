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
 * File         : libs/repl.h
 * Author       : HPS Research Group
 * Date         : 3/10/2022
 * Description  : this is part of the cache library that deals with 
 *                replacement policy
 ***************************************************************************************/

#include "globals/global_defs.h"
#include "libs/list_lib.h"
#include <vector>

#ifndef __REPL_CPP_H__
#define __REPL_CPP_H__

#define max_rrpv 3

typedef enum Repl_Policy_CPP_enum {
  LRU_REPL,
  RANDOM_REPL,
  MRU_REPL,
  SRRIP_REPL,
  REPL_NUM
} Repl_Policy_CPP;

class Cache_address {
  public:
  Flag valid;
  uns set;
  uns way; 

  Cache_address() : 
    valid(false){
  }
};

class per_line_data {
    public:
    Flag valid;
    Flag prefetch;
    uns proc_id;
    Counter insert_cycle;
    Counter access_cycle;
    uns8 rrpv;

    per_line_data() {
        valid = false;
        access_cycle = MAX_CTR;
        insert_cycle = MAX_CTR;
        rrpv = max_rrpv; 
    }
};

class repl_class {   
    public:

    Repl_Policy_CPP repl_policy;

    std::vector<std::vector<per_line_data>> repl_data;

    repl_class(Repl_Policy_CPP policy, uns num_sets, uns assoc);

    Cache_address get_next_repl(std::vector<Cache_address> list);

    void insert(Cache_address pos, uns proc_id, Flag is_prefetch);

    void access(Cache_address cache_addr);

    void invalidate(Cache_address pos);

};

#endif  // __REPL_CPP_H__