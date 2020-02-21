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

#include "../pin/pin_lib/message_queue_interface_lib.h"
#include "gtest/gtest.h"
#include "message_test_case.h"

class MessageTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  MessageTestCase message_test;
};

TEST_F(MessageTest, IsEmptyInitially) {
  Message<char> char_message;
  EXPECT_EQ(char_message.size(), 0);
}

TEST_F(MessageTest, SerializeMessage) {
  char test_char_message = message_test.char_message;
  EXPECT_EQ(test_char_message, message_test.expected_char_message);

  int32_t test_int_message = message_test.int_message;
  EXPECT_EQ(test_int_message, message_test.expected_int_message);

  int64_t test_long_int_message = message_test.long_int_message;
  EXPECT_EQ(test_long_int_message, message_test.expected_long_int_message);

  MessageTestCase::TestMsgStruct test_custom_message =
    message_test.custom_message;
  EXPECT_EQ(test_custom_message, message_test.expected_custom_message);
}

TEST_F(MessageTest, ComplexMessage) {
  std::vector<uint32_t> test_vector_message = message_test.vector_message;
  EXPECT_EQ(test_vector_message, message_test.expected_vector_message);

  std::deque<uint32_t> test_deque_message = message_test.deque_message;
  EXPECT_EQ(test_deque_message, message_test.expected_deque_message);

  std::vector<uint8_t> test_super_big_message = message_test.super_big_message;
  EXPECT_EQ(test_super_big_message, message_test.expected_super_big_message);
}
