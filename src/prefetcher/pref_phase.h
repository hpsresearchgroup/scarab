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
 * File         : pref_phase.h
 * Author       : HPS Research Group
 * Date         : 11/16/2004
 * Description  :
 ***************************************************************************************/
#ifndef __PREF_PHASE_H__
#define __PREF_PHASE_H__

#include "pref_common.h"

/***************************************************************************************
 * Phase Based Prefetching:
 * This prefetcher works by predicting the future memory access pattern based on
 * the current access pattern. This is currently modeled more as collection of
 * accesses rather than as a permutation of the accesses -> Order is not
important.

 * Currently this prefetcher collects the L2 miss pattern for the current
"phase"
 * which is based on number of instructions retired.
 *
 * Largest Prime < 16384 = 16381
***************************************************************************************/
#define MAX_PREF_PHASE_REGIONENTRIES 64

// This struct keeps info on the regions being targeted.
typedef struct Phase_Region_Struct {
  Addr PageNumber;
  Flag RegionMemAccess[MAX_PREF_PHASE_REGIONENTRIES];  // This is the access
                                                       // pattern
                                                       // for this region

  Counter last_access;
  Flag    valid;
} Phase_Region;

typedef struct PhaseInfoEntry_Struct {
  Flag* MemAccess;  // This is the access pattern for the whole of memory
  // for the last interval

  Phase_Region* mapped_regions;  // Given the last phase, what is the current
                                 // access pattern

  Counter last_access;  // used for lru
  Flag    valid;
} PhaseInfoEntry;

typedef struct Pref_PHASE_Struct {
  HWP_Info* hwp_info;

  PhaseInfoEntry* phase_table;

  Counter interval_start;

  uns curr_phaseid;  // Current phase entry we are prefetching for

  Flag* MemAccess;  // Current miss pattern - used to find the next phase
  Phase_Region* mapped_regions;  // Used to update the phase table

  uns     currsent_regid;
  uns     currsent_regid_offset;
  Counter num_misses;
} Pref_PHASE;

/*************************************************************/
/* HWP Interface */
void pref_phase_init(HWP* hwp);
void pref_phase_ul1_train(Addr lineAddr, Addr loadPC, Flag pref_hit);
void pref_phase_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                         uns32 global_hist);
void pref_phase_ul1_hit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                        uns32 global_hist);

void pref_phase_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                            uns32 global_hist);

/*************************************************************/
/* Misc functions */

void pref_phase_updateregioninfo(Phase_Region*, Addr lineAddr);

int pref_phase_computenextphase(void);

#endif /*  __PREF_PHASE_H__*/
