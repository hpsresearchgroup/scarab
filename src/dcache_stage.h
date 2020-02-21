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
 * File         : dcache_stage.h
 * Author       : HPS Research Group
 * Date         : 3/8/1999
 * Description  :
 ***************************************************************************************/

#ifndef __DCACHE_STAGE_H__
#define __DCACHE_STAGE_H__

#include "libs/cache_lib.h"
#include "stage_data.h"

/**************************************************************************************/
/* Forward Declarations */

struct Mem_Req_struct;
struct Ports_struct;

/**************************************************************************************/
/* Types */

typedef struct Dcache_Stage_struct {
  uns8       proc_id;
  Stage_Data sd; /* stage interface data */

  Cache  dcache;      /* the data cache */
  Ports* ports;       /* read and write ports to the data cache (per bank) */
  Cache  pref_dcache; /* prefetcher cache for data cache */

  Counter idle_cycle;  /* Cycle the cache will be idle */
  Flag    mem_blocked; /* Are memory request buffers (aka MSHRs) full? */

  char rand_wb_state[31]; /* state of random number generator for random
                             writebacks */
} Dcache_Stage;


typedef struct Dcache_Data_struct {
  Flag dirty;       /* is the line dirty? */
  Flag prefetch;    /* was the line prefetched? */
  Flag HW_prefetch; /* was the hardware prefetcher - Be careful with this when
                       using multiple prefetchers */
  Flag
      HW_prefetched; /* stick HW_prefetch - always set even if the data is used */
  uns read_count[2];  /* number of reads, including the first */
  uns write_count[2]; /* number of writes, including the first */
  uns misc_state;     /* bit 0: was line most recently accessed by off-path op?
                       * bit 1: was line brought into cache by off-path op? */
  Counter rdy_cycle;
  Flag    fetched_by_offpath; /* fetched by an off_path op? */
  Addr    offpath_op_addr;    /* PC of the off path op that fetched this line */
  Counter
    offpath_op_unique; /* unique of the off path op that fetched this line */

  Counter fetch_cycle;      /* when was this data fetched into the cache? */
  Counter onpath_use_cycle; /* when was this data last used by correct path? */
} Dcache_Data;


/**************************************************************************************/
/* External variables */

extern Dcache_Stage* dc;

/**************************************************************************************/
/* Prototypes */

void set_dcache_stage(Dcache_Stage*);
void init_dcache_stage(uns8, const char*);
void reset_dcache_stage(void);
void recover_dcache_stage(void);
void debug_dcache_stage(void);
void update_dcache_stage(Stage_Data*);
void cmp_update_dcache_stage(Stage_Data*, uns);
void wp_process_dcache_hit(Dcache_Data* line, Op* op);
void wp_process_dcache_fill(Dcache_Data* line, Mem_Req* req);
Flag dcache_fill_line(Mem_Req*);
void update_iso_miss(Op*);
Flag do_oracle_dcache_access(Op*, Addr*);

/**************************************************************************************/

#endif /* #ifndef __DCACHE_STAGE_H__ */
