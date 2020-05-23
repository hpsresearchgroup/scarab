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
 * File         : inst_info.h
 * Author       : HPS Research Group
 * Date         : 2/19/2001
 * Description  :
 ***************************************************************************************/

#ifndef __INST_INFO_H__
#define __INST_INFO_H__

#include "ctype_pin_inst.h"
#include "table_info.h"


/**************************************************************************************/
// Defines

#define MAX_SRCS 32  // up to 16 for a gather instruction
#define MAX_DESTS 6


/**************************************************************************************/
// {{{ Reg_Type
typedef enum Reg_Type_enum {
  INT_REG,
  FP_REG,
  SPEC_REG,
  EXTRA_REG,
  NUM_REG_MAPS,
} Reg_Type;
// }}}

/*------------------------------------------------------------------------------------*/
// {{{ Reg_Info
typedef struct Reg_Info_struct {
#if 1
  uns16    reg;   // register number within the register set
  Reg_Type type;  // integer, floating point, extra
#endif
  uns16 id;  // flattened register number (unique across sets)
} Reg_Info;
// }}}


/**************************************************************************************/
// static trace info
typedef struct Trace_Info_struct {
  uns8 inst_size;          // instruction size in x86 instructions
  uns8 num_uop;            // number of uop for x86 instructions
  Flag is_gather_scatter;  // is a gather or scatter instruction
  uns8 load_seq_num;  // sequence number for load uops (0 is the first load, 1
  // the second, etc.)
  uns store_seq_num;  // sequence number for store uops (0 is the first store, 1
                      // the second, etc.)
} Trace_info;


/**************************************************************************************/
// {{{ Inst_Info
// The 'Inst_Info' type is made up of information that is unique to a
// static instruction (eg. address).
// typedef in globals/global_types.h
struct Inst_Info_struct {
  Addr addr;         // address of the instruction
  uns  uop_seq_num;  // static op num used to differentiate ops with same pc
  Table_Info* table_info;  // pointer into the table of static instruction
                           // information

  Reg_Info srcs[MAX_SRCS];    // source register information
  Reg_Info dests[MAX_DESTS];  // destination register information

  int latency;  // The normal latency of this instruction

  Flag trigger_op_fetched_hook;  // if true, the op will trigger the model's
                                 // fetch hook
  int extra_ld_latency;  // extra latency this load instruction should incurr

  struct Trace_Info_struct trace_info;  // trace_info;

  Flag fake_inst;  // is a fake op that PIN execution-driven frontend generates
                   // for handling exceptions and uninstrumented code.
  Wrongpath_Nop_Mode_Reason fake_inst_reason;
};
// }}}

/**************************************************************************************/

#endif /* #ifndef __INST_INFO_H__ */
