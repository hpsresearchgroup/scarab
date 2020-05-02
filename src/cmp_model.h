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

#ifndef __CMP_MODEL_H__
#define __CMP_MODEL_H__

#include "bp/bp.h"
#include "cmp_model_support.h"
#include "dcache_stage.h"
#include "decode_stage.h"
#include "exec_ports.h"
#include "exec_stage.h"
#include "icache_stage.h"
#include "map.h"
#include "map_stage.h"
#include "memory/memory.h"
#include "node_stage.h"
#include "thread.h"

/**************************************************************************************/
/* cmp model data  */

typedef struct Cmp_Model_struct {
  Thread_Data* thread_data;  // cmp: one thread for each core,
  // "single_td" in sim.c is only for single core

  Pb_Data* pb_data;

  Map_Data*         map_data;
  Bp_Recovery_Info* bp_recovery_info;
  Bp_Data*          bp_data;

  Memory memory;

  Icache_Stage* icache_stage;
  Decode_Stage* decode_stage;
  Map_Stage*    map_stage;
  Node_Stage*   node_stage;
  Exec_Stage*   exec_stage;
  Dcache_Stage* dcache_stage;

  uns window_size;

} Cmp_Model;

/**************************************************************************************/
/* Global vars */

Cmp_Model        cmp_model;
extern Cmp_Model cmp_model;

/**************************************************************************************/
/* Prototypes */

void cmp_init(uns mode);
void cmp_reset(void);
void cmp_cycle(void);
void cmp_debug(void);
void cmp_per_core_done(uns8);
void cmp_done(void);
void cmp_wake(Op*, Op*, uns8);
void cmp_retire_hook(Op*);
void cmp_warmup(Op*);

/**************************************************************************************/

#endif /* #ifndef __CMP_MODEL_H__ */
