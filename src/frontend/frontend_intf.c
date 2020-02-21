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
 * File         : frontend/frontend_intf.c
 * Author       : HPS Research Group
 * Date         :
 * Description  :
 ***************************************************************************************/

#include "frontend/frontend_intf.h"
#include "general.param.h"
#include "globals/global_defs.h"

/* Include headers of all the implementations here */
#include "frontend/pin_exec_driven_fe.h"
#include "frontend/pin_trace_fe.h"

Frontend_Impl frontend_table[] = {
#define FRONTEND_IMPL(id, name, prefix) \
  {name,                                \
   prefix##_next_fetch_addr,            \
   prefix##_can_fetch_op,               \
   prefix##_fetch_op,                   \
   prefix##_redirect,                   \
   prefix##_recover,                    \
   prefix##_retire},
#include "frontend/frontend_table.def"
#undef FRONTEND_IMPL
};

Frontend_Impl* frontend = NULL;

void frontend_intf_init() {
  frontend = &frontend_table[FRONTEND];
}
