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
 * File         : stat_mon.c
 * Author       : HPS Research Group
 * Date         : 12/13/2012
 * Description  : Statistic monitor: allows any statistic to be examined at any
 *interval
 ***************************************************************************************/

#include "stat_mon.h"
#include "globals/assert.h"
#include "globals/global_types.h"
#include "globals/utils.h"
#include "statistics.h"

/**************************************************************************************/
/* Types */

typedef struct Stat_Datum_struct {
  union {
    Counter count;
    double  value;
  };
} Stat_Datum;

typedef struct Stat_Info_struct {
  Stat_Enum stat_idx; /* index of the stat */
  /* value of the stat at the last reset (for each core) */
  Stat_Datum* last_data;
} Stat_Info;

struct Stat_Mon_struct {
  Stat_Info* stat_infos;
  uns        num_stats;
};

/**************************************************************************************/
/* Local Prototypes */

static Stat_Info* find_stat_info(Stat_Mon* mon, uns stat_idx);
static void       init_stat_info(Stat_Info* info, uns stat_idx);

/**************************************************************************************/
/* stat_mon_create_from_array: */

Stat_Mon* stat_mon_create_from_array(uns* stat_idx_array, uns num) {
  Stat_Mon* mon   = malloc(sizeof(Stat_Mon));
  mon->num_stats  = num;
  mon->stat_infos = malloc(num * sizeof(Stat_Info));
  for(uns i = 0; i < num; i++) {
    init_stat_info(&mon->stat_infos[i], stat_idx_array[i]);
  }
  stat_mon_reset(mon);
  return mon;
}

/**************************************************************************************/
/* stat_mon_create_from_range: */

Stat_Mon* stat_mon_create_from_range(uns first_stat_idx, uns last_stat_idx) {
  ASSERT(0, last_stat_idx >= first_stat_idx);
  ASSERT(0, last_stat_idx < NUM_GLOBAL_STATS);
  Stat_Mon* mon   = malloc(sizeof(Stat_Mon));
  mon->num_stats  = last_stat_idx - first_stat_idx + 1;
  mon->stat_infos = malloc(mon->num_stats * sizeof(Stat_Info));
  for(uns i = 0; i < mon->num_stats; i++) {
    init_stat_info(&mon->stat_infos[i], first_stat_idx + i);
  }
  stat_mon_reset(mon);
  return mon;
}

/**************************************************************************************/
/* stat_mon_get_count: */

Counter stat_mon_get_count(Stat_Mon* mon, uns proc_id, uns stat_idx) {
  ASSERT(0, proc_id < NUM_CORES);
  ASSERT(proc_id, stat_idx < NUM_GLOBAL_STATS);
  Stat* stat = &global_stat_array[proc_id][stat_idx];
  ASSERT(proc_id, stat->type != FLOAT_TYPE_STAT);
  Stat_Info* info = find_stat_info(mon, stat_idx);
  return stat->count + stat->total_count - info->last_data[proc_id].count;
}

/**************************************************************************************/
/* stat_mon_get_value: */

double stat_mon_get_value(Stat_Mon* mon, uns proc_id, uns stat_idx) {
  ASSERT(0, proc_id < NUM_CORES);
  ASSERT(proc_id, stat_idx < NUM_GLOBAL_STATS);
  Stat* stat = &global_stat_array[proc_id][stat_idx];
  ASSERT(proc_id, stat->type == FLOAT_TYPE_STAT);
  Stat_Info* info = find_stat_info(mon, stat_idx);
  return stat->value + stat->total_value - info->last_data[proc_id].value;
}

/**************************************************************************************/
/* stat_mon_get_reset: */

/**
 * @brief
 * TODO: check how stat->count and stat->value is reset
 * @param mon
 */
void stat_mon_reset(Stat_Mon* mon) {
  for(uns i = 0; i < mon->num_stats; i++) {
    Stat_Info* info = &mon->stat_infos[i];
    for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
      Stat* stat = &global_stat_array[proc_id][info->stat_idx];
      if(stat->type == FLOAT_TYPE_STAT) {
        info->last_data[proc_id].value = stat->value + stat->total_value;
      } else {
        info->last_data[proc_id].count = stat->count + stat->total_count;
      }
    }
  }
}

/**************************************************************************************/
/* stat_mon_free: */

void stat_mon_free(Stat_Mon* mon) {
  for(uns i = 0; i < mon->num_stats; i++) {
    free(mon->stat_infos[i].last_data);
  }
  free(mon->stat_infos);
  free(mon);
}

/**************************************************************************************/
/* find_stat_info: */

static Stat_Info* find_stat_info(Stat_Mon* mon, uns stat_idx) {
  /* linear search is a little slow, but stat monitors are not
     supposed to be used often, so it should be a minor perf hit */
  for(uns i = 0; i < mon->num_stats; i++) {
    if(mon->stat_infos[i].stat_idx == stat_idx) {
      return &mon->stat_infos[i];
    }
  }
  FATAL_ERROR(0, "Stat %s not in stat monitor\n",
              global_stat_array[0][stat_idx].name);
}

/**************************************************************************************/
/* init_stat_info: */

static void init_stat_info(Stat_Info* info, uns stat_idx) {
  ASSERT(0, stat_idx < NUM_GLOBAL_STATS);
  Stat* stat = &global_stat_array[0][stat_idx];
  if(stat->noreset)
    WARNINGU_ONCE(0, "NORESET stats are treated as resettable by stat_mon\n");
  info->stat_idx  = stat_idx;
  info->last_data = malloc(NUM_CORES * sizeof(Stat_Datum));
}
