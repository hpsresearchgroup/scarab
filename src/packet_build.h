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
 * File         : packet_build.h
 * Author       : HPS Research Group
 * Date         : 2/23/2001
 * Description  :
 ***************************************************************************************/

#ifndef __PACKET_BUILD_H__
#define __PACKET_BUILD_H__

#include "thread.h"


/**************************************************************************************/
/* Types */

typedef enum Fu_Type_enum {
  PB_FU_B,     // branch
  PB_FU_IS,    // integer-simple
  PB_FU_IC,    // integer-complex
  PB_FU_I,     // integer {IS, IC}
  PB_FU_FM,    // floating point-multiply
  PB_FU_FD,    // floating point-divide
  PB_FU_FX,    // floating point-other
  PB_FU_F,     // floating point {FM, FD, FX}
  PB_FU_MI,    // memory-integer
  PB_FU_MF,    // memory-floating point
  PB_FU_M,     // memory {MI, MF}
  PB_FU_RB,    // redundant binary
  PB_FU_INRB,  // integer, not redundant binary
  PB_FU_G,     // general {B, I, F, M}
  PB_NUM_FU_TYPES
} Fu_Type;

typedef enum Packet_Break_Condition_enum {
  NUM_CF,
  NUM_LOAD_STORE,
  NUM_RB,      // number of redundant-binary ops per chkpt
  NUM_NON_RB,  // number of MUST BE 2's comp ops per chkpt
  NUM_SLOW_SCHED_OPS,
  NUM_FAST_SCHED_OPS,
  PB_NUM_CONDITIONS
} Packet_Break_Condition;

typedef struct Fu {
  uns32   type;
  Counter max;
  Counter count;
} Fu;

typedef enum Packet_Build_Condition_enum {
  PB_BREAK_DONT,
  PB_BREAK_BEFORE,
  PB_BREAK_AFTER
} Packet_Build_Condition;

typedef enum Packet_Build_Identifier_enum {
  PB_ICACHE,
  PB_OTHER
} Packet_Build_Identifier;

// typedef in globals/global_types.h
struct Pb_Data_struct {
  uns                     proc_id;
  Packet_Build_Identifier pb_ident;
  Fu                      fu_info[PB_NUM_FU_TYPES];
  Counter                 break_conditions[PB_NUM_CONDITIONS];
};


/**************************************************************************************/
// Local types

// don't change this order without fixing stats in fetch.stat.def
typedef enum Break_Reason_enum {
  BREAK_DONT,          // don't break fetch yet
  BREAK_ISSUE_WIDTH,   // break because it's reached maximum issue width
  BREAK_CF,            // break because it's reached maximum control flows
  BREAK_BTB_MISS,      // break because of a btb miss
  BREAK_ICACHE_MISS,   // break because of icache miss
  BREAK_LINE_END,      // break because the current cache line has ended
  BREAK_STALL,         // break because the pipeline is stalled
  BREAK_BARRIER,       // break because of a system call or a fetch barrier
                       // instruction
  BREAK_OFFPATH,       // break because the machine is offpath
  BREAK_ALIGNMENT,     // break because of misaligned fetch (offpath)
  BREAK_TAKEN,         // break because of nonsequential control flow
  BREAK_MODEL_BEFORE,  // break because of model hook
  BREAK_MODEL_AFTER,   // break because of model hook
} Break_Reason;


/**************************************************************************************/
/* Prototypes */

void init_packet_build(Pb_Data*, Packet_Build_Identifier);
void reset_packet_build(Pb_Data*);
Flag packet_build(Pb_Data*, Break_Reason*, Op* const, uns const);


/**************************************************************************************/

#endif /* #ifndef __PACKET_BUILD_H__ */
