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
* File         : assert.h
* Author       : HPS Research Group
* Date         : 10/15/1997
* Description  : Assert macros tailored to Scarab' specifics.

On failure, these assert macros print out file name and line number of
the assert, simulation information (proc_id, op count, instruction
count, and cycle count), the failed invariant, and, in case of
ASSERTM(), a diagnostic message.

The macros also call breakpoint(), which is useful for
debugging. While debugging, setting a breakpoint on the breakpoint()
function (e.g., "b breakpoint" in GDB) allows the process to stop
whenever any assert fails.
***************************************************************************************/

#ifndef __ASSERT_H__
#define __ASSERT_H__

#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include "debug/debug_macros.h"
#include "globals/global_vars.h"

#ifdef NO_ASSERT
#define ENABLE_ASSERTIONS FALSE /* default FALSE */
#else
#define ENABLE_ASSERTIONS TRUE /* default TRUE */
#endif

/**************************************************************************************/
/* Prints the current call stack of Scarab. For the function names to be
 * printed, Scarab should be linked with -rdynamic flag. */
inline void print_backtrace(void) {
  void*  array[30];
  char** strings;
  size_t size = backtrace(array, 30);
  strings     = backtrace_symbols(array, size);

  fprintf(mystderr, "Obtained %zd stack frames.\n", size);
  for(size_t i = 0; i < size; i++)
    fprintf(mystderr, "%s\n", strings[i]);

  free(strings);
}

/**************************************************************************************/
/* Asserts that cond is true. If cond is false, prints simulation
 * information and stops the simulation. May be disabled by defining
 * NO_ASSERT. */
#define ASSERT(proc_id, cond)                                           \
  do {                                                                  \
    if(ENABLE_ASSERTIONS && !(cond)) {                                  \
      fflush(mystdout);                                                 \
      fprintf(mystderr, "\n");                                          \
      fprintf(mystderr,                                                 \
              "%s:%d: ASSERT FAILED (P=%u  O=%llu  I=%llu  C=%llu):  ", \
              __FILE__, __LINE__, proc_id, op_count[proc_id],           \
              inst_count[proc_id], cycle_count);                        \
      fprintf(mystderr, "%s\n", #cond);                                 \
      breakpoint(__FILE__, __LINE__);                                   \
      WRITE_STATUS("ASSERT");                                           \
      print_backtrace();                                                \
      exit(15);                                                         \
    }                                                                   \
  } while(0)

/**************************************************************************************/
/* Asserts that cond is true. If cond is false, prints simulation
 * information, prints the printf-style message specified by args, and
 * stops the simulation. May be disabled by defining NO_ASSERT. */
#define ASSERTM(proc_id, cond, args...)                                 \
  do {                                                                  \
    if(ENABLE_ASSERTIONS && !(cond)) {                                  \
      fflush(mystdout);                                                 \
      fprintf(mystderr, "\n");                                          \
      fprintf(mystderr,                                                 \
              "%s:%d: ASSERT FAILED (P=%u  O=%llu  I=%llu  C=%llu):  ", \
              __FILE__, __LINE__, proc_id, op_count[proc_id],           \
              inst_count[proc_id], cycle_count);                        \
      fprintf(mystderr, "%s\n", #cond);                                 \
      fprintf(mystderr,                                                 \
              "%s:%d: ASSERT FAILED (P=%u  O=%llu  I=%llu  C=%llu):  ", \
              __FILE__, __LINE__, proc_id, op_count[proc_id],           \
              inst_count[proc_id], cycle_count);                        \
      fprintf(mystderr, ##args);                                        \
      breakpoint(__FILE__, __LINE__);                                   \
      WRITE_STATUS("ASSERT");                                           \
      print_backtrace();                                                \
      exit(15);                                                         \
    }                                                                   \
  } while(0)


/**************************************************************************************/
/* Asserts that cond is true. If cond is false, prints simulation
 * information and stops the simulation. Always enabled (NO_ASSERT has
 * no effect). */
#define ASSERTU(proc_id, cond)                                          \
  do {                                                                  \
    if(!(cond)) {                                                       \
      fflush(mystdout);                                                 \
      fprintf(mystderr, "\n");                                          \
      fprintf(mystderr,                                                 \
              "%s:%d: ASSERT FAILED (P=%u  O=%llu  I=%llu  C=%llu):  ", \
              __FILE__, __LINE__, proc_id, op_count[proc_id],           \
              inst_count[proc_id], cycle_count);                        \
      fprintf(mystderr, "%s\n", #cond);                                 \
      breakpoint(__FILE__, __LINE__);                                   \
      WRITE_STATUS("ASSERT");                                           \
      print_backtrace();                                                \
      exit(15);                                                         \
    }                                                                   \
  } while(0)


/**************************************************************************************/
/* Asserts that cond is true. If cond is false, prints simulation
 * information, prints the printf-style message specified by args, and
 * stops the simulation. Always enabled (NO_ASSERT has no effect). */
#define ASSERTUM(proc_id, cond, args...)                                \
  do {                                                                  \
    if(!(cond)) {                                                       \
      fflush(mystdout);                                                 \
      fprintf(mystderr, "\n");                                          \
      fprintf(mystderr,                                                 \
              "%s:%d: ASSERT FAILED (P=%u  O=%llu  I=%llu  C=%llu):  ", \
              __FILE__, __LINE__, proc_id, op_count[proc_id],           \
              inst_count[proc_id], cycle_count);                        \
      fprintf(mystderr, "%s\n", #cond);                                 \
      fprintf(mystderr,                                                 \
              "%s:%d: ASSERT FAILED (P=%u  O=%llu  I=%llu  C=%llu):  ", \
              __FILE__, __LINE__, proc_id, op_count[proc_id],           \
              inst_count[proc_id], cycle_count);                        \
      fprintf(mystderr, ##args);                                        \
      breakpoint(__FILE__, __LINE__);                                   \
      WRITE_STATUS("ASSERT");                                           \
      print_backtrace();                                                \
      exit(15);                                                         \
    }                                                                   \
  } while(0)


  /**************************************************************************************/

#endif /* #ifndef __ASSERT_H__ */
