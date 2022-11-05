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

#include "pc_table.h"


#include <vector>
#include "libs/cache_lib/cache.h"
extern "C" {
#include "bp/bp.param.h"
#include "core.param.h"
#include "globals/assert.h"
#include "statistics.h"
}

//#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_BP_DIR, ##args)
#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_PC_TABLE, ##args)

namespace {

struct l0_btb_entry {
    Addr pc;
    Addr target;
};

std::vector<Cache_cpp<l0_btb_entry>> l0_btbs_across_all_cores;

//uns32 get_pht_index(const Addr addr, const uns32 hist) {
//  const uns32 cooked_hist = hist >> (32 - HIST_LENGTH);
//  const uns32 cooked_addr = (addr >> 2) & N_BIT_MASK(HIST_LENGTH);
//  return cooked_hist ^ cooked_addr;
//}
}  // namespace

// The only speculative state of gshare is the global history which is managed
// by bp.c. Thus, no internal timestamping and recovery mechanism is needed.
void bp_pc_table_timestamp(Op* op) {}
void bp_pc_table_recover(Recovery_Info* info) {}
void bp_pc_table_spec_update(Op* op) {}
void bp_pc_table_retire(Op* op) {}


void bp_pc_table_init() {
  for(uns i = 0; i < NUM_CORES; i++) {
    //Cache(std::string name, uns cache_size, uns assoc, uns line_size, Repl_Policy_enum repl_policy) :
    l0_btbs_across_all_cores.push_back(Cache_cpp<l0_btb_entry>("l0_btb", L0_BTB_SIZE, L0_BTB_ASSOC, 1, SRRIP_REPL));
  }
}


uns8 bp_pc_table_pred(Op* op) {

  Addr        pc          = op->inst_info->addr;
  auto&       l0_btb      = l0_btbs_across_all_cores.at(op->proc_id);
  Cache_access_result<l0_btb_entry> res = l0_btb.probe(op->proc_id, pc);
  uns8 pred = res.hit;
  DEBUG(op->proc_id, "Predicting for op_num:%s addr:%s, p_dir:%d, t_dir:%d\n",
        unsstr64(op->op_num), hexstr64s(pc), pred, op->oracle_info.dir);
  return pred; 
}

void bp_pc_table_update(Op* op) {
  auto cf_type = op->table_info->cf_type;
  if(cf_type != CF_CBR && cf_type != CF_BR && cf_type != CF_CALL) {
    //only deal with direct branches, maybe can remove call later as well
    return;
  }

  const uns   proc_id      = op->proc_id;
  auto&       l0_btb       = l0_btbs_across_all_cores.at(proc_id);
  const Addr  pc           = op->inst_info->addr;


  if(op->oracle_info.dir) {
    auto access_res = l0_btb.access(proc_id, pc);
    if(!access_res.hit){
        l0_btb_entry new_entry = {pc, op->oracle_info.target};
        auto insert_res = l0_btb.insert(proc_id, pc, /*is_prefetch =*/ FALSE, new_entry);
        if(!insert_res.hit){
          printf("nothing replaced\n");
        }
        DEBUG(proc_id, "write l0btb op %llu, pc=x%llx, repl: %d, replpc = %llx\n", op->op_num, pc, insert_res.hit, insert_res.line_addr);
    }
    DEBUG(proc_id, "Drop l0btb for l0btb hit op %llu\n", op->op_num);
  } 
  else{
    //DEBUG(proc_id, "Drop l0btb for NT op %llu\n", op->op_num);
  }
}
