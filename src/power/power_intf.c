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
 * File         : power_intf.h
 * Author       : HPS Research Group
 * Date         : 04/21/2012
 * Description  : Interface to the combined McPAT/CACTI power model.
 ***************************************************************************************/

#include "power_intf.h"
#include <stdio.h>
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/utils.h"
#include "statistics.h"

#include "general.param.h"
#include "power/power.param.h"
#include "power/power_scarab_config.h"
#include "ramulator.h"
#include "ramulator.param.h"

/**************************************************************************************/
/* Enum Definitions */

DEFINE_ENUM(Power_Domain, POWER_DOMAIN_LIST);
DEFINE_ENUM(Power_Result, POWER_RESULT_LIST);


/**************************************************************************************/
/* Types */

typedef struct Value_struct {
  Flag   set;        /* Has this value been set? */
  double intf_value; /* Provided by external tools using their reference V/f */
  double scaled_value; /* Scaled to Scarab's V/f */
} Value;


/**************************************************************************************/
/* Local Prototypes */

// static void dump_power_stats(void);
static void           run_power_model_exec(void);
static void           parse_power_model_results(void);
static void           update_energy_stats(void);
static void           scale_values(Power_Domain domain);
static Freq_Domain_Id freq_domain(Power_Domain);
void                  dump_power_energy_stats(void);

static const char* model_results_filename = "power_model_results";


/**************************************************************************************/
/* Global Variables */

static Value  values[POWER_DOMAIN_NUM_ELEMS][POWER_RESULT_NUM_ELEMS];
static double elapsed_time;  // time elapsed in this interval, seconds

/**************************************************************************************/
/* power_intf_init: */

void power_intf_init(void) {
  if(!POWER_INTF_ON)
    return;

  ASSERTM(0, NUM_CORES <= 8, "power_intf supports up to 8 cores\n");

  for(uns stat = POWER_STATS_BEGIN; stat <= POWER_STATS_END; ++stat) {
    ASSERT(0, GET_TOTAL_STAT_EVENT(0, stat) == 0);
  }
}

/**************************************************************************************/
/* power_intf_calc: */

void power_intf_calc(void) {
  double fempto_elapsed_time = (double)GET_TOTAL_STAT_EVENT(0, POWER_TIME);
  elapsed_time               = fempto_elapsed_time * 1.0e-15;

  run_power_model_exec();
  parse_power_model_results();
  update_energy_stats();
}

/**************************************************************************************/
/* power_intf_result: */

double power_intf_result(Power_Domain domain, Power_Result result) {
  ASSERT(0, domain < POWER_DOMAIN_NUM_ELEMS);
  ASSERT(0, result < POWER_RESULT_NUM_ELEMS);
  ASSERTM(0, values[domain][result].set,
          "Requested power result {%s, %s} not set\n", Power_Domain_str(domain),
          Power_Result_str(result));

  return values[domain][result].scaled_value;
}

/**************************************************************************************/
/* power_intf_done: */

void power_intf_done(void) {
  if(!POWER_INTF_ON)
    return;
  if(GET_TOTAL_STAT_EVENT(0, POWER_TIME) == 0)
    return;
  power_intf_calc();
}

void run_power_model_exec(void) {
  power_print_mcpat_xml_infile();
  power_print_cacti_cfg_infile();

  char cmd[MAX_STR_LENGTH];
  uns  len = sprintf(cmd, "python3 %s/%s %s %d %d %s", BINDIR, POWER_INTF_EXEC,
                    ".", POWER_INTF_ENABLE_SCALING, DEBUG_POWER_UTILS,
                    FILE_TAG);
  ASSERT(0, len < MAX_STR_LENGTH);

  int rc = system(cmd);
  ASSERTM(0, rc == 0, "Command \"%s\" failed\n", cmd);
}

void parse_power_model_results(void) {
  /* Mark all values as unset */
  for(uns domain = 0; domain < POWER_DOMAIN_NUM_ELEMS; ++domain) {
    for(uns result = 0; result < POWER_RESULT_NUM_ELEMS; ++result) {
      values[domain][result].set = FALSE;
    }
  }

  FILE* file = file_tag_fopen(NULL, model_results_filename, "r");
  ASSERTM(0, file, "Could not open %s\n", model_results_filename);

  char   line[MAX_STR_LENGTH + 1];
  char   domain_str[MAX_STR_LENGTH + 1];
  char   result_str[MAX_STR_LENGTH + 1];
  double value;
  Flag   read[POWER_DOMAIN_NUM_ELEMS][POWER_RESULT_NUM_ELEMS] = {};

  while(fgets(line, MAX_STR_LENGTH, file)) {
    uns num_matches = sscanf(line, "%s\t%s\t%le", domain_str, result_str,
                             &value);
    ASSERT(0, num_matches == 3);
    DEBUG(0, "Parsing domain: %s result: %s value: %le\n", domain_str,
          result_str, value);

    Power_Domain domain = Power_Domain_parse(domain_str);
    Power_Result result = Power_Result_parse(result_str);
    ASSERTM(0, !read[domain][result],
            "Power model result {%s, %s} read twice\n", domain_str, result_str);
    values[domain][result].intf_value = value;
    values[domain][result].set        = TRUE;
    read[domain][result]              = TRUE;
  }

  ASSERTM(0, feof(file) && !ferror(file), "Error reading %s\n",
          model_results_filename);
  fclose(file);

  /* Adjusting DRAM power */
  /* CACTI reports numbers for a single DRAM chip:
   * 1. For static power, we need to adjust the value by multiplying to the
   * number of chips we have.
   * 2. For dynamic power, we use the total number of
   * activate/precharge/read/write in the memory to calculate the overall
   * dynamic power. */
  values[POWER_DOMAIN_MEMORY][POWER_RESULT_STATIC].intf_value *=
    ramulator_get_num_chips();
  DEBUG(0, "Number of DRAM chips: %d\n", ramulator_get_num_chips());

  /* Set other system power */
  values[POWER_DOMAIN_OTHER][POWER_RESULT_STATIC].intf_value  = POWER_OTHER;
  values[POWER_DOMAIN_OTHER][POWER_RESULT_STATIC].set         = TRUE;
  values[POWER_DOMAIN_OTHER][POWER_RESULT_DYNAMIC].intf_value = 0;
  values[POWER_DOMAIN_OTHER][POWER_RESULT_DYNAMIC].set        = TRUE;

  /* Check if we need scaling */
  for(uns domain = 0; domain < POWER_DOMAIN_NUM_ELEMS; ++domain) {
    if(POWER_INTF_ENABLE_SCALING && domain != POWER_DOMAIN_OTHER) {
      scale_values(domain);
    } else {
      /* Other system power stays constant */
      for(uns result = 0; result < POWER_RESULT_NUM_ELEMS; ++result) {
        values[domain][result].scaled_value = values[domain][result].intf_value;
      }
    }
  }

  /* calculating total power */
  for(uns domain = 0; domain < POWER_DOMAIN_NUM_ELEMS; ++domain) {
    if(values[domain][POWER_RESULT_STATIC].set &&
       values[domain][POWER_RESULT_DYNAMIC].set) {
      values[domain][POWER_RESULT_TOTAL].intf_value =
        values[domain][POWER_RESULT_STATIC].intf_value +
        values[domain][POWER_RESULT_DYNAMIC].intf_value;

      values[domain][POWER_RESULT_TOTAL].scaled_value =
        values[domain][POWER_RESULT_STATIC].scaled_value +
        values[domain][POWER_RESULT_DYNAMIC].scaled_value;

      values[domain][POWER_RESULT_TOTAL].set = TRUE;
    }
  }
}

void update_energy_stats(void) {
  INC_STAT_VALUE(0, TIME, elapsed_time);

  /* Per-core energy */
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    INC_STAT_VALUE(
      proc_id, ENERGY_CORE,
      elapsed_time *
        power_intf_result(POWER_DOMAIN_CORE_0 + proc_id, POWER_RESULT_TOTAL));
    INC_STAT_VALUE(
      proc_id, ENERGY_CORE_STATIC,
      elapsed_time *
        power_intf_result(POWER_DOMAIN_CORE_0 + proc_id, POWER_RESULT_STATIC));
    INC_STAT_VALUE(
      proc_id, ENERGY_CORE_DYNAMIC,
      elapsed_time *
        power_intf_result(POWER_DOMAIN_CORE_0 + proc_id, POWER_RESULT_DYNAMIC));

    /* total system energy */
    INC_STAT_VALUE(
      0, ENERGY,
      elapsed_time *
        power_intf_result(POWER_DOMAIN_CORE_0 + proc_id, POWER_RESULT_TOTAL));
  }

  /* Uncore energy */
  INC_STAT_VALUE(
    0, ENERGY_UNCORE,
    elapsed_time * power_intf_result(POWER_DOMAIN_UNCORE, POWER_RESULT_TOTAL));
  INC_STAT_VALUE(
    0, ENERGY_UNCORE_STATIC,
    elapsed_time * power_intf_result(POWER_DOMAIN_UNCORE, POWER_RESULT_STATIC));
  INC_STAT_VALUE(0, ENERGY_UNCORE_DYNAMIC,
                 elapsed_time * power_intf_result(POWER_DOMAIN_UNCORE,
                                                  POWER_RESULT_DYNAMIC));
  /* Memory Energy */
  INC_STAT_VALUE(
    0, ENERGY_MEMORY,
    elapsed_time * power_intf_result(POWER_DOMAIN_MEMORY, POWER_RESULT_TOTAL));
  INC_STAT_VALUE(
    0, ENERGY_MEMORY_STATIC,
    elapsed_time * power_intf_result(POWER_DOMAIN_MEMORY, POWER_RESULT_STATIC));
  INC_STAT_VALUE(0, ENERGY_MEMORY_DYNAMIC,
                 elapsed_time * power_intf_result(POWER_DOMAIN_MEMORY,
                                                  POWER_RESULT_DYNAMIC));
  /* Other */
  INC_STAT_VALUE(
    0, ENERGY_OTHER,
    elapsed_time * power_intf_result(POWER_DOMAIN_OTHER, POWER_RESULT_TOTAL));
  INC_STAT_VALUE(
    0, ENERGY_OTHER_STATIC,
    elapsed_time * power_intf_result(POWER_DOMAIN_OTHER, POWER_RESULT_STATIC));
  INC_STAT_VALUE(
    0, ENERGY_OTHER_DYNAMIC,
    elapsed_time * power_intf_result(POWER_DOMAIN_OTHER, POWER_RESULT_DYNAMIC));

  /* total system energy */
  INC_STAT_VALUE(
    0, ENERGY,
    elapsed_time * (power_intf_result(POWER_DOMAIN_UNCORE, POWER_RESULT_TOTAL) +
                    power_intf_result(POWER_DOMAIN_MEMORY, POWER_RESULT_TOTAL) +
                    power_intf_result(POWER_DOMAIN_OTHER, POWER_RESULT_TOTAL)));

  dump_power_energy_stats();
}

void dump_power_energy_stats(void) {
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    dump_stats(proc_id, TRUE, global_stat_array[proc_id] + POWER_STATS_BEGIN,
               ENERGY_STATS_END - POWER_STATS_BEGIN + 1);
  }
}

/**************************************************************************************/
/* scale_value: Scale a power value received from the external tools to match
 * the frequency and voltage modeled by Scarab.
 *
 * Note: to use this scaling function, enable the POWER_INTF_ENABLE_SCALING knob
 * and apply the patch files (found in bin/power/mcpat.patch,cacti.patch) to
 * McPat and CACTI respectively.
 *
 * Discussed in Rustam Miftakhutdinov Thesis, Page 39.
 * https://hps.ece.utexas.edu/people/rustam/pub/diss.pdf
 * */

void scale_values(Power_Domain domain) {
  /* We don't scale total power directly; it should be calculated from scaled
   * dynamic and scaled static power */

  values[domain][POWER_RESULT_MIN_VOLTAGE].scaled_value =
    values[domain][POWER_RESULT_MIN_VOLTAGE].intf_value;

  double intf_freq    = values[domain][POWER_RESULT_FREQUENCY].intf_value;
  double intf_voltage = values[domain][POWER_RESULT_VOLTAGE].intf_value;
  if(domain == POWER_DOMAIN_UNCORE) {
    printf("intf v: %g\n", intf_voltage);
  }

  Counter scarab_cycle_time = freq_get_cycle_time(freq_domain(domain));
  double  scarab_freq       = 1.0e15 / (double)scarab_cycle_time;

  double min_voltage    = values[domain][POWER_RESULT_MIN_VOLTAGE].intf_value;
  double scarab_voltage = MAX2(scarab_freq / intf_freq * intf_voltage,
                               min_voltage);

  double freq_ratio    = scarab_freq / intf_freq;
  double voltage_ratio = scarab_voltage / intf_voltage;

  /* P=1/2*C*V^2*f */
  if(domain == POWER_DOMAIN_UNCORE) {
    printf("p: %g, v: %g, f: %g\n",
           values[domain][POWER_RESULT_DYNAMIC].intf_value, voltage_ratio,
           freq_ratio);
  }

  values[domain][POWER_RESULT_DYNAMIC].scaled_value =
    values[domain][POWER_RESULT_DYNAMIC].intf_value * voltage_ratio *
    voltage_ratio * freq_ratio;
  values[domain][POWER_RESULT_PEAK_DYNAMIC].scaled_value =
    values[domain][POWER_RESULT_PEAK_DYNAMIC].intf_value * voltage_ratio *
    voltage_ratio * freq_ratio;

  /* P=V*N*k*Ileak (from Butts & Sohi, "A Static Power Model for Architects") */
  values[domain][POWER_RESULT_STATIC].scaled_value =
    values[domain][POWER_RESULT_STATIC].intf_value * voltage_ratio;
  values[domain][POWER_RESULT_SUBTHR_LEAKAGE].scaled_value =
    values[domain][POWER_RESULT_SUBTHR_LEAKAGE].intf_value * voltage_ratio;
  values[domain][POWER_RESULT_GATE_LEAKAGE].scaled_value =
    values[domain][POWER_RESULT_GATE_LEAKAGE].intf_value * voltage_ratio;

  values[domain][POWER_RESULT_VOLTAGE].scaled_value   = scarab_voltage;
  values[domain][POWER_RESULT_FREQUENCY].scaled_value = scarab_freq;
}

Freq_Domain_Id freq_domain(Power_Domain domain) {
  switch(domain) {
    case POWER_DOMAIN_CORE_0:
      return FREQ_DOMAIN_CORES[0];
    case POWER_DOMAIN_CORE_1:
      return FREQ_DOMAIN_CORES[1];
    case POWER_DOMAIN_CORE_2:
      return FREQ_DOMAIN_CORES[2];
    case POWER_DOMAIN_CORE_3:
      return FREQ_DOMAIN_CORES[3];
    case POWER_DOMAIN_CORE_4:
      return FREQ_DOMAIN_CORES[4];
    case POWER_DOMAIN_CORE_5:
      return FREQ_DOMAIN_CORES[5];
    case POWER_DOMAIN_CORE_6:
      return FREQ_DOMAIN_CORES[6];
    case POWER_DOMAIN_CORE_7:
      return FREQ_DOMAIN_CORES[7];
    case POWER_DOMAIN_UNCORE:
      return FREQ_DOMAIN_L1;
    case POWER_DOMAIN_MEMORY:
      return FREQ_DOMAIN_MEMORY;
    default:
      FATAL_ERROR(0, "Unknown power domain (idx: %d)\n", domain);
  }
}
