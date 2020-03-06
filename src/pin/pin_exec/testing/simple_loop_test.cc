/***************************************************************************************
 * File         : simple_loop_test.cc
 * Author       : HPS Research Group
 * Date         : 1/31/2020
 * Description  :
 ***************************************************************************************/

#include <algorithm>
#include <chrono>
#include <thread>

#include "gtest/gtest.h"

#include "fake_scarab.h"
#include "utils.h"

using namespace scarab::pin::testing;

class Simple_Loop_Info {
 public:
  enum Basic_Block_Id {
    INIT,
    LOOP_BODY_CHECK_COND,
    LOOP_BODY_CONDITIONAL_INCREMENT,
    LOOP_EXIT_BLOCK,
    PROGRAM_EXIT,
    NUM_BASIC_BLOCKS,
  };

  Simple_Loop_Info(const Parsed_Binary& parsed_binary) :
      basic_block_opcodes_{
        {"xor", "xor"},            // INIT
        {"mov", "and", "je"},      // LOOP_BODY_CHECK_COND
        {"add"},                   // LOOP_BODY_CONDITIONAL_INCREMENT
        {"add", "cmp", "jl"},      // LOOP_EXIT_BLOCK
        {"xor", "mov", "syscall"}  // PROGRAM_EXIT
      },
      basic_block_addresses_(
        verify_binary_and_get_addresses(parsed_binary, basic_block_opcodes_)) {}

  const std::vector<uint64_t>& get_basic_block_addresses(Basic_Block_Id id) {
    return basic_block_addresses_.at(id);
  }

 private:
  static std::vector<std::vector<uint64_t>> verify_binary_and_get_addresses(
    const Parsed_Binary&                         parsed_binary,
    const std::vector<std::vector<const char*>>& basic_block_opcodes) {
    std::vector<std::vector<uint64_t>> basic_block_addresses;

    auto binary_itr = parsed_binary.begin();
    for(auto& basic_block : basic_block_opcodes) {
      basic_block_addresses.emplace_back();
      for(auto& opcode : basic_block) {
        if(binary_itr == parsed_binary.end()) {
          throw std::runtime_error(std::string("expected to see instruction ") +
                                   opcode +
                                   ", but reached the end of the binary.");
        }
        if(binary_itr->second != opcode) {
          throw std::runtime_error(std::string("expected to see instruction ") +
                                   opcode + ", but saw " + binary_itr->second +
                                   " in the binary.");
        }

        basic_block_addresses.back().push_back(binary_itr->first);
        ++binary_itr;
      }
    }

    return basic_block_addresses;
  }

  // Holds a vector char strings of the opcodes for each basic block.
  const std::vector<std::vector<const char*>> basic_block_opcodes_;
  const std::vector<std::vector<uint64_t>>    basic_block_addresses_;
};

class Simple_Loop_Test : public ::testing::Test {
 protected:
  void SetUp() override {
    simple_loop_info_ = std::make_unique<Simple_Loop_Info>(
      get_instructions_in_binary(std::string("./") + SIMPLE_LOOP));
  }
  void TearDown() override {}

  std::unique_ptr<Simple_Loop_Info> simple_loop_info_;
};

TEST_F(Simple_Loop_Test, OnPathExecutesCorrectly) {
  std::vector<Simple_Loop_Info::Basic_Block_Id> expected_basic_block_ids;
  expected_basic_block_ids.push_back(Simple_Loop_Info::INIT);
  for(int i = 0; i < 10; ++i) {
    expected_basic_block_ids.push_back(Simple_Loop_Info::LOOP_BODY_CHECK_COND);
    if(i & 1) {
      expected_basic_block_ids.push_back(
        Simple_Loop_Info::LOOP_BODY_CONDITIONAL_INCREMENT);
    }
    expected_basic_block_ids.push_back(Simple_Loop_Info::LOOP_EXIT_BLOCK);
  }
  expected_basic_block_ids.push_back(Simple_Loop_Info::PROGRAM_EXIT);


  Fake_Scarab fake_scarab;
  for(const auto& expected_basic_block_id : expected_basic_block_ids) {
    ASSERT_NO_FATAL_FAILURE(fake_scarab.execute_and_verify_instructions(
      simple_loop_info_->get_basic_block_addresses(expected_basic_block_id)));
  }
  ASSERT_TRUE(fake_scarab.has_reached_end());
}