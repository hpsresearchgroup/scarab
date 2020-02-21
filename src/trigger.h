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
 * File         : trigger.h
 * Author       : HPS Research Group
 * Date         : 10/31/2012
 * Description  : Generates a trigger from a text specification. For example, to
 *generate a trigger that fires after 1M instructions, use specification
 *'inst:1000000'
 ***************************************************************************************/

#ifndef __TRIGGER_H__
#define __TRIGGER_H__

#include "globals/global_types.h"

/**************************************************************************************/
/* Types */

typedef enum Trigger_Type_enum {
  TRIGGER_ONCE,
  TRIGGER_REPEAT,
  TRIGGER_NUM_ELEMS
} Trigger_Type;

struct Trigger_struct;
typedef struct Trigger_struct Trigger;

/**************************************************************************************/
/* Prototypes */

Trigger* trigger_create(const char* name, const char* spec, Trigger_Type type);

Flag trigger_fired(Trigger* trigger);

Flag trigger_on(Trigger* trigger);

double trigger_progress(Trigger* trigger);

void trigger_free(Trigger* trigger);

#endif  // __TRIGGER_H__
