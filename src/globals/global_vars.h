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
 * File         : globals/global_vars.h
 * Author       : HPS Research Group
 * Date         : 2/3/1998
 * Description  : This file is for global variable externs.
 ***************************************************************************************/

#ifndef __GLOBAL_VARS_H__
#define __GLOBAL_VARS_H__

/**************************************************************************************/

#include <stdio.h>
#include "globals/global_types.h"


/**************************************************************************************/

extern Counter  unique_count;
extern Counter* unique_count_per_core;
extern Counter* op_count;
extern Counter* inst_count;
extern Counter  cycle_count;
extern Counter  sim_time;
extern Counter* uop_count;
extern Counter* pret_inst_count;
extern uns      operating_mode;

extern Flag* trace_read_done;
extern Flag* reached_exit;
extern Flag* retired_exit;
extern Flag* sim_done;

extern FILE* mystderr;
extern FILE* mystdout;
extern FILE* mystatus;
extern int   mystatus_fd;

extern Flag frontend_gated;
extern uns  num_fetched_lowconf_brs;

/**************************************************************************************/

#endif /* #ifndef __GLOBAL_VARS_H__ */
