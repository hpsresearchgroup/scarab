/* Copyright 2020 University of California Santa Cruz
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
 * File         : pin/pin_lib/pin_api_to_xed.h
 * Author       : Heiner Litz
 * Date         : 05/15/2020
 * Notes        : This code has been adapted from zsim which was released under
 *                GNU General Public License as published by the Free Software
 *                Foundation, version 2.
 * Description  :
 ***************************************************************************************/

/*
 * Wrapper file to enable support for different instruction decoders.
 * Currently we support PIN (execution driven) and Intel xed (trace driven)
 */

#ifndef THIRD_PARTY_ZSIM_SRC_WRAPPED_PIN_H_
#define THIRD_PARTY_ZSIM_SRC_WRAPPED_PIN_H_

extern "C" {
#include "xed-interface.h"
}

enum class CustomOp : uint8_t { NONE, PREFETCH_CODE };

struct InstInfo {
  uint64_t                  pc;           // instruction address
  const xed_decoded_inst_t* ins;          // XED info
  uint64_t                  pid;          // process ID
  uint64_t                  tid;          // thread ID
  uint64_t                  target;       // branch target
  uint64_t                  mem_addr[2];  // memory addresses
  bool                      mem_used[2];  // mem address usage flags
  CustomOp                  custom_op;    // Special or non-x86 ISA instruction
  bool                      taken;        // branch taken
  bool unknown_type;  // No available decode info (presents a nop)
  bool valid;         // True until the end of the sequence
};

#define XED_OP_NAME(ins, op) \
  xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(ins), op))

#define XED_INS_Nop(ins) (XED_INS_Category(ins)) == XED_CATEGORY_NOP || XED_INS_Category(ins) == XED_CATEGORY_WIDENOP)
#define XED_INS_LEA(ins) (XED_INS_Opcode(ins)) == XO(LEA))
#define XED_INS_Opcode(ins) xed_decoded_inst_get_iclass(ins)
#define XED_INS_Category(ins) xed_decoded_inst_get_category(ins)
#define XED_INS_IsAtomicUpdate(ins) xed_decoded_inst_get_attribute(ins), XED_ATTRIBUTE_LOCKED)
// FIXME: Check if REPs are translated correctly
#define XED_INS_IsRep(ins) xed_decoded_inst_get_attribute(ins), XED_ATTRIBUTE_REP)
#define XED_INS_HasRealRep(ins)    \
  xed_operand_values_has_real_rep( \
    xed_decoded_inst_operands((xed_decoded_inst_t*)ins))
#define XED_INS_OperandCount(ins) xed_decoded_inst_noperands(ins)
#define XED_INS_OperandIsImmediate(ins, op) XED_IS_IMM(ins, op)
#define XED_INS_OperandRead(ins, op) \
  xed_operand_read(xed_inst_operand(xed_decoded_inst_inst(ins), op))
#define XED_INS_OperandWritten(ins, op)     \
  XED_INS_OperandIsMemory(ins, op) ?        \
    xed_decoded_inst_mem_written(ins, op) : \
    xed_operand_written(xed_inst_operand(xed_decoded_inst_inst(ins), op))
#define XED_INS_OperandReadOnly(ins, op) \
  (XED_INS_OperandRead(ins, op) && !XED_INS_OperandWritten(ins, op))
#define XED_INS_OperandWrittenOnly(ins, op) \
  (!XED_INS_OperandRead(ins, op) && XED_INS_OperandWritten(ins, op))
#define XED_INS_OperandIsReg(ins, op) \
  xed_operand_is_register(            \
    xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(ins), op)))
#define XED_INS_OperandIsMemory(ins, op) XED_MEM(ins, op)
//((xed_decoded_inst_mem_read(ins, op) | xed_decoded_inst_mem_written(ins, op)))
#define XED_INS_IsMemory(ins) (xed_decoded_inst_number_of_memory_operands(ins))
#define XED_INS_OperandReg(ins, op) \
  xed_decoded_inst_get_reg(         \
    ins, xed_operand_name(xed_inst_operand(xed_decoded_inst_inst(ins), op)))
#define XED_INS_OperandMemoryBaseReg(ins, op) \
  xed_decoded_inst_get_base_reg(ins, op)
#define XED_INS_OperandMemoryIndexReg(ins, op) \
  xed_decoded_inst_get_index_reg(ins, op)
//#define XED_INS_OperandMemoryDisplacement(ins, op)
// xed_operand_values_get_displacement_for_memop(ins, op)
#define XED_INS_OperandMemoryScale(ins, op) (XED_INS_OperandWidth(ins, op) >> 3)
#define XED_INS_LockPrefix(ins) \
  xed_decoded_inst_get_attribute(ins, XED_ATTRIBUTE_LOCKED)
#define XED_INS_OperandWidth(ins, op) \
  xed_decoded_inst_operand_length_bits(ins, op)
#define XED_INS_MemoryOperandIsRead(ins, op) XED_MEM_READ(ins, op)
#define XED_INS_MemoryOperandIsWritten(ins, op) XED_MEM_WRITTEN(ins, op)
#define XED_INS_MemoryOperandCount(ins) \
  xed_decoded_inst_number_of_memory_operands(ins)
#define XED_INS_IsDirectBranch(ins) xed3_operand_get_brdisp_width(ins)
#define XED_INS_Size(ins) xed_decoded_inst_get_length(ins)
#define XED_INS_Valid(ins) xed_decoded_inst_valid(ins)
/* Just like PIN we break BBLs on a number of additional instructions such as
 * REP */
#define XED_INS_ChangeControlFlow(ins) (XED_INS_Category(ins) == XC(COND_BR) || XED_INS_Category(ins) == XC(UNCOND_BR) || XED_INS_Category(ins) == XC(CALL) || XED_INS_Category(ins) == XC(RET) || XED_INS_Category(ins) == XC(SYSCALL) || XED_INS_Category(ins) == XC(SYSRET) || XED_INS_Opcode(ins) == XO(CPUID) || XED_INS_Opcode(ins) == XO(POPF) || XED_INS_Opcode(ins) == XO(POPFD) || XED_INS_Opcode(ins) == XO(POPFQ) || XED_INS_IsRep(ins)
#define REG_FullRegName(reg) xed_get_largest_enclosing_register(reg)

#define XED_INS_Mnemonic(ins) \
  std::string(xed_iclass_enum_t2str(xed_decoded_inst_get_iclass(ins)))
#define XED_IS_REG(ins, op)                    \
  (XED_OP_NAME(ins, op) >= XED_OPERAND_REG0 && \
   XED_OP_NAME(ins, op) <= XED_OPERAND_REG8)
#define XED_MEM(ins, op)                       \
  (XED_OP_NAME(ins, op) == XED_OPERAND_MEM0 || \
   XED_OP_NAME(ins, op) == XED_OPERAND_MEM1)
#define XED_LEA(ins, op) (XED_OP_NAME(ins, op) == XED_OPERAND_AGEN)
#define XED_IS_IMM(ins, op)                    \
  (XED_OP_NAME(ins, op) == XED_OPERAND_IMM0 || \
   XED_OP_NAME(ins, op) == XED_OPERAND_IMM1)
#define XED_MEM_READ(ins, op) xed_decoded_inst_mem_read(ins, op)
#define XED_MEM_WRITTEN(ins, op) xed_decoded_inst_mem_written(ins, op)
#define XED_INS_MemoryReadSize(ins, op) \
  xed_decoded_inst_get_memory_operand_length(ins, op)
#define XED_INS_MemoryWriteSize(ins, op) XED_INS_MemoryReadSize(ins, op)
#define XED_INS_IsRet(ins) (XED_INS_Category(ins) == XED_CATEGORY_RET)
// TODO: Double check that below works calls and branches
#define XED_INS_IsDirectBranchOrCall(ins) XED_INS_IsDirectBranch(ins)
#define XED_INS_IsIndirectBranchOrCall(ins) !XED_INS_IsDirectBranchOrCall(ins)
#define XED_INS_IsSyscall(ins) (XED_INS_Category(ins) == XED_CATEGORY_SYSCALL)
#define XED_INS_IsSysret(ins) (XED_INS_Category(ins) == XED_CATEGORY_SYSRET)
#define XED_INS_IsInterrupt(ins) \
  (XED_INS_Category(ins) == XED_CATEGORY_INTERRUPT)

#define XED_INS_IsVgather(ins) (XED_INS_Category(ins) == XED_CATEGORY_GATHER)
#define XED_INS_IsVscatter(ins) (XED_INS_Category(ins) == XED_CATEGORY_SCATTER)
#define XED_REG_Size(reg) (xed_get_register_width_bits(reg) >> 3)
#define XED_REG_is_xmm_ymm_zmm(reg)           \
  (xed_reg_class(reg) == XED_REG_CLASS_XMM || \
   xed_reg_class(reg) == XED_REG_CLASS_YMM || \
   xed_reg_class(reg) == XED_REG_CLASS_ZMM)
#define XED_REG_is_xmm(reg) (xed_reg_class(reg) == XED_REG_CLASS_XMM)
#define XED_REG_is_ymm(reg) (xed_reg_class(reg) == XED_REG_CLASS_YMM)
// TODO: Check if mask is sufficient or if we need to check for K-mask reg
#define XED_REG_is_k_mask(reg) (xed_reg_class(reg) == XED_REG_CLASS_MASK)
#define XED_REG_is_gr32(reg) (xed_reg_class(reg) == XED_REG_CLASS_GPR32)
#define XED_REG_is_gr64(reg) (xed_reg_class(reg) == XED_REG_CLASS_GPR64)
// TODO: What is the difference between PINs REG_Size and REG_Width?
#define XED_REG_Width(reg) xed_get_register_width_bits(reg)
#define XED_REG_valid(reg) (xed_reg_class(reg) != XED_REG_CLASS_INVALID)
#define XED_REG_StringShort(reg) xed_reg_enum_t2str(reg)
#define XED_CATEGORY_StringShort(cat) xed_category_enum_t2str(cat)
#define XED_REG_INVALID() XED_REG_INVALID
#define XED_REGWIDTH_128 128
#define XED_REGWIDTH_256 256
#define XED_REGWIDTH_512 512

//#define REG_GR_BASE REG_RDI
// Used in decoder.h. zsim (pin) requires REG_LAST whereas zsim_trace requires
// XED_REG_LAST #define REG_LAST XED_REG_LAST

// XED expansion macros (enable us to type opcodes at a reasonable speed)
#define XC(cat) (XED_CATEGORY_##cat)
#define XO(opcode) (XED_ICLASS_##opcode)

#define ADDRINT uint64_t
#define THREADID uint32_t

//#define BOOL bool

#endif  // THIRD_PARTY_ZSIM_SRC_WRAPPED_PIN_H_
