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
* File         : debug/debug_macros.h
* Author       : HPS Research Group
* Date         : 4/14/1998
* Description  : Debug output macros.

To use debug output macros, first put the following line in your source file:

    #define DEBUG(args...)		_DEBUG(DEBUG_FEATURE, ## args)

Replace DEBUG_FEATURE with a parameter specific to your code (e.g.,
DEBUG_REPL_STUDY).

Now you can just call DEBUG(<printf arguments>) and get nice debugging
output when
- running the debug version of Scarab,
- DEBUG_FEATURE is on, and
- simulation progress is withing the debug range specified by
  DEBUG_INST_START, DEBUG_INST_STOP, and other similar parameters.
***************************************************************************************/

#ifndef __DEBUG_MACROS_H__
#define __DEBUG_MACROS_H__

#include <stdio.h>
#include "debug/debug.param.h"
#include "freq.h"
#include "globals/global_defs.h"
#include "globals/utils.h"


/**************************************************************************************/
/* Returns whether simulation progress is within the debugging range */
#define DEBUG_RANGE_COND(proc_id)                                    \
  (((DEBUG_INST_START && inst_count[proc_id] >= DEBUG_INST_START) && \
    (!DEBUG_INST_STOP || inst_count[proc_id] <= DEBUG_INST_STOP)) || \
   ((DEBUG_CYCLE_START && cycle_count >= DEBUG_CYCLE_START) &&       \
    (!DEBUG_CYCLE_STOP || cycle_count <= DEBUG_CYCLE_STOP)) ||       \
   ((DEBUG_TIME_START && freq_time() >= DEBUG_TIME_START) &&         \
    (!DEBUG_TIME_STOP || freq_time() <= DEBUG_TIME_STOP)) ||         \
   ((DEBUG_OP_START && op_count[proc_id] >= DEBUG_OP_START) &&       \
    (!DEBUG_OP_STOP || op_count[proc_id] <= DEBUG_OP_STOP)))


#ifdef NO_DEBUG
#define ENABLE_GLOBAL_DEBUG_PRINT FALSE /* default FALSE */
#else
#define ENABLE_GLOBAL_DEBUG_PRINT TRUE /* default TRUE */
#endif

#define GLOBAL_DEBUG_STREAM mystdout /* default mystdout */


/**************************************************************************************/
/* Unconditional debug printf that cannot be turned off. */
#define DPRINTF(args...) fprintf(GLOBAL_DEBUG_STREAM, ##args);


/**************************************************************************************/
/* Prints a horizontal line to debug output. */
#if ENABLE_GLOBAL_DEBUG_PRINT
#define FPRINT_LINE(proc_id, stream)                                           \
  {                                                                            \
    if(DEBUG_RANGE_COND(proc_id)) {                                            \
      fprintf(stream, "#*****************************************************" \
                      "**************************\n");                         \
    }                                                                          \
  }
#else
#define FPRINT_LINE(proc_id, stream) \
  {}
#endif

/**************************************************************************************/

#if ENABLE_GLOBAL_DEBUG_PRINT
/* Prints args printf-style if debug_flag is on and simulation is in
   the debugging range. */
#define _DEBUG(proc_id, debug_flag, args...)                             \
  do {                                                                   \
    if(debug_flag && DEBUG_RANGE_COND(proc_id)) {                        \
      fprintf(GLOBAL_DEBUG_STREAM,                                       \
              "%s:%u: " #debug_flag " (P=%u O=%llu  I=%llu  C=%llu):  ", \
              __FILE__, __LINE__, proc_id, op_count[proc_id],            \
              inst_count[proc_id], cycle_count);                         \
      fprintf(GLOBAL_DEBUG_STREAM, ##args);                              \
      fflush(GLOBAL_DEBUG_STREAM);                                       \
    }                                                                    \
  } while(0)

/* Prints args printf-style if debug_flag is on and simulation is in
   the debugging range. Does not print proc_id, op_count, inst_count, cycle
   count. i.e., it only prints the given statement.*/
#define _DEBUG_LEAN(proc_id, debug_flag, args...) \
  do {                                            \
    if(debug_flag && DEBUG_RANGE_COND(proc_id)) { \
      fprintf(GLOBAL_DEBUG_STREAM, ##args);       \
      fflush(GLOBAL_DEBUG_STREAM);                \
    }                                             \
  } while(0)

/* Macro for tracing args to a file stream. */
#define _TRACE(debug_flag, stream, args...) \
  do {                                      \
    if(debug_flag && DEBUG_RANGE_COND(0)) { \
      fprintf(stream, ##args);              \
    }                                       \
  } while(0)

/* Prints args printf-style if debug_flag is on, regardless of whether
   simulation is in the debugging range. */
#define _DEBUGU(proc_id, debug_flag, args...)                            \
  do {                                                                   \
    if(debug_flag) {                                                     \
      fprintf(GLOBAL_DEBUG_STREAM,                                       \
              "%s:%u: " #debug_flag " (P=%u O=%llu  I=%llu  C=%llu):  ", \
              __FILE__, __LINE__, proc_id, op_count[proc_id],            \
              inst_count[proc_id], cycle_count);                         \
      fprintf(GLOBAL_DEBUG_STREAM, ##args);                              \
      fflush(GLOBAL_DEBUG_STREAM);                                       \
    }                                                                    \
  } while(0)

/* Prints args printf-style if debug_flag is on, cond is true, and
   simulation is in the debugging range. */
#define _DEBUGC(proc_id, debug_flag, cond, args...)                      \
  do {                                                                   \
    if((cond) && debug_flag && DEBUG_RANGE_COND(proc_id)) {              \
      fprintf(GLOBAL_DEBUG_STREAM,                                       \
              "%s:%u: " #debug_flag " (P=%u O=%llu  I=%llu  C=%llu):  ", \
              __FILE__, __LINE__, proc_id, op_count[proc_id],            \
              inst_count[proc_id], cycle_count);                         \
      fprintf(GLOBAL_DEBUG_STREAM, ##args);                              \
      fflush(GLOBAL_DEBUG_STREAM);                                       \
    }                                                                    \
  } while(0)

/* Unused */
#define _DEBUGL(proc_id, debug_lvl, which_lvl, args...)                       \
  do {                                                                        \
    if(which_lvl >= debug_lvl && DEBUG_RANGE_COND(proc_id)) {                 \
      char str0[MAX_STR_LENGTH + 1], str1[MAX_STR_LENGTH + 1],                \
        str2[MAX_STR_LENGTH + 1];                                             \
      snprintf(str0, MAX_STR_LENGTH, "%s:%u:", __FILE__, __LINE__);           \
      snprintf(str1, MAX_STR_LENGTH, " " #which_lvl ":" #debug_lvl);          \
      snprintf(str2, MAX_STR_LENGTH,                                          \
               " (P=%u O=%llu  I=%llu  C=%llu):", proc_id, op_count[proc_id], \
               inst_count[proc_id], cycle_count);                             \
      fprintf(GLOBAL_DEBUG_STREAM, "%-22s%-18s%-30s  ", str0, str1, str2);    \
      fprintf(GLOBAL_DEBUG_STREAM, ##args);                                   \
      fflush(GLOBAL_DEBUG_STREAM);                                            \
    }                                                                         \
  } while(0)

/* Unused */
#define _DEBUGLU(proc_id, debug_lvl, which_lvl, args...)      \
  do {                                                        \
    if(which_lvl >= debug_lvl) {                              \
      fprintf(GLOBAL_DEBUG_STREAM,                            \
              "%s:%u: " #which_lvl ":" #debug_lvl             \
              " (P=%u O=%llu  I=%llu  C=%llu):  ",            \
              __FILE__, __LINE__, proc_id, op_count[proc_id], \
              inst_count[proc_id], cycle_count);              \
      fprintf(GLOBAL_DEBUG_STREAM, ##args);                   \
      fflush(GLOBAL_DEBUG_STREAM);                            \
    }                                                         \
  } while(0)

#else
#define _DEBUG(proc_id, debug_flag, args...) \
  do {                                       \
  } while(0)
#define _DEBUG_LEAN(proc_id, debug_flag, args...) \
  do {                                            \
  } while(0)
#define _TRACE(debug_flag, stream, args...) \
  do {                                      \
  } while(0)
#define _DEBUGU(proc_id, debug_flag, args...) \
  do {                                        \
  } while(0)
#define _DEBUGC(proc_id, debug_flag, cond, args...) \
  do {                                              \
  } while(0)
#define _DEBUGL(proc_id, debug_lvl, which_lvl, args...) \
  do {                                                  \
  } while(0)
#define _DEBUGLU(proc_id, debug_lvl, which_lvl, cond, args...) \
  do {                                                         \
  } while(0)
#endif

/* Almost unused */
#define _DEBUGA(proc_id, debug_flag, args...)                            \
  do {                                                                   \
    if(debug_flag && DEBUG_RANGE_COND(proc_id)) {                        \
      fprintf(GLOBAL_DEBUG_STREAM,                                       \
              "%s:%u: " #debug_flag " (P=%u O=%llu  I=%llu  C=%llu):  ", \
              __FILE__, __LINE__, proc_id, op_count[proc_id],            \
              inst_count[proc_id], cycle_count);                         \
      fprintf(GLOBAL_DEBUG_STREAM, ##args);                              \
      fflush(GLOBAL_DEBUG_STREAM);                                       \
    }                                                                    \
  } while(0)

/* Unused */
#define _DEBUGLA(proc_id, debug_lvl, which_lvl, args...)                      \
  do {                                                                        \
    if(which_lvl >= debug_lvl && DEBUG_RANGE_COND(proc_id)) {                 \
      char str0[MAX_STR_LENGTH + 1], str1[MAX_STR_LENGTH + 1],                \
        str2[MAX_STR_LENGTH + 1];                                             \
      snprintf(str0, MAX_STR_LENGTH, "%s:%u:", __FILE__, __LINE__);           \
      snprintf(str1, MAX_STR_LENGTH, " " #which_lvl ":" #debug_lvl);          \
      snprintf(str2, MAX_STR_LENGTH,                                          \
               " (P=%u O=%llu  I=%llu  C=%llu):", proc_id, op_count[proc_id], \
               inst_count[proc_id], cycle_count);                             \
      fprintf(GLOBAL_DEBUG_STREAM, "%-22s%-18s%-30s  ", str0, str1, str2);    \
      fprintf(GLOBAL_DEBUG_STREAM, ##args);                                   \
      fflush(GLOBAL_DEBUG_STREAM);                                            \
    }                                                                         \
  } while(0)


/**************************************************************************************/

#endif /* #ifndef __DEBUG_MACROS_H__ */
