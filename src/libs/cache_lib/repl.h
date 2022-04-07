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
#include "globals/utils.h"
#include "globals/assert.h"
#include <vector>
#include <string>
#include <cstdlib>

typedef enum Repl_Policy_enum {
  REPL_TRUE_LRU,    /* actual least-recently-used replacement */
  REPL_RANDOM,      /* random replacement */
  REPL_MRU,
  NUM_REPL
} Repl_Policy;

class per_line_data {
    public:
    Flag valid;
    Flag prefetch;
    uns proc_id;
    Counter insert_cycle;
    Counter access_cycle;

    per_line_data() {
        valid = false;
        access_cycle = MAX_CTR;
        insert_cycle = MAX_CTR;
    }
};

class repl_class {   
    public:

    Repl_Policy repl_policy;
    std::vector<per_line_data> repl_data;

    repl_class(Repl_Policy policy, uns num_lines);

    uns get_next_repl(std::vector<uns> list);

    void insert(uns pos, uns proc_id, Flag is_prefetch);

    void access(uns pos);

    void invalid(uns pos);

};