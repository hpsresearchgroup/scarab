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

#include "scarab_interface.h"

Scarab_To_Pin_Msg get_scarab_cmd() {
  Scarab_To_Pin_Msg cmd;

  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "START: Receiving from Scarab\n");
  cmd = scarab->receive<Scarab_To_Pin_Msg>();
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "END: %d Received from Scarab\n", cmd.type);

  return cmd;
}

void insert_scarab_op_in_buffer(compressed_op& cop) {
  scarab_op_buffer.push_back(cop);
}

bool scarab_buffer_full() {
  return scarab_op_buffer.size() > (max_buffer_size - 2);
  // Two spots are always reserved in the buffer just in case the
  // exit syscall and sentinel nullop are
  // the last two elements of a packet sent to Scarab.
}

void scarab_send_buffer() {
  Message<ScarabOpBuffer_type> message = scarab_op_buffer;
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "START: Sending message to Scarab.\n");
  scarab->send(message);
  DBG_PRINT(uid_ctr, dbg_print_start_uid, dbg_print_end_uid,
            "END: Sending message to Scarab.\n");
  scarab_op_buffer.clear();
}

void scarab_clear_all_buffers() {
  scarab_op_buffer.clear();
  op_mailbox_full = false;
}
