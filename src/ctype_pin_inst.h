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
 * File         : ctype_pin_inst.h
 * Author       : HPS Research Group
 * Date         : 11/10/2006
 * Description  :
 ***************************************************************************************/

#ifndef CTYPE_PIN_INST_H_SEEN
#define CTYPE_PIN_INST_H_SEEN

#include <inttypes.h>

#define MAX_SRC_REGS_NUM 8
#define MAX_DST_REGS_NUM 8
#define MAX_MEM_ADDR_REGS_NUM 2
#define MAX_LD_NUM 16
#define MAX_ST_NUM 16

typedef uint8_t compressed_reg_t;

enum MemHint {
  NONE,
  NT,  // non-temporal
  T0,
  T1,
  T2,
  W,
  WT1,
  EXCLUSIVE,
  RESERVED
};

typedef enum Wrongpath_Nop_Mode_Reason_enum {
  WPNM_NOT_IN_WPNM,
  WPNM_REASON_REDIRECT_TO_NOT_INSTRUMENTED,
  WPNM_REASON_RETURN_TO_NOT_INSTRUMENTED,
  WPNM_REASON_NONRET_CF_TO_NOT_INSTRUMENTED,
  WPNM_REASON_NOT_TAKEN_TO_NOT_INSTRUMENTED,
  WPNM_REASON_WRONG_PATH_STORE_TO_NEW_REGION,
  WPNM_NUM_REASONS
} Wrongpath_Nop_Mode_Reason;

typedef struct ctype_pin_inst_struct {
  /* static information */

  uint64_t inst_uid;  // unique ID produced by the frontend

  uint64_t instruction_addr;  // 8 bytes
  uint8_t  size;              // 5 bits
  // uint8_t opcode;           // 6 bits
  uint8_t op_type;  // 6 bits
  uint8_t cf_type;  // 4 bits
  uint8_t is_fp;    // 1 bit

  uint8_t num_src_regs;
  uint8_t num_dst_regs;
  uint8_t num_ld1_addr_regs;
  uint8_t num_ld2_addr_regs;
  uint8_t num_st_addr_regs;

  compressed_reg_t src_regs[MAX_SRC_REGS_NUM];
  compressed_reg_t dst_regs[MAX_DST_REGS_NUM];
  compressed_reg_t ld1_addr_regs[MAX_MEM_ADDR_REGS_NUM];
  compressed_reg_t ld2_addr_regs[MAX_MEM_ADDR_REGS_NUM];
  compressed_reg_t st_addr_regs[MAX_MEM_ADDR_REGS_NUM];

  uint8_t num_simd_lanes;  // Number of data parallel operations in the
                           // instruction. For non-SIMD instructions, this is 1.
  uint8_t lane_width_bytes;  // Operand width of each SIMD lane.
                             // For non-SIMD instructions, this is still set.

  uint8_t num_ld;
  uint8_t num_st;

  uint8_t has_immediate;  // 1 bit

  /* dynamic information */

  uint64_t ld_vaddr[MAX_LD_NUM];
  uint64_t st_vaddr[MAX_ST_NUM];
  uint8_t  ld_size;
  uint8_t  st_size;

  uint64_t branch_target;  // not the dynamic info. static info  // 8 bytes
  uint8_t  actually_taken : 1;
  uint8_t  is_string : 1;
  uint8_t  is_call : 1;
  uint8_t  is_move : 1;
  uint8_t  is_prefetch : 1;
  uint8_t  has_push : 1;
  uint8_t  has_pop : 1;
  uint8_t  is_ifetch_barrier : 1;
  uint8_t  is_lock : 1;
  uint8_t  is_repeat : 1;
  uint8_t  is_simd : 1;
  uint8_t  is_gather_scatter : 1;
  uint8_t  is_sentinel : 1;
  uint8_t  fake_inst : 1;
  uint8_t  exit : 1;

  Wrongpath_Nop_Mode_Reason fake_inst_reason;
  uint64_t instruction_next_addr;  // the original trace does not have this
                                   // information

  char pin_iclass[16];

} __attribute__((packed)) ctype_pin_inst;

typedef ctype_pin_inst compressed_op;

#endif
