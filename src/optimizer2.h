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
 * File         : optimizer2.h
 * Author       : HPS Research Group
 * Date         : 2/10/2009
 * Description  : A different version of optimizer support (choosing best
 *performing params at run-time for limit studies). This version lets each slave
 *                control when it spawns children.
 ***************************************************************************************/

#ifndef __OPTIMIZER2__
#define __OPTIMIZER2__

#include "globals/global_types.h"

/* Spawns the slaves, calls setup_param_fn(slave_num) on each and returns
 * control to them, spawns master */
void opt2_init(uns n, uns n_to_keep, void (*setup_param_fn)(int));

/* Called by slave once a comparison barrier is reached. Slave may die. */
void opt2_comparison_barrier(double metric);

/* Called by slave when the decision point of the studied adaptive scheme would
 * be reached */
void opt2_decision_point(void);

/* Called by slave when its simulation is complete */
void opt2_sim_complete(void);

/* Is optimizer2 being used? */
Flag opt2_in_use(void);

/* Is current process the "leader" of spawned Scarab processes? This
   function can be used to avoid duplicating output such as
   heartbeats */
Flag opt2_is_leader(void);

#endif
