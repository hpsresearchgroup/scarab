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
 * File         : pref_phase.c
 * Author       : HPS Research Group
 * Date         :
 * Description  :
 ***************************************************************************************/

#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"

#include "globals/assert.h"
#include "globals/utils.h"
#include "op.h"

#include "core.param.h"
#include "dcache_stage.h"
#include "debug/debug.param.h"
#include "general.param.h"
#include "libs/cache_lib.h"
#include "libs/hash_lib.h"
#include "libs/list_lib.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "prefetcher/l2l1pref.h"
#include "prefetcher/pref.param.h"
#include "prefetcher/pref_common.h"
#include "prefetcher/pref_phase.h"
#include "prefetcher/pref_phase.param.h"
#include "statistics.h"

/**************************************************************************************/
/* Macros */
#define DEBUG(args...) _DEBUG(DEBUG_PREF_PHASE, ##args)

#define PAGENUM(x) (x >> PREF_PHASE_LOG2REGIONSIZE)

Pref_PHASE* phase_hwp;

FILE* PREF_PHASE_OUT;

void pref_phase_init(HWP* hwp) {
  int          ii;
  static char* pref_phase_filename = "pref_phase";

  if(!PREF_PHASE_ON)
    return;

  phase_hwp                    = (Pref_PHASE*)malloc(sizeof(Pref_PHASE));
  phase_hwp->hwp_info          = hwp->hwp_info;
  phase_hwp->hwp_info->enabled = TRUE;

  phase_hwp->phase_table = (PhaseInfoEntry*)calloc(PREF_PHASE_TABLE_SIZE,
                                                   sizeof(PhaseInfoEntry));

  for(ii = 0; ii < PREF_PHASE_TABLE_SIZE; ii++) {
    phase_hwp->phase_table[ii].MemAccess = (Flag*)calloc(PREF_PHASE_INFOSIZE,
                                                         sizeof(Flag));
    phase_hwp->phase_table[ii].mapped_regions = (Phase_Region*)calloc(
      PREF_PHASE_TRACKEDREGIONS, sizeof(Phase_Region));
  }
  phase_hwp->MemAccess      = (Flag*)calloc(PREF_PHASE_INFOSIZE, sizeof(Flag));
  phase_hwp->mapped_regions = (Phase_Region*)calloc(PREF_PHASE_TRACKEDREGIONS,
                                                    sizeof(Phase_Region));
  phase_hwp->interval_start = 0;
  phase_hwp->curr_phaseid   = 0;
  phase_hwp->num_misses     = 0;

  if(PREF_PHASE_STUDY) {
    PREF_PHASE_OUT = file_tag_fopen(NULL, pref_phase_filename, "w");
  }

  ASSERTM(0, PREF_PHASE_REGIONENTRIES <= MAX_PREF_PHASE_REGIONENTRIES,
          "The value of PREF_PHASE_REGIONENTRIES knob (%d) should be less than "
          "MAX_PREF_PHASE_REGIONENTRIES constant (%d)",
          PREF_PHASE_REGIONENTRIES, MAX_PREF_PHASE_REGIONENTRIES);
}

void pref_phase_ul1_hit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                        uns32 global_hist) {
  // Do nothing on a ul1 hit
}

void pref_phase_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC,
                            uns32 global_hist) {
  pref_phase_ul1_train(lineAddr, loadPC, TRUE);  // FIXME
}

void pref_phase_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC,
                         uns32 global_hist) {
  pref_phase_ul1_train(lineAddr, loadPC, FALSE);  // FIXME
}

void pref_phase_ul1_train(Addr lineAddr, Addr loadPC, Flag pref_hit) {
  int next_phaseid;

  Flag qFull     = FALSE;
  Addr lineIndex = lineAddr >> LOG2(DCACHE_LINE_SIZE);
  Addr hashIndex = lineIndex % PREF_PHASE_PRIME_HASH;

  // Update access pattern
  phase_hwp->MemAccess[hashIndex] = TRUE;

  pref_phase_updateregioninfo(phase_hwp->mapped_regions, lineIndex);

  phase_hwp->num_misses++;
  if(inst_count[0] - phase_hwp->interval_start > PREF_PHASE_INTERVAL) {
    phase_hwp->interval_start = inst_count[0];
    if(phase_hwp->num_misses > PREF_PHASE_MIN_MISSES) {
      if(PREF_PHASE_STUDY) {
        int ii;
        for(ii = 0; ii < PREF_PHASE_INFOSIZE; ii++) {
          fprintf(PREF_PHASE_OUT, (phase_hwp->MemAccess[ii] ? "1" : "0"));
        }
        fprintf(PREF_PHASE_OUT, "\n");
      }

      phase_hwp->num_misses = 0;
      next_phaseid          = pref_phase_computenextphase();
      STAT_EVENT(0, PREF_PHASE_NEWPHASE_DET);
      {
        // Set the memAccess pattern for the next phase correctly
        Flag* tmp            = phase_hwp->MemAccess;
        phase_hwp->MemAccess = phase_hwp->phase_table[next_phaseid].MemAccess;
        phase_hwp->phase_table[next_phaseid].MemAccess = tmp;

        memset(phase_hwp->MemAccess, 0, sizeof(Flag) * PREF_PHASE_INFOSIZE);
      }
      {
        // Set the mapped regions for the current region correctly
        Phase_Region* tmp = phase_hwp->mapped_regions;
        phase_hwp->mapped_regions =
          phase_hwp->phase_table[phase_hwp->curr_phaseid].mapped_regions;
        phase_hwp->phase_table[phase_hwp->curr_phaseid].mapped_regions = tmp;

        memset(phase_hwp->mapped_regions, 0,
               sizeof(Phase_Region) * PREF_PHASE_TRACKEDREGIONS);
      }

      if(!phase_hwp->phase_table[next_phaseid].valid) {
        STAT_EVENT(0, PREF_PHASE_NEWPHASE_NOTVALID);

        phase_hwp->phase_table[next_phaseid].valid = TRUE;
        memset(phase_hwp->phase_table[next_phaseid].mapped_regions, 0,
               sizeof(Phase_Region) * PREF_PHASE_TRACKEDREGIONS);
      }

      phase_hwp->curr_phaseid                          = next_phaseid;
      phase_hwp->phase_table[next_phaseid].last_access = cycle_count;
      phase_hwp->currsent_regid                        = 0;
      phase_hwp->currsent_regid_offset                 = 0;
    }
  }
  // Send out prefetches
  while(phase_hwp->currsent_regid < PREF_PHASE_REGIONENTRIES) {
    Addr          lineIndex, startIndex;
    Phase_Region* region = &phase_hwp->phase_table[phase_hwp->curr_phaseid]
                              .mapped_regions[phase_hwp->currsent_regid];
    if(region->valid) {
      startIndex = region->PageNumber
                   << (PREF_PHASE_LOG2REGIONSIZE - LOG2(DCACHE_LINE_SIZE));

      for(; phase_hwp->currsent_regid_offset < PREF_PHASE_REGIONENTRIES;
          phase_hwp->currsent_regid_offset++) {
        if(region->RegionMemAccess[phase_hwp->currsent_regid_offset]) {
          lineIndex = startIndex + phase_hwp->currsent_regid_offset;
          if(!pref_addto_ul1req_queue(0, lineIndex,
                                      phase_hwp->hwp_info->id)) {  // FIXME
            qFull = TRUE;
            break;
          }
          STAT_EVENT(0, PREF_PHASE_SENTPREF);
        }
      }
      if(qFull)
        break;
    }
    phase_hwp->currsent_regid++;
    phase_hwp->currsent_regid_offset = 0;
  }
}


void pref_phase_updateregioninfo(Phase_Region* mapped_regions, Addr lineAddr) {
  int  ii, id;
  Addr pagenum       = PAGENUM(lineAddr);
  int  region_offset = (lineAddr >> LOG2(DCACHE_LINE_SIZE)) &
                      N_BIT_MASK(PREF_PHASE_LOG2REGIONSIZE -
                                 LOG2(DCACHE_LINE_SIZE));
  id = -1;

  for(ii = 0; ii < PREF_PHASE_TRACKEDREGIONS; ii++) {
    if(mapped_regions[ii].valid && mapped_regions[ii].PageNumber == pagenum) {
      id = ii;
      break;
    }
    if(!mapped_regions[ii].valid || id == -1) {
      id = ii;
    } else if(mapped_regions[id].valid &&
              mapped_regions[ii].last_access < mapped_regions[id].last_access) {
      id = ii;
    }
  }
  if(!mapped_regions[id].valid || mapped_regions[id].PageNumber != pagenum) {
    memset(mapped_regions[id].RegionMemAccess, 0,
           sizeof(Flag) * PREF_PHASE_REGIONENTRIES);
  }
  if(mapped_regions[id].PageNumber != pagenum) {
    STAT_EVENT(0, PREF_PHASE_OVERWRITE_PAGE);
  }
  mapped_regions[id].PageNumber                     = pagenum;
  mapped_regions[id].last_access                    = cycle_count;
  mapped_regions[id].valid                          = TRUE;
  mapped_regions[id].RegionMemAccess[region_offset] = TRUE;
}

int pref_phase_computenextphase(void) {
  int ii, jj;
  int id = -1;
  for(ii = 0; ii < PREF_PHASE_TABLE_SIZE; ii++) {
    if(phase_hwp->phase_table[ii].valid) {
      int   diffnum = 0;
      int   missnum = 0;
      float missper = 0.0;
      for(jj = 0; jj < PREF_PHASE_INFOSIZE; jj++) {
        if(phase_hwp->MemAccess[jj] !=
           phase_hwp->phase_table[ii].MemAccess[jj]) {
          diffnum++;
        }
        if(phase_hwp->MemAccess[jj] == 1) {
          missnum++;
        }
      }
      missper = (1.0 * diffnum) / (1.0 * missnum);
      if(diffnum < PREF_PHASE_MAXDIFF_THRESH && missper < PREF_PHASE_MISSPER) {
        // Found a match
        return ii;
      }
    }
    if(id == -1 || !phase_hwp->phase_table[ii].valid) {
      id = ii;
    } else if(phase_hwp->phase_table[id].valid &&
              phase_hwp->phase_table[ii].last_access <
                phase_hwp->phase_table[id].last_access) {
      id = ii;
    }
  }
  // Taken another entry...
  // So set it to invalid
  phase_hwp->phase_table[id].valid = FALSE;
  return id;
}
