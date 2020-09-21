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
 * File         : frontend/pin_exec_driven_fe.cc
 * Author       : HPS Research Group
 * Date         :
 * Description  :
 ****************************************************************************************/

extern "C" {
#include "debug/debug.param.h"
#include "general.param.h"
#include "globals/assert.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"
#include "globals/utils.h"
#include "op.h"
#include "sim.h"
}

#include "frontend/pin_exec_driven_fe.h"
#include "pin/pin_lib/message_queue_interface_lib.h"
#include "pin/pin_lib/pin_scarab_common_lib.h"
#include "pin/pin_lib/uop_generator.h"

#include <time.h>

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_PIN_EXEC_DRIVEN, ##args)

Server*                          server;
std::vector<ScarabOpBuffer_type> cached_cop_buffers;

void get_next_op_buffer_from_pin(uns proc_id);
void update_op_buffer_if_empty(uns proc_id);
void invalidate_op_buffer(uns proc_id);


/**********************************************************
 * Cached Op interface
 **********************************************************/
void get_next_op_buffer_from_pin(uns proc_id) {
  Scarab_To_Pin_Msg msg;
  msg.type      = FE_FETCH_OP;
  msg.inst_addr = 0;
  msg.inst_uid  = 0;

  server->send(proc_id, (Message<Scarab_To_Pin_Msg>)msg);  // blocking
  cached_cop_buffers[proc_id] = server->receive<ScarabOpBuffer_type>(
    proc_id);  // blocking
}

void update_op_buffer_if_empty(uns proc_id) {
  if(cached_cop_buffers[proc_id].size() == 0) {
    DEBUG(proc_id, "Calling FETCH_OP to PIN\n");
    get_next_op_buffer_from_pin(proc_id);
  }
}

inline void invalidate_op_buffer(uns proc_id) {
  cached_cop_buffers[proc_id].clear();
}

Addr get_fetch_address(uns proc_id, compressed_op* cop) {
  return convert_to_cmp_addr(proc_id, cop->instruction_addr);
}

/**********************************************************
 * PIN Exec Driven Interface Functions
 **********************************************************/
void pin_exec_driven_init(uns numProcs) {
  server = new Server(PIN_EXEC_DRIVEN_FE_SOCKET, numProcs);
  cached_cop_buffers.resize(numProcs);
  uop_generator_init(numProcs);
}

void pin_exec_driven_done(Flag* retired_exit) {
  // Send final exit message, telling client to stop running.
  for(uint32_t i = 0; i < server->getNumClients(); ++i) {
    if(!retired_exit[i]) {
      pin_exec_driven_retire(i, -1);
    }
  }

  // Must wait for all clients to close socket before we shutdown,
  // otherwise they may crash when reading the final retire.
  for(uint32_t i = 0; i < server->getNumClients(); ++i) {
    server->wait_for_client_to_close(i);
  }
  delete server;
}


Flag pin_exec_driven_can_fetch_op(uns proc_id) {
  DEBUG(proc_id, "Can Fetch Op begin:\n");
  update_op_buffer_if_empty(proc_id);

  return !cached_cop_buffers[proc_id].empty() &&
         !is_sentinal_op(&cached_cop_buffers[proc_id].front());
}

Addr pin_exec_driven_next_fetch_addr(uns proc_id) {
  DEBUG(proc_id, "Next Fetch Addr begin:\n");
  update_op_buffer_if_empty(proc_id);

  Addr next_fetch_addr = get_fetch_address(
    proc_id, &cached_cop_buffers[proc_id].front());
  ASSERT_PROC_ID_IN_ADDR(proc_id, next_fetch_addr);
  return next_fetch_addr;
}

void pin_exec_driven_fetch_op(uns proc_id, Op* op) {
  DEBUG(proc_id, "Fetch Op begin:\n");
  update_op_buffer_if_empty(proc_id);

  Flag eom = uop_generator_extract_op(proc_id, op,
                                      &cached_cop_buffers[proc_id].front());
  if(eom)
    cached_cop_buffers[proc_id].pop_front();

  DEBUG(proc_id, "Fetch Op end: %llx (%llu)\n", op->fetch_addr, op->inst_uid);
}

void pin_exec_driven_redirect(uns proc_id, uns64 inst_uid, Addr fetch_addr) {
  DEBUG(proc_id, "Fetch Redirect: %llx (%llu)\n", fetch_addr, inst_uid);
  /* PIN will asynchronously redirect, Scarab does not need to wait for PIN to
   * finish. Processes will synchronize when Scarab sends next command to PIN*/
  Scarab_To_Pin_Msg msg;
  msg.type      = FE_REDIRECT;
  msg.inst_addr = convert_to_cmp_addr(0, fetch_addr);  // removing proc_id
  msg.inst_uid  = inst_uid;
  uop_generator_recover(proc_id);

  server->send(proc_id, (Message<Scarab_To_Pin_Msg>)msg);  // blocking
  invalidate_op_buffer(proc_id);
  DEBUG(proc_id, "Fetch Redirect end: %llx\n", fetch_addr);
}

void pin_exec_driven_recover(uns proc_id, uns64 inst_uid) {
  DEBUG(proc_id, "Fetch Recover: %llu\n", inst_uid);
  /* PIN will asynchronously recover, Scarab does not need to wait for PIN to
   * finish. Processes will synchronize when Scarab sends next command to PIN*/
  Scarab_To_Pin_Msg msg;
  msg.type      = FE_RECOVER_AFTER;
  msg.inst_addr = 0;
  msg.inst_uid  = inst_uid;
  uop_generator_recover(proc_id);

  server->send(proc_id, (Message<Scarab_To_Pin_Msg>)msg);  // blocking
  invalidate_op_buffer(proc_id);
  DEBUG(proc_id, "Fetch Recover end: %llu\n", inst_uid);
}

void pin_exec_driven_retire(uns proc_id, uns64 inst_uid) {
  DEBUG(proc_id, "Fetch Retire: %llu\n", inst_uid);
  Scarab_To_Pin_Msg msg;
  msg.type      = FE_RETIRE;
  msg.inst_addr = inst_uid == (uns64)-1;
  msg.inst_uid  = inst_uid;

  server->send(proc_id, (Message<Scarab_To_Pin_Msg>)msg);  // blocking
  DEBUG(proc_id, "Fetch Retire end: %llu\n", inst_uid);
}
