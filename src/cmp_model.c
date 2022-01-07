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
 * File         : cmp_model.c
 * Author       : HPS Research Group
 * Date         : 11/27/2006
 * Description  : CMP with runahead
 ***************************************************************************************/

/**************************************************************************************/
/* Global variables */
#include "cmp_model.h"
#include "bp/bp.param.h"
#include "core.param.h"
#include "debug/debug.param.h"
#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "dvfs/dvfs.h"
#include "dvfs/dvfs.param.h"
#include "dvfs/perf_pred.h"
#include "general.param.h"
#include "globals/assert.h"
#include "memory/cache_part.h"
#include "memory/memory.param.h"
#include "op_pool.h"
#include "prefetcher/pref.param.h"
#include "prefetcher/pref_common.h"
#include "sim.h"
#include "statistics.h"

#include "freq.h"

Flag perf_pred_started = FALSE;

/**************************************************************************************/
/* Static prototypes */

static void cmp_recover(void);
static void cmp_redirect(void);
static void cmp_measure_chip_util(void);
static void cmp_istreams(void);
static void cmp_cores(void);
static void warmup_uncore(uns proc_id, Addr addr, Flag write);

/**************************************************************************************/
/* cmp_init */

void cmp_init(uns mode) {
  if(mode == SIMULATION_MODE) {
    // set repl to LRU for warming up, waiting for partition trigger to switch
    // it back  to REPL_PARTITION
    if(L1_PART_ON && L1_PART_WARMUP) {
      ASSERT(
        0, cmp_model.memory.uncores[0].l1->cache.repl_policy == REPL_PARTITION);
      cmp_model.memory.uncores[0].l1->cache.repl_policy = REPL_TRUE_LRU;
    }
    return;
  }

  /**
   * The real initialization is done in warmup (guaranteed to happence once
   * before switch into simulation mode)
   *
   */
  ASSERT(0, mode == WARMUP_MODE);

  uns8 proc_id;

  freq_init();
  cmp_init_cmp_model();

  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    /* initialize the stages */
    cmp_set_all_stages(proc_id);

    cmp_init_thread_data(proc_id);

    init_icache_stage(proc_id, "ICACHE");
    init_icache_trace();

    init_decode_stage(proc_id, "DECODE");

    init_map_stage(proc_id, "MAP");

    init_node_stage(proc_id, "NODE");

    init_exec_stage(proc_id, "EXEC");

    init_exec_ports(proc_id, "EXEC_PORTS");

    init_dcache_stage(proc_id, "DCACHE");

    /* initialize the common data structures */
    init_bp_recovery_info(proc_id, &cmp_model.bp_recovery_info[proc_id]);
    init_bp_data(proc_id, &cmp_model.bp_data[proc_id]);
  }

  cmp_model.window_size = NODE_TABLE_SIZE;

  set_memory(&cmp_model.memory);

  // init_memory will call init_uncores, which setup the partition stuffs
  init_memory();

  if(DVFS_ON)
    dvfs_init();

  cache_part_init();

  ASSERTM(0, !USE_LATE_BP || LATE_BP_LATENCY < (DECODE_CYCLES + MAP_CYCLES),
          "Late branch prediction latency should be less than the total "
          "latency of the frontend stages of the pipeline (decode + map)");
}


/**************************************************************************************/
/* cmp_reset: */

void cmp_reset() {
  uns8 proc_id;

  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    cmp_set_all_stages(proc_id);
    reset_icache_stage();
    reset_decode_stage();
    reset_map_stage();
    reset_node_stage();
    reset_exec_stage();
    reset_dcache_stage();
  }
  reset_memory();
}

/**************************************************************************************/
/* cmp_cycle: */

void cmp_cycle() {
  cmp_istreams();

  /* Frequency domain checking is inside this function, since it
     handles both shared cache and memory */
  update_memory();

  cmp_cores();

  if(DVFS_ON)
    dvfs_cycle();
  cache_part_update();
}

void cmp_istreams(void) {
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    if(DUMB_CORE_ON && DUMB_CORE == proc_id)
      continue;

    if(freq_is_ready(FREQ_DOMAIN_CORES[proc_id])) {
      cycle_count = freq_cycle_count(FREQ_DOMAIN_CORES[proc_id]);

      set_bp_recovery_info(&cmp_model.bp_recovery_info[proc_id]);

      if(cycle_count >= bp_recovery_info->recovery_cycle) {
        set_bp_data(&cmp_model.bp_data[proc_id]);
        cmp_set_all_stages(proc_id);
        cmp_recover();
      }
      if(cycle_count >= bp_recovery_info->redirect_cycle) {
        set_icache_stage(&cmp_model.icache_stage[proc_id]);
        ASSERT(proc_id, proc_id == bp_recovery_info->redirect_op->proc_id);
        ASSERT_PROC_ID_IN_ADDR(
          proc_id, bp_recovery_info->redirect_op->oracle_info.pred_npc);
        cmp_redirect();
      }
    }
  }
}

void cmp_cores(void) {
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    if(DUMB_CORE_ON && DUMB_CORE == proc_id)
      continue;

    if(freq_is_ready(FREQ_DOMAIN_CORES[proc_id])) {
      cycle_count = freq_cycle_count(FREQ_DOMAIN_CORES[proc_id]);

      set_bp_data(&cmp_model.bp_data[proc_id]);
      set_bp_recovery_info(&cmp_model.bp_recovery_info[proc_id]);
      cmp_set_all_stages(proc_id);

      update_dcache_stage(&exec->sd);
      update_exec_stage(&node->sd);
      update_node_stage(map->last_sd);
      update_map_stage(dec->last_sd);
      update_decode_stage(&ic->sd);
      update_icache_stage();

      node_sched_ops();

      cmp_measure_chip_util();
    }
  }
}

/**************************************************************************************/
/* cmp_debug: */

void cmp_debug() {
  uns8 proc_id;

  for(proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    cycle_count = freq_cycle_count(FREQ_DOMAIN_CORES[proc_id]);

    // cmp FIXME print out per core information
    FPRINT_LINE(proc_id, GLOBAL_DEBUG_STREAM);
    cmp_set_all_stages(proc_id);

    debug_icache_stage();
    debug_decode_stage();
    debug_map_stage();
    debug_node_stage();
    debug_exec_stage();
    debug_dcache_stage();

    FPRINT_LINE(proc_id, GLOBAL_DEBUG_STREAM);
  }

  debug_memory();
}

/**************************************************************************************/
/* cmp_done: */

void cmp_done() {
  if(PREF_FRAMEWORK_ON)
    pref_done();
  if(DVFS_ON)
    dvfs_done();

  finalize_memory();
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    cmp_set_all_stages(proc_id);
  }
  // if(L2L1PREF_ON) l2l1_done();    // FIXME prefetchers What should I do for
  // this
}

/**************************************************************************************/
/* cmp_done: */

void cmp_per_core_done(uns8 proc_id) {
  stats_per_core_collect(proc_id);
  if(PREF_FRAMEWORK_ON)
    pref_per_core_done(proc_id);
}
/**************************************************************************************/
/* cmp_wake: */

void cmp_wake(Op* src_op, Op* dep_op, uns8 rdy_bit) {
  /* Make the op independent, if it is dependent on a BOGUS op */
  // cmp: since this function use node, we need to set node properly
  set_node_stage(&cmp_model.node_stage[src_op->proc_id]);

  ASSERTM(src_op->proc_id, src_op->proc_id == dep_op->proc_id,
          "src id: %i, dep id: %i\n", src_op->proc_id, dep_op->proc_id);
  ASSERTM(dep_op->proc_id, dep_op->proc_id == node->proc_id,
          "dep id: %i, node id: %i\n", dep_op->proc_id, node->proc_id);

  /* Only wake up ops that are in RS */
  if(dep_op->state != OS_IN_RS) {
    /* However, update the rdy_cycle now so that the dependence is
       maintained when the op enters RS. */
    dep_op->rdy_cycle = MAX2(dep_op->rdy_cycle, src_op->wake_cycle);
    return;
  }

  simple_wake(src_op, dep_op, rdy_bit);

  if(dep_op->srcs_not_rdy_vector == 0x0 && cycle_count >= dep_op->issue_cycle &&
     !dep_op->in_rdy_list) {
    _DEBUG(dep_op->proc_id, DEBUG_NODE_STAGE,
           "Adding to ready list  op_num:%s\n", unsstr64(dep_op->op_num));
    dep_op->next_rdy    = node->rdy_head;
    node->rdy_head      = dep_op;
    dep_op->in_rdy_list = TRUE;
  }
}

/**************************************************************************************/
/* cmp_recover: */

void cmp_recover() {
  _DEBUG(bp_recovery_info->proc_id, DEBUG_BP, "Recovery caused by op_num:%s\n",
         unsstr64(bp_recovery_info->recovery_op_num));
  ASSERT(bp_recovery_info->proc_id,
         bp_recovery_info->recovery_cycle != MAX_CTR);
  ASSERT(bp_recovery_info->proc_id,
         bp_recovery_info->proc_id == g_bp_data->proc_id);
  ASSERT(bp_recovery_info->proc_id,
         bp_recovery_info->proc_id == map_data->proc_id);
  bp_recovery_info->recovery_cycle = MAX_CTR;
  bp_recovery_info->redirect_cycle = MAX_CTR;

  bp_recover_op(g_bp_data, bp_recovery_info->recovery_cf_type,
                &bp_recovery_info->recovery_info);

  if(USE_LATE_BP && bp_recovery_info->late_bp_recovery) {
    Op* op                   = bp_recovery_info->recovery_op;
    op->oracle_info.pred     = op->oracle_info.late_pred;
    op->oracle_info.pred_npc = op->oracle_info.late_pred_npc;
    ASSERT_PROC_ID_IN_ADDR(op->proc_id, op->oracle_info.pred_npc);
    op->oracle_info.mispred  = op->oracle_info.late_mispred;
    op->oracle_info.misfetch = op->oracle_info.late_misfetch;

    // Reset to FALSE to allow for another potential recovery after the branch
    // is resolved when executed.
    op->oracle_info.recovery_sch = FALSE;
  }

  recover_thread(td, bp_recovery_info->recovery_fetch_addr,
                 bp_recovery_info->recovery_op_num,
                 bp_recovery_info->recovery_inst_uid,
                 bp_recovery_info->late_bp_recovery_wrong);

  recover_icache_stage();
  recover_decode_stage();
  recover_map_stage();
  recover_node_stage();
  recover_exec_stage();
  recover_dcache_stage();
  recover_memory();
}

/**************************************************************************************/
/* cmp_redirect: */

void cmp_redirect() {
  _DEBUG(bp_recovery_info->proc_id, DEBUG_BP, "Redirect caused by op_num:%s\n",
         unsstr64(bp_recovery_info->redirect_op_num));
  ASSERT(bp_recovery_info->proc_id,
         bp_recovery_info->redirect_cycle != MAX_CTR);
  bp_recovery_info->redirect_cycle                             = MAX_CTR;
  bp_recovery_info->redirect_op->oracle_info.btb_miss_resolved = TRUE;
  ASSERT_PROC_ID_IN_ADDR(bp_recovery_info->proc_id,
                         bp_recovery_info->redirect_op->oracle_info.pred_npc);
  redirect_icache_stage();
}

/**************************************************************************************/
// cmp_retire_hook:  Called right before the op retires

void cmp_retire_hook(Op* op) {
  free_op(op);
}

void warmup_uncore(uns proc_id, Addr addr, Flag write) {
  Addr dummy_line_addr;
  ASSERTM(0, !MLC_PRESENT, "Warmup for MLC not implemented\n");

  Cache*   l1_cache = &(cmp_model.memory.uncores[proc_id].l1->cache);
  L1_Data* l1_data  = cache_access(l1_cache, addr, &dummy_line_addr, TRUE);
  if(l1_data) {  // hit
    if(write)
      l1_data->dirty = TRUE;
  } else {  // miss
    Addr repl_line_addr;
    Flag repl_line_valid;
    get_next_repl_line(l1_cache, proc_id, addr, &repl_line_addr,
                       &repl_line_valid);
    STAT_EVENT(proc_id, NORESET_L1_FILL);
    STAT_EVENT(proc_id, NORESET_L1_FILL_NONPREF);
    if(repl_line_valid) {
      uns repl_proc_id = get_proc_id_from_cmp_addr(repl_line_addr);
      STAT_EVENT(repl_proc_id, NORESET_L1_EVICT);
      STAT_EVENT(repl_proc_id, NORESET_L1_EVICT_NONPREF);
    }
    l1_data = (L1_Data*)cache_insert(l1_cache, proc_id, addr, &dummy_line_addr,
                                     &repl_line_addr);
    l1_data->proc_id = proc_id;
    l1_data->dirty   = write;
  }
  if(L1_PART_SHADOW_WARMUP)
    cache_part_l1_warmup(proc_id, addr);
}

/**************************************************************************************/
/* Warm up select microarchitectural structures: BP, icache, dcache,
   and L1. No wrong path warmup.
*/

void cmp_warmup(Op* op) {
  uns  proc_id = op->proc_id;
  Addr ia      = op->inst_info->addr;
  Addr va      = op->oracle_info.va;
  Addr dummy_line_addr;

  // Warmup caches for instructions
  Icache_Stage* ic = &(cmp_model.icache_stage[proc_id]);
  // keep next_fetch_addr current to avoid confusing simulation mode
  if(op->eom) {
    ic->next_fetch_addr = op->oracle_info.npc;
    ASSERT_PROC_ID_IN_ADDR(ic->proc_id, ic->next_fetch_addr)
  }
  Cache*      icache  = &(ic->icache);
  Inst_Info** ic_data = (Inst_Info**)cache_access(icache, ia, &dummy_line_addr,
                                                  TRUE);
  if(!ic_data) {
    warmup_uncore(proc_id, ia, FALSE);
    Addr repl_line_addr;
    ic_data = (Inst_Info**)cache_insert(icache, proc_id, ia, &dummy_line_addr,
                                        &repl_line_addr);
  }

  // Warmup caches for data
  Flag is_load  = op->table_info->mem_type == MEM_LD;
  Flag is_store = op->table_info->mem_type == MEM_ST;
  if(is_load || is_store) {
    Cache*       dcache  = &(cmp_model.dcache_stage[proc_id].dcache);
    Dcache_Data* dc_data = cache_access(dcache, va, &dummy_line_addr, TRUE);
    if(dc_data) {
      // set some fields to meet expectations of the simulation mode
      if(is_store)
        dc_data->dirty = TRUE;
      dc_data->read_count[0] += is_load;
      dc_data->write_count[0] += is_store;
    } else {
      warmup_uncore(proc_id, va, FALSE);
      Addr repl_line_addr;
      dc_data = (Dcache_Data*)cache_insert(dcache, proc_id, va,
                                           &dummy_line_addr, &repl_line_addr);
      if(dc_data->dirty)
        warmup_uncore(proc_id, repl_line_addr, TRUE);
      dc_data->dirty          = is_store;
      dc_data->read_count[0]  = is_load;
      dc_data->write_count[0] = is_store;
    }
  }

  // Warmup BP for CF instructions
  if(op->table_info->cf_type != NOT_CF) {
    Bp_Data* bp_data = &(cmp_model.bp_data[proc_id]);
    bp_predict_op(bp_data, op, 1, ia);
    bp_target_known_op(bp_data, op);
    bp_resolve_op(bp_data, op);
    if(op->oracle_info.mispred || op->oracle_info.misfetch) {
      bp_recover_op(bp_data, op->table_info->cf_type, &op->recovery_info);
    }
    bp_data->bp->retire_func(op);
  }
}

static void cmp_measure_chip_util() {
  Flag chip_busy = exec->fus_busy ||
                   mem->uncores[exec->proc_id].num_outstanding_l1_accesses >
                     0 ||
                   dc->idle_cycle > cycle_count;
  perf_pred_core_busy(exec->proc_id, chip_busy);
}
