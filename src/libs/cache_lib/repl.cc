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

#include "libs/cache_lib/repl.h"
#include <vector>
#include <string>
#include <cstdlib>
extern "C" {
#include "libs/list_lib.h"
#include "globals/utils.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
}

repl_class::repl_class(Repl_Policy_CPP policy, uns num_sets, uns assoc) :
    repl_policy(policy), repl_data(num_sets) {
    for(uns ii = 0; ii < num_sets; ii++){
        repl_data[ii].resize(assoc);
    }
}

Cache_address repl_class::get_next_repl(std::vector<Cache_address> list){
    int res;
    int prefetch_res;
    int current;
    Counter min_access_cycle; 
    Counter min_prefetch_cycle; 
    Counter max_access_cycle; 
    Counter max_prefetch_cycle; 
    switch(repl_policy){
        case LRU_REPL:
            res = -1;
            prefetch_res = -1;
            current = 0;
            min_access_cycle = MAX_INT; 
            min_prefetch_cycle = MAX_INT; 
            for(auto index:list){
                if(!index.valid){
                    //the list is hard-coded to be the associcatity of the cache
                    //this allows the cache to send less than that to the repl class
                    continue;
                }
                if(repl_data[index.set][index.way].valid == false)
                    return list.at(current);
                if(repl_data[index.set][index.way].prefetch && repl_data[index.set][index.way].insert_cycle < min_prefetch_cycle){
                    prefetch_res = current;
                    min_prefetch_cycle = repl_data[index.set][index.way].insert_cycle;
                }
                if(repl_data[index.set][index.way].access_cycle < min_access_cycle){
                    res = current;
                    min_access_cycle = repl_data[index.set][index.way].access_cycle;
                }
                current ++; 
            }
            if(prefetch_res != -1){
                return list.at(prefetch_res);
            }
            ASSERT(0, res != -1);
            return list.at(res);
            break;
        case RANDOM_REPL:
            return list[rand()%list.size()];
            break;
        case MRU_REPL:
            current = 0;
            res = -1;
            prefetch_res = -1;
            max_access_cycle = 0; 
            max_prefetch_cycle = 0; 
            for(auto index:list){
                if(!index.valid){
                    continue;
                }
                if(repl_data[index.set][index.way].valid == false)
                    return list.at(current);
                if(repl_data[index.set][index.way].prefetch && repl_data[index.set][index.way].insert_cycle > max_prefetch_cycle){
                    prefetch_res = current;
                    max_prefetch_cycle = repl_data[index.set][index.way].insert_cycle;
                }
                if(repl_data[index.set][index.way].access_cycle > max_access_cycle){
                    res = current;
                    max_access_cycle = repl_data[index.set][index.way].access_cycle;
                }
                current ++; 
            }
            if(prefetch_res != -1){
                return list.at(prefetch_res);
            }
            ASSERT(0, res != -1);
            return list.at(res);
            break;
        case SRRIP_REPL:
            while(1) {
                for(auto index:list){
                    if(!index.valid){
                        continue;
                    }
                    if(repl_data[index.set][index.way].valid == false){
                        return index;
                    }
                }
                for(auto index:list){
                    if(!index.valid){
                        continue;
                    }
                    if(repl_data[index.set][index.way].rrpv == max_rrpv){
                        return index;
                    }
                }
                for(auto index:list){
                    if(!index.valid){
                        continue;
                    }
                    ASSERT(0, repl_data[index.set][index.way].rrpv != max_rrpv);
                    repl_data[index.set][index.way].rrpv += 1;
                }
            }
            break;
        default:
            ASSERT(0, false);

    }
    //should never reach here
    return list[0];
}

void repl_class::insert(Cache_address pos, uns proc_id, Flag is_prefetch){
    repl_data[pos.set][pos.way].valid = true;
    repl_data[pos.set][pos.way].prefetch = is_prefetch;
    repl_data[pos.set][pos.way].proc_id = proc_id;
    repl_data[pos.set][pos.way].insert_cycle = cycle_count;
    repl_data[pos.set][pos.way].access_cycle = cycle_count;
    if(repl_policy == SRRIP_REPL){
        repl_data[pos.set][pos.way].rrpv = max_rrpv - 1;
    }
}

void repl_class::access(Cache_address pos){
    ASSERT(0, repl_data[pos.set][pos.way].valid);
    repl_data[pos.set][pos.way].access_cycle = cycle_count;
    repl_data[pos.set][pos.way].prefetch = false;
    if(repl_policy == SRRIP_REPL){
        repl_data[pos.set][pos.way].rrpv = 0;
    }
}

void repl_class::invalidate(Cache_address pos){
    ASSERT(0, repl_data[pos.set][pos.way].valid);
    repl_data[pos.way][pos.set].valid = false;
    repl_data[pos.way][pos.set].access_cycle = MAX_CTR;
    repl_data[pos.way][pos.set].insert_cycle = MAX_CTR;
    if(repl_policy == SRRIP_REPL){
        repl_data[pos.set][pos.way].rrpv = max_rrpv;
    }
}
