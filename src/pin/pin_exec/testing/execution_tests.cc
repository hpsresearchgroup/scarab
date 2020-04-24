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

#ifndef SIMPLE_LOOP_BINARY
#define SIMPLE_LOOP_BINARY "./simple_loop"
#endif

#ifndef SIMPLE_C_PROGRAM
#define SIMPLE_C_PROGRAM "./simple_c_program"
#endif

using namespace scarab::pin::testing;

class Simple_Binary_Loop_Info {
 public:
  Simple_Binary_Loop_Info(const Parsed_Binary& parsed_binary) :
      basic_block_opcodes_{
        {"xor", "xor"},             // INIT
        {"mov", "and", "je"},       // LOOP_BODY_CHECK_COND
        {"add"},                    // LOOP_BODY_CONDITIONAL_INCREMENT
        {"add", "cmp", "jl"},       // LOOP_EXIT_BLOCK
        {"xor", "mov", "syscall"},  // PROGRAM_EXIT
        {"mov", "mov", "jmp"},      // WRONGPATH_LOOP
      },
      basic_block_addresses_(
        verify_binary_and_get_addresses(parsed_binary, basic_block_opcodes_)) {}

  std::vector<uint64_t> get_expected_addresses() {
    std::vector<Basic_Block_Id> basic_block_ids;
    basic_block_ids.push_back(INIT);
    for(int i = 0; i < 10; ++i) {
      basic_block_ids.push_back(LOOP_BODY_CHECK_COND);
      if(i & 1) {
        basic_block_ids.push_back(LOOP_BODY_CONDITIONAL_INCREMENT);
      }
      basic_block_ids.push_back(LOOP_EXIT_BLOCK);
    }
    basic_block_ids.push_back(PROGRAM_EXIT);

    return convert_basic_block_ids_to_addresses(basic_block_ids);
  }

  struct Wrong_Path_Test_Info {
    // Normal execution split
    size_t                branch_instruction_addr;
    std::vector<uint64_t> expected_instruction_addresses_before_redirect;
    std::vector<uint64_t> expected_instruction_addresses_after_recovery;

    uint64_t redirect_fetch_addr;

    // Could be empty if WRONGPATH NOP mode
    bool                      is_wrong_path_nop_mode;
    Wrongpath_Nop_Mode_Reason wrong_path_nop_reason;
    std::vector<uint64_t>     wrongpath_expected_instruction_addresses;
  };

  Wrong_Path_Test_Info get_wrongpath_nop_node_test_info() {
    Wrong_Path_Test_Info info;
    info.redirect_fetch_addr =
      basic_block_addresses_.at(WRONGPATH_LOOP).front();
    info.is_wrong_path_nop_mode = true;
    info.wrong_path_nop_reason  = WPNM_REASON_REDIRECT_TO_NOT_INSTRUMENTED;

    size_t split_inst_index =
      basic_block_opcodes_.at(INIT).size() +
      basic_block_opcodes_.at(LOOP_BODY_CHECK_COND).size();

    fill_rightpath_instructions(split_inst_index, &info);
    return info;
  }

  Wrong_Path_Test_Info get_normal_wrongpath_test_info() {
    Wrong_Path_Test_Info info;
    info.redirect_fetch_addr =
      basic_block_addresses_.at(LOOP_EXIT_BLOCK).front();
    info.is_wrong_path_nop_mode = false;

    size_t split_inst_index =
      basic_block_opcodes_.at(INIT).size() +
      basic_block_opcodes_.at(LOOP_BODY_CHECK_COND).size() +
      basic_block_opcodes_.at(LOOP_EXIT_BLOCK).size() +
      basic_block_opcodes_.at(LOOP_BODY_CHECK_COND).size();

    fill_rightpath_instructions(split_inst_index, &info);

    std::vector<Basic_Block_Id> basic_block_ids = {LOOP_EXIT_BLOCK};
    for(int i = 2; i < 10; ++i) {
      basic_block_ids.push_back(LOOP_BODY_CHECK_COND);
      if(i & 1) {
        basic_block_ids.push_back(LOOP_BODY_CONDITIONAL_INCREMENT);
      }
      basic_block_ids.push_back(LOOP_EXIT_BLOCK);
    }
    basic_block_ids.push_back(PROGRAM_EXIT);
    info.wrongpath_expected_instruction_addresses =
      convert_basic_block_ids_to_addresses(basic_block_ids);

    return info;
  }


 private:
  enum Basic_Block_Id {
    INIT,
    LOOP_BODY_CHECK_COND,
    LOOP_BODY_CONDITIONAL_INCREMENT,
    LOOP_EXIT_BLOCK,
    PROGRAM_EXIT,
    WRONGPATH_LOOP,
  };

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

  std::vector<uint64_t> convert_basic_block_ids_to_addresses(
    const std::vector<Basic_Block_Id>& basic_block_ids) {
    std::vector<uint64_t> addresses;
    for(const auto& basic_block_id : basic_block_ids) {
      const auto& bb_addresses = basic_block_addresses_.at(basic_block_id);
      addresses.insert(addresses.end(), bb_addresses.begin(),
                       bb_addresses.end());
    }
    return addresses;
  }

  void fill_rightpath_instructions(size_t                split_inst_index,
                                   Wrong_Path_Test_Info* info) {
    const auto expected_addresses = get_expected_addresses();
    info->branch_instruction_addr = expected_addresses[split_inst_index];
    info->expected_instruction_addresses_before_redirect =
      std::vector<uint64_t>(expected_addresses.begin(),
                            expected_addresses.begin() + split_inst_index);
    info->expected_instruction_addresses_after_recovery = std::vector<uint64_t>(
      expected_addresses.begin() + split_inst_index, expected_addresses.end());
  }

  // Holds a vector char strings of the opcodes for each basic block.
  const std::vector<std::vector<const char*>> basic_block_opcodes_;
  const std::vector<std::vector<uint64_t>>    basic_block_addresses_;
};

class Simple_Loop_Test : public ::testing::Test {
 protected:
  void SetUp() override {
    simple_loop_info_ = std::make_unique<Simple_Binary_Loop_Info>(
      get_instructions_in_binary(SIMPLE_LOOP_BINARY));
  }
  void TearDown() override {}

  std::unique_ptr<Simple_Binary_Loop_Info> simple_loop_info_;
};

TEST_F(Simple_Loop_Test, OnPathExecutesCorrectly) {
  auto        expected_addresses = simple_loop_info_->get_expected_addresses();
  Fake_Scarab fake_scarab(SIMPLE_LOOP_BINARY);
  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_instructions(expected_addresses));
  ASSERT_TRUE(fake_scarab.has_reached_end());
  ASSERT_NO_FATAL_FAILURE(fake_scarab.retire_all());
}

void test_body_for_fetching_wrongpath(
  Simple_Binary_Loop_Info::Wrong_Path_Test_Info test_info) {
  Fake_Scarab fake_scarab(SIMPLE_LOOP_BINARY);

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_instructions(
    test_info.expected_instruction_addresses_before_redirect));

  const auto redirect_uid = fake_scarab.get_latest_inst_uid();
  ASSERT_NO_FATAL_FAILURE(fake_scarab.redirect(test_info.redirect_fetch_addr));

  if(test_info.is_wrong_path_nop_mode) {
    ASSERT_NO_FATAL_FAILURE(
      fake_scarab.fetch_instructions_in_wrongpath_nop_mode(
        test_info.redirect_fetch_addr, 10, test_info.wrong_path_nop_reason));
  } else {
    ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_instructions(
      test_info.wrongpath_expected_instruction_addresses));
  }

  ASSERT_NO_FATAL_FAILURE(fake_scarab.recover(redirect_uid));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_instructions(
    test_info.expected_instruction_addresses_after_recovery));

  ASSERT_TRUE(fake_scarab.has_reached_end());
  ASSERT_NO_FATAL_FAILURE(fake_scarab.retire_all());
}

TEST_F(Simple_Loop_Test, CanFetchAndRecoverNormalWrongPath) {
  const auto test_info = simple_loop_info_->get_normal_wrongpath_test_info();
  test_body_for_fetching_wrongpath(test_info);
}

TEST_F(Simple_Loop_Test, CanFetchAndRecoverWrongPathNopMode) {
  const auto test_info = simple_loop_info_->get_wrongpath_nop_node_test_info();
  test_body_for_fetching_wrongpath(test_info);
}

TEST(C_Program_Test, CanExecuteGlibc) {
  Fake_Scarab fake_scarab(SIMPLE_C_PROGRAM);
  fake_scarab.fetch_retire_until_completion();
  ASSERT_TRUE(fake_scarab.has_reached_end());
}