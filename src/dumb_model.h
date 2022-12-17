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
 * File         : dumb_model.h
 * Author       : HPS Research Group
 * Date         : 7/23/2013
 * Description  : Model that drives the memory system with randomly
 *                generated memory requests (no core modeling)
 ***************************************************************************************/

#ifndef __DUMB_MODEL_H__
#define __DUMB_MODEL_H__

#include "memory/memory.h"

/**************************************************************************************/
/* dumb model data  */

typedef struct Dumb_Model_struct {
  Memory memory;
} Dumb_Model;

/**************************************************************************************/
/* Global vars */

extern Dumb_Model dumb_model;

/**************************************************************************************/
/* Prototypes */

void dumb_init(uns mode);
void dumb_reset(void);
void dumb_cycle(void);
void dumb_debug(void);
void dumb_done(void);


/**************************************************************************************/

#endif /* #ifndef __DUMB_MODEL_H__ */
