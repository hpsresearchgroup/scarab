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
 * File         : table_info.h
 * Author       : HPS Research Group
 * Date         : 2/19/2001
 * Description  :
 ***************************************************************************************/

#ifndef __TABLE_INFO_H__
#define __TABLE_INFO_H__

#include "globals/enum.h"

/**************************************************************************************/
// {{{ Op_Type
// Op_Type is a field of Op that indicates what kind of execution is
// needed for that Op.  It is used for several things, including
// functional unit distribution and latency.  Make sure all types here
// have a latency entry in core.param.def and an entry in inst.stats.def

#define OP_TYPE_LIST(elem)                                            \
  elem(INV) /* invalid opcode*/                                       \
                                                                      \
    elem(NOP) /* is a decoded nop*/                                   \
                                                                      \
    /* these instructions use all integer regs*/                      \
    elem(CF)    /* change of flow*/                                   \
    elem(MOV)   /* move*/                                             \
    elem(CMOV)  /* conditional move*/                                 \
    elem(LDA)   /* load address*/                                     \
    elem(IMEM)  /* int memory instruction*/                           \
    elem(IADD)  /* integer add*/                                      \
    elem(IMUL)  /* integer multiply*/                                 \
    elem(IDIV)  /* integer divide*/                                   \
    elem(ICMP)  /* integer compare*/                                  \
    elem(LOGIC) /* logical*/                                          \
    elem(SHIFT) /* shift*/                                            \
                                                                      \
    /* fmem reads one int reg and writes a fp reg*/                   \
    elem(FMEM) /* fp memory instruction*/                             \
                                                                      \
    /* everything below here is floating point regs only*/            \
    elem(FCVT)  /* floating point convert*/                           \
    elem(FADD)  /* floating point add*/                               \
    elem(FMUL)  /* floating point multiply*/                          \
    elem(FMA)   /* floating point fused multiply-add*/                \
    elem(FDIV)  /* floating point divide*/                            \
    elem(FCMP)  /* floating point compare*/                           \
    elem(FCMOV) /* floating point cond move*/                         \
                                                                      \
    /* all other op types that don't fit existing ops*/               \
    elem(PIPELINED_FAST)         /* i.e. <=2 cycles, pipelined */     \
    elem(PIPELINED_MEDIUM)       /* i.e. <=5 cycles, pipelined */     \
    elem(PIPELINED_SLOW)         /* i.e. > 5 cycles, pipelined */     \
    elem(NOTPIPELINED_MEDIUM)    /* i.e. <=5 cycles, not pipelined */ \
    elem(NOTPIPELINED_SLOW)      /* i.e. >5 cycles, not pipelined */  \
    elem(NOTPIPELINED_VERY_SLOW) /* i.e. >50 cycles, not pipelined */

DECLARE_ENUM(Op_Type, OP_TYPE_LIST, OP_);

#define NUM_OP_TYPES OP_NUM_ELEMS
// }}}

/* Set up op type bit definitions */
#define OP_TYPE_BIT_DEF(op_type) OP_##op_type##_BIT = 1 << OP_##op_type,
enum { OP_TYPE_LIST(OP_TYPE_BIT_DEF) };
#undef OP_TYPE_BIT_DEF

/*------------------------------------------------------------------------------------*/
// {{{ Mem_Type
// Mem_Type breaks down the different types of memory operations into
// loads, stores, and prefetches.

typedef enum Mem_Type_enum {
  NOT_MEM,    // not a memory instruction
  MEM_LD,     // a load instruction
  MEM_ST,     // a store instruction
  MEM_PF,     // a prefetch
  MEM_WH,     // a write hint
  MEM_EVICT,  // a cache block eviction hint
  NUM_MEM_TYPES,
} Mem_Type;
// }}}

/*------------------------------------------------------------------------------------*/
// {{{ Cf_Type
// Cf_Type breaks down the different types of control flow instructions.

typedef enum Cf_Type_enum {
  NOT_CF,   // not a control flow instruction
  CF_BR,    // an unconditional branch
  CF_CBR,   // a conditional branch
  CF_CALL,  // a call
  // below this point are indirect cfs
  CF_IBR,    // an indirect branch
  CF_ICALL,  // an indirect call
  CF_ICO,    // an indirect jump to co-routine
  CF_RET,    // a return
  CF_SYS,    // a system call
  NUM_CF_TYPES,
} Cf_Type;
// }}}

/*------------------------------------------------------------------------------------*/
// {{{ Bar_Type
// Bar_Type breaks denotes different kinds of barriers that can be
// recognized by the engine. Use these as a mask to allow an
// instruction to have multiple barrier types.

typedef enum Bar_Type_enum {
  NOT_BAR   = 0x0,  // not a barrier-causing instruction
  BAR_FETCH = 0x1,  // causes fetch to halt until a redirect occurs
  BAR_ISSUE = 0x2,  // causes issue to serialize around the instruction
} Bar_Type;
// }}}

/**************************************************************************************/
// {{{ Table_Info
/* The 'Table_Info' type is the static information associated with an
 * instruction. */

struct Op_struct;
struct Op_Info_struct;
struct Inst_Info_struct;

// typedef in globals/global_types.h
struct Table_Info_struct {
  Op_Type  op_type;   // type of operation
  Mem_Type mem_type;  // type of memory instruction
  Cf_Type  cf_type;   // type of control flow instruction
  Bar_Type bar_type;  // type of barrier caused by instruction

  Flag has_lit;        // does it have a literal? (only integer operates can)
  uns  num_dest_regs;  // number of destination registers written
  uns  num_src_regs;   // number of source registers read

  Flag is_simd;         // Is it a SIMD opcode (even if it is a scalar operation
                        // like MOVSD)
  uns8 num_simd_lanes;  // Number of data parallel operations in the
                        // instruction. For non-SIMD instructions, this is 1.
  uns8 lane_width_bytes;  // Operand width of each SIMD lane.
                          // For non-SIMD instructions, this is still set.

  uns mem_size;  // number of bytes read/written by a memory instruction

  Binary op_func;   // the 6-bit opcode plus the function code
  char   name[16];  // Mnemonic of the instruction
  uns8   type;      // the format type code for the instruction (see table)
  uns32  mask;

  // Function that decodes the instruction fields
  Flag (*dec_func)(Inst_Info*);
  // Function that sources the register values for the oracle
  void (*src_func)(Op_Info*);
  // Function that simulates the instruction
  void (*sim_func)(Op_Info*, Flag);

  uns8 qualifiers;  // Floating point qualifier bit string (/d, /s, /ud, etc.)
};
// }}}

/**************************************************************************************/

#endif /* #ifndef __TABLE_INFO_H__ */
