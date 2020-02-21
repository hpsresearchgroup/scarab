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
 * File         : message_test_case.h
 * Author       : HPS Research Group
 * Date         : 1/11/2019
 * Description  :
 ***************************************************************************************/

#ifndef __MESSAGE_TEST_CASE_H__
#define __MESSAGE_TEST_CASE_H__

#include "../pin/pin_lib/message_queue_interface_lib.h"
#include "../pin/pin_lib/pin_scarab_common_lib.h"

#include <queue>
#include <vector>

struct MessageTestCase {
  struct TestMsgStruct {
    int32_t a;
    int8_t  b;

    bool operator==(const TestMsgStruct& rhs) const {
      return a == rhs.a && b == rhs.b;
    }
  };

  MessageTestCase() :
      expected_char_message('a'), expected_int_message(32),
      expected_long_int_message(0xFFFFAAAA3333DDDD),
      expected_custom_message({32, -1}), expected_vector_message({1, 2, 3}),
      expected_deque_message({1, 2, 3}),
      expected_super_big_message((1 << 12), 42) {
    char_message      = expected_char_message;
    int_message       = expected_int_message;
    long_int_message  = expected_long_int_message;
    custom_message    = expected_custom_message;
    vector_message    = expected_vector_message;
    deque_message     = expected_deque_message;
    super_big_message = expected_super_big_message;
  }

  const char                  expected_char_message;
  const int32_t               expected_int_message;
  const int64_t               expected_long_int_message;
  const TestMsgStruct         expected_custom_message;
  const std::vector<uint32_t> expected_vector_message;
  const std::deque<uint32_t>  expected_deque_message;
  const std::vector<uint8_t>  expected_super_big_message;

  Message<char>                  char_message;
  Message<int32_t>               int_message;
  Message<int64_t>               long_int_message;
  Message<TestMsgStruct>         custom_message;
  Message<std::vector<uint32_t>> vector_message;
  Message<std::deque<uint32_t>>  deque_message;
  Message<std::vector<uint8_t>>  super_big_message;
};

#endif
