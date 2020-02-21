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
 * File         : server_client_socket_test.cc
 * Author       : HPS Research Group
 * Date         : 1/11/2019
 * Description  :
 ***************************************************************************************/

#include "../pin/pin_lib/message_queue_interface_lib.h"
#include "../pin/pin_lib/pin_scarab_common_lib.h"
#include "gtest/gtest.h"
#include "message_test_case.h"

#include <chrono>
#include <ctime>

#define ESTIMATED_OP_SIZE 94
#define NUM_REPEAT 1000

#ifndef TEST_SOCKET_FILE
#define TEST_SOCKET_FILE "/tmp/test_socket.tmp"
#endif

#ifndef NUM_CLIENTS
#define NUM_CLIENTS 1
#endif

#ifdef SERVER_TEST


class ServerTest : public ::testing::Test {
 protected:
  // called once in the beginning of all tests
  ServerTest() {
    printf("Attempting to open socket: %s\n", TEST_SOCKET_FILE);
    server = new Server(TEST_SOCKET_FILE, NUM_CLIENTS);
  }

  // called before ever test
  void SetUp() override {}

  void TearDown() override {}

  void server_bandwidth_test();

  Server*         server;
  MessageTestCase message_test;
};

TEST_F(ServerTest, ServerSendRecvTest) {
  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    server->send(i, message_test.char_message);
  }

  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    char test_char_message = server->receive<char>(i);
    EXPECT_EQ(message_test.expected_char_message, test_char_message);
  }

  /***********************************************/

  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    server->send(i, message_test.int_message);
  }

  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    int32_t test_int_message = server->receive<int32_t>(i);
    EXPECT_EQ(message_test.expected_int_message, test_int_message);
  }

  /***********************************************/

  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    server->send(i, message_test.long_int_message);
  }

  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    int64_t test_long_int_message = server->receive<int64_t>(i);
    EXPECT_EQ(message_test.expected_long_int_message, test_long_int_message);
  }

  /***********************************************/

  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    server->send(i, message_test.custom_message);
  }

  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    MessageTestCase::TestMsgStruct test_custom_message =
      server->receive<MessageTestCase::TestMsgStruct>(i);
    EXPECT_EQ(message_test.expected_custom_message, test_custom_message);
  }

  /***********************************************/

  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    server->send(i, message_test.vector_message);
  }

  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    std::vector<uint32_t> test_vector_message =
      server->receive<std::vector<uint32_t>>(i);
    EXPECT_EQ(message_test.expected_vector_message, test_vector_message);
  }

  /***********************************************/

  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    server->send(i, message_test.deque_message);
  }

  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    std::deque<uint32_t> test_deque_message =
      server->receive<std::deque<uint32_t>>(i);
    EXPECT_EQ(message_test.expected_deque_message, test_deque_message);
  }

  server_bandwidth_test();
}

// TEST_F(ServerTest, ServerBandwidthTest) {
void ServerTest::server_bandwidth_test() {
  std::chrono::duration<double> elapsed_time = std::chrono::duration<double>(
    0.0);

  for(uint32_t i = 0; i < NUM_CLIENTS; ++i) {
    for(uint32_t j = 0; j < NUM_REPEAT; ++j) {
      auto start = std::chrono::system_clock::now();
      server->send(i, message_test.super_big_message);

      std::vector<uint8_t> test_super_big_message =
        server->receive<std::vector<uint8_t>>(i);
      auto end = std::chrono::system_clock::now();

      elapsed_time += (end - start);

      EXPECT_EQ(message_test.expected_super_big_message,
                test_super_big_message);
    }
  }

  uint64_t total_bytes_sent = message_test.expected_super_big_message.size() *
                              2 * NUM_REPEAT * NUM_CLIENTS;

  fprintf(stderr,
          "Total Elapsed Time: %fs, Total Bandwidth: %f MBps (%f KOPS)\n",
          elapsed_time.count(),
          ((double)total_bytes_sent) / (1.0 * elapsed_time.count() * (1 << 20)),
          ((double)total_bytes_sent) /
            (1.0 * elapsed_time.count() * 1000 * ESTIMATED_OP_SIZE));
}

  /******************************************************************/

#else

class ClientTest : public ::testing::Test {
 protected:
  // called once in the beginning of all tests
  ClientTest() {
    printf("Attempting to open socket: %s\n", TEST_SOCKET_FILE);
    client = new Client(TEST_SOCKET_FILE);
  }

  // called before ever test
  void SetUp() override {}

  void TearDown() override {}

  void client_bandwidth_test();

  Client*         client;
  MessageTestCase message_test;
};

TEST_F(ClientTest, ClientSendRecvTest) {
  char test_char_message = client->receive<char>();
  EXPECT_EQ(message_test.expected_char_message, test_char_message);

  client->send(message_test.char_message);

  /***********************************************/

  int32_t test_int_message = client->receive<int32_t>();
  EXPECT_EQ(message_test.expected_int_message, test_int_message);

  client->send(message_test.int_message);

  /***********************************************/

  int64_t test_long_int_message = client->receive<int64_t>();
  EXPECT_EQ(message_test.expected_long_int_message, test_long_int_message);

  client->send(message_test.long_int_message);

  /***********************************************/

  MessageTestCase::TestMsgStruct test_custom_message =
    client->receive<MessageTestCase::TestMsgStruct>();
  EXPECT_EQ(message_test.expected_custom_message, test_custom_message);

  client->send(message_test.custom_message);

  /***********************************************/

  std::vector<uint32_t> test_vector_message =
    client->receive<std::vector<uint32_t>>();
  EXPECT_EQ(message_test.expected_vector_message, test_vector_message);

  client->send(message_test.vector_message);

  /***********************************************/

  std::deque<uint32_t> test_deque_message =
    client->receive<std::deque<uint32_t>>();
  EXPECT_EQ(message_test.expected_deque_message, test_deque_message);

  client->send(message_test.deque_message);

  client_bandwidth_test();
}

// TEST_F(ClientTest, ClientBandwidthTest) {
void ClientTest::client_bandwidth_test() {
  for(uint32_t j = 0; j < NUM_REPEAT; ++j) {
    std::vector<uint8_t> test_super_big_message =
      client->receive<std::vector<uint8_t>>();
    EXPECT_EQ(message_test.expected_super_big_message, test_super_big_message);

    client->send(message_test.super_big_message);
  }
}

#endif

/******************************************************************/
