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
 * File         : debug/pipeview.c
 * Author       : HPS Research Group
 * Date         : 4/24/2012
 * Description  : Pipeline visualization tracing.
 ***************************************************************************************/

#include "debug/pipeview.h"
#include "core.param.h"
#include "debug/debug_print.h"
#include "general.param.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "op.h"

/**************************************************************************************

Input file format:
O3PipeView:fetch:<timestamp>:<inst addr>:<uop addr>:<seq num>:<disasm>
O3PipeView:<event>:<timestamp>
<event> can be map, issue, sched, etc.
All events for a uop must be on consecutive lines

***************************************************************************************/

/**************************************************************************************/
/* Global variables: */

static FILE** files = NULL;

/**************************************************************************************/
/* Constants: */

static const char* PREFIX = "O3PipeView";

/**************************************************************************************/
/* Local prototypes: */

void print_header(FILE*, Op*);
void print_event(FILE*, Op*, const char*, Counter);

/**************************************************************************************/
/* pipeview_init: */

void pipeview_init(void) {
  files = malloc(sizeof(FILE*) * NUM_CORES);
  if(PIPEVIEW) {
    for(uns proc_id = 0; proc_id < NUM_CORES; ++proc_id) {
      char filename[MAX_STR_LENGTH + 1];
      sprintf(filename, "%s.%d.trace", PIPEVIEW_FILE, proc_id);
      files[proc_id] = fopen(filename, "w");
      ASSERT(proc_id, files[proc_id]);
    }
  }
}

/**************************************************************************************/
/* pipeview_print_op: */

void pipeview_print_op(struct Op_struct* op) {
  if(!DEBUG_RANGE_COND(op->proc_id))
    return;

  FILE* file = files[op->proc_id];
  print_header(file, op);
  if(op->off_path) {
    print_event(file, op, "fetch_offpath", op->fetch_cycle);
  } else {
    print_event(file, op, "fetch", op->fetch_cycle);
  }
  print_event(file, op, "decode", op->fetch_cycle + 1);
  print_event(file, op, "decode_done", op->fetch_cycle + 1 + DECODE_CYCLES);
  print_event(file, op, "map", op->map_cycle);
  print_event(file, op, "map_done", op->map_cycle + MAP_CYCLES);
  print_event(file, op, "issue", op->issue_cycle);
  print_event(file, op, "issue_done", op->issue_cycle + 1);
  if(op->srcs_not_rdy_vector == 0) {
    // op was ready at rdy_cycle only if all sources are ready
    print_event(file, op, "ready", MAX2(op->rdy_cycle, op->issue_cycle + 1));
  } else {
    ASSERT(op->proc_id, op->off_path);
  }
  print_event(file, op, "sched", op->sched_cycle);
  print_event(file, op, "exec", op->exec_cycle);
  print_event(file, op, "dcache", op->dcache_cycle);
  print_event(file, op, "done", op->done_cycle);
  if(op->off_path) {
    print_event(file, op, "flush", cycle_count);
    print_event(file, op, "end", cycle_count);
  } else {
    ASSERT(op->proc_id, op->retire_cycle <= cycle_count);
    print_event(file, op, "retire", op->retire_cycle);
    print_event(file, op, "end", op->retire_cycle);
  }
}

/**************************************************************************************/
/* pipeview_done: */

void pipeview_done(void) {
  if(PIPEVIEW) {
    for(uns proc_id = 0; proc_id < NUM_CORES; ++proc_id) {
      fclose(files[proc_id]);
    }
  }
}

/**************************************************************************************/
/* print_event: */

void print_event(FILE* file, Op* op, const char* name, Counter cycle) {
  /* print only events that make sense because flushed ops may not
     have all *_cycle fields set and non mem ops will not have
     dcache_cycle set  */
  if(cycle >= op->fetch_cycle && cycle <= cycle_count) {
    fprintf(file, "%s:%s:%lld\n", PREFIX, name, cycle);
  }
}

/**************************************************************************************/
/* print_header: */

void print_header(FILE* file, Op* op) {
  fprintf(file, "%s:new:%lld:%llx:%d:%lld:%s\n", PREFIX, op->fetch_cycle,
          op->inst_info->addr, 0, op->unique_num_per_proc, disasm_op(op, TRUE));
}
