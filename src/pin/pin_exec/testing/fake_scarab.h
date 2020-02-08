#ifndef __PIN_PIN_EXEC_TESTING_FAKE_SCARAB_H__
#define __PIN_PIN_EXEC_TESTING_FAKE_SCARAB_H__

#include <cstdlib>
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
  Fake_Scarab();
  ~Fake_Scarab();

  void execute_and_verify_instructions(const std::vector<uint64_t>& addresses);
  void execute_until_completion();
  bool has_reached_end();

  int64_t num_fetched_instructions = 0;
  int64_t num_retired_instructions = 0;

 private:
  void fetch_new_ops();
  void fetch_new_ops_if_buffer_is_empty();
  void retire_latest_op();

  std::string             tmpdir_path_;
  Process_Runner          pintool_process_;
  std::unique_ptr<Server> server_communicator_;

  ScarabOpBuffer_type cached_cop_buffers;
};

}  // namespace testing
}  // namespace pin
}  // namespace scarab

#endif  //__PIN_PIN_EXEC_TESTING_FAKE_SCARAB_H__