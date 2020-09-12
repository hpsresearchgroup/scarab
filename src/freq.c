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
 * File         : freq.c
 * Author       : HPS Research Group
 * Date         : 03/13/2012
 * Description  : Modeling frequency domains
 ***************************************************************************************/

#include "freq.h"
#include "core.param.h"
#include "debug/debug_macros.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "memory/memory.param.h"
#include "ramulator.param.h"
#include "statistics.h"

/**************************************************************************************/
/* Macros */

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_FREQ, ##args)
#define MAX_FREQ_DOMAINS 100

/**************************************************************************************/
/* Types */

typedef struct Domain_Info_struct {
  Counter cycles;
  uns     cycle_time;
  uns     time_until_next_cycle;
  char*   name;
} Domain_Info;

/**************************************************************************************/
/* Global variables */

/* Convention: the unit of time is a femtosecond, allowing for both
   high time accuracy (one million timesteps in a 1GHz clock cycle)
   and for long simulated latencies (up to 5 hours with 64 bit
   Counters). */
static Counter cur_time;
static uns     num_domains = 0;
Domain_Info    domains[MAX_FREQ_DOMAINS];

Freq_Domain_Id FREQ_DOMAIN_CORES[MAX_NUM_PROCS];
Freq_Domain_Id FREQ_DOMAIN_L1;
Freq_Domain_Id FREQ_DOMAIN_MEMORY;

/**************************************************************************************/
/* Local prototypes */

static Freq_Domain_Id freq_domain_create(char* name, uns cycle_time);

/**************************************************************************************/
/* Function definitions */

void freq_init(void) {
  char buf[MAX_STR_LENGTH + 1];
  uns  core_cycle_times[MAX_NUM_PROCS] = {
    CORE_0_CYCLE_TIME,  CORE_1_CYCLE_TIME,  CORE_2_CYCLE_TIME,
    CORE_3_CYCLE_TIME,  CORE_4_CYCLE_TIME,  CORE_5_CYCLE_TIME,
    CORE_6_CYCLE_TIME,  CORE_7_CYCLE_TIME,  CORE_8_CYCLE_TIME,
    CORE_9_CYCLE_TIME,  CORE_10_CYCLE_TIME, CORE_11_CYCLE_TIME,
    CORE_12_CYCLE_TIME, CORE_13_CYCLE_TIME, CORE_14_CYCLE_TIME,
    CORE_15_CYCLE_TIME, CORE_16_CYCLE_TIME, CORE_17_CYCLE_TIME,
    CORE_18_CYCLE_TIME, CORE_19_CYCLE_TIME, CORE_20_CYCLE_TIME,
    CORE_21_CYCLE_TIME, CORE_22_CYCLE_TIME, CORE_23_CYCLE_TIME,
    CORE_24_CYCLE_TIME, CORE_25_CYCLE_TIME, CORE_26_CYCLE_TIME,
    CORE_27_CYCLE_TIME, CORE_28_CYCLE_TIME, CORE_29_CYCLE_TIME,
    CORE_30_CYCLE_TIME, CORE_31_CYCLE_TIME, CORE_32_CYCLE_TIME,
    CORE_33_CYCLE_TIME, CORE_34_CYCLE_TIME, CORE_35_CYCLE_TIME,
    CORE_36_CYCLE_TIME, CORE_37_CYCLE_TIME, CORE_38_CYCLE_TIME,
    CORE_39_CYCLE_TIME, CORE_40_CYCLE_TIME, CORE_41_CYCLE_TIME,
    CORE_42_CYCLE_TIME, CORE_43_CYCLE_TIME, CORE_44_CYCLE_TIME,
    CORE_45_CYCLE_TIME, CORE_46_CYCLE_TIME, CORE_47_CYCLE_TIME,
    CORE_48_CYCLE_TIME, CORE_49_CYCLE_TIME, CORE_50_CYCLE_TIME,
    CORE_51_CYCLE_TIME, CORE_52_CYCLE_TIME, CORE_53_CYCLE_TIME,
    CORE_54_CYCLE_TIME, CORE_55_CYCLE_TIME, CORE_56_CYCLE_TIME,
    CORE_57_CYCLE_TIME, CORE_58_CYCLE_TIME, CORE_59_CYCLE_TIME,
    CORE_60_CYCLE_TIME, CORE_61_CYCLE_TIME, CORE_62_CYCLE_TIME,
    CORE_63_CYCLE_TIME,
  };
  uns l1_cycle_time = L1_CYCLE_TIME;
  if(CHIP_CYCLE_TIME) {
    // if CHIP_CYCLE_TIME is set, it overrides core and L1 cycle times
    for(int proc_id = 0; proc_id < MAX_NUM_PROCS; proc_id++) {
      core_cycle_times[proc_id] = CHIP_CYCLE_TIME;
    }
    l1_cycle_time = CHIP_CYCLE_TIME;
  }
  for(int proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    sprintf(buf, "CORE_%d", proc_id);
    FREQ_DOMAIN_CORES[proc_id]                     = freq_domain_create(buf,
                                                    core_cycle_times[proc_id]);
    GET_STAT_EVENT(proc_id, PARAM_CORE_CYCLE_TIME) = core_cycle_times[proc_id];
  }
  FREQ_DOMAIN_L1 = freq_domain_create("L1", l1_cycle_time);
  // FREQ_DOMAIN_MEMORY = freq_domain_create("MEMORY", MEMORY_CYCLE_TIME);
  FREQ_DOMAIN_MEMORY = freq_domain_create("MEMORY", RAMULATOR_TCK);
  /* These stats simplify data analysis by allowing cycle times to
     be used in get_cmp_data stat formulas */
  GET_STAT_EVENT(0, PARAM_L1_CYCLE_TIME) = l1_cycle_time;
  // GET_STAT_EVENT(0, PARAM_MEMORY_CYCLE_TIME) = MEMORY_CYCLE_TIME;
  GET_STAT_EVENT(0, PARAM_MEMORY_CYCLE_TIME) = RAMULATOR_TCK;
}

static Freq_Domain_Id freq_domain_create(char* name, uns cycle_time) {
  ASSERT(0, num_domains < MAX_FREQ_DOMAINS);
  ASSERT(0, cycle_time > 0);
  domains[num_domains].cycles     = 0;
  domains[num_domains].cycle_time = cycle_time;
  // every domain's first cycle can start at time zero
  domains[num_domains].time_until_next_cycle = 0;
  domains[num_domains].name                  = strdup(name);
  num_domains++;
  return num_domains - 1;
}

Flag freq_is_ready(Freq_Domain_Id id) {
  ASSERT(0, id < num_domains);
  return domains[id].time_until_next_cycle == 0;
}

void freq_advance_time(void) {
  /* Make currently ready domains wait for their next cycles */
  for(uns i = 0; i < num_domains; i++) {
    if(domains[i].time_until_next_cycle == 0) {
      domains[i].time_until_next_cycle = domains[i].cycle_time;
    }
  }

  /* Find the time until the next cycle */
  uns min_time_until_next_cycle = domains[0].time_until_next_cycle;

  for(uns i = 1; i < num_domains; i++) {
    if(domains[i].time_until_next_cycle < min_time_until_next_cycle) {
      min_time_until_next_cycle = domains[i].time_until_next_cycle;
    }
  }

  /* Time until the next cycle is the time delta */
  uns time_delta = min_time_until_next_cycle;
  ASSERT(0, time_delta > 0);

  /* Update externally visible state */
  cur_time += time_delta;
  INC_STAT_EVENT_ALL(EXECUTION_TIME, time_delta);
  INC_STAT_EVENT_ALL(POWER_TIME, time_delta);
  DEBUG(0, "Advancing time to %lld fs\n", cur_time);

  /* Update every domain's info using the time delta */
  for(uns i = 0; i < num_domains; i++) {
    domains[i].time_until_next_cycle -= time_delta;
    if(domains[i].time_until_next_cycle == 0) {
      /* This domain is now ready. Update its cycle count. */
      domains[i].cycles++;
      DEBUG(0, "Domain %s ready to simulate cycle %lld\n", domains[i].name,
            domains[i].cycles);
    }
  }
}

void freq_reset_cycle_counts(void) {
  for(uns i = 0; i < num_domains; i++) {
    domains[i].cycles                = 0;
    domains[i].time_until_next_cycle = 0;
  }
}

Counter freq_cycle_count(Freq_Domain_Id id) {
  ASSERT(0, id < num_domains);
  return domains[id].cycles;
}

Counter freq_time(void) {
  return cur_time;
}

Counter freq_future_time(Freq_Domain_Id id, Counter cycles) {
  ASSERT(0, id < num_domains);
  ASSERT(0, domains[id].cycles <= cycles);

  return freq_time() + (cycles - domains[id].cycles) * domains[id].cycle_time;
}

void freq_set_cycle_time(Freq_Domain_Id id, uns cycle_time) {
  ASSERT(0, id < num_domains);
  ASSERT(0, cycle_time > 0);
  domains[id].cycle_time = cycle_time;
  // Not changing time_until_next_cycle for simplicity (the
  // frequency change will take effect after the current cycle
  // finishes).
}

uns freq_get_cycle_time(Freq_Domain_Id id) {
  ASSERT(0, id < num_domains);
  return domains[id].cycle_time;
}

Counter freq_convert(Freq_Domain_Id src, Counter src_cycle_count,
                     Freq_Domain_Id dst) {
  /* This will not work once we model runtime DVFS */
  return src_cycle_count * domains[src].cycle_time / domains[dst].cycle_time;
}

Counter freq_convert_future_cycle(Freq_Domain_Id src, Counter src_cycle_count,
                                  Freq_Domain_Id dst) {
  ASSERT(0, src_cycle_count >= domains[src].cycles);
  Counter remaining_src_cycles = src_cycle_count - domains[src].cycles;
  Flag    src_cycle_ready_now  = (domains[src].time_until_next_cycle == 0);
  Counter last_src_cycle_time  = cur_time + domains[src].time_until_next_cycle -
                                (src_cycle_ready_now ? 0 :
                                                       domains[src].cycle_time);
  Counter time_after_last_src_cycle = remaining_src_cycles *
                                      domains[src].cycle_time;
  Counter future_time = last_src_cycle_time + time_after_last_src_cycle;

  Flag dst_cycle_ready_now = (domains[dst].time_until_next_cycle == 0);
  if(future_time <= cur_time + domains[dst].time_until_next_cycle) {
    // either this cycle or next cycle
    return domains[dst].cycles + !dst_cycle_ready_now;
  }

  Counter time_remaining_after_immediate_dst_cycle =
    future_time - cur_time - domains[dst].time_until_next_cycle;

  // make sure we don't add an extra cycle if the future time is a cycle
  // boundary for both domains
  Counter remaining_dst_cycles = (time_remaining_after_immediate_dst_cycle -
                                  1) /
                                   domains[dst].cycle_time +
                                 1;
  return domains[dst].cycles + (!dst_cycle_ready_now) + remaining_dst_cycles;
}

void freq_done(void) {
  for(uns i = 0; i < num_domains; i++) {
    free(domains[i].name);
  }
}
