/***************************************************************************************
 * File         : simple_loop_test.cc
 * Author       : HPS Research Group
 * Date         : 4/22/2020
 * Description  :
 ***************************************************************************************/

#include <algorithm>

#include "gtest/gtest.h"

#include "fake_scarab.h"
#include "utils.h"

#ifndef NOP_MODE_RET_BINARY
#define NOP_MODE_RET_BINARY "./nop_mode_ret"
#endif

#ifndef NOP_MODE_NONRET_DIRECT_BINARY
#define NOP_MODE_NONRET_DIRECT_BINARY "./nop_mode_nonret_direct"
#endif

#ifndef NOP_MODE_NONRET_INDIRECT_BINARY
#define NOP_MODE_NONRET_INDIRECT_BINARY "./nop_mode_nonret_indirect"
#endif

#ifndef NOP_MODE_NOT_TAKEN_BINARY
#define NOP_MODE_NOT_TAKEN_BINARY "./nop_mode_not_taken"
#endif

#ifndef NOP_MODE_BAD_STORE_BINARY
#define NOP_MODE_BAD_STORE_BINARY "./nop_mode_bad_store"
#endif

using namespace scarab::pin::testing;

class Nop_Mode_Binary_Info {
 public:
  Nop_Mode_Binary_Info(const char* binary_path) {
    binary_ = get_instructions_in_binary(binary_path);
  }

  uint64_t find_addr(const char* opcode, int n = 1) {
    auto next_itr    = binary_.begin();
    auto matched_itr = binary_.begin();
    for(int i = 0; i < n; ++i) {
      matched_itr = std::find_if(
        next_itr, binary_.end(),
        [opcode](const std::pair<uint64_t, std::string>& inst) -> bool {
          return inst.second == opcode;
        });

      if(matched_itr == binary_.end()) {
        break;
      }

      next_itr = std::next(matched_itr);
    }

    if(matched_itr == binary_.end()) {
      throw std::runtime_error(std::string("Could not find the ") + opcode +
                               " instruction");
    }
    return matched_itr->first;
  }

 private:
  Parsed_Binary binary_;
};

class Nop_Mode_Ret_Binary_Info : public Nop_Mode_Binary_Info {
 public:
  Nop_Mode_Ret_Binary_Info() :
      Nop_Mode_Binary_Info(NOP_MODE_RET_BINARY),
      ret_instruction_addr(find_addr("retq")) {}

  const uint64_t ret_instruction_addr;
};

class Nop_Mode_Nonret_Direct_Binary_Info : public Nop_Mode_Binary_Info {
 public:
  Nop_Mode_Nonret_Direct_Binary_Info() :
      Nop_Mode_Binary_Info(NOP_MODE_NONRET_DIRECT_BINARY),
      test_instruction_addr(find_addr("test")),
      jne_instruction_addr(find_addr("jne")),
      far_target_addr(find_addr("jmp", 2)) {}

  const uint64_t test_instruction_addr;
  const uint64_t jne_instruction_addr;
  const uint64_t far_target_addr;
};

class Nop_Mode_Nonret_Indirect_Binary_Info : public Nop_Mode_Binary_Info {
 public:
  Nop_Mode_Nonret_Indirect_Binary_Info() :
      Nop_Mode_Binary_Info(NOP_MODE_NONRET_INDIRECT_BINARY),
      indirect_jmp_instruction_addr(find_addr("jmpq")),
      far_target_addr(find_addr("jmp", 2)) {}

  const uint64_t indirect_jmp_instruction_addr;
  const uint64_t far_target_addr;
};

class Nop_Mode_Not_Taken_Binary_Info : public Nop_Mode_Binary_Info {
 public:
  Nop_Mode_Not_Taken_Binary_Info() :
      Nop_Mode_Binary_Info(NOP_MODE_NOT_TAKEN_BINARY),
      redirect_addr(find_addr("sub", 2)) {}

  const uint64_t redirect_addr;
};

class Nop_Mode_Bad_Store_Binary_Info : public Nop_Mode_Binary_Info {
 public:
  Nop_Mode_Bad_Store_Binary_Info() :
      Nop_Mode_Binary_Info(NOP_MODE_BAD_STORE_BINARY),
      redirect_addr(find_addr("movq")),
      instruction_after_store(find_addr("mov")) {}

  const uint64_t redirect_addr;
  const uint64_t instruction_after_store;
};

TEST(RET_NOP_MODE, ReturningToUntracedAddressTriggersNopMode) {
  Nop_Mode_Ret_Binary_Info binary_info;
  Fake_Scarab              fake_scarab(NOP_MODE_RET_BINARY);

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_first_control_flow());

  const auto redirect_uid  = fake_scarab.get_latest_inst_uid();
  const auto redirect_addr = binary_info.ret_instruction_addr;
  ASSERT_NO_FATAL_FAILURE(fake_scarab.redirect(redirect_addr));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_instructions({redirect_addr}));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_instructions_in_wrongpath_nop_mode(
    redirect_addr - 0x10000, 10, WPNM_REASON_RETURN_TO_NOT_INSTRUMENTED));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.recover(redirect_uid));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_completion());
  ASSERT_TRUE(fake_scarab.has_reached_end());
  ASSERT_NO_FATAL_FAILURE(fake_scarab.retire_all());
}

TEST(RET_NOP_MODE, DirectJumpingToUntracedAddressTriggersNopMode) {
  Nop_Mode_Nonret_Direct_Binary_Info binary_info;

  Fake_Scarab fake_scarab(NOP_MODE_NONRET_DIRECT_BINARY);

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_first_control_flow());
  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_first_control_flow());

  const auto redirect_uid  = fake_scarab.get_latest_inst_uid();
  const auto redirect_addr = binary_info.test_instruction_addr;
  ASSERT_NO_FATAL_FAILURE(fake_scarab.redirect(redirect_addr));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_instructions(
    {redirect_addr, binary_info.jne_instruction_addr}));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_instructions_in_wrongpath_nop_mode(
    binary_info.far_target_addr, 10,
    WPNM_REASON_NONRET_CF_TO_NOT_INSTRUMENTED));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.recover(redirect_uid));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_completion());
  ASSERT_TRUE(fake_scarab.has_reached_end());
  ASSERT_NO_FATAL_FAILURE(fake_scarab.retire_all());
}

TEST(RET_NOP_MODE, IndirectJumpingToUntracedAddressTriggersNopMode) {
  Nop_Mode_Nonret_Indirect_Binary_Info binary_info;

  Fake_Scarab fake_scarab(NOP_MODE_NONRET_INDIRECT_BINARY);

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_first_control_flow());
  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_first_control_flow());

  const auto redirect_uid  = fake_scarab.get_latest_inst_uid();
  const auto redirect_addr = binary_info.indirect_jmp_instruction_addr;
  ASSERT_NO_FATAL_FAILURE(fake_scarab.redirect(redirect_addr));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_instructions({redirect_addr}));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_instructions_in_wrongpath_nop_mode(
    binary_info.far_target_addr, 10,
    WPNM_REASON_NONRET_CF_TO_NOT_INSTRUMENTED));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.recover(redirect_uid));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_completion());
  ASSERT_TRUE(fake_scarab.has_reached_end());
  ASSERT_NO_FATAL_FAILURE(fake_scarab.retire_all());
}

TEST(RET_NOP_MODE, FallThroughToUntracedAddressTriggersNopMode) {
  Nop_Mode_Not_Taken_Binary_Info binary_info;

  Fake_Scarab fake_scarab(NOP_MODE_NOT_TAKEN_BINARY);

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_first_control_flow());
  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_first_control_flow());

  const auto redirect_uid  = fake_scarab.get_latest_inst_uid();
  const auto redirect_addr = binary_info.redirect_addr;
  ASSERT_NO_FATAL_FAILURE(fake_scarab.redirect(redirect_addr));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_first_wrongpath_nop_mode(
    10, WPNM_REASON_NOT_TAKEN_TO_NOT_INSTRUMENTED));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.recover(redirect_uid));
  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_completion());
  ASSERT_TRUE(fake_scarab.has_reached_end());
  ASSERT_NO_FATAL_FAILURE(fake_scarab.retire_all());
}

TEST(RET_NOP_MODE, StoreToUnseenAddressTriggersNopMode) {
  Nop_Mode_Bad_Store_Binary_Info binary_info;

  Fake_Scarab fake_scarab(NOP_MODE_BAD_STORE_BINARY);

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_first_control_flow());

  const auto redirect_uid  = fake_scarab.get_latest_inst_uid();
  const auto redirect_addr = binary_info.redirect_addr;
  ASSERT_NO_FATAL_FAILURE(fake_scarab.redirect(redirect_addr));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_instructions({redirect_addr}));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_instructions_in_wrongpath_nop_mode(
    binary_info.instruction_after_store, 10,
    WPNM_REASON_WRONG_PATH_STORE_TO_NEW_REGION));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.recover(redirect_uid));

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_completion());
  ASSERT_TRUE(fake_scarab.has_reached_end());
  ASSERT_NO_FATAL_FAILURE(fake_scarab.retire_all());
}