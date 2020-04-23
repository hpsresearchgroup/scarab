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
 ** File         : uop_generator.c
 ** Author       : HPS Research Group
 ** Date         : 1/3/2018
 ** Description  : API to extract uops out of ctype_pin_inst_struct instances.
 ****************************************************************************************/

#include "../../debug/debug.param.h"
#include "../../debug/debug_macros.h"
#include "../../globals/assert.h"
#include "../../globals/global_defs.h"
#include "../../globals/global_types.h"
#include "../../globals/global_vars.h"
#include "../../globals/utils.h"

#include "../../bp/bp.h"
#include "../../bp/bp.param.h"
#include "../../general.param.h"
#include "../../statistics.h"

#include "../../ctype_pin_inst.h"
#include "../../isa/isa.h"
#include "../../libs/hash_lib.h"

#include "uop_generator.h"

/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_TRACE_READ, ##args)
#define MEM_MAX_SIZE 64
#define MAX_PUP 256

/**************************************************************************************/
/* Types */

struct Trace_Uop_struct {
  Op_Type  op_type;   // type of operation
  Mem_Type mem_type;  // type of memory instruction
  Cf_Type  cf_type;   // type of control flow instruction
  Bar_Type bar_type;  // type of barrier caused by instruction

  Flag has_lit;        // does it have a literal? (only integer operates can)
  uns  num_dest_regs;  // number of destination registers written
  uns  num_src_regs;   // number of source registers read
  uns  num_agen_src_regs;  // memory address calculation read for ztrace

  uns   inst_size;  // instruction size
  uns64 ztrace_binary;
  Addr  addr;

  Reg_Info srcs[MAX_SRCS];  // source register information
  Reg_Info dests[MAX_DESTS];


  // Dynamic (Runtime) Info
  uint64_t inst_uid;
  Flag     actual_taken;
  Addr     va;
  uns      mem_size;  // number of bytes read/written by a memory instruction
  Addr     target;
  Addr     npc;
  Flag     bom;
  Flag     eom;
  Flag     exit;
  //////////////////

  Flag       pin_2nd_mem;
  Inst_Info* info;
  Flag       alu_uop;
};
typedef struct Trace_Uop_struct Trace_Uop;

/**************************************************************************************/
/* Global Variables */

extern int op_type_delays[NUM_OP_TYPES];
extern uns NEW_INST_TABLE_SIZE;  // TODO: what is this?

char* trace_files[MAX_NUM_PROCS];

Trace_Uop*** trace_uop_bulk;
Flag*        bom;
Flag*        eom;
uns*         num_sending_uop;
uns*         num_uops;
Addr*        last_ga_va;

Hash_Table*
  inst_info_hash; /* hash table of all static instruction information */

/**************************************************************************************/
/* Local prototypes */

static void convert_pinuop_to_t_uop(uns8 proc_id, ctype_pin_inst* pi,
                                    Trace_Uop** trace_uop);
static void convert_t_uop_to_info(uns8 proc_id, Trace_Uop* t_uop,
                                  Inst_Info* info);
static void convert_dyn_uop(uns8 proc_id, Inst_Info* info, ctype_pin_inst* pi,
                            Trace_Uop* trace_uop, uns mem_size,
                            Flag is_last_uop);

/**************************************************************************************/

void uop_generator_init(uint32_t num_cores) {
  inst_info_hash = (Hash_Table*)malloc(num_cores * sizeof(Hash_Table));
  for(uns ii = 0; ii < num_cores; ii++) {
    init_hash_table(&inst_info_hash[ii], "instruction hash table",
                    INST_HASH_TABLE_SIZE, sizeof(Inst_Info));
  }

  trace_uop_bulk = (Trace_Uop***)malloc(num_cores * sizeof(Trace_Uop**));
  for(uns ii = 0; ii < num_cores; ii++) {
    trace_uop_bulk[ii] = (Trace_Uop**)malloc(MAX_PUP * sizeof(Trace_Uop*));
    for(uns jj = 0; jj < MAX_PUP; jj++) {
      trace_uop_bulk[ii][jj] = (Trace_Uop*)malloc(sizeof(Trace_Uop));
    }
  }

  bom = (Flag*)malloc(num_cores * sizeof(Flag));
  memset(bom, 1, num_cores * sizeof(Flag));
  eom = (Flag*)malloc(num_cores * sizeof(Flag));
  memset(eom, 1, num_cores * sizeof(Flag));
  num_uops = (uns*)malloc(num_cores * sizeof(uns));
  memset(num_uops, 0, num_cores * sizeof(uns));
  num_sending_uop = (uns*)malloc(num_cores * sizeof(uns));
  memset(num_sending_uop, 0, num_cores * sizeof(uns));

  last_ga_va = (Addr*)malloc(num_cores * sizeof(Addr));
}

Flag uop_generator_extract_op(uns proc_id, Op* op, compressed_op* cop) {
  if(uop_generator_get_bom(proc_id)) {
    uop_generator_get_uop(proc_id, op, cop);
  } else {
    uop_generator_get_uop(proc_id, op, NULL);
  }

  if(uop_generator_get_eom(proc_id)) {
    return TRUE;
  }

  return FALSE;
}

void uop_generator_get_uop(uns proc_id, Op* op, ctype_pin_inst* inst) {
  Trace_Uop*     trace_uop = NULL;
  Trace_Uop**    trace_uop_array;
  uns            ii;
  Inst_Info*     info    = NULL;
  static Counter read_ci = 0;

  // cmp: find the correct core
  trace_uop_array = trace_uop_bulk[proc_id];

  if(bom[proc_id]) {
    ASSERT(proc_id, inst != NULL);
    convert_pinuop_to_t_uop(proc_id, inst, trace_uop_array);

    op->bom           = TRUE;
    trace_uop         = trace_uop_array[0];
    info              = trace_uop->info;
    num_uops[proc_id] = info->trace_info.num_uop;

    num_sending_uop[proc_id] = 1;
    eom[proc_id]             = trace_uop->eom;

    DEBUG(proc_id,
          "readed pi%s addr is 0x%s next_addr: 0x%s op_type:%d num_st:%d "
          "num_ld:%d is_fp:%d cf_type:%d size:%d branch_target:%s ld_size:%d "
          "st_size:%d ld_vaddr[0]:%s ld_vaddr[1]:%s st_vaddr[0]:%s taken:%d "
          "num_uop:%d eom:%d\n",
          unsstr64(read_ci), hexstr64s(inst.instruction_addr),
          hexstr64s(inst.instruction_next_addr), inst.op_type, inst.num_st,
          inst.num_ld, inst.is_fp, inst.cf_type, inst.size,
          hexstr64s(inst.branch_target), inst.ld_size, inst.st_size,
          hexstr64s(inst.ld_vaddr[0]), hexstr64s(inst.ld_vaddr[1]),
          hexstr64s(inst.st_vaddr[0]), inst.actually_taken, num_uops[proc_id],
          eom[proc_id]);

    for(ii = 0; ii < num_uops[proc_id]; ii++) {
      int kk;
      DEBUG(proc_id,
            "uop[%d] addr:%s npc:%s op_opcode:%s va:%s num_src:%d num_dest:%d",
            ii, hexstr64s(trace_uop_array[ii]->addr),
            hexstr64s(trace_uop_array[ii]->npc),
            Op_Type_str(trace_uop_array[ii]->op_type),
            hexstr64s(trace_uop_array[ii]->va),
            trace_uop_array[ii]->num_src_regs,
            trace_uop_array[ii]->num_dest_regs);
      for(kk = 0; kk < trace_uop_array[ii]->num_src_regs; kk++) {
        DEBUG(proc_id, "src[%d]:%s ", kk,
              disasm_reg(trace_uop_array[ii]->srcs[kk].id));
      }
      for(kk = 0; kk < trace_uop_array[ii]->num_dest_regs; kk++) {
        DEBUG(proc_id, "dest[%d]:%s ", kk,
              disasm_reg(trace_uop_array[ii]->dests[kk].id));
      }
      DEBUG(proc_id, "\n");
    }
    if(proc_id == 0)
      read_ci++;
  } else {
    trace_uop = trace_uop_array[num_sending_uop[proc_id]];
    ASSERTM(proc_id, trace_uop, "%i\n", num_sending_uop[proc_id]);
    num_sending_uop[proc_id]++;

    info = trace_uop->info;
    ASSERTM(proc_id, info, "%i\n", num_sending_uop[proc_id]);
    eom[proc_id] = trace_uop->eom;
  }

  if(eom[proc_id]) {
    bom[proc_id] = TRUE;
  } else
    bom[proc_id] = FALSE;

  op->op_num                   = op_count[proc_id];
  op->inst_uid                 = trace_uop->inst_uid;
  op->unique_num               = unique_count;
  op->unique_num_per_proc      = unique_count_per_core[proc_id];
  op->proc_id                  = proc_id;
  op->thread_id                = 0;
  op->eom                      = trace_uop->eom;
  op->inst_info                = info;
  op->table_info               = info->table_info;
  op->oracle_info.inst_info    = info;
  op->oracle_info.table_info   = info->table_info;
  op->engine_info.inst_info    = info;
  op->engine_info.table_info   = info->table_info;
  op->off_path                 = FALSE;
  op->fetch_addr               = op->inst_info->addr;
  op->state                    = OS_FETCHED;
  op->fu_num                   = -1;
  op->issue_cycle              = MAX_CTR;
  op->map_cycle                = MAX_CTR;
  op->rdy_cycle                = 1;
  op->sched_cycle              = MAX_CTR;
  op->exec_cycle               = MAX_CTR;
  op->dcache_cycle             = MAX_CTR;
  op->done_cycle               = MAX_CTR;
  op->replay_cycle             = MAX_CTR;
  op->retire_cycle             = MAX_CTR;
  op->replay                   = FALSE;
  op->replay_count             = 0;
  op->dont_cause_replays       = FALSE;
  op->exec_count               = 0;
  op->in_rdy_list              = FALSE;
  op->in_node_list             = FALSE;
  op->oracle_info.recovery_sch = FALSE;

  op->req    = NULL;
  op->marked = FALSE;

  /* pipelined scheduler fields */
  op->chkpt_num = MAX_CTR;
  // op->row_num	         = MAX_CTR;
  op->node_id          = MAX_CTR;
  op->rs_id            = MAX_CTR;
  op->same_src_last_op = 0;

  op->oracle_cp_num                  = -1;
  op->engine_info.l1_miss            = FALSE;
  op->engine_info.l1_miss_satisfied  = FALSE;
  op->engine_info.dep_on_l1_miss     = FALSE;
  op->engine_info.was_dep_on_l1_miss = FALSE;

  /* multi path support */

  /* execute op */

  if(op->table_info->op_type == OP_CF)
    op->oracle_info.dir = (trace_uop->actual_taken == 0) ? NOT_TAKEN : TAKEN;
  else
    op->oracle_info.dir = NOT_TAKEN;

  if((op->table_info->cf_type == CF_ICALL) ||
     (op->table_info->cf_type == CF_IBR) || (op->table_info->cf_type == CF_ICO))
    op->oracle_info.dir = 1;  // FIXME Hack!! because of StringMOV

  op->oracle_info.target = trace_uop->target ? trace_uop->target :
                                               trace_uop->npc;
  op->oracle_info.va  = trace_uop->va;
  op->oracle_info.npc = trace_uop->npc;
  if(op->proc_id)
    ASSERT(op->proc_id, op->oracle_info.npc);
  op->oracle_info.mem_size = trace_uop->mem_size;
  // op->table_info->mem_size = trace_uop->mem_size;  // because of repeat move
  // mem size is dynamic info  WRONG!!!!


  if(op->table_info->mem_type && !(op->oracle_info.va)) {
    op->oracle_info.va = last_ga_va[proc_id];  // QUESTION why? //TODO: Really
                                               // why?
  } else if(op->oracle_info.va)
    last_ga_va[proc_id] = op->oracle_info.va;


  if((op->eom && trace_read_done[proc_id]) || trace_uop->exit) {
    op->exit = TRUE;
  } else {
    op->exit = FALSE;
  }

  DEBUG(proc_id,
        "op_num:%s unique_num:%s pc:0x%s npc:0x%s  va:0x%s mem_type:%d "
        "cf_type:%d oracle_target:%s dir:%d va:%s mem_size:%d \n",
        unsstr64(op->op_num), unsstr64(op->unique_num),
        hexstr64s(op->inst_info->addr), hexstr64s(op->oracle_info.npc),
        hexstr64s(op->oracle_info.va), op->table_info->mem_type,
        op->table_info->cf_type, hexstr64s(op->oracle_info.target),
        op->oracle_info.dir, hexstr64s(op->oracle_info.va),
        op->oracle_info.mem_size);

  for(ii = 0; ii < op->inst_info->table_info->num_src_regs; ii++) {
    DEBUG(proc_id,
          "op_num:%s unique_num:%s pc:0x%s npc:0x%s src_num:%d , src_id:%d \n",
          unsstr64(op->op_num), unsstr64(op->unique_num),
          hexstr64s(op->inst_info->addr), hexstr64s(op->oracle_info.npc),
          op->inst_info->table_info->num_src_regs, op->inst_info->srcs[ii].id);
  }

  for(ii = 0; ii < op->inst_info->table_info->num_dest_regs; ii++) {
    DEBUG(
      proc_id,
      "op_num:%s unique_num:%s pc:0x%s npc:0x%s dest_num:%d , dest_id:%d \n",
      unsstr64(op->op_num), unsstr64(op->unique_num),
      hexstr64s(op->inst_info->addr), hexstr64s(op->oracle_info.npc),
      op->inst_info->table_info->num_dest_regs, op->inst_info->dests[ii].id);
  }
}

Flag uop_generator_get_bom(uns proc_id) {
  return bom[proc_id];
}

Flag uop_generator_get_eom(uns proc_id) {
  return eom[proc_id];
}

void convert_t_uop_to_info(uns8 proc_id, Trace_Uop* t_uop, Inst_Info* info) {
  int ii;

  // build info // we  can optimize to build this info only once
  info->table_info = (Table_Info*)malloc(sizeof(Table_Info));  // FIXME. at
                                                               // least a hash
                                                               // function based
                                                               // on the same
                                                               // table info.

  ASSERT(proc_id, info);
  ASSERT(proc_id, info->table_info);

  info->table_info->op_type       = t_uop->op_type;
  info->table_info->mem_type      = t_uop->mem_type;
  info->table_info->cf_type       = t_uop->cf_type;
  info->table_info->bar_type      = t_uop->bar_type;
  info->table_info->has_lit       = t_uop->has_lit;
  info->table_info->num_dest_regs = t_uop->num_dest_regs;
  info->table_info->num_src_regs  = t_uop->num_src_regs;
  info->table_info->mem_size      = t_uop->mem_size;  // IGNORED FOR REP STRING
                                                      // instructions

  info->table_info->type       = 0;    /* scarab internals, ignore */
  info->table_info->mask       = 0;    /* scarab internals, ignore */
  info->table_info->dec_func   = NULL; /* FIXME */
  info->table_info->src_func   = NULL; /* FIXME */
  info->table_info->sim_func   = NULL; /* FIXME */
  info->table_info->qualifiers = 0;    /* FIXME */

  /* op->inst_info */
  info->ztrace_binary        = t_uop->ztrace_binary;
  info->addr                 = t_uop->addr;
  info->trace_info.inst_size = (t_uop->inst_size);  // FIXME

  for(ii = 0; ii < info->table_info->num_src_regs; ii++) {
    info->srcs[ii].type = INT_REG;
    info->srcs[ii].id   = t_uop->srcs[ii].id;
    info->srcs[ii].reg  = t_uop->srcs[ii].reg;
    /* If an op sources a predicate, it is always the last source - here we
     * avoid sourcing the last source */
  }

  /* only one destination - temporary that is going to be read by the second
   * t_uop */
  for(ii = 0; ii < info->table_info->num_dest_regs; ii++) {
    info->dests[ii].type = INT_REG;
    info->dests[ii].id   = t_uop->dests[ii].id;
    info->dests[ii].reg  = t_uop->dests[ii].reg;
  }


  info->latency = op_type_delays[t_uop->op_type];
  if(info->latency == 0)
    info->latency = 1; /* insure latency is not 0 */


  info->trace_info.second_mem = t_uop->pin_2nd_mem;

  info->lit  = 0; /* FIXME */
  info->disp = 0; /* FIXME */

  info->trigger_op_fetched_hook = FALSE; /* FIXME */
  info->track_preloaded         = FALSE; /* FIXME */
  info->on_addr_stream          = FALSE; /* FIXME */
  info->hard_to_predict         = FALSE; /* FIXME */
  info->important_ld            = FALSE; /* FIXME */
  info->extra_ld_latency        = 0;
  info->vlp_info                = NULL;
}

static void clear_t_uop(Trace_Uop* uop) {
  memset(uop, 0, sizeof(Trace_Uop));
}

static void add_t_uop_src_reg(Trace_Uop* uop, Reg_Id reg) {
  ASSERT(0, uop->num_src_regs < MAX_SRCS);
  uop->srcs[uop->num_src_regs].type = 0;
  uop->srcs[uop->num_src_regs].id   = reg;
  uop->srcs[uop->num_src_regs].reg  = reg;
  uop->num_src_regs++;
}

static void add_t_uop_dest_reg(Trace_Uop* uop, Reg_Id reg) {
  ASSERT(0, uop->num_dest_regs < MAX_DESTS);
  uop->dests[uop->num_dest_regs].type = 0;
  uop->dests[uop->num_dest_regs].id   = reg;
  uop->dests[uop->num_dest_regs].reg  = reg;
  uop->num_dest_regs++;
}

static Flag is_stack_reg(Reg_Id reg) {
  return reg == REG_RSP || reg == REG_SS;
}

static void add_rep_uops(ctype_pin_inst* pi, Trace_Uop** trace_uop, uns* idx) {
  Flag add_rsi_add = FALSE;
  Flag add_rdi_add = FALSE;

  for(int i = 0; i < pi->num_ld1_addr_regs; ++i) {
    if(pi->ld1_addr_regs[i] == REG_RSI)
      add_rsi_add = TRUE;
    if(pi->ld1_addr_regs[i] == REG_RDI)
      add_rdi_add = TRUE;
  }
  for(int i = 0; i < pi->num_ld2_addr_regs; ++i) {
    if(pi->ld2_addr_regs[i] == REG_RSI)
      add_rsi_add = TRUE;
    if(pi->ld2_addr_regs[i] == REG_RDI)
      add_rdi_add = TRUE;
  }
  for(int i = 0; i < pi->num_st_addr_regs; ++i) {
    if(pi->st_addr_regs[i] == REG_RSI)
      add_rsi_add = TRUE;
    if(pi->st_addr_regs[i] == REG_RDI)
      add_rdi_add = TRUE;
  }

  if(add_rsi_add) {
    Trace_Uop* uop = trace_uop[*idx];
    clear_t_uop(uop);
    uop->op_type = OP_IADD;
    uop->alu_uop = TRUE;
    add_t_uop_src_reg(uop, REG_RSI);
    add_t_uop_dest_reg(uop, REG_RSI);
    *idx = *idx + 1;
  }

  if(add_rdi_add) {
    Trace_Uop* uop = trace_uop[*idx];
    clear_t_uop(uop);
    uop->op_type = OP_IADD;
    uop->alu_uop = TRUE;
    add_t_uop_src_reg(uop, REG_RDI);
    add_t_uop_dest_reg(uop, REG_RDI);
    *idx = *idx + 1;
  }

  if(pi->is_repeat) {
    // Decrement RCX micro-op
    Trace_Uop* uop = trace_uop[*idx];
    clear_t_uop(uop);
    uop->op_type = OP_IADD;
    uop->alu_uop = TRUE;
    add_t_uop_src_reg(uop, REG_RCX);
    add_t_uop_dest_reg(uop, REG_RCX);
    *idx = *idx + 1;
    // Control flow micro-op
    uop = trace_uop[*idx];
    clear_t_uop(uop);
    uop->op_type = OP_CF;
    uop->cf_type = CF_CBR;
    add_t_uop_src_reg(uop, REG_ZPS);
    *idx = *idx + 1;
  }
}

static uns generate_uops(uns8 proc_id, ctype_pin_inst* pi,
                         Trace_Uop** trace_uop) {
  /* Generating microinstructions for the trace instruction. The
   * general sequence of every instruction other than REP insts is:
   *     load_1, load_2, operate, store, control                 */

  uns  idx         = 0;
  Flag has_load    = pi->num_ld > 0;
  Flag has_push    = pi->has_push;
  Flag has_pop     = pi->has_pop;
  Flag has_store   = pi->num_st;
  Flag has_control = pi->cf_type != NOT_CF;
  Flag has_alu =
    !(pi->is_move && (has_load || has_store)) &&  // not a simple LD/ST move
    ((!has_control && !has_load &&
      !has_store)             // not memory, not control, must be operate
     || has_push || has_pop   // need ALU for stack address generation
     || pi->num_dst_regs > 0  // if it writes to a registers, must be operate
     || (has_load &&
         has_store)  // must be read-modify-write operate (e.g. add $1, [%eax])
     || (pi->op_type >= OP_PIPELINED_FAST &&  // special instructions always
         pi->op_type <= OP_NOTPIPELINED_VERY_SLOW));  // need an alu uop

  /* both REP MOVS and REP STOS are is_rep_st, meaning alu uop is independent of
   * mem uops */
  Flag is_rep_st = (pi->is_string) && has_store;

  /* Loads */
  for(uns i = 0; i < pi->num_ld; ++i) {
    Trace_Uop* uop = trace_uop[idx];
    idx += 1;
    clear_t_uop(uop);

    uop->mem_type    = (pi->is_prefetch) ? MEM_PF : MEM_LD;
    uop->op_type     = (pi->is_fp || pi->is_simd) ? OP_FMEM : OP_IMEM;
    uop->mem_size    = pi->ld_size;
    uop->pin_2nd_mem = (i == 1);

    for(uns j = 0; j < pi->num_ld1_addr_regs; ++j) {
      Reg_Id reg = (i == 0 ? pi->ld1_addr_regs : pi->ld2_addr_regs)[j];
      add_t_uop_src_reg(uop, reg);
    }

    if((has_alu && !has_push && !has_pop) || has_store ||
       has_control) {  // load result used further down
      add_t_uop_dest_reg(uop, REG_TMP0 + i);
    } else {
      for(uns j = 0; j < pi->num_dst_regs; ++j) {
        Reg_Id reg = pi->dst_regs[j];
        add_t_uop_dest_reg(uop, reg);
      }
    }
  }

  /* Operate */
  if(has_alu) {
    Trace_Uop* uop = trace_uop[idx];
    idx += 1;
    clear_t_uop(uop);

    ASSERT(proc_id, pi->op_type < NUM_OP_TYPES);
    uop->op_type = pi->op_type;

    ASSERT(proc_id, uop->op_type != OP_INV);
    uop->alu_uop = TRUE;

    if(has_push ||
       has_pop) {  // ALU op only changes the stack pointer in stack insts
      add_t_uop_src_reg(uop, REG_RSP);
      add_t_uop_dest_reg(uop, REG_RSP);
    } else if(!is_rep_st) {
      for(uns i = 0; i < pi->num_ld; ++i) {
        add_t_uop_src_reg(uop, REG_TMP0 + i);
      }
    }

    for(uns j = 0; j < pi->num_src_regs; ++j) {
      Reg_Id reg = pi->src_regs[j];

      if(has_push || has_pop) {
        if(is_stack_reg(reg))
          add_t_uop_src_reg(uop, reg);
      } else {
        add_t_uop_src_reg(uop, reg);
      }
    }

    if((!has_push && !has_pop) && (has_store || has_control) && !is_rep_st) {
      add_t_uop_dest_reg(uop, REG_TMP2);
    }

    if(!has_push && !has_pop) {
      for(uns j = 0; j < pi->num_dst_regs; ++j) {
        Reg_Id reg = pi->dst_regs[j];
        add_t_uop_dest_reg(uop, reg);
      }
    }
  }

  /* Store */
  if(has_store) {
    Trace_Uop* uop = trace_uop[idx];
    idx += 1;
    clear_t_uop(uop);

    uop->mem_type = MEM_ST;
    uop->op_type  = (pi->is_fp || pi->is_simd) ? OP_FMEM : OP_IMEM;
    uop->mem_size = pi->st_size;

    if(pi->is_call) {
      // only storing (invisible) EIP on calls
    } else if(!has_alu || has_pop || has_push || is_rep_st) {
      for(uns i = 0; i < pi->num_ld; ++i) {
        add_t_uop_src_reg(uop, REG_TMP0 + i);
      }
    } else if(has_alu && !has_push && !has_pop) {
      add_t_uop_src_reg(uop, REG_TMP2);
    }

    for(uns j = 0; j < pi->num_st_addr_regs; ++j) {
      Reg_Id reg = pi->st_addr_regs[j];
      add_t_uop_src_reg(uop, reg);
    }

    for(uns j = 0; j < pi->num_src_regs; ++j) {
      Reg_Id reg = pi->src_regs[j];

      if(!has_load && (!has_alu || has_push))
        add_t_uop_src_reg(uop, reg);
    }

    // store has no dest regs
  }

  /* Control */
  if(has_control) {
    Trace_Uop* uop = trace_uop[idx];
    idx += 1;
    clear_t_uop(uop);

    uop->cf_type = pi->cf_type;
    uop->op_type = OP_CF;

    if(has_load) {
      for(uns i = 0; i < pi->num_ld; ++i) {
        add_t_uop_src_reg(uop, REG_TMP0 + i);
      }
    } else {
      for(uns j = 0; j < pi->num_src_regs; ++j) {
        Reg_Id reg = pi->src_regs[j];

        // When calling/returning, the control op does not use
        // the stack pointer
        if(!is_stack_reg(reg) || !(has_pop || has_push))
          add_t_uop_src_reg(uop, reg);
      }
    }
  }

  if(pi->is_string) {
    add_rep_uops(pi, trace_uop, &idx);
  }

  // make a nop if no ops were generated
  if(idx == 0) {
    Trace_Uop* uop = trace_uop[idx];
    idx += 1;
    clear_t_uop(uop);

    uop->op_type = OP_NOP;
    STAT_EVENT(0, STATIC_PIN_NOP);
  }

  return idx;
}

void convert_pinuop_to_t_uop(uns8 proc_id, ctype_pin_inst* pi,
                             Trace_Uop** trace_uop) {
  Flag       new_entry = FALSE;
  Addr       key_addr  = (pi->instruction_addr << 3);
  Inst_Info* info;
  if(pi->fake_inst) {
    info                   = (Inst_Info*)calloc(1, sizeof(Inst_Info));
    info->fake_inst        = TRUE;
    info->fake_inst_reason = pi->fake_inst_reason;
  } else {
    info = (Inst_Info*)hash_table_access_create(&inst_info_hash[proc_id],
                                                key_addr, &new_entry);
    info->fake_inst        = FALSE;
    info->fake_inst_reason = WPNM_NOT_IN_WPNM;
  }
  int ii;
  int num_uop = 0;

  if(pi->is_string) {
    pi->branch_target  = pi->instruction_addr;
    pi->actually_taken = (pi->branch_target == pi->instruction_next_addr);
  }

  pi->instruction_addr = convert_to_cmp_addr(proc_id, pi->instruction_addr);
  pi->instruction_next_addr = convert_to_cmp_addr(proc_id,
                                                  pi->instruction_next_addr);
  pi->branch_target         = convert_to_cmp_addr(proc_id, pi->branch_target);
  pi->ld_vaddr[0]           = convert_to_cmp_addr(proc_id, pi->ld_vaddr[0]);
  pi->ld_vaddr[1]           = convert_to_cmp_addr(proc_id, pi->ld_vaddr[1]);
  pi->st_vaddr[0]           = convert_to_cmp_addr(proc_id, pi->st_vaddr[0]);

  if(new_entry || pi->fake_inst) {
    num_uop = generate_uops(proc_id, pi, trace_uop);
    ASSERT(proc_id, num_uop > 0);

    info->trace_info.num_uop = num_uop;

    for(ii = 0; ii < num_uop; ii++) {
      if(ii > 0) {
        if(pi->fake_inst) {
          info                   = (Inst_Info*)calloc(1, sizeof(Inst_Info));
          info->fake_inst        = TRUE;
          info->fake_inst_reason = pi->fake_inst_reason;
        } else {
          key_addr = ((pi->instruction_addr << 3) + ii);
          info = (Inst_Info*)hash_table_access_create(&inst_info_hash[proc_id],
                                                      key_addr, &new_entry);
          info->fake_inst        = FALSE;
          info->fake_inst_reason = WPNM_NOT_IN_WPNM;
        }
      }
      ASSERT(proc_id, new_entry || pi->fake_inst);

      trace_uop[ii]->addr      = pi->instruction_addr;
      trace_uop[ii]->inst_size = pi->size;

      if(ii == (num_uop - 1)) {
        /* last uop's info */
        if(pi->is_ifetch_barrier) {
          trace_uop[num_uop - 1]->bar_type = BAR_FETCH;  // only the last
                                                         // instruction will
                                                         // have bar type
        }
      }

      convert_t_uop_to_info(proc_id, trace_uop[ii], info);
      trace_uop[ii]->info = info;

      trace_uop[ii]->info->table_info->is_simd = pi->is_simd;
      trace_uop[ii]->info->uop_seq_num         = ii;
      strcpy(trace_uop[ii]->info->table_info->name, pi->pin_iclass);
      if(trace_uop[ii]->alu_uop) {
        trace_uop[ii]->info->table_info->num_simd_lanes = pi->num_simd_lanes;
        trace_uop[ii]->info->table_info->lane_width_bytes =
          pi->lane_width_bytes;
      }

      ASSERT(proc_id, info->trace_info.inst_size == pi->size);

      Flag is_last_uop = (ii == (num_uop - 1));
      convert_dyn_uop(proc_id, info, pi, trace_uop[ii],
                      info->table_info->mem_size, is_last_uop);
    }
  } else {
    // instructions is decoded before .

    num_uop = info->trace_info.num_uop;
    for(ii = 0; ii < num_uop; ii++) {
      if(ii > 0) {
        key_addr = ((pi->instruction_addr << 3) + ii);
        info = (Inst_Info*)hash_table_access_create(&inst_info_hash[proc_id],
                                                    key_addr, &new_entry);
      }
      ASSERT(proc_id, !new_entry);

      trace_uop[ii]->info = info;
      trace_uop[ii]->eom  = FALSE;
      ASSERT(proc_id, info->addr == pi->instruction_addr);
      ASSERT(proc_id, info->trace_info.inst_size == pi->size);

      Flag is_last_uop = (ii == (num_uop - 1));
      convert_dyn_uop(proc_id, info, pi, trace_uop[ii],
                      info->table_info->mem_size, is_last_uop);
    }
  }

  ASSERT(proc_id, num_uop > 0);
  trace_uop[num_uop - 1]->eom = TRUE;

  trace_uop[num_uop - 1]->npc = pi->instruction_next_addr;
}


void convert_dyn_uop(uns8 proc_id, Inst_Info* info, ctype_pin_inst* pi,
                     Trace_Uop* trace_uop, uns mem_size, Flag is_last_uop) {
  trace_uop->inst_uid = pi->inst_uid;
  trace_uop->va       = 0;
  trace_uop->mem_size = 0;

  if(info->table_info->cf_type) {
    trace_uop->actual_taken = pi->actually_taken;
    trace_uop->target       = pi->branch_target;  // FIXME
  } else if(info->table_info->mem_type) {
    if(info->table_info->mem_type == MEM_ST) {
      trace_uop->va = pi->st_vaddr[0];

      if(mem_size <= MEM_MAX_SIZE) {
        DEBUG(proc_id,
              "Generate a store with large size: @%llx opcode: %s num_ld: %i "
              "st?: %u size: %u\n",
              (long long unsigned int)pi->instruction_addr,
              Op_Type_str(pi->op_type), pi->num_ld, pi->num_st, pi->st_size);
      }

      trace_uop->mem_size = mem_size;
    } else if(info->table_info->mem_type == MEM_LD) {
      trace_uop->va = pi->ld_vaddr[info->trace_info.second_mem];

      if(mem_size <= MEM_MAX_SIZE) {
        DEBUG(proc_id,
              "Generate a load with large size: @%llx opcode: %s num_ld: %i "
              "st?: %u size: %u\n",
              (long long unsigned int)pi->instruction_addr,
              Op_Type_str(pi->op_type), pi->num_ld, pi->num_st, pi->ld_size);
      }

      trace_uop->mem_size = mem_size;
    }
  }

  trace_uop->exit = is_last_uop ? pi->exit : 0;

  trace_uop->npc = trace_uop->addr;  // CHECKME!!
}

void uop_generator_recover(uns8 proc_id) {
  bom[proc_id] = 1;
}
