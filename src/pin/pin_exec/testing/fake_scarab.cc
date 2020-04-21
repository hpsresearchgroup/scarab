#include "fake_scarab.h"

#include <exception>
#include <sstream>

namespace scarab {
namespace pin {
namespace testing {

Fake_Scarab::Fake_Scarab(const char* binary_path) :
    tmpdir_path_(get_new_tmpdir_path()),
    pintool_process_(
      create_pin_exec_cmd(binary_path, tmpdir_path_ + "/socket")) {
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
    ASSERT_TRUE(CHECK_EQUAL_IN_HEX(cached_cop_buffers_.front().instruction_addr,
                                   address, "instruction address"));
    retire_latest_op();
  }
}

void Fake_Scarab::fetch_wrongpath_and_verify_instructions(
  uint64_t next_fetch_addr, uint64_t redirect_addr,
  const std::vector<uint64_t>& wrongpath_addresses) {
  fetch_new_ops_if_buffer_is_empty();
  ASSERT_TRUE(CHECK_EQUAL_IN_HEX(cached_cop_buffers_.front().instruction_addr,
                                 next_fetch_addr, "instruction address"));
  const auto redirect_inst_uid = cached_cop_buffers_.front().inst_uid;

  redirect(redirect_addr, redirect_inst_uid);
  cached_cop_buffers_.clear();
  for(auto address : wrongpath_addresses) {
    fetch_new_ops_if_buffer_is_empty();
    ASSERT_TRUE(CHECK_EQUAL_IN_HEX(cached_cop_buffers_.front().instruction_addr,
                                   address, "instruction address"));
    cached_cop_buffers_.pop_front();
  }

  recover(redirect_inst_uid);
}

void Fake_Scarab::fetch_wrongpath_nop_mode(uint64_t next_fetch_addr,
                                           uint64_t redirect_addr,
                                           int      num_instrucitons) {
  fetch_new_ops_if_buffer_is_empty();
  ASSERT_TRUE(CHECK_EQUAL_IN_HEX(cached_cop_buffers_.front().instruction_addr,
                                 next_fetch_addr, "instruction address"));
  const auto redirect_inst_uid = cached_cop_buffers_.front().inst_uid;

  redirect(redirect_addr, redirect_inst_uid);
  cached_cop_buffers_.clear();
  for(int i = 0; i < num_instrucitons; ++i) {
    fetch_new_ops_if_buffer_is_empty();
    ASSERT_TRUE(CHECK_EQUAL_IN_HEX(cached_cop_buffers_.front().instruction_addr,
                                   redirect_addr, "instruction address"));
    ASSERT_TRUE(cached_cop_buffers_.front().fake_inst);

    redirect_addr += 1;
    cached_cop_buffers_.pop_front();
  }

  recover(redirect_inst_uid);
}

void Fake_Scarab::execute_until_completion() {
  while(true) {
    fetch_new_ops_if_buffer_is_empty();
    if(is_sentinal_op(&cached_cop_buffers_.front())) {
      break;
    }
    retire_latest_op();
  }
}

bool Fake_Scarab::has_reached_end() {
  fetch_new_ops_if_buffer_is_empty();
  return is_sentinal_op(&cached_cop_buffers_.front());
}

void Fake_Scarab::fetch_new_ops() {
  Scarab_To_Pin_Msg msg;
  msg.type      = FE_FETCH_OP;
  msg.inst_addr = 0;
  msg.inst_uid  = 0;

  server_communicator_->send(0, Message<Scarab_To_Pin_Msg>(msg));
  cached_cop_buffers_ = server_communicator_->receive<ScarabOpBuffer_type>(
    /*proc_id=*/0);

  num_fetched_instructions += cached_cop_buffers_.size();
}

void Fake_Scarab::fetch_new_ops_if_buffer_is_empty() {
  if(cached_cop_buffers_.empty()) {
    fetch_new_ops();
  }
}

void Fake_Scarab::retire_latest_op() {
  if(cached_cop_buffers_.empty())
    return;

  const auto inst_uid = cached_cop_buffers_.front().inst_uid;

  Scarab_To_Pin_Msg msg;
  msg.type      = FE_RETIRE;
  msg.inst_addr = 0;
  msg.inst_uid  = inst_uid;

  server_communicator_->send(0, Message<Scarab_To_Pin_Msg>(msg));
  cached_cop_buffers_.pop_front();

  num_retired_instructions += 1;
}

void Fake_Scarab::redirect(uint64_t fetch_addr, uint64_t inst_uid) {
  cached_cop_buffers_.clear();

  Scarab_To_Pin_Msg msg;
  msg.type      = FE_REDIRECT;
  msg.inst_addr = fetch_addr;
  msg.inst_uid  = inst_uid;
  server_communicator_->send(0, Message<Scarab_To_Pin_Msg>(msg));
}

void Fake_Scarab::recover(uint64_t inst_uid) {
  cached_cop_buffers_.clear();

  Scarab_To_Pin_Msg msg;
  msg.type      = FE_RECOVER_BEFORE;
  msg.inst_addr = 0;
  msg.inst_uid  = inst_uid;
  server_communicator_->send(0, Message<Scarab_To_Pin_Msg>(msg));
}

}  // namespace testing
}  // namespace pin
}  // namespace scarab