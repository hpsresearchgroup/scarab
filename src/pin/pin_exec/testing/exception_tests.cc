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

#ifndef WRONG_PATH_EXCEPTION_BINARY
#define WRONG_PATH_EXCEPTION_BINARY "./wrong_path_exception"
#endif

#ifndef C_PROGRAM_WITH_UD2_BINARY
#define C_PROGRAM_WITH_UD2_BINARY "./c_program_with_ud2"
#endif

using namespace scarab::pin::testing;

class Wrong_Path_Exception_Binary_Info : public Binary_Info {
 public:
  Wrong_Path_Exception_Binary_Info() :
      Binary_Info(WRONG_PATH_EXCEPTION_BINARY),
      bad_store_addr(find_addr("movq")),
      illegal_instruction_addr(find_addr("ud2")) {}

  const uint64_t bad_store_addr;
  const uint64_t illegal_instruction_addr;
};

TEST(Exception_Test, WrongPathDoesNotExcuteExceptions) {
  Wrong_Path_Exception_Binary_Info binary_info;

  Fake_Scarab fake_scarab(WRONG_PATH_EXCEPTION_BINARY);

  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_first_control_flow());

  const auto redirect_uid      = fake_scarab.get_latest_inst_uid();
  const auto store_addr        = binary_info.bad_store_addr;
  const auto illegal_inst_addr = binary_info.illegal_instruction_addr;

  ASSERT_NO_FATAL_FAILURE(fake_scarab.redirect(store_addr));
  ASSERT_NO_FATAL_FAILURE(
    fake_scarab.fetch_instructions({store_addr, illegal_inst_addr}));
  ASSERT_TRUE(fake_scarab.has_fetched_ifetch_barrier());

  ASSERT_NO_FATAL_FAILURE(fake_scarab.recover(redirect_uid));
  ASSERT_NO_FATAL_FAILURE(fake_scarab.fetch_until_completion());
  ASSERT_TRUE(fake_scarab.has_reached_end());
  ASSERT_NO_FATAL_FAILURE(fake_scarab.retire_all());
}

TEST(Exception_Test, CanExecuteCustomHandler) {
  Fake_Scarab fake_scarab(C_PROGRAM_WITH_UD2_BINARY);
  fake_scarab.fetch_retire_until_completion();
  ASSERT_TRUE(fake_scarab.has_reached_end());
}