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
 * File         : sim.h
 * Author       : HPS Research Group
 * Date         : 11/10/1997
 * Description  :
 ***************************************************************************************/

#ifndef __SIM_H__
#define __SIM_H__

/**************************************************************************************/
/* Types */

enum sim_mode_enum { UOP_SIM_MODE, FULL_SIM_MODE, NUM_SIM_MODES };

enum operating_mode_enum {
  SIMULATION_MODE,
  WARMUP_MODE /* not implemented yet */
};

enum exit_cond_enum { LAST_DONE, FIRST_DONE, NUM_EXIT_CONDS };

/**************************************************************************************/
/* External Variables */

extern const char* sim_mode_names[];
extern Counter*    inst_limit;


/**************************************************************************************/
/* Prototypes */

void init_global(char* [], char* []);
void uop_sim(void);
void monitor_sim(void);
void sampling_sim(void);
void full_sim(void);
void handle_SIGINT(int);
void close_output_streams(void);


/**************************************************************************************/

#endif /* #ifndef __SIM_H__ */
