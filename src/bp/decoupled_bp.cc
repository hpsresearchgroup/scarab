#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <fstream>
#include <deque>

extern "C" {
#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"

#include "bp/bp.h"
#include "map.h"
#include "op_pool.h"
#include "op.h"
#include "packet_build.h"
#include "thread.h"
#include "bp/bp.param.h"
#include "cmp_model.h"
#include "core.param.h"
#include "debug/debug.param.h"
#include "frontend/frontend.h"
#include "frontend/pin_trace_fe.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "prefetcher/l2l1pref.h"
#include "prefetcher/stream_pref.h"
#include "statistics.h"
#include "bp/decoupled_bp.h"
}

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_ICACHE_STAGE, ##args)

uns last_packet_fetch_time = 0; /* for computing fetch break latency */

std::vector<std::deque<fetch_queue_entry>> fetch_queue;
Decoupled_BP* dbp = NULL;

extern Flag USE_LATE_BP;

void set_dbp_stage(Decoupled_BP* new_dbp) {
  dbp = new_dbp;
}

void reset_dbp_stage() {
  ASSERT(0, dbp);
  fetch_queue.resize(fetch_queue.size() + 1);
  ASSERT(0, fetch_queue.size() - 1 == dbp->proc_id);
  fetch_queue[dbp->proc_id].clear();
  dbp->next_addr = td->inst_addr;
  op_count[dbp->proc_id] = 0;
}

void init_dbp_stage(uns8 proc_id){
  ASSERT(0, dbp);
  ASSERT(0, DECOUPLED_BP);
  memset(dbp, 0, sizeof(Decoupled_BP));
  dbp->proc_id = proc_id; 
  reset_dbp_stage();
  //dbp->next_addr = td->inst_addr;
  ASSERT_PROC_ID_IN_ADDR(dbp->proc_id, dbp->next_addr);
  dbp->off_path = FALSE;
  dbp->off_path_btb_miss = FALSE;
  dbp->back_on_path = FALSE;
  dbp->next_addr = frontend_next_fetch_addr(dbp->proc_id);
}

Op* read_fetch_queue(uns proc_id)
{
  if(fetch_queue[proc_id].empty()) return NULL;
  ASSERT(proc_id, fetch_queue[proc_id].front().valid);
  return fetch_queue[proc_id].front().op;
}

Flag pop_fetch_queue(uns proc_id)
{
  ASSERT(proc_id, proc_id == dbp->proc_id);
  if(!fetch_queue[proc_id].empty()){
    ASSERT(0, fetch_queue[proc_id].front().valid);
    if(fetch_queue[proc_id].front().op->table_info->cf_type){
      dbp->num_branches_in_fetch_queue--;
      if(USE_LATE_BP ? fetch_queue[proc_id].front().op->oracle_info.late_pred : fetch_queue[proc_id].front().op->oracle_info.pred){
        dbp->num_taken_branches_in_fetch_queue--;
        ASSERT(proc_id, dbp->num_taken_branches_in_fetch_queue >=0);
      }
      ASSERT(proc_id, dbp->num_branches_in_fetch_queue >=0);
    }
    fetch_queue[proc_id].pop_front();
    return TRUE;
  }
  return FALSE;
}

void update_decoupled_bp()
{
  dbp->state = dbp->next_state;
  
  switch(dbp->state) {
    case BP_NORMAL: {

      dbp->off_path &= !dbp->back_on_path;
      dbp->back_on_path = FALSE;

      if(!FETCH_OFF_PATH_OPS && dbp->off_path)
        return;
  
      if(fetch_queue[dbp->proc_id].size() == FETCH_QUEUE_SIZE || dbp->num_taken_branches_in_fetch_queue == FETCH_QUEUE_NUM_TAKEN) {
        dbp->next_state = BP_NORMAL;
        STAT_EVENT(dbp->proc_id, BP_BW_FULL_FQ);
        return;
      }
      
      dbp->next_state = cycle_decoupled_bp(dbp->proc_id);
      DEBUG(dbp->proc_id, "DBP next state: %d \n", dbp->next_state);
      STAT_EVENT(dbp->proc_id, FETCH_ON_PATH + dbp->off_path);

      //STAT_EVENT(dbp->proc_id, FETCH_0_OPS + fetch_packet_op_count);
    } break;

    case BP_WAIT_TIMER: {
      DEBUG(dbp->proc_id, "Decoupled BP waiting on timer:%llu\n", dbp->timer_cycle);
      STAT_EVENT(dbp->proc_id, BP_BW_LATE_BP_REDIRECT);
      if(cycle_count >= dbp->timer_cycle){
        dbp->next_state = BP_NORMAL;
      }
    } break;

    case BP_WAIT_REDIRECT: {
      DEBUG(dbp->proc_id, "decoupled bp waiting for redirect\n");
      STAT_EVENT(dbp->proc_id, BP_BW_REDIRECT);
    } break;

    case BP_WAIT_EMPTY_ROB: {
      STAT_EVENT(dbp->proc_id, BP_BW_SYS_CALL);
      if(td->seq_op_list.count == 0){
        DEBUG(dbp->proc_id, "empty pipeline, decoupled bp back to normal\n");
        dbp->next_state = BP_NORMAL;
      }
    } break;

    default:
      FATAL_ERROR(dbp->proc_id, "Invalid dbp state.\n");
  }
}

Bp_State cycle_decoupled_bp(uns proc_id) {
  uns cf_num    = 0;
  uns taken_count = 0;
  uns ops_count = 0;

  ASSERT(dbp->proc_id, dbp->proc_id == td->proc_id);

  Bp_Break break_predict = BP_NO_BREAK;
  while(break_predict == BP_NO_BREAK) {
    dbp->curr_addr = dbp->next_addr;
    Op*        op   = alloc_op(dbp->proc_id);
    Inst_Info* inst = 0;
    UNUSED(inst);

    if(frontend_can_fetch_op(dbp->proc_id)) {
      frontend_fetch_op(dbp->proc_id, op);
      ASSERTM(dbp->proc_id, dbp->curr_addr == op->inst_info->addr,
              "Fetch address 0x%llx does not match op address 0x%llx\n",
              dbp->curr_addr, op->inst_info->addr);
      op->fetch_addr = dbp->curr_addr;
      ASSERT_PROC_ID_IN_ADDR(dbp->proc_id, op->fetch_addr);
      op->off_path  = dbp->off_path;
      td->inst_addr = op->inst_info->addr;  // FIXME: BUG 54
      ASSERT_PROC_ID_IN_ADDR(dbp->proc_id, td->inst_addr);
      //if(!op->off_path) {
      //  if(op->eom)
      //    issued_real_inst++;
      //  issued_uop++;
      //}
      inst = op->inst_info;
    } else {
      free_op(op);
      return BP_NORMAL;
    }

    if(!op->off_path &&
       (op->table_info->mem_type == MEM_LD ||
        op->table_info->mem_type == MEM_ST) &&
       op->oracle_info.va == 0) {
      // don't care if the va is 0x0 if mem_type is MEM_PF(SW prefetch),
      // MEM_WH(write hint), or MEM_EVICT(cache block eviction hint)
      print_func_op(op);
      FATAL_ERROR(dbp->proc_id, "Access to 0x0\n");
    }

    if(DUMP_TRACE && DEBUG_RANGE_COND(dbp->proc_id))
      print_func_op(op);

    if(DIE_ON_CALLSYS && !op->off_path) {
      ASSERT(dbp->proc_id, op->table_info->cf_type != CF_SYS);
    }

    /* add to sequential op list */
    add_to_seq_op_list(td, op);

    ASSERT(dbp->proc_id, (uns) td->seq_op_list.count <= op_pool_active_ops);

    /* map the op based on true dependencies & set information in
     * op->oracle_info */
    thread_map_op(op);

    STAT_EVENT(op->proc_id, FETCH_ALL_INST);
    STAT_EVENT(op->proc_id, ORACLE_ON_PATH_INST + op->off_path);
    STAT_EVENT(op->proc_id, ORACLE_ON_PATH_INST_MEM +
                              (op->table_info->mem_type == NOT_MEM) +
                              2 * op->off_path);

    thread_map_mem_dep(op);
    op->fetch_cycle = cycle_count;

    
    op_count[dbp->proc_id]++;          /* increment instruction counters */
    unique_count_per_core[dbp->proc_id]++;
    unique_count++;
    
    fetch_queue_entry new_entry;
    new_entry.op = op;
    new_entry.valid = true;
    fetch_queue[op->proc_id].push_back(new_entry);

    /* check trigger */
    if(op->inst_info->trigger_op_fetched_hook)
      model->op_fetched_hook(op);

    /* move on to next instruction in the cache line */
    INC_STAT_EVENT(dbp->proc_id, INST_LOST_FETCH + dbp->off_path, 1);

    DEBUG(dbp->proc_id,
          "Fetching op from Decoupled BP addr: %s off: %d inst_info: %p ii_addr: %s "
          "dis: %s, uid:%llu opnum: (%s:%s)\n",
          hexstr64s(op->inst_info->addr), op->off_path, op->inst_info,
          hexstr64s(op->inst_info->addr), disasm_op(op, TRUE),
          op->inst_uid,
          unsstr64(op->op_num), unsstr64(op->unique_num));

    /* figure out next address after current instruction */
    if(op->table_info->cf_type) {
      // For pipeline gating
      if(op->table_info->cf_type == CF_CBR){
        td->td_info.fetch_br_count++;
      }

      if(IS_CALLSYS(op->table_info) || op->table_info->bar_type & BAR_FETCH) {
        // for fetch barriers (including syscalls), we do not want to do
        // redirect/recovery, BUT we still want to update the branch predictor.
        bp_predict_op(g_bp_data, op, cf_num, dbp->curr_addr);
        op->oracle_info.mispred   = 0;
        op->oracle_info.misfetch  = 0;
        op->oracle_info.btb_miss  = 0;
        op->oracle_info.no_target = 0;
        dbp->next_addr     = ADDR_PLUS_OFFSET(dbp->curr_addr, op->inst_info->trace_info.inst_size);
        ASSERT_PROC_ID_IN_ADDR(dbp->proc_id, dbp->next_addr)
        break_predict = BP_BREAK_ON_BARRIER;
      } else {
        dbp->next_addr = bp_predict_op(g_bp_data, op, cf_num, dbp->curr_addr);
        // initially bp_predict_op can return a garbage, for multi core run,
        // addr must follow cmp addr convention
        dbp->next_addr = convert_to_cmp_addr(dbp->proc_id, dbp->next_addr);
        DEBUG(0, "dbp next addr after bp_predict op is %llx\n", dbp->next_addr);
        ASSERT_PROC_ID_IN_ADDR(dbp->proc_id, dbp->next_addr);
      }

      cf_num++;
      dbp->num_branches_in_fetch_queue++;
      //if(USE_LATE_BP ? op->oracle_info.late_pred : op->oracle_info.pred){
      if(op->oracle_info.pred || (USE_LATE_BP && op->oracle_info.late_pred)){
        taken_count++;
        dbp->num_taken_branches_in_fetch_queue++;
      }

      ASSERT(dbp->proc_id,
             (op->oracle_info.mispred << 2 | op->oracle_info.misfetch << 1 |
              op->oracle_info.btb_miss) <= 0x7);

      //const uns8 mispred       = op->oracle_info.mispred;
      //const uns8 late_mispred  = op->oracle_info.late_mispred;
      //const uns8 misfetch      = op->oracle_info.misfetch;
      //const uns8 late_misfetch = op->oracle_info.late_misfetch;


      if(break_predict == BP_NO_BREAK){
        if(op->oracle_info.btb_miss){
          //if(FETCH_NT_AFTER_BTB_MISS){
          //  if(op->oracle_info.dir && dbp->oldest_btb_miss_op_num != MAX_CTR) {
          //    dbp->oldest_btb_miss_op_num = op->op_num; 
          //  }   
          //  final_prediction = false;
          //  dbp->next_addr = ADDR_PLUS_OFFSET(dbp->curr_addr, op->inst_info->trace_info.inst_size);
          //  DEBUG(0, "dbp next addr after btb miss is %llx\n", dbp->next_addr);
          //}
          //else{
            DEBUG(dbp->proc_id, "Change dbp to wait for redirect\n");
            dbp->oldest_btb_miss_op_num = op->op_num; 
            break_predict = BP_BREAK_ON_BTB_MISS;
          //}
        }
        else if(USE_LATE_BP && !op->oracle_info.btb_miss) {
          if(op->oracle_info.early_late_disagree){
            dbp->timer_cycle = cycle_count + LATE_BP_LATENCY - 1;
            break_predict = BP_BREAK_ON_EARLY_LATE_DISAGREE;
            //ASSERT(dbp->proc_id, dbp->next_addr == op->oracle_info.late_pred_npc);
            //if(!op->off_path){
            //  if(op->oracle_info.pred == false){
            //    STAT_EVENT(dbp->proc_id, TOTAL_EARLY_LATE_DISAGREE);
            //  }
            //  STAT_EVENT(dbp->proc_id, TOTAL_EARLY_LATE_DISAGREE);
            //  STAT_EVENT(dbp->proc_id, TOTAL_EARLY_LATE_DISAGREE);
            //}
          }
        }
      }

      //DEBUG(0, "pred=%d, late_pred=%d, final_pred=%d\n", op->oracle_info.pred, op->oracle_info.late_pred, final_prediction);
      //bool final_mispred = op->oracle_info.pred != op->oracle_info.dir && (dbp->next_addr != op->oracle_info.npc);
      //bool final_misfetch = !final_mispred && (dbp->next_addr != op->oracle_info.npc) && !op->oracle_info.no_target;
      if(IS_CALLSYS(op->table_info) || op->table_info->bar_type & BAR_FETCH) {
        op->oracle_info.mispred = FALSE;
        op->oracle_info.misfetch = FALSE;
      }

      //DEBUG(0, "final_mispred=%d, final_misfetch=%d\n", final_mispred, final_misfetch);

      if(op->oracle_info.misfetch || op->oracle_info.mispred) {
        dbp->off_path = TRUE;

        if(!op->off_path)
          td->td_info.last_bp_miss_op = op;

        if(FETCH_OFF_PATH_OPS) {
          DEBUG(dbp->proc_id, "redirected frontend to 0x%s\n",
                hexstr64s(dbp->next_addr));
          frontend_redirect(td->proc_id, op->inst_uid, dbp->next_addr);
        }
        else{
          if(break_predict == BP_NO_BREAK)
            break_predict = BP_BREAK_ON_MISPRED;
        }
      }
      if(op->oracle_info.pred && !op->off_path){
        STAT_EVENT(dbp->proc_id, TOTAL_ON_PATH_TAKEN);
      }
      //TODO: INCREMENT THE TAKEN COUNT BASED ON THE EARLY BP
      if(TAKEN_PER_CYCLE && taken_count == TAKEN_PER_CYCLE && break_predict == BP_NO_BREAK) { 
        break_predict = BP_BREAK_ON_N_TAKEN;
        DEBUG(dbp->proc_id, "break_predict on taken\n");
        if(!op->off_path){
          STAT_EVENT(dbp->proc_id, TOTAL_ON_PATH_CYCLES);
        }
      }
      if(CFS_PER_CYCLE && cf_num > CFS_PER_CYCLE && break_predict == BP_NO_BREAK){
        break_predict = BP_BREAK_ON_NUM_OP;
        DEBUG(dbp->proc_id, "break_predict on number of cfs\n");
      }
    } else {
      if(op->eom) {
        dbp->next_addr = ADDR_PLUS_OFFSET(
          dbp->curr_addr, op->inst_info->trace_info.inst_size);
      }
      // pass the global branch history to all the instructions
      op->oracle_info.pred_global_hist = g_bp_data->global_hist;
      ASSERT_PROC_ID_IN_ADDR(dbp->proc_id, dbp->next_addr);
    }
    ops_count++;
    if(BP_OP_PER_CYCLE && ops_count >= BP_OP_PER_CYCLE && break_predict == BP_NO_BREAK){ 
      break_predict = BP_BREAK_ON_NUM_OP; 
    }
    if(fetch_queue[dbp->proc_id].size() >= FETCH_QUEUE_SIZE && break_predict == BP_NO_BREAK){
      break_predict = BP_BREAK_ON_FULL_FETCH_QUEUE;
    }
    if(dbp->num_taken_branches_in_fetch_queue >= FETCH_QUEUE_NUM_TAKEN && break_predict == BP_NO_BREAK){
      break_predict = BP_BREAK_ON_FULL_FETCH_QUEUE;
    }
  } // end of while loop
  ASSERT(dbp->proc_id, ops_count <= BP_OP_PER_CYCLE);
  if(ops_count == BP_OP_PER_CYCLE){
    STAT_EVENT(dbp->proc_id, BP_BW_FULL_WIDTH);
  }
  else{
    //this is a hack to get the stats correctly without 50 lines of code
    STAT_EVENT(dbp->proc_id, BP_BW_REDIRECT + ops_count);
  }

  DEBUG(dbp->proc_id, "end of predict packet, %d\n", break_predict);
  switch (break_predict) {
    case BP_NO_BREAK:
      ASSERT(dbp->proc_id, FALSE);
      break;
    case BP_BREAK_ON_EARLY_LATE_DISAGREE:
      return BP_WAIT_TIMER;
      break;
    case BP_BREAK_ON_BARRIER:
      return BP_WAIT_EMPTY_ROB;
      break;
    case BP_BREAK_ON_N_TAKEN:
    case BP_BREAK_ON_NUM_OP:
    case BP_BREAK_ON_FULL_FETCH_QUEUE:
    case BP_BREAK_ON_MISPRED:
      return BP_NORMAL;
      break;
    case BP_BREAK_ON_BTB_MISS:
      return BP_WAIT_REDIRECT;
      break;
  }
  ASSERT(dbp->proc_id, FALSE);
  return BP_NORMAL;
}

void recover_fetch_queue()
{
  //EARLY AND LATE BP DISAGREEMENT ON DONE BY STALLING THE BP FOR N CYCLES
  //ALL THE FLUSHES SHOULD CLEAR THE FETCH QUEUE
  ASSERT(dbp->proc_id, dbp->proc_id == bp_recovery_info->proc_id);
  /* 
  DEBUG(0, "state of the fetch queue\n");
  for(auto thing : fetch_queue[dbp->proc_id]){
    DEBUG(0, "op num %llu\n", thing.op->op_num);
  }
  */
  
  while(!fetch_queue[dbp->proc_id].empty())
  {
    ASSERT(dbp->proc_id, FLUSH_OP(fetch_queue[dbp->proc_id].back().op));
    free_op(fetch_queue[dbp->proc_id].back().op);
    fetch_queue[dbp->proc_id].pop_back();
  }
  ASSERT(dbp->proc_id, fetch_queue[dbp->proc_id].empty());

  dbp->num_branches_in_fetch_queue = 0;
  dbp->num_taken_branches_in_fetch_queue = 0;
  dbp->back_on_path = !bp_recovery_info->recovery_force_offpath;
  dbp->next_addr = bp_recovery_info->recovery_fetch_addr;
  op_count[dbp->proc_id] = bp_recovery_info->recovery_op_num + 1;
  dbp->next_state = BP_NORMAL;
}

void redirect_decoupled_bp(){
  ASSERT(dbp->proc_id, dbp->proc_id == bp_recovery_info->proc_id);
  ASSERT(dbp->proc_id, dbp->state == BP_WAIT_REDIRECT);

  Op*  op                = bp_recovery_info->redirect_op;
  Addr next_fetch_addr   = op->oracle_info.pred_npc;
  op->redirect_scheduled = FALSE;

  DEBUG(dbp->proc_id, "BP stage redirect signaled. next_fetch_addr: 0x%s\n",
        hexstr64s(next_fetch_addr));
  //ASSERT(dbp->proc_id, !FETCH_NT_AFTER_BTB_MISS);

  Flag main_predictor_wrong = op->oracle_info.mispred ||
                              op->oracle_info.misfetch;

  if(USE_LATE_BP) {
    main_predictor_wrong = FALSE;
  }

  Flag late_predictor_wrong = (USE_LATE_BP && (op->oracle_info.late_mispred ||
                                               op->oracle_info.late_misfetch));
  dbp->back_on_path          = !(op->off_path || main_predictor_wrong ||
                       late_predictor_wrong);
  dbp->next_addr       = next_fetch_addr;
  ASSERT_PROC_ID_IN_ADDR(dbp->proc_id, dbp->next_addr);
  dbp->next_state = BP_NORMAL;
}
