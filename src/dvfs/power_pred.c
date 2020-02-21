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
 * File         : power_pred.c
 * Author       : HPS Research Group
 * Date         : 05/19/2012
 * Description  : Power prediction for DVFS
 ***************************************************************************************/

#include "power_pred.h"
#include "core.param.h"
#include "globals/assert.h"
#include "power/power_intf.h"

/**************************************************************************************/
/* Local prototypes */

static double power_pred_domain_power(Power_Domain domain,
                                      uns target_cycle_time, double slowdown);

/**************************************************************************************/
/* power_pred_norm_power: */

double power_pred_norm_power(uns* core_cycle_times, uns memory_cycle_time,
                             double* memory_access_fracs, double* slowdowns) {
  /* We assume other system power is purely static */
  double other_power = power_intf_result(POWER_DOMAIN_OTHER,
                                         POWER_RESULT_TOTAL);
  ASSERT(0, power_intf_result(POWER_DOMAIN_OTHER, POWER_RESULT_DYNAMIC) == 0);

  double uncore_power = power_intf_result(POWER_DOMAIN_UNCORE,
                                          POWER_RESULT_TOTAL);

  double ref_total_power = power_intf_result(POWER_DOMAIN_MEMORY,
                                             POWER_RESULT_TOTAL) +
                           uncore_power + other_power;
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    ref_total_power += power_intf_result(POWER_DOMAIN_CORE_0 + proc_id,
                                         POWER_RESULT_TOTAL);
  }

  double pred_total_power = 0.0;
  for(uns proc_id = 0; proc_id < NUM_CORES; proc_id++) {
    pred_total_power += power_pred_domain_power(POWER_DOMAIN_CORE_0 + proc_id,
                                                core_cycle_times[proc_id],
                                                slowdowns[proc_id]);
    pred_total_power += memory_access_fracs[proc_id] *
                        power_pred_domain_power(POWER_DOMAIN_MEMORY,
                                                memory_cycle_time,
                                                slowdowns[proc_id]);
  }
  pred_total_power += uncore_power + other_power;

  return pred_total_power / ref_total_power;
}

double power_pred_domain_power(Power_Domain domain, uns target_cycle_time,
                               double slowdown) {
  double min_voltage = power_intf_result(domain, POWER_RESULT_MIN_VOLTAGE);

  double ref_freq    = power_intf_result(domain, POWER_RESULT_FREQUENCY);
  double target_freq = 1.0e15 / (double)target_cycle_time;
  double freq_ratio  = target_freq / ref_freq;

  double ref_voltage    = power_intf_result(domain, POWER_RESULT_VOLTAGE);
  double target_voltage = MAX2(freq_ratio * ref_voltage, min_voltage);
  double voltage_ratio  = target_voltage / ref_voltage;

  /* Estimate dynamic power by scaling dynamic energy (E=1/2*C*V^2)
     and dividing by predicted time. This ignores effect of clock
     dynamic power. */
  double dynamic_power = power_intf_result(domain, POWER_RESULT_DYNAMIC) *
                         voltage_ratio * voltage_ratio / slowdown;
  /* P=V*N*k*Ileak (from Butts & Sohi, "A Static Power Model for Architects") */
  double static_power = power_intf_result(domain, POWER_RESULT_STATIC) *
                        voltage_ratio;

  return dynamic_power + static_power;
}
