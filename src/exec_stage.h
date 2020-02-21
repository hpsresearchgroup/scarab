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
 * File         : exec_stage.h
 * Author       : HPS Research Group
 * Date         : 1/27/1999
 * Description  :
 ***************************************************************************************/

#ifndef __EXEC_STAGE_H__
#define __EXEC_STAGE_H__

#include "stage_data.h"

/**************************************************************************************/
/* Types */

typedef struct Func_Unit_struct {
  uns  proc_id;
  char name[EXEC_PORTS_MAX_NAME_LEN]; /* unique name of the FU, from
                                         exec_ports.def */
  uns32   fu_id; /* id of the FU, corresponds to it's slot number*/
  uns64   type;  /* bitwise-OR of all OP_<type>_BITs that the fu can execute */
  Counter avail_cycle; /* cycle when the functional unit becomes available */
  Counter
       idle_cycle;  /* cycle when the FU becomes idle (no op in its pipeline) */
  Flag held_by_mem; /* when true, the memory system has determined a stall for
                       the func unit */
} Func_Unit;


typedef struct Exec_Stage_struct {
  uns8       proc_id;
  Stage_Data sd; /* stage interface data */

  Func_Unit* fus; /* functional units (dynamically allocated) */

  FILE* fu_util_plot_file;
  uns8  fus_busy; /* for FU util plot and performance prediction, does not
                     include mem stalls */
} Exec_Stage;


/**************************************************************************************/
/* External Variables */

extern Exec_Stage* exec;


/**************************************************************************************/
/* Prototypes */

/* vanilla hps model */
void set_exec_stage(Exec_Stage*);
void init_exec_stage(uns8, const char*);
void reset_exec_stage(void);
void recover_exec_stage(void);
void debug_exec_stage(void);
void update_exec_stage(Stage_Data*);
void finalize_exec_stage(void);

/**************************************************************************************/


#endif /* #ifndef __EXEC_STAGE_H__ */
