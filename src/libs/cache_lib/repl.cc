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

repl_class::repl_class(Repl_Policy policy, uns num_lines) :
    repl_policy(policy), repl_data(num_lines) {
}

uns repl_class::get_next_repl(std::vector<uns> list){
    switch(repl_policy){
        case REPL_TRUE_LRU:
            int res = -1;
            int prefetch_res = -1;
            Counter min_access_cycle = MAX_INT; 
            Counter min_prefetch_cycle = MAX_INT; 
            Counter current_min_cycle;
            for(auto index:list){
                if(repl_data.at(index).valid == false)
                    return index;
                if(repl_data.at(index).prefetch && repl_data.at(index).insert_cycle < min_prefetch_cycle){
                    prefetch_res = index;
                    min_prefetch_cycle = repl_data.at(index).insert_cycle;
                }
                if(repl_data.at(index).access_cycle < min_access_cycle){
                    res = index;
                    min_access_cycle = repl_data.at(index).access_cycle;
                }
            }
            if(prefetch_res != -1){
                return (uns)prefetch_res;
            }
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
            return (uns)res;
            break;
        default:
            ASSERT(0, false);
    }
    //should never reach here
    return 0;
}

void repl_class::insert(uns pos, uns proc_id, Flag is_prefetch){
    repl_data[pos].valid = true;
    repl_data[pos].prefetch = is_prefetch;
    repl_data[pos].proc_id = proc_id;
    repl_data[pos].insert_cycle = cycle_count;
    repl_data[pos].access_cycle = cycle_count;
}

void repl_class::access(uns pos){
    ASSERT(0, repl_data[pos].valid);
    repl_data[pos].access_cycle = cycle_count;
    repl_data[pos].prefetch = false;
}

void repl_class::invalidate(uns pos){
    repl_data[pos].valid = false;
    repl_data[pos].access_cycle = MAX_CTR;
    repl_data[pos].insert_cycle = MAX_CTR;
}