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
 * File         : debug/debug_print.c
 * Author       : HPS Research Group
 * Date         : 4/14/1998
 * Description  : Functions to print out various debugging information.
 ***************************************************************************************/

#include <stdio.h>
#include "debug/debug_macros.h"
#include "globals/assert.h"
#include "globals/enum.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"
#include "isa/isa.h"

#include "debug/debug_print.h"
#include "thread.h"

#include "core.param.h"


/**************************************************************************************/
/* Macros */

/* these masks control what debug op fields get printed.  note that
   they are always printed in the order defined in Field_Enum. */

#define FULL_FIELD_MASK (0xffffffff)

#define SHORT_FIELD_MASK                                                   \
  ((0x1 << TOP_LINE_FIELD) | (0x1 << DISASM_FIELD) | (0x1 << ADDR_FIELD) | \
   (0x1 << BOTTOM_LINE_FIELD))


#define DONT_SHOW_FIELDS ((0x1 << NODE_INFO_FIELD) | (0x1 << OP_TYPE_FIELD))


/**************************************************************************************/
/* Local prototypes */

static int compare_reg_ids(const void* p1, const void* p2);
static int print_reg_array(char* buf, Reg_Info* regs, uns num);

/**************************************************************************************/
/* External Variables */

extern const char* const int_reg_names[];
extern const char* const fp_reg_names[];


/**************************************************************************************/
/* An enum to enumerate all of the different rows to be printed. */

typedef enum {
  TOP_LINE_FIELD,
  DISASM_FIELD,
  ADDR_FIELD,
  OP_NUM_FIELD,
  OP_TYPE_FIELD,
  MEM_INFO_FIELD,
  NODE_INFO_FIELD,
  BOTTOM_LINE_FIELD,
  NUM_OP_FIELDS,
} Field_Enum;


/**************************************************************************************/
/* Type field enum definitions. */

DEFINE_ENUM(Op_Type, OP_TYPE_LIST);

const char* const mem_type_names[] = {"NOT_MEM", "MEM_LD", "MEM_ST", "MEM_PF"};

const char* const cf_type_names[] = {"NOT_CF",  "CF_BR",  "CF_CBR",
                                     "CF_CALL", "CF_IBR", "CF_ICALL",
                                     "CF_ICO",  "CF_RET", "CF_SYS"};

const char* const bar_type_names[] = {"NOT_BAR"};

DEFINE_ENUM(Op_State, OP_STATE_LIST);

const char* const icache_state_names[] = {
  "IC_FETCH",         "IC_REFETCH",           "IC_FILL",
  "IC_WAIT_FOR_MISS", "IC_WAIT_FOR_REDIRECT", "IC_WAIT_FOR_EMPTY_ROB",
  "IC_WAIT_FOR_TIMER"};

const char* const tcache_state_names[] = {"TC_FETCH",
                                          "TC_WAIT_FOR_MISS",
                                          "TC_WAIT_FOR_REDIRECT",
                                          "TC_WAIT_FOR_CALLSYS",
                                          "TC_ICACHE_FETCH",
                                          "TC_ICACHE_REFETCH",
                                          "TC_ICACHE_FILL",
                                          "TC_ICACHE_WAIT_FOR_MISS",
                                          "TC_ICACHE_WAIT_FOR_REDIRECT",
                                          "TC_ICACHE_WAIT_FOR_CALLSYS"
                                          "TC_TCACHE_FETCH",
                                          "TC_TCACHE_REFETCH",
                                          "TC_TCACHE_FILL",
                                          "TC_TCACHE_WAIT_FOR_MISS",
                                          "TC_TCACHE_WAIT_FOR_REDIRECT",
                                          "TC_TCACHE_WAIT_FOR_CALLSYS"};

const char* const sm_state_names[] = {"RS_NULL",   "RS_ICACHE",    "RS_DECODE",
                                      "RS_MAP",    "RS_NODE",      "RS_EXEC",
                                      "RS_RETIRE", "NUM_SM_STATES"};


/**************************************************************************************/
/* print_field_head: */

void print_field_head(FILE* stream, uns field) {
  switch(field) {
    case TOP_LINE_FIELD:
    case BOTTOM_LINE_FIELD:
      fprintf(stream, "+");
      break;
    case DISASM_FIELD:
    case ADDR_FIELD:
    case OP_NUM_FIELD:
    case MEM_INFO_FIELD:
    case NODE_INFO_FIELD:
    case OP_TYPE_FIELD:
      fprintf(stream, "|");
      break;
    default:
      FATAL_ERROR(0, "Invalid field number (%d) in print_field_head.\n", field);
  }
}


/**************************************************************************************/
/* print_field_head: */

void print_field_tail(FILE* stream, uns field) {
  switch(field) {
    case TOP_LINE_FIELD:
    case BOTTOM_LINE_FIELD:
    case DISASM_FIELD:
    case ADDR_FIELD:
    case OP_NUM_FIELD:
    case MEM_INFO_FIELD:
    case NODE_INFO_FIELD:
    case OP_TYPE_FIELD:
      fprintf(stream, "\n");
      break;
    default:
      FATAL_ERROR(0, "Invalid field number (%d) in print_field_tail.\n", field);
  }
}


/**************************************************************************************/
/* print_op_field: */

void print_op_field(FILE* stream, Op* op, uns field) {
  switch(field) {
    case TOP_LINE_FIELD:
    case BOTTOM_LINE_FIELD:
      fprintf(stream, "--------------------+");
      break;
    case DISASM_FIELD:
      if(op)
        fprintf(stream, "%-20s|", disasm_op(op, FALSE));
      else
        fprintf(stream, "xxxxxxxxxxxxxxxxxxxx|");
      break;
    case ADDR_FIELD:
      if(op) {
        fprintf(stream, "a:%-9s f:%-2d%c", hexstr64s(op->inst_info->addr),
                op->fu_num, (op->off_path ? 'O' : ' '));
        if(OP_DONE(op))
          fprintf(stream, "D ");
        else {
          if(op->state == OS_WAIT_FWD)
            if(cycle_count < op->rdy_cycle - 1)
              fprintf(stream, "%c%lld", op->replay ? 'w' : 'W',
                      op->rdy_cycle - cycle_count - 1);
            else
              fprintf(stream, "%c%c", 'R', op->replay ? 'r' : ' ');
          else
            fprintf(stream, "%c%c", Op_State_str(op->state)[0],
                    op->replay ? 'r' : ' ');
        }
        if(op->table_info->cf_type) {
          Flag bits = op->oracle_info.mispred << 2 |
                      op->oracle_info.misfetch << 1 | op->oracle_info.btb_miss;
          switch(bits) {
            case 0x4:
              fprintf(stream, "P|");
              break; /* mispredict */
            case 0x3:
            case 0x2:
              fprintf(stream, "F|");
              break; /* misfetch or (misfetch and btb miss) */
            case 0x1:
              fprintf(stream, "M|");
              break; /* btb miss */
            case 0x5:
            case 0x6:
              fprintf(stream, "B|");
              break; /* mispredict, (misfetch or btb miss) */
            case 0x7:
              fprintf(stream, "A|");
              break; /* mispredict, misfetch, btb miss */
            case 0x0:
              fprintf(stream, " |");
              break;
            default:
              ERROR(op->proc_id, "Invalid prediction result state (0x%x).\n",
                    bits);
          }
        } else
          fprintf(stream, " |");
      } else
        fprintf(stream, "xxxxxxxxxxxxxxxxxxxx|");
      break;
    case OP_NUM_FIELD:
      if(op)
        fprintf(stream, "o:%-3d %3d%c %3d%c %3d%c|", (int)(op->op_num % 1000),
                op->oracle_info.num_srcs > 0 ?
                  (int)(op->oracle_info.src_info[0].op_num % 1000) :
                  -1,
                ((op->srcs_not_rdy_vector & 1) == 0 ? 'r' : 'w'),
                op->oracle_info.num_srcs > 1 ?
                  (int)(op->oracle_info.src_info[1].op_num % 1000) :
                  -1,
                ((op->srcs_not_rdy_vector & 2) == 0 ? 'r' : 'w'),
                op->oracle_info.num_srcs > 2 ?
                  (int)(op->oracle_info.src_info[2].op_num % 1000) :
                  -1,
                ((op->srcs_not_rdy_vector & 4) == 0 ? 'r' : 'w'));
      else
        fprintf(stream, "xxxxxxxxxxxxxxxxxxxx|");
      break;
    case OP_TYPE_FIELD:
      if(op)
        fprintf(stream, "%19s |",
                Op_Type_str(op->inst_info->table_info->op_type));
      else
        fprintf(stream, "xxxxxxxxxxxxxxxxxxxx|");
      break;
    case MEM_INFO_FIELD:
      if(!op || op->table_info->mem_type == NOT_MEM)
        fprintf(stream, "xxxxxxxxxxxxxxxxxxxx|");
      else {
        Counter addr_dep = 0;
        Counter data_dep = 0;
        uns     ii;
        for(ii = 0; ii < op->oracle_info.num_srcs; ii++) {
          Src_Info* src = &op->oracle_info.src_info[ii];
          if(src->type == MEM_ADDR_DEP)
            addr_dep = src->op_num;
          if(src->type == MEM_DATA_DEP)
            data_dep = src->op_num;
        }
        fprintf(stream, "va:%-9s %3d %3d|", hexstr64s(op->oracle_info.va),
                (uns)(addr_dep % 1000), (uns)(data_dep % 1000));
      }
      break;
    case NODE_INFO_FIELD:
      if(!op)
        fprintf(stream, "xxxxxxxxxxxxxxxxxxxx|");
      else
        fprintf(stream, "        node_id:%-3d |", (uns)(op->node_id % 1000));
      break;
    default:
      FATAL_ERROR(0, "Invalid field number (%d) in print_op_field.\n", field);
  }
}


/**************************************************************************************/
/* print_op_array: */

void print_op_array(FILE* stream, Op* ops[], uns array_length, uns op_count) {
  int field_mask = FULL_FIELD_MASK & ~DONT_SHOW_FIELDS;
  uns ii, jj;

  for(ii = 0; ii < NUM_OP_FIELDS; ii++) {
    if(TESTBIT(field_mask, ii)) {
      print_field_head(stream, ii);
      for(jj = 0; jj < array_length; jj++) {
        Op* op = jj < op_count ? ops[jj] : NULL;
        print_op_field(stream, op, ii);
      }
      print_field_tail(stream, ii);
    }
  }
}


/**************************************************************************************/
/* print_open_op_array: Leave off bottom line */

void print_open_op_array(FILE* stream, Op* ops[], uns array_length,
                         uns op_count) {
  int field_mask = FULL_FIELD_MASK & ~(0x1 << BOTTOM_LINE_FIELD) &
                   ~DONT_SHOW_FIELDS;
  uns ii, jj;

  for(ii = 0; ii < NUM_OP_FIELDS; ii++) {
    if(TESTBIT(field_mask, ii)) {
      print_field_head(stream, ii);
      for(jj = 0; jj < array_length; jj++) {
        Op* op = jj < op_count ? ops[jj] : NULL;
        print_op_field(stream, op, ii);
      }
      print_field_tail(stream, ii);
    }
  }
}


/**************************************************************************************/
/* print_open_op_array_end: Print bottom line for an open op array */

void print_open_op_array_end(FILE* stream, uns array_length) {
  int field_mask = FULL_FIELD_MASK & (0x1 << BOTTOM_LINE_FIELD);
  uns jj;

  if(TESTBIT(field_mask, BOTTOM_LINE_FIELD)) {
    print_field_head(stream, BOTTOM_LINE_FIELD);
    for(jj = 0; jj < array_length; jj++)
      print_op_field(stream, NULL, BOTTOM_LINE_FIELD);
    print_field_tail(stream, BOTTOM_LINE_FIELD);
  }
}


/**************************************************************************************/
/* print_op: */

void print_op(Op* op) {
  uns ii;

  for(ii = 0; ii < NUM_OP_FIELDS; ii++) {
    if(TESTBIT(FULL_FIELD_MASK, ii)) {
      print_field_head(GLOBAL_DEBUG_STREAM, ii);
      print_op_field(GLOBAL_DEBUG_STREAM, op, ii);
      print_field_tail(GLOBAL_DEBUG_STREAM, ii);
    }
  }
}

/**************************************************************************************/
/* print_func_op: */

void print_func_op(Op* op) {
  char opcode[MAX_STR_LENGTH + 1];
  if(op->table_info->op_type == OP_CF) {
    sprintf(opcode, "%s", cf_type_names[op->table_info->cf_type]);
  } else if(op->table_info->op_type == OP_IMEM ||
            op->table_info->op_type == OP_FMEM) {
    sprintf(opcode, "%s", mem_type_names[op->table_info->mem_type]);
  } else {
    sprintf(opcode, "%s", Op_Type_str(op->table_info->op_type));
  }

  fprintf(GLOBAL_DEBUG_STREAM, "%2d  %08x  %10s", op->proc_id,
          (uns32)op->inst_info->addr, opcode);

  char buf[MAX_STR_LENGTH + 1];
  print_reg_array(buf, op->inst_info->srcs, op->table_info->num_src_regs);
  fprintf(GLOBAL_DEBUG_STREAM, "  in: %-30s", buf);

  print_reg_array(buf, op->inst_info->dests, op->table_info->num_dest_regs);
  fprintf(GLOBAL_DEBUG_STREAM, "  out: %-30s", buf);

  if(op->oracle_info.mem_size) {
    fprintf(GLOBAL_DEBUG_STREAM, "  %2d @ %08x", op->oracle_info.mem_size,
            (uns32)op->oracle_info.va);
  }

  fprintf(GLOBAL_DEBUG_STREAM, "\n");
}

static int compare_reg_ids(const void* p1, const void* p2) {
  uns16 v1 = *((uns16*)p1);
  uns16 v2 = *((uns16*)p2);
  if(v1 < v2)
    return -1;
  if(v1 > v2)
    return 1;
  return 0;
}

static int print_reg_array(char* buf, Reg_Info* regs, uns num) {
  uns   i;
  char* orig_buf = buf;
  buf[0]         = '\0';  // empty string in case there are zero regs
  // printing sorted array for easy comparison between frontends
  uns16 reg_buf[MAX2(MAX_SRCS, MAX_DESTS)];
  ASSERT(0, num <= MAX2(MAX_SRCS, MAX_DESTS));
  for(i = 0; i < num; ++i)
    reg_buf[i] = regs[i].id;
  qsort(reg_buf, num, sizeof(uns16), compare_reg_ids);
  for(i = 0; i < num; ++i)
    buf += sprintf(buf, " %s", disasm_reg(reg_buf[i]));
  return buf - orig_buf;
}

/**************************************************************************************/
/* print_short_op_array: */

void print_short_op_array(FILE* stream, Op* ops[], uns array_length) {
  uns ii, jj;

  for(ii = 0; ii < NUM_OP_FIELDS; ii++) {
    if(TESTBIT(SHORT_FIELD_MASK, ii)) {
      print_field_head(stream, ii);
      for(jj = 0; jj < array_length; jj++) {
        Op* op = ops[jj];
        print_op_field(stream, op, ii);
      }
      print_field_tail(stream, ii);
    }
  }
}

/**************************************************************************************/
/* disasm_op: */

char* disasm_op(Op* op, Flag wide) {
  static char buf[MAX_STR_LENGTH + 1];

  const char* opcode;
  if(op->table_info->op_type == OP_CF) {
    opcode = cf_type_names[op->table_info->cf_type];
  } else if(op->table_info->op_type == OP_IMEM ||
            op->table_info->op_type == OP_FMEM) {
    opcode = mem_type_names[op->table_info->mem_type];
  } else {
    opcode = Op_Type_str(op->table_info->op_type);
  }

  uns i = 0;
  i += sprintf(&buf[i], "%s", opcode);

  if(wide) {
    i += sprintf(&buf[i], "(");
    i += print_reg_array(&buf[i], op->inst_info->srcs,
                         op->table_info->num_src_regs);
    if(op->table_info->mem_type == MEM_LD && op->oracle_info.mem_size > 0) {
      i += sprintf(&buf[i], " %d@%08x", op->oracle_info.mem_size,
                   (int)op->oracle_info.va);
    }
    if(op->table_info->num_src_regs + op->table_info->num_dest_regs > 0)
      i += sprintf(&buf[i], " ->");
    i += print_reg_array(&buf[i], op->inst_info->dests,
                         op->table_info->num_dest_regs);
    if(op->table_info->mem_type == MEM_ST && op->oracle_info.mem_size > 0) {
      i += sprintf(&buf[i], " %d@%08x", op->oracle_info.mem_size,
                   (int)op->oracle_info.va);
    }
    i += sprintf(&buf[i], " )");
  }

  return buf;
}
