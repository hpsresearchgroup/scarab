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
 * File         : globals/global_defs.h
 * Author       : HPS Research Group
 * Date         : 10/15/1997
 * Description  : Global defines that are intended to be included in every
 *source file.
 ***************************************************************************************/

#ifndef __GLOBAL_DEFS_H__
#define __GLOBAL_DEFS_H__


#include <malloc.h>
#include <string.h>
#include <time.h>


/**************************************************************************************/
/* Constants */

#define TRUE 1
#define FALSE 0

#define SUCCESS 1
#define FAILURE 0

#define TAKEN 1
#define NOT_TAKEN 0

#define MAX_NUM_PROCS 64

#define MAX_STR_LENGTH 1024
#define MAX_SIMULTANEOUS_STRINGS 32 /* default 32 */ /* power of 2 */

#define MAX_CTR 0xffffffffffffffffULL
#define MAX_SCTR 0x7fffffffffffffffLL

#define MAX_INT64 0x7fffffffffffffffLL
#define MAX_INT 0x7fffffff
#define MAX_UNS64 0xffffffffffffffffULL
#define MAX_UNS 0xffffffffU
#define MAX_ADDR 0xffffffffffffffffULL

#undef UNUSED
#define UNUSED(X) (void)(X)

/**************************************************************************************/

#ifndef NULL
#define NULL ((void*)0x0)
#endif


/**************************************************************************************/

#endif /* #ifndef __GLOBAL_DEFS_H__ */
