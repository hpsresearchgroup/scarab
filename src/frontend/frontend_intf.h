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
 * File         : frontend/frontend_intf.h
 * Author       : HPS Research Group
 * Date         : 12/12/2011
 * Description  : Interface for an external frontend implementation.
 ***************************************************************************************/
#ifndef __FRONTEND_INTF_H__
#define __FRONTEND_INTF_H__

#include "globals/global_types.h"

/*************************************************************/
/* Forward Declarations */

struct Op_struct;

/*************************************************************/
/* External frontend interface */

typedef struct Frontend_Intf_struct {
  // Name
  const char* name;

  /* Initialization is not part of the interface because the
   * arguments depend on whether the front-end is trace or execution
   * driven. */

  /* Get next instruction fetch address */
  Addr (*next_fetch_addr)(uns proc_id);

  /* Check whether we can get an op from the frontend (that is,
     process proc_id is running) */
  Flag (*can_fetch_op)(uns proc_id);

  /* Get an op from the frontend */
  void (*fetch_op)(uns proc_id, struct Op_struct* op);

  /* Redirect the front end (down the wrong path) */
  void (*redirect)(uns proc_id, uns64 inst_uid, Addr fetch_addr);

  /* Recover the front end (restart the right path) */
  void (*recover)(uns proc_id, uns64 inst_uid);

  /* Let the frontend know that this instruction is retired) */
  void (*retire)(uns proc_id, uns64 inst_uid);
} Frontend_Impl;

typedef enum Frontend_Id_enum {
#define FRONTEND_IMPL(id, name, prefix) FE_##id,
#include "frontend/frontend_table.def"
#undef FRONTEND_IMPL
  NUM_FRONTENDS
} Frontend_Id;

extern Frontend_Impl* frontend;
extern Frontend_Impl  frontend_table[];

/* Initialize the above frontend pointer */
void frontend_intf_init(void);

/*************************************************************/

#endif /*  __FRONTEND_INTF_H__*/
