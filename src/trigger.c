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
 * File         : trigger.c
 * Author       : HPS Research Group
 * Date         : 10/31/2012
 * Description  : Generates a trigger from a text specification. For example, to
 *generate a trigger that fires after 1M instructions, use specification
 *'inst:1000000'
 ***************************************************************************************/

#include "trigger.h"
#include <stdio.h>
#include "core.param.h"
#include "globals/assert.h"
#include "statistics.h"

/**************************************************************************************/
/* Types */

struct Trigger_struct {
  Flag         armed;
  const Stat*  stat;
  char*        name;
  Trigger_Type type;
  Counter      period;
  Counter      next_threshold;
};

/**************************************************************************************/
/* Implementation */

Trigger* trigger_create(const char* name, const char* spec, Trigger_Type type) {
  ASSERT(0, name);
  ASSERT(0, spec);
  Trigger* trigger = malloc(sizeof(Trigger));
  trigger->name    = strdup(name);
  ASSERT(0, type < TRIGGER_NUM_ELEMS);
  trigger->type = type;

  if(!strcmp(spec, "none") || !strcmp(spec, "never")) {
    trigger->stat  = NULL;
    trigger->armed = FALSE;  // will never trigger
    return trigger;
  }
  char* buf = strdup(spec);

  char* colon = strchr(buf, ':');
  ASSERTM(0, colon,
          "Spec '%s' for trigger '%s' does not fit required format, e.g. "
          "'inst[0]:1000'\n",
          spec, name);
  *colon           = 0;
  char* stat_str   = buf;
  char* number_str = colon + 1;

  uns   proc_id      = 0;
  char* open_bracket = strchr(stat_str, '[');
  if(open_bracket) {
    char* close_bracket = strchr(stat_str, ']');
    ASSERTM(0, close_bracket,
            "Spec '%s' for trigger '%s' does not fit required format, e.g. "
            "'inst[0]:1000'\n",
            spec, name);
    *close_bracket    = 0;
    char* proc_id_str = open_bracket + 1;
    proc_id           = atoi(proc_id_str);
    ASSERT(0, proc_id < NUM_CORES);
    *open_bracket = 0;
  }

  switch(*stat_str) {
    case 'i':
      trigger->stat = &global_stat_array[proc_id][NODE_INST_COUNT];
      break;
    case 'c':
      trigger->stat = &global_stat_array[proc_id][NODE_CYCLE];
      break;
    case 't':
      trigger->stat = &global_stat_array[proc_id][EXECUTION_TIME];
      break;
    default:
      trigger->stat = get_stat(proc_id, stat_str);
      ASSERTM(0, trigger->stat, "Stat '%s' for trigger '%s' not found\n",
              stat_str, name);
      ASSERTM(0, trigger->stat->type != FLOAT_TYPE_STAT,
              "Stat '%s' for trigger '%s' is a float (triggers support counter "
              "stats only)\n",
              stat_str, name);
  }

  trigger->period = atoll(number_str);
  if(trigger->period == 0 && trigger->type == TRIGGER_REPEAT) {
    FATAL_ERROR(0, "Repeat trigger '%s' has a zero period\n", name);
  }
  trigger->next_threshold = trigger->period;
  trigger->armed          = TRUE;

  return trigger;
}

Flag trigger_fired(Trigger* trigger) {
  // common (false) case first
  if(!trigger->armed || (trigger->stat->count + trigger->stat->total_count) <
                          trigger->next_threshold) {
    return FALSE;
  }

  // trigger fired
  if(trigger->type == TRIGGER_ONCE) {
    trigger->armed = FALSE;
  } else {
    trigger->next_threshold += trigger->period;
    uns skipped = 0;
    while(trigger->stat->count + trigger->stat->total_count >=
          trigger->next_threshold) {
      trigger->next_threshold += trigger->period;
      skipped++;
    }
    if(skipped > 0) {
      ERROR(0, "Trigger '%s' skipped %d firings\n", trigger->name, skipped);
    }
  }
  return TRUE;
}

Flag trigger_on(Trigger* trigger) {
  ASSERT(0, trigger->type == TRIGGER_ONCE);
  return trigger->stat && (!trigger->armed || trigger_fired(trigger));
}

double trigger_progress(Trigger* trigger) {
  if(!trigger->stat)
    return 0.0;  // trigger set to "never"
  if(!trigger->armed)
    return 1.0;

  ASSERT(0, trigger->next_threshold >= trigger->period);
  Counter stat_count = trigger->stat->count + trigger->stat->total_count;
  ASSERT(0, stat_count >= trigger->next_threshold - trigger->period);
  if(stat_count >= trigger->next_threshold)
    return 1.0;

  return (double)(stat_count - (trigger->next_threshold - trigger->period)) /
         (double)trigger->period;
}

void trigger_free(Trigger* trigger) {
  free(trigger->name);
  free(trigger);
}
