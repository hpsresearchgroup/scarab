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
 * File         : scarab_dummy_client_test.cc
 * Author       : HPS Research Group
 * Date         : 1/16/2019
 * Description  :
 ***************************************************************************************/

#include <algorithm>
#include "../frontend/pin_exec_driven_fe.h"
#include "../frontend/pin_trace_read.h"
#include "../op.h"
#include "../pin/pin_lib/message_queue_interface_lib.h"
#include "../pin/pin_lib/pin_scarab_common_lib.h"
#include "gtest/gtest.h"

#ifndef TEST_SOCKET_FILE
#define TEST_SOCKET_FILE "/tmp/test_socket.tmp"
#endif

const char* PIN_EXEC_DRIVEN_FE_SOCKET = TEST_SOCKET_FILE;

#ifndef NUM_CLIENTS
#define NUM_CLIENTS 1
#endif

#define CLIENT_TRACE_FILE ((const char*)"./simple_loop.trace.bz2")
#define NUM_OPS_IN_PACKET 10


#define NEW_GTEST(testname, servername, clientname)                          \
  TEST(ScarabDummyClientTest, testname) {                                    \
    pthread_t scarab_thread;                                                 \
    pthread_t client_thread[NUM_CLIENTS];                                    \
                                                                             \
    read_trace_file_into_memory();                                           \
    client.resize(NUM_CLIENTS, nullptr);                                     \
                                                                             \
    ::pthread_create(&scarab_thread, nullptr, scarab_test_##servername,      \
                     nullptr);                                               \
    ::sleep(1);                                                              \
                                                                             \
    int i_array[NUM_CLIENTS];                                                \
    for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {                              \
      i_array[i] = i;                                                        \
      ::pthread_create(&client_thread[i], nullptr, client_test_##clientname, \
                       (void*)&i_array[i]);                                  \
    }                                                                        \
                                                                             \
                                                                             \
    ::pthread_join(scarab_thread, nullptr);                                  \
    for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {                              \
      ::pthread_join(client_thread[i], nullptr);                             \
    }                                                                        \
                                                                             \
    scarab_teardown();                                                       \
    client_teardown();                                                       \
  }

std::vector<Client*>       client;
std::vector<compressed_op> trace;
std::vector<uint32_t>      scarab_side_trace_index;

extern std::vector<ScarabOpBuffer_type> cached_cop_buffers;

void  setup_dummy_globals();
void  teardown_dummy_globals();
void  scarab_setup();
void  scarab_teardown();
void  client_setup(uint32_t i);
void  client_teardown();
void* scarab_test_CanFetchOp(void*);
void* scarab_test_FetchOp(void*);
void* scarab_test_Retire(void*);
void* client_test_Retire(void*);
void* client_test_DummyClient(void*);
void* scarab_test(void*);
void* client_test2(void*);
void  read_trace_file_into_memory();

/*********************************************************************
 * Gtest functions
 *********************************************************************/

NEW_GTEST(CanFetchOp, CanFetchOp, DummyClient);
NEW_GTEST(FetchOp, FetchOp, DummyClient);
NEW_GTEST(Retire, Retire, Retire);

/*********************************************************************
 * Test Functions
 *********************************************************************/

void* scarab_test_CanFetchOp(void* ptr) {
  scarab_setup();

  std::vector<uint32_t> expectedBufferSize(NUM_CLIENTS, NUM_OPS_IN_PACKET);
  std::vector<uint32_t> numOpsRemaining(NUM_CLIENTS, trace.size());

  for(uint32_t j = 0; j < trace.size(); ++j) {
    for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
      compressed_op cop;
      bool          success = pin_exec_driven_can_fetch_op(i);

      bool trace_success = scarab_side_trace_index[i] < trace.size();
      if(success) {
        cop = trace[scarab_side_trace_index[i]];
        scarab_side_trace_index[i]++;
      }

      EXPECT_EQ(memcmp(&cached_cop_buffers[i].front(), &cop, sizeof(cop)), 0);
      EXPECT_EQ(cached_cop_buffers[i].size(), expectedBufferSize[i]);
      EXPECT_EQ(success, j < trace.size());
      EXPECT_EQ(success, trace_success);

      expectedBufferSize[i]--;
      numOpsRemaining[i]--;
      if(expectedBufferSize[i] == 0)
        expectedBufferSize[i] = std::min((uint32_t)NUM_OPS_IN_PACKET,
                                         numOpsRemaining[i]);
      cached_cop_buffers[i].pop_front();
    }
  }

  EXPECT_NE(trace.size(), 0);
}

void* scarab_test_FetchOp(void* ptr) {
  scarab_setup();

  std::vector<uint32_t> expectedBufferSize(NUM_CLIENTS, NUM_OPS_IN_PACKET);
  std::vector<uint32_t> numOpsRemaining(NUM_CLIENTS, trace.size());

  for(uint32_t j = 0; j < trace.size(); ++j) {
    for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
      Op op;

      do {
        pin_exec_driven_can_fetch_op(i);

        EXPECT_EQ(
          memcmp(&cached_cop_buffers[i].front(),
                 &trace[scarab_side_trace_index[i]], sizeof(compressed_op)),
          0);
        EXPECT_EQ(cached_cop_buffers[i].size(), expectedBufferSize[i]);

        pin_exec_driven_fetch_op(i, &op);

        EXPECT_EQ(op.fetch_addr,
                  trace[scarab_side_trace_index[i]].instruction_addr);
      } while(!op.eom);

      scarab_side_trace_index[i]++;
      expectedBufferSize[i]--;
      numOpsRemaining[i]--;

      if(expectedBufferSize[i] == 0)
        expectedBufferSize[i] = std::min((uint32_t)NUM_OPS_IN_PACKET,
                                         numOpsRemaining[i]);
    }
  }

  EXPECT_NE(trace.size(), 0);
}

void* scarab_test_Retire(void* ptr) {
  scarab_setup();

  pin_exec_driven_retire(0, 0);
  pin_exec_driven_retire(0, 1);
  pin_exec_driven_retire(0, 2);
  pin_exec_driven_retire(0, 3);
  pin_exec_driven_retire(0, 4);
}

void* client_test_Retire(void* ptr) {
  uint32_t client_id = *((uint32_t*)ptr);
  client_setup(client_id);
  Scarab_To_Pin_Msg msg;

  ::sleep(5);

  msg = client[client_id]->pin_receive<Scarab_To_Pin_Msg>();
  EXPECT_EQ(msg.inst_uid, 0);

  msg = client[client_id]->pin_receive<Scarab_To_Pin_Msg>();
  EXPECT_EQ(msg.inst_uid, 1);

  msg = client[client_id]->pin_receive<Scarab_To_Pin_Msg>();
  EXPECT_EQ(msg.inst_uid, 2);

  msg = client[client_id]->pin_receive<Scarab_To_Pin_Msg>();
  EXPECT_EQ(msg.inst_uid, 3);

  msg = client[client_id]->pin_receive<Scarab_To_Pin_Msg>();
  EXPECT_EQ(msg.inst_uid, 4);
}

void* client_test_DummyClient(void* ptr) {
  uint32_t client_id = *((uint32_t*)ptr);
  client_setup(client_id);

  bool     done     = false;
  uint32_t numSends = 0, numOps = 0;

  while(!done) {
    Scarab_To_Pin_Msg msg = client[client_id]->receive<Scarab_To_Pin_Msg>();
    switch(msg.type) {
      case FE_FETCH_OP: {
        ScarabOpBuffer_type buffer;

        for(uint32_t i = 0; i < NUM_OPS_IN_PACKET; ++i) {
          compressed_op cop;
          bool          success = numOps < trace.size();

          if(!success) {
            done = true;
            break;
          } else {
            cop = trace[numOps];
          }

          numOps++;
          buffer.push_back(cop);
        }

        client[client_id]->send<ScarabOpBuffer_type>(buffer);
        numSends++;

        break;
      }
      default:
        EXPECT_EQ(1, 0);
    }
  }

  EXPECT_EQ(trace.size(), numOps);
  EXPECT_EQ((trace.size() / NUM_OPS_IN_PACKET) + 1, numSends);
}


/*********************************************************************
 * Common Functions
 *********************************************************************/

void read_trace_file_into_memory() {
  int file_success = 1;
  pin_trace_file_pointer_init(1);
  pin_trace_open(0, CLIENT_TRACE_FILE);

  trace.clear();
  while(file_success) {
    compressed_op cop;
    file_success = pin_trace_read(0, &cop);
    trace.push_back(cop);
  }

  pin_trace_close(0);
}

void setup_dummy_globals() {
  op_count   = (Counter*)malloc(sizeof(Counter) * NUM_CLIENTS);
  inst_count = (Counter*)malloc(sizeof(Counter) * NUM_CLIENTS);
  // op_count = new Counter [NUM_CLIENTS];
  // inst_count = new Counter [NUM_CLIENTS];
  unique_count_per_core = new Counter[NUM_CLIENTS];
  trace_read_done       = new Flag[NUM_CLIENTS];
  std::fill_n(trace_read_done, NUM_CLIENTS, 0);
}

void teardown_dummy_globals() {
  free(op_count);
  free(inst_count);
  delete unique_count_per_core;
  delete trace_read_done;
}

void scarab_setup() {
  pin_exec_driven_init(NUM_CLIENTS);
  scarab_side_trace_index.clear();
  scarab_side_trace_index.resize(NUM_CLIENTS, 0);
  setup_dummy_globals();
}

void scarab_teardown() {
  teardown_dummy_globals();
}

void client_setup(uint32_t i) {
  client[i] = new Client(TEST_SOCKET_FILE);
}

void client_teardown() {
  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    delete client[i];
  }
}


/******************************************************************/
