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
 * File         : libs/repl.cc
 * Author       : HPS Research Group
 * Date         : 3/10/2022
 * Description  : this is part of the cache library that deals with 
 *                replacement policy
 ***************************************************************************************/

#include "globals/global_defs.h"
#include "libs/list_lib.h"
#include "libs/cache_lib/repl.h"
#include "globals/utils.h"
#include <vector>
#include <string>

repl_class::repl_class(Repl_Policy policy, uns num_sets, uns assoc) :
    repl_policy(policy), repl_data(num_sets) {
    for(int ii = 0; ii < num_sets; ii++){
        repl_data[ii].resize(assoc);
    }
}

Cache_address repl_class::get_next_repl(std::vector<Cache_address> list){
    Cache_address ret_address;
    switch(repl_policy){
        case REPL_TRUE_LRU:
            Cache_address res = -1;
            Cache_prefetch_res = -1;
            Counter min_access_cycle = MAX_INT; 
            Counter min_prefetch_cycle = MAX_INT; 
            Counter current_min_cycle;
            for(auto index:list){
                if(!index.valid){
                    continue;
                }
                if(repl_data[index.set][index.way].valid == false)
                    return index;
                if(repl_data[index.set][index.way].prefetch && repl_data[index.set][index.way].insert_cycle < min_prefetch_cycle){
                    prefetch_res = index;
                    min_prefetch_cycle = repl_data.insert_cycle;
                }
                if(repl_data[index.set][index.way].access_cycle < min_access_cycle){
                    res = index;
                    min_access_cycle = repl_data.access_cycle;
                }
            }
            if(prefetch_res != -1){
                return (uns)prefetch_res;
            }
            ASSERT(0, res != -1);
            return (uns)res;
            break;
        case REPL_RANDOM:
            return list[rand()%list.size()];
            break;
        case REPL_MRU:
            int index;
            int res = -1;
            int prefetch_res = -1;
            Counter max_access_cycle = 0; 
            Counter max_prefetch_cycle = 0; 
            Counter current_min_cycle;
            for(index:list){
                if(!index.valid){
                    continue;
                }
                if(repl_data.at(index).valid == false)
                    return index;
                if(repl_data.at(index).prefetch && repl_data.at(index).insert_cycle > min_prefetch_cycle){
                    prefetch_res = index;
                    min_prefetch_cycle = repl_data.at(index).insert_cycle;
                }
                if(repl_data.at(index).access_cycle > min_access_cycle){
                    res = index;
                    min_access_cycle = repl_data.at(index).access_cycle;
                }
            }
            if(prefetch_res != -1){
                return (uns)prefetch_res;
            }
            ASSERT(res != -1)
            return res;
            break;
        default:
            ASSERT(0, false);
    }
    //should never reach here
    return 0;
}

void repl_class::insert(Cache_address pos, uns proc_id, Flag is_prefetch){
    repl_data[pos.set][pos.way].valid = true;
    repl_data[pos.set][pos.way].prefetch = is_prefetch;
    repl_data[pos.set][pos.way].proc_id = proc_id;
    repl_data[pos.set][pos.way].insert_cycle = cycle_count;
    repl_data[pos.set][pos.way].access_cycle = cycle_count;
}

void repl_class::access(Cache_address pos){
    ASSERT(0, repl_data[pos.set][pos.way].valid);
    repl_data[pos.set][pos.way].access_cycle = cycle_count;
    repl_data[pos.set][pos.way].prefetch = false;
}

void repl_class::invalidate(uns pos){
    ASSERT(0, repl_data[pos.set][pos.way].valid);
    repl_data[pos.way][pos.set].valid = false;
    repl_data[pos.way][pos.set].access_cycle = MAX_CTR;
    repl_data[pos.way][pos.set].insert_cycle = MAX_CTR;
}