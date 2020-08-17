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
 * File         : stage.h
 * Author       : HPS Research Group
 * Date         : 1/26/1999
 * Description  :
 ***************************************************************************************/

#ifndef __STAGE_H__
#define __STAGE_H__

#include "globals/global_types.h"
#include "model.h"

/**************************************************************************************/
/* Defines */
#define EXEC_PORTS_MAX_NAME_LEN 32

/**************************************************************************************/
/* Types */

typedef enum Rob_Block_Issue_Reason_enum {
  ROB_BLOCK_ISSUE_NONE          = 0,
  ROB_BLOCK_ISSUE_FULL          = 7,
  ROB_BLOCK_ISSUE_GAP_TOO_LARGE = 8,
} Rob_Block_Issue_Reason;

typedef enum Rob_Stall_Reason_enum {
  ROB_STALL_NONE              = 0,
  ROB_STALL_OTHER             = 1,
  ROB_STALL_WAIT_FOR_RECOVERY = 2,
  ROB_STALL_WAIT_FOR_REDIRECT = 3,
  ROB_STALL_WAIT_FOR_GAP_FILL = 4,
  ROB_STALL_WAIT_FOR_L1_MISS  = 5,
  ROB_STALL_WAIT_FOR_MEMORY   = 6,
  ROB_STALL_WAIT_FOR_DC_MISS  = 7,
} Rob_Stall_Reason;

typedef struct Stage_Data_struct {
  char* name;         /* name of the stage */
  int   op_count;     /* number of ops in the stage */
  int   max_op_count; /* max value of op_count */
  Op**  ops;          /* array of ops in the stage */
} Stage_Data;


/**************************************************************************************/

#endif /* #ifndef __STAGE_H__ */
