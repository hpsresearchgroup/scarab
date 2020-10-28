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
 * File         : exec_ports.c
 * Author       : HPS Research Group
 * Date         : 1/9/18
 * Description  : Defines the macros used in the exec_ports.def file
 ***************************************************************************************/


#include <stdio.h>
#include "core.param.h"
#include "exec_stage.h"
#include "general.param.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/utils.h"
#include "node_stage.h"
#include "table_info.h"

#include "exec_ports.h"
#include "stat_trace.h"

/**************************************************************************************/
/* Macros */

// Each op_type can have non-simd and simd versions
#define FU_TYPE_WIDTH (2 * NUM_OP_TYPES)

/**************************************************************************************/
/* Global Variables */
uns POWER_TOTAL_RS_SIZE     = 0;
uns POWER_TOTAL_INT_RS_SIZE = 0;
uns POWER_TOTAL_FP_RS_SIZE  = 0;
uns POWER_NUM_ALUS          = 0;
uns POWER_NUM_MULS_AND_DIVS = 0;
uns POWER_NUM_FPUS          = 0;

/**************************************************************************************/
/* Local Function Prototypes */
void init_exec_ports_fu_list(uns, Func_Unit*);
void init_exec_ports_rs_list(uns, Reservation_Station*, Func_Unit*);
Flag parse_next_elt(char*, uns64*);
Flag is_fpu_type(uns64 fu_type);
Flag is_mul_or_div_type(uns64 fu_type);
Flag is_alu_type(uns64 fu_type);
void power_count_fu_types(uns64 fu_type);
void power_calc_instruction_window_size(Reservation_Station* rs);

/**************************************************************************************/
/* Local Function Definitions */

Flag parse_next_elt(char* str, uns64* type) {
  char* temp = strtok(str, DELIMITERS);
  *type      = -1;
  if(temp) {
    // token was found
    switch(temp[0]) {
      case 'x': /*Hex*/
        ASSERTM(
          node->proc_id, strlen(temp) > 1,
          "Hex numbers must start with x and have at least one hex-digit.\n");
        *type = (uns64)strtoul(&temp[1], NULL, 16);
        return TRUE;
      case 'b': /*Binary*/
        ASSERTM(
          node->proc_id, strlen(temp) > 1,
          "Binary numbers must start with b and have at least one bit.\n");
        *type = (uns64)strtoul(&temp[1], NULL, 2);
        return TRUE;
      default: /*Decimal*/
        ASSERTM(node->proc_id, strlen(temp) > 0,
                "Decimal numbers must have at least one digit.\n");
        *type = (uns64)strtoul(&temp[0], NULL, 10);
        return TRUE;
    }
  }
  return FALSE;
}

Flag is_fpu_type(uns64 fu_type) {
  uns64 fpu_ops = get_fu_type(OP_FCVT, FALSE) | get_fu_type(OP_FADD, FALSE) |
                  get_fu_type(OP_FMUL, FALSE) | get_fu_type(OP_FMA, FALSE) |
                  get_fu_type(OP_FDIV, FALSE) | get_fu_type(OP_FCMP, FALSE) |
                  get_fu_type(OP_FCMOV, FALSE);

  return (fpu_ops & fu_type) != 0;
}

Flag is_mul_or_div_type(uns64 fu_type) {
  uns64 mul_or_div_ops = get_fu_type(OP_IMUL, FALSE) |
                         get_fu_type(OP_IDIV, FALSE) |
                         get_fu_type(OP_NOTPIPELINED_SLOW, FALSE) |
                         get_fu_type(OP_NOTPIPELINED_VERY_SLOW, FALSE);

  return (mul_or_div_ops & fu_type) != 0;
}

Flag is_alu_type(uns64 fu_type) {
  uns64 alu_ops = get_fu_type(OP_CF, FALSE) | get_fu_type(OP_MOV, FALSE) |
                  get_fu_type(OP_CMOV, FALSE) | get_fu_type(OP_LDA, FALSE) |
                  get_fu_type(OP_IADD, FALSE) | get_fu_type(OP_ICMP, FALSE) |
                  get_fu_type(OP_LOGIC, FALSE) | get_fu_type(OP_SHIFT, FALSE) |
                  get_fu_type(OP_GATHER, FALSE) |
                  get_fu_type(OP_SCATTER, FALSE) |
                  get_fu_type(OP_PIPELINED_FAST, FALSE) |
                  get_fu_type(OP_PIPELINED_MEDIUM, FALSE) |
                  get_fu_type(OP_PIPELINED_SLOW, FALSE) |
                  get_fu_type(OP_NOTPIPELINED_MEDIUM, FALSE);

  return (alu_ops & fu_type) != 0;
}

Power_FU_Type power_get_fu_type(Op_Type op_type, Flag is_simd) {
  if(is_alu_type(get_fu_type(op_type, is_simd)))
    return POWER_FU_ALU;
  if(is_mul_or_div_type(get_fu_type(op_type, is_simd)))
    return POWER_FU_MUL_DIV;
  if(is_fpu_type(get_fu_type(op_type, is_simd)))
    return POWER_FU_FPU;
  return POWER_FU_ALU; /*should never happen*/
}

void power_count_fu_types(uns64 fu_type) {
  if(is_fpu_type(fu_type))
    POWER_NUM_FPUS++;
  if(is_mul_or_div_type(fu_type))
    POWER_NUM_MULS_AND_DIVS++;
  if(is_alu_type(fu_type))
    POWER_NUM_ALUS++;
}

void power_calc_instruction_window_size(Reservation_Station* rs) {
  uns32 fu_idx;
  Flag  connected_to_fp  = FALSE;
  Flag  connected_to_int = FALSE;

  POWER_TOTAL_RS_SIZE += rs->size;
  for(fu_idx = 0; fu_idx < rs->num_fus; ++fu_idx) {
    if(is_fpu_type(rs->connected_fus[fu_idx]->type)) {
      connected_to_fp = TRUE;
    }

    if(is_alu_type(rs->connected_fus[fu_idx]->type) ||
       is_mul_or_div_type(rs->connected_fus[fu_idx]->type)) {
      connected_to_int = TRUE;
    }

    if(connected_to_fp && connected_to_int)
      break;
  }

  if(connected_to_fp) {
    POWER_TOTAL_FP_RS_SIZE += rs->size;
  }
  if(connected_to_int) {
    POWER_TOTAL_INT_RS_SIZE += rs->size;
  }
}

void init_exec_ports_fu_list(uns proc_id, Func_Unit* fu) {
  uns32      i;
  uns64      next_type;
  const char base_name[] = "EU";
  ASSERT(proc_id, strlen(base_name) + 5 < EXEC_PORTS_MAX_NAME_LEN);
  uns64 check_fu_type = 0;


  char* fu_types_copy = strdup(FU_TYPES);
  Flag  tmp           = parse_next_elt(fu_types_copy, &next_type);
  for(i = 0; i < NUM_FUS; ++i, tmp = parse_next_elt(NULL, &next_type)) {
    ASSERT(proc_id, FU_TYPE_WIDTH <= sizeof(fu[i].type) * CHAR_BIT);
    fu[i].fu_id = i;
    fu[i].type  = (next_type == 0) ? N_BIT_MASK(FU_TYPE_WIDTH) :
                                    next_type;  // zero means all ops
    check_fu_type |= fu[i].type;  // accumulating all types from all FUs to make
                                  // sure every op is covered
    ASSERTM(proc_id, tmp, "Found less FU_TYPES than expected\n");
    fu[i].proc_id = proc_id;
    ASSERTM(proc_id, i < 99999,
            "Only 5 digits allocated for name (see above allocation of "
            "basename and subsequent assert)");
    sprintf(fu[i].name, "%s%d", base_name, i);

    power_count_fu_types(fu[i].type);
  }
  ASSERTM(proc_id, tmp == FALSE, "Found more FU_TYPES than expected\n");
  ASSERTM(proc_id, check_fu_type == N_BIT_MASK(FU_TYPE_WIDTH),
          "FU types do not cover all possible ops");
  free(fu_types_copy);
}

void init_exec_ports_rs_list(uns proc_id, Reservation_Station* rs,
                             Func_Unit* local_fus) {
  uns32       i;
  uns64       next;
  const char* base_name = "RS";
  ASSERT(proc_id, strlen(base_name) + 5 < EXEC_PORTS_MAX_NAME_LEN);

  char* rs_sizes_copy     = strdup(RS_SIZES);
  Flag  tmp               = parse_next_elt(rs_sizes_copy, &next);
  POWER_TOTAL_RS_SIZE     = 0;
  POWER_TOTAL_INT_RS_SIZE = 0;
  POWER_TOTAL_FP_RS_SIZE  = 0;
  for(i = 0; i < NUM_RS; ++i, tmp = parse_next_elt(NULL, &next)) {
    rs[i].proc_id = proc_id;
    ASSERTM(proc_id, i < 99999,
            "Only 5 digits allocated for name (see above allocation of "
            "basename and subsequent assert)");
    sprintf(rs[i].name, "%s%d", base_name, i);
    ASSERTM(proc_id, tmp, "Found less RS_SIZES than expected\n");
    rs[i].size = next;
  }
  ASSERTM(proc_id, tmp == FALSE, "Found more RS_SIZES than expected\n");
  free(rs_sizes_copy);

  char* rs_connections_copy = strdup(RS_CONNECTIONS);
  tmp                       = parse_next_elt(rs_connections_copy, &next);
  for(i = 0; i < NUM_RS; ++i, tmp = parse_next_elt(NULL, &next)) {
    ASSERTM(proc_id, tmp, "Found less RS_CONNECTIONS than expected\n");

    // zero means fully connected
    if(next == 0) {
      next = (NUM_FUS == 64) ? N_BIT_MASK_64 : N_BIT_MASK(NUM_FUS);
    }
    ASSERTM(proc_id, NUM_FUS <= 64,
            "NUM_FUS cannot exceed 64 (using a 64 bit int for bitmask)\n");

    int32 num_fus     = 0;
    int32 num_fus_pre = __builtin_popcount(
      next);  // count the number of
              // connections (number of bits
              // set) in the bit vector.
    rs[i].connected_fus = (Func_Unit**)malloc(sizeof(Func_Unit*) * num_fus_pre);
    ASSERTM(proc_id, rs[i].connected_fus,
            "Malloc is failing is exec_ports.c\n");

    int32 idx = __builtin_ffs(next);  // Find the first set bit
    while(idx) {                      // decode the connections bit vector
      idx = idx - 1;                  // built in returns 1 + the true index
      ASSERTM(proc_id, idx < NUM_FUS,
              "Attempted connections with an FU that does not exist\n");
      rs[i].connected_fus[num_fus] = &local_fus[idx];
      num_fus++;
      next = next & ~(0x01 << idx);  // Clear the bit we just connected
      idx  = __builtin_ffs(next);    // Find the next set bit
    }
    rs[i].num_fus = num_fus;
    ASSERTM(
      proc_id, num_fus <= NUM_FUS,
      "RS must be connected to less than or equal the total number of FUs\n");
    ASSERTM(proc_id,
            num_fus == num_fus_pre || (num_fus_pre == 0 && num_fus == NUM_FUS),
            "Decoded a different number of connections that was counted before "
            "the loop\n");

    power_calc_instruction_window_size(&rs[i]);
  }
  ASSERTM(proc_id, tmp == FALSE, "Found more RS_CONNECTIONS than expected\n");
  free(rs_connections_copy);
}

// Note: this function must be called *after* init_node_stage and
// init_exec_stage.
void init_exec_ports(uns8 proc_id, const char* name) {
  ASSERTM(proc_id, proc_id == node->proc_id,
          "%s and Node Stage must be from same proc!\n", name);
  ASSERTM(proc_id, proc_id == exec->proc_id,
          "%s and Exec Stage must be from same proc!\n", name);
  ASSERTM(proc_id, node, "Node Stage must be allocated\n");
  ASSERTM(proc_id, exec, "Exec Stage must be allocated\n");

  exec->fus = (Func_Unit*)calloc(NUM_FUS, sizeof(Func_Unit));
  init_exec_ports_fu_list(proc_id, exec->fus);

  node->rs = (Reservation_Station*)calloc(NUM_RS, sizeof(Reservation_Station));
  init_exec_ports_rs_list(proc_id, node->rs, exec->fus);
}

uns64 get_fu_type(Op_Type op_type, Flag is_simd) {
  return (1ull << op_type) << (is_simd ? NUM_OP_TYPES : 0);
}
