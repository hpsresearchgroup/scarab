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
 * File         : stat_trace.c
 * Author       : HPS Research Group
 * Date         : 04/21/2012
 * Description  : Statistic trace
 ***************************************************************************************/

#include "stat_trace.h"
#include <stdio.h>
#include "core.param.h"
#include "globals/assert.h"
#include "stat_mon.h"
#include "statistics.h"
#include "trigger.h"

/**************************************************************************************/
/* Types */

typedef struct Stat_Info_struct {
  const Stat* ptr; /* pointer to the stat struct */
  /* value of the stat after the last interval */
  union {
    Counter last_count;
    double  last_value;
  };
} Stat_Info;

/**************************************************************************************/
/* Global Variables */

static Stat_Mon*  stat_mon;
static Stat_Enum* stat_indices;
static uns        num_stats;
static Trigger*   interval_trigger = NULL;
static FILE*      file;
const char*       DELIMITERS = " ,";

/**************************************************************************************/
/* Local Prototypes */

static void trace_stats(void);

/**************************************************************************************/
/* stat_trace_init: */

void stat_trace_init(void) {
  if(!STATS_TO_TRACE)
    return;

  /* open the trace file */
  char stats_trace_file[100];
  sprintf(stats_trace_file, "%s%s", FILE_TAG, STAT_TRACE_FILE);
  file = fopen(stats_trace_file, "w");
  ASSERTM(0, file, "Could not open %s", STAT_TRACE_FILE);

  /* parse the stats to trace */
  num_stats       = num_tokens(STATS_TO_TRACE, DELIMITERS);
  stat_indices    = malloc(num_stats * sizeof(Stat_Enum));
  char* stats_str = strdup(STATS_TO_TRACE);
  char* stat_name = strtok(stats_str, DELIMITERS);
  uns   ii        = 0;
  fprintf(file, "Instructions");
  while(stat_name) {
    Stat_Enum stat_idx = get_stat_idx(stat_name);
    ASSERTM(0, stat_idx < NUM_GLOBAL_STATS, "Stat %s not found\n", stat_name);
    stat_indices[ii] = stat_idx;
    ii++;
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      fprintf(file, "\t%s[%d]", stat_name, proc_id);
    }
    stat_name = strtok(NULL, DELIMITERS);
  }
  fprintf(file, "\n");
  ASSERT(0, ii == num_stats);
  free(stats_str);

  stat_mon = stat_mon_create_from_array(stat_indices, num_stats);

  /* do an initial trace print (all zeros) */
  trace_stats();

  interval_trigger = trigger_create("STAT_TRACE_INTERVAL", STAT_TRACE_INTERVAL,
                                    TRIGGER_REPEAT);
}

/**************************************************************************************/
/* stat_trace_cycle: */

void stat_trace_cycle(void) {
  if(!STATS_TO_TRACE)
    return;

  if(trigger_fired(interval_trigger)) {
    trace_stats();
  }
}

/**************************************************************************************/
/* stat_trace_done: */

void stat_trace_done(void) {
  if(!STATS_TO_TRACE)
    return;

  /* trace the final stat values */
  trace_stats();

  fclose(file);
  file = NULL;

  stat_mon_free(stat_mon);
  trigger_free(interval_trigger);
}

/**************************************************************************************/
/* num_tokens: */

uns num_tokens(const char* str, const char* delim) {
  uns   n     = 0;
  char* buf   = strdup(str);
  char* token = strtok(buf, delim);
  while(token) {
    n += 1;
    token = strtok(NULL, delim);
  }
  free(buf);
  return n;
}

/**************************************************************************************/
/* trace_stats: */

static void trace_stats(void) {
  fprintf(file, "%lld", inst_count[0]);
  for(uns ii = 0; ii < num_stats; ++ii) {
    for(uns proc_id = 0; proc_id < NUM_CORES; ++proc_id) {
      Stat_Enum stat_idx = stat_indices[ii];
      Stat*     stat     = &global_stat_array[proc_id][stat_idx];
      if(stat->type == FLOAT_TYPE_STAT) {
        fprintf(file, "\t%le", stat_mon_get_value(stat_mon, proc_id, stat_idx));
      } else {
        fprintf(file, "\t%lld",
                stat_mon_get_count(stat_mon, proc_id, stat_idx));
      }
    }
  }
  fprintf(file, "\n");
  stat_mon_reset(stat_mon);
}
