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
 * File         : cmp_model_support.c
 * Author       : HPS Research Group
 * Date         : 11/27/2006
 * Description  : CMP with runahead
 ***************************************************************************************/

#include "cmp_model_support.h"
#include "cmp_model.h"
#include "core.param.h"
#include "frontend/pin_trace_fe.h"
#include "general.param.h"
#include "globals/assert.h"
#include "globals/utils.h"
#include "packet_build.h"
#include "statistics.h"

/**************************************************************************************/
/* cmp_init_cmp_model  */
void cmp_init_cmp_model() {
  ASSERT(0, NUM_CORES <= 32 || NUM_CORES > 0);

  cmp_model.thread_data = (Thread_Data*)malloc(sizeof(Thread_Data) * NUM_CORES);

  cmp_model.map_data = (Map_Data*)malloc(sizeof(Map_Data) * NUM_CORES);

  cmp_model.pb_data = (Pb_Data*)malloc(sizeof(Pb_Data) *
                                       NUM_CORES);  // packet data

  cmp_model.bp_recovery_info = (Bp_Recovery_Info*)malloc(
    sizeof(Bp_Recovery_Info) * NUM_CORES);
  cmp_model.bp_data      = (Bp_Data*)malloc(sizeof(Bp_Data) * NUM_CORES);
  cmp_model.icache_stage = (Icache_Stage*)malloc(sizeof(Icache_Stage) *
                                                 NUM_CORES);
  cmp_model.decode_stage = (Decode_Stage*)malloc(sizeof(Decode_Stage) *
                                                 NUM_CORES);
  cmp_model.map_stage    = (Map_Stage*)malloc(sizeof(Map_Stage) * NUM_CORES);
  cmp_model.node_stage   = (Node_Stage*)malloc(sizeof(Node_Stage) * NUM_CORES);
  cmp_model.exec_stage   = (Exec_Stage*)malloc(sizeof(Exec_Stage) * NUM_CORES);
  cmp_model.dcache_stage = (Dcache_Stage*)malloc(sizeof(Dcache_Stage) *
                                                 NUM_CORES);
}


void cmp_init_thread_data(uns8 proc_id) {
  td->proc_id = proc_id;
  init_map(proc_id);
  init_list(&td->seq_op_list, "SEQ_OP_LIST", sizeof(Op*), TRUE);
  td->inst_addr = convert_to_cmp_addr(td->proc_id, 0);
}


/**************************************************************************************/
/* cmp_set_all_stages  */
void cmp_set_all_stages(uns8 proc_id) {
  set_thread_data(&cmp_model.thread_data[proc_id]);
  set_map_data(&td->map_data);

  set_pb_data(&cmp_model.pb_data[proc_id]);

  set_icache_stage(&cmp_model.icache_stage[proc_id]);
  set_decode_stage(&cmp_model.decode_stage[proc_id]);
  set_map_stage(&cmp_model.map_stage[proc_id]);
  set_node_stage(&cmp_model.node_stage[proc_id]);
  set_exec_stage(&cmp_model.exec_stage[proc_id]);
  set_dcache_stage(&cmp_model.dcache_stage[proc_id]);
}

/**************************************************************************************/
/* cmp_init_bogus_sim:
 *  Bogus simulation is used during multicore runs with the trace FE. Once a
 *  process terminates, it is restarted in bogus mode to create interference
 *  for other processes that have not terminated.
 *
 *  If using the exec FE, bogus mode is not needed because program can continue
 *  running after inst_limit is reached.
 */
void cmp_init_bogus_sim(uns8 proc_id) {
  trace_read_done[proc_id] = FALSE;
  reached_exit[proc_id]    = FALSE;
  retired_exit[proc_id]    = FALSE;

  cmp_set_all_stages(proc_id);

  trace_close_trace_file(proc_id);

  op_count[proc_id] = uop_count[proc_id] + 1;

  trace_setup(proc_id);
  ic->next_fetch_addr = trace_next_fetch_addr(proc_id);
  ASSERT_PROC_ID_IN_ADDR(ic->proc_id, ic->next_fetch_addr);

  td->inst_addr = ic->next_fetch_addr;
  ASSERT_PROC_ID_IN_ADDR(ic->proc_id, td->inst_addr);
  reset_seq_op_list(td);
  reset_map();

  reset_all_ops_icache_stage();
  reset_decode_stage();
  reset_map_stage();
  reset_all_ops_node_stage();
  reset_exec_stage();
  reset_dcache_stage();
}
