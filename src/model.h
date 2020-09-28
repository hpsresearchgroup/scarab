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
 * File         : model.h
 * Author       : HPS Research Group
 * Date         : 12/6/1998
 * Description  :
 ***************************************************************************************/

#ifndef __MODEL_H__
#define __MODEL_H__

#include "globals/global_types.h"
#include "packet_build.h"


/**************************************************************************************/
/* Types */

typedef enum Model_Id_enum {
  CMP_MODEL,
  DUMB_MODEL,
  NUM_MODELS,
} Model_Id;


typedef enum Model_Mem_enum {
  MODEL_MEM,
  NUM_MODEL_MEMS,
} Model_Mem;


typedef struct Model_struct {
  Model_Id    id;
  Model_Mem   mem;
  const char* name;

  /* these functions are called by the mode functions in sim.c */
  void (*init_func)(uns mode); /* called to initialize data structures before
                                  warmup and main simulation loop */
  void (*reset_func)(void);    /* called at the end of a sample to clear model
                                  state */
  void (*cycle_func)(void);    /* called once each cycle */
  void (*debug_func)(void);    /* called after the cycle_func when debugging
                                  conditions are true */
  void (*per_core_done_func)(
    uns8); /* called simulation before stats are dumped (may be NULL) */
  void (*done_func)(void); /* called after the main loop terminates (may be
                              NULL) */

  /* these are general hook functions for various processor events
     that can be handled differently by different models */
  void (*wake_hook)(Op*, Op*, uns8);
  enum Break_Reason_enum (*break_hook)(Op*);
  void (*op_fetched_hook)(Op*);
  void (*op_retired_hook)(Op*);  // called just before the op is freed
  void (*warmup_func)(Op* op);   /* called for warmup(may be NULL) */

  /*      void (*l0_cache_miss_hook)      (Op *); */
  /*      void (*resolve_mispredict_hook) (Op *); */
} Model;


/**************************************************************************************/
/* Global Variables */

extern struct Model_struct model_table[];
extern Model*              model;


/**************************************************************************************/

#endif /* #ifndef __MODEL_H__ */
