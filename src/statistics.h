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
 * File         : statistics.h
 * Author       : HPS Research Group
 * Date         : 2/10/1998
 * Description  : Header for statistics.c
 ***************************************************************************************/

#ifndef __STATISTICS_H__
#define __STATISTICS_H__

#include <stdio.h>
#include "core.param.h"
#include "general.param.h"
#include "globals/global_defs.h"


/**************************************************************************************/
/* Type Declarations */

#define DEF_STAT(name, type, ratio) name,

typedef enum Stat_Enum_enum {
#include "stat_files.def"
  NUM_GLOBAL_STATS
} Stat_Enum;

#undef DEF_STAT


typedef enum Stat_Type_enum {
  COUNT_TYPE_STAT,  // stat is a simple counter
  //   output is a number
  FLOAT_TYPE_STAT,  // stat is a floating point value
  //   output is a number
  DIST_TYPE_STAT,  // stat is the beginning of end of a distribution
  //   output is a histogram
  PER_INST_TYPE_STAT,  // stat is measured per instruction
  //   output is a ratio of count/inst_count
  PER_1000_INST_TYPE_STAT,  // stat is measured per 1000 instructions
  //   output is a ratio of 1000*count/inst_count
  PER_1000_PRET_INST_TYPE_STAT,  // stat is measured per 1000 pseudo-retired
                                 // instructions
  //   output is a ratio of 1000*count/pret_inst_count
  PER_CYCLE_TYPE_STAT,  // stat is measured per cycle
  //   output is a ratio of count/cycle_count
  RATIO_TYPE_STAT,  // stat is measured per <some other stat>
  //   output is a ratio of count/other
  PERCENT_TYPE_STAT,  // stat is measured per <some other stat>
  //   output is a percent of count/other
  LINE_TYPE_STAT,  // stat is a line (name will be printed as comment)
  NUM_STAT_TYPES,
} Stat_Type;


typedef struct Stat_struct {
  Stat_Type   type;  // see types above
  const char* name;  // name of stat
  union {
    Counter count;  // count during the current stat interval
    double  value;  // value during the current stat interval
  };
  union {
    Counter total_count;  // total count from beginning of run
    double  total_value;  // total value from beginning of run
  };
  Stat_Enum   ratio_stat;  // stat that to use in the ratio
  const char* file_name;   // name of file to print stats
  Flag noreset;  // this stat does not get reset (name has prefix "NORESET")
} Stat;


/**************************************************************************************/
/* Macros */

#ifndef NO_STAT
#define STAT_EVENT(proc_id, stat)             \
  do {                                        \
    global_stat_array[proc_id][stat].count++; \
  } while(0)

#define STAT_EVENT_ALL(stat)                             \
  do {                                                   \
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) \
      global_stat_array[proc_id][stat].count++;          \
  } while(0)

#define INC_STAT_EVENT(proc_id, stat, inc)           \
  do {                                               \
    global_stat_array[proc_id][stat].count += (inc); \
  } while(0)

#define INC_STAT_EVENT_ALL(stat, inc)                    \
  do {                                                   \
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) \
      global_stat_array[proc_id][stat].count += (inc);   \
  } while(0)

#define INC_STAT_VALUE(proc_id, stat, inc)           \
  do {                                               \
    global_stat_array[proc_id][stat].value += (inc); \
  } while(0)

#define INC_STAT_VALUE_ALL(stat, inc)                    \
  do {                                                   \
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) \
      global_stat_array[proc_id][stat].value += (inc);   \
  } while(0)

#define GET_STAT_EVENT(proc_id, stat) (global_stat_array[proc_id][stat].count)
#define GET_TOTAL_STAT_EVENT(proc_id, stat) \
  (global_stat_array[proc_id][stat].count + \
   global_stat_array[proc_id][stat].total_count)
#define GET_TOTAL_STAT_VALUE(proc_id, stat) \
  (global_stat_array[proc_id][stat].value + \
   global_stat_array[proc_id][stat].total_value)
#define GET_ACCUM_STAT_EVENT(stat) get_accum_stat_event(stat)
#define RESET_STAT(proc_id, stat) (global_stat_array[proc_id][stat].count = 0)

#define NO_RATIO NUM_GLOBAL_STATS

#else

#define STAT_EVENT(proc_id, stat)
#define STAT_EVENT_ALL(stat)
#define INC_STAT_EVENT(proc_id, stat, inc)
#define INC_STAT_EVENT_ALL(stat, inc)
#define INC_STAT_VALUE(proc_id, stat, inc)
#define INC_STAT_VALUE_ALL(stat, inc)
#define GET_STAT_EVENT(proc_id, stat) 0
#define GET_TOTAL_STAT_EVENT(proc_id, stat) 0
#define GET_TOTAL_STAT_VALUE(proc_id, stat)
#define GET_ACCUM_STAT_EVENT(stat)
#define RESET_STAT(proc_id, stat)
#define NO_RATIO

#endif

/**************************************************************************************/
/* Global Variables */

#ifndef NO_STAT
extern Stat** global_stat_array;
#endif


/**************************************************************************************/
/* Prototypes */

void        init_global_stats_array(void);
void        gen_stat_output_file(char*, uns8, Stat*);
void        init_global_stats(uns8);
void        dump_stats(uns8, Flag, Stat[], uns);
void        reset_stats(Flag);
void        fprint_line(FILE*);
Stat_Enum   get_stat_idx(const char* name);
const Stat* get_stat(uns8, const char*);
Counter     get_accum_stat_event(Stat_Enum name);


/**************************************************************************************/

#endif /* #ifndef __STATISTICS_H__ */
