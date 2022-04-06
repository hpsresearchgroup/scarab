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
#include "globals/utils.h"
#include "globals/assert.h"
#include <vector>
#include <string>

typedef enum Repl_Policy_enum {
  REPL_TRUE_LRU,    /* actual least-recently-used replacement */
  REPL_RANDOM,      /* random replacement */
  REPL_NOT_MRU,     /* not-most-recently-used replacement */
  REPL_MRU,
  NUM_REPL
} Repl_Policy;

class per_line_data {
    Flag valid;
    Flag prefetch;
    uns proc_id;
    Counter insert_cycle;
    Counter access_cycle;

    per_line_data() {
        valid = false;
        repl_data[pos].access_cycle = MAX_CTR;
        repl_data[pos].insert_cycle = MAX_CTR;
    }
};

class repl {   
    public:
        Repl_Policy repl_policy;
        std::vector<per_line_data> repl_data;

    repl(Repl_Policy policy, uns num_lines) :
        repl_policy(policy), repl_data(num_lines) {
    }

    uns get_next_repl(std::vector<uns> list){
        switch(repl_policy){
            case REPL_TRUE_LRU:
                for(uns item:list){
                    
                }
                break;
            case REPL_RANDOM:
                break;
            case REPL_NOT_MRU:
                break;
            case REPL_MRU:
                break;
            default:
                ASSERT(0, false);
        }
    }

    void insert(uns pos, uns proc_id, Flog is_prefetch){
        repl_data[pos].valid = True;
        repl_data[pos].prefetch = is_prefetch;
        repl_data[pos].proc_id = proc_id;
        repl_data[pos].insert_cycle = cycle_count;
        repl_data[pos].access_cycle = cycle_count;
    }

    void access(uns pos){
        ASSERT(0, repl_data[pos].valid);
        repl_data[pos].access_cycle = cycle_count;
        repl_data[pos].prefetch = false;
    }

    void invalid(uns pos){
        repl_data[pos].valid = false;
        repl_data[pos].access_cycle = MAX_CTR;
        repl_data[pos].insert_cycle = MAX_CTR;
    }

};