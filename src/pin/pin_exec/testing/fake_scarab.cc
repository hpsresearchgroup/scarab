#include "fake_scarab.h"

#include <exception>
#include <sstream>

namespace scarab {
namespace pin {
namespace testing {

Fake_Scarab::Fake_Scarab() :
    tmpdir_path_(get_new_tmpdir_path()),
    pintool_process_(
      create_pin_exec_cmd(SIMPLE_LOOP, tmpdir_path_ + "/socket")) {
  // Socket constructor blocks until it connects to its client(s), so the
  // pintool process should be started before the socker server is created.
  pintool_process_.start();
  server_communicator_ = std::make_unique<Server>(tmpdir_path_ + "/socket",
                                                  /*num_processes=*/1);
}

Fake_Scarab::~Fake_Scarab() {
  if(!tmpdir_path_.empty()) {
    const auto cmd = std::string("rm -r ") + tmpdir_path_;
    system(cmd.c_str());
  }
}

void Fake_Scarab::execute_and_verify_instructions(
  const std::vector<uint64_t>& addresses) {
  for(auto address : addresses) {
    fetch_new_ops_if_buffer_is_empty();
    ASSERT_TRUE(CHECK_EQUAL_IN_HEX(cached_cop_buffers.front().instruction_addr,
                                   address, "instruction address"));
    retire_latest_op();
  }
}

void Fake_Scarab::execute_until_completion() {
  while(true) {
    fetch_new_ops_if_buffer_is_empty();
    if(is_sentinal_op(&cached_cop_buffers.front())) {
      break;
    }
    retire_latest_op();
  }
}

bool Fake_Scarab::has_reached_end() {
  fetch_new_ops_if_buffer_is_empty();
  return is_sentinal_op(&cached_cop_buffers.front());
}

void Fake_Scarab::fetch_new_ops() {
  Scarab_To_Pin_Msg msg;
  msg.type      = FE_FETCH_OP;
  msg.inst_addr = 0;
  msg.inst_uid  = 0;

  server_communicator_->send(0, (Message<Scarab_To_Pin_Msg>)msg);
  cached_cop_buffers = server_communicator_->receive<ScarabOpBuffer_type>(
    /*proc_id=*/0);

  num_fetched_instructions += cached_cop_buffers.size();
}

void Fake_Scarab::fetch_new_ops_if_buffer_is_empty() {
  if(cached_cop_buffers.empty()) {
    fetch_new_ops();
  }
}

void Fake_Scarab::retire_latest_op() {
  if(cached_cop_buffers.empty())
    return;

  const auto inst_uid = cached_cop_buffers.front().inst_uid;

  Scarab_To_Pin_Msg msg;
  msg.type      = FE_RETIRE;
  msg.inst_addr = 0;
  msg.inst_uid  = inst_uid;

  server_communicator_->send(0, (Message<Scarab_To_Pin_Msg>)msg);
  cached_cop_buffers.pop_front();

  num_retired_instructions += 1;
}

}  // namespace testing
}  // namespace pin
}  // namespace scarab