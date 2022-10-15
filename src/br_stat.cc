#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"
#include "op_pool.h"

#include "bp/bp.h"
#include "exec_ports.h"
#include "frontend/frontend.h"
#include "memory/memory.h"
#include "node_stage.h"
#include "thread.h"

#include "bp/bp.param.h"
#include "core.param.h"
#include "debug/debug.param.h"
#include "map.h"
#include "memory/memory.param.h"
#include "sim.h"
#include "statistics.h"

#include "bp/tagescl.h"
#include "br_stat.h"
#include <unordered_map>


class per_branch_stat{
  public: 
  uns64 times_taken;
  uns64 times_not_taken;
  
  per_branch_stat(){
    times_taken = 0;
    times_not_taken = 0;
  }
};

std::unordered_map<Addr, per_branch_stat> br_stats;

void collect_br_stats(Op* op){
  Addr pc = op->inst_info->addr;
  auto curr_br = br_stats.find(pc);
  if(curr_br == br_stats.end()){
    br_stats[pc] = per_branch_stat(); 
  }
  if(op->oracle_info.dir){
    br_stats[pc].times_taken++;
  }
  else{
    br_stats[pc].times_not_taken++;
  }
}

void final_br_stat_print(){
  int t_ratios[12];
  int num_unique_brs = 0;
  for(int i = 0; i < 12; i++){
    t_ratios[i] = 0;
  }
  for(auto curr = br_stats.begin(); curr != br_stats.end(); curr++){
    uns64 total = curr->second.times_taken + curr->second.times_not_taken;
    uns idx = curr->second.times_taken * 10 / total + 1;
    if (curr->second.times_not_taken == total){
      idx = 0;  
    }
    t_ratios[idx]++;
    num_unique_brs++;
  }

  printf("br taken ratios 0: %d\n", t_ratios[0]);
  for(int i = 0; i < 10; i++){
    printf("br taken ratios %d to %d: %d\n", i*10, i*10+10, t_ratios[i+1]);
  }
  printf("br taken ratios 100: %d\n", t_ratios[11]);
  printf("total branches %d\n", num_unique_brs);
}

void print_top_N_branches(int N){
  
}

