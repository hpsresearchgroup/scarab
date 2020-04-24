#ifndef __PIN_PIN_EXEC_TESTING_FAKE_SCARAB_H__
#define __PIN_PIN_EXEC_TESTING_FAKE_SCARAB_H__

#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#include "gtest/gtest.h"
#include "utils.h"

#include "../../pin_lib/message_queue_interface_lib.h"
#include "../../pin_lib/pin_scarab_common_lib.h"

namespace scarab {
namespace pin {
namespace testing {

class Fake_Scarab {
 public:
  Fake_Scarab(const char* binary_path);
  ~Fake_Scarab();

  void fetch_instructions(const std::vector<uint64_t>& addresses);

  void fetch_instructions_in_wrongpath_nop_mode(
    uint64_t next_fetch_addr, int num_instructions,
    Wrongpath_Nop_Mode_Reason expected_reason);

  void fetch_retire_until_completion();

  void fetch_until_completion();

  void fetch_until_first_control_flow();

  void fetch_until_first_wrongpath_nop_mode(
    int max_num_instructions, Wrongpath_Nop_Mode_Reason expected_reason);

  uint64_t get_latest_inst_uid();

  bool has_fetched_ifetch_barrier();

  bool has_reached_end();

  void recover(uint64_t inst_uid);

  void redirect(uint64_t fetch_addr);

  void retire_all();


 private:
  void fetch_new_ops();
  void fetch_next_instruction();
  void flush_cops_after_uid(uint64_t inst_uid);
  void refill_op_buffer();
  void retire(uint64_t inst_uid);

  std::string             tmpdir_path_;
  Process_Runner          pintool_process_;
  std::unique_ptr<Server> server_communicator_;

  ScarabOpBuffer_type       op_buffer_;
  std::deque<compressed_op> fetched_ops_;
};

}  // namespace testing
}  // namespace pin
}  // namespace scarab

#endif  //__PIN_PIN_EXEC_TESTING_FAKE_SCARAB_H__