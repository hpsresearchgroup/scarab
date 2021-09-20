#include "fake_scarab.h"

#include <algorithm>
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
    int        ret = system(cmd.c_str());
    (void)(ret);  // ignore variable explicitly to circumvent unused variable
                  // warning
  }
}

void Fake_Scarab::fetch_instructions(const std::vector<uint64_t>& addresses) {
  for(auto address : addresses) {
    fetch_next_instruction();
    ASSERT_TRUE(CHECK_EQUAL_IN_HEX(fetched_ops_.back().instruction_addr,
                                   address, "instruction address"));
  }
}

void Fake_Scarab::fetch_instructions_in_wrongpath_nop_mode(
  uint64_t next_fetch_addr, int num_instructions,
  Wrongpath_Nop_Mode_Reason expected_reason) {
  for(int i = 0; i < num_instructions; ++i) {
    fetch_next_instruction();
    ASSERT_TRUE(CHECK_EQUAL_IN_HEX(fetched_ops_.back().instruction_addr,
                                   next_fetch_addr, "instruction address"));
    ASSERT_TRUE(fetched_ops_.back().fake_inst);
    ASSERT_EQ(fetched_ops_.back().fake_inst_reason, expected_reason);

    next_fetch_addr += 1;
  }
}

void Fake_Scarab::fetch_retire_until_completion() {
  while(!has_reached_end()) {
    while(!has_reached_end() && fetched_ops_.size() < 1000) {
      fetch_next_instruction();
      if(has_fetched_ifetch_barrier()) {
        break;
      }
    }
    retire_all();
  }
}

void Fake_Scarab::fetch_until_completion() {
  while(!has_reached_end()) {
    fetch_next_instruction();
  }
}

void Fake_Scarab::fetch_until_first_control_flow() {
  do {
    ASSERT_FALSE(has_reached_end());
    fetch_next_instruction();
  } while(fetched_ops_.back().cf_type == 0);
}

void Fake_Scarab::fetch_until_first_wrongpath_nop_mode(
  int max_num_instructions, Wrongpath_Nop_Mode_Reason expected_reason) {
  for(int i = 0; i < max_num_instructions; ++i) {
    fetch_next_instruction();

    if(fetched_ops_.back().fake_inst) {
      ASSERT_EQ(fetched_ops_.back().fake_inst_reason, expected_reason);
      return;
    }
  }

  GTEST_FATAL_FAILURE_("Did not go to wrongpath nop mode");
}

uint64_t Fake_Scarab::get_latest_inst_uid() {
  if(fetched_ops_.empty()) {
    return std::numeric_limits<uint64_t>::max();
  } else {
    return fetched_ops_.back().inst_uid;
  }
}

bool Fake_Scarab::has_fetched_ifetch_barrier() {
  return !fetched_ops_.empty() && fetched_ops_.back().is_ifetch_barrier;
}

bool Fake_Scarab::has_reached_end() {
  if(op_buffer_.empty()) {
    refill_op_buffer();
  }
  return is_sentinal_op(&op_buffer_.front());
}

void Fake_Scarab::recover(uint64_t inst_uid) {
  flush_cops_after_uid(inst_uid);
  Scarab_To_Pin_Msg msg;
  msg.type      = FE_RECOVER_AFTER;
  msg.inst_addr = 0;
  msg.inst_uid  = inst_uid;
  server_communicator_->send(0, Message<Scarab_To_Pin_Msg>(msg));
}

void Fake_Scarab::redirect(uint64_t fetch_addr) {
  op_buffer_.clear();

  Scarab_To_Pin_Msg msg;
  msg.type      = FE_REDIRECT;
  msg.inst_addr = fetch_addr;
  msg.inst_uid  = fetched_ops_.back().inst_uid;
  server_communicator_->send(0, Message<Scarab_To_Pin_Msg>(msg));
}

void Fake_Scarab::retire_all() {
  for(const auto& op : fetched_ops_) {
    retire(op.inst_uid);
  }
  fetched_ops_.clear();
}

void Fake_Scarab::fetch_next_instruction() {
  if(op_buffer_.empty()) {
    refill_op_buffer();
  }

  fetched_ops_.push_back(op_buffer_.front());
  op_buffer_.pop_front();
}

void Fake_Scarab::flush_cops_after_uid(uint64_t inst_uid) {
  op_buffer_.clear();
  fetched_ops_.erase(
    std::upper_bound(fetched_ops_.begin(), fetched_ops_.end(), inst_uid,
                     [](uint64_t inst_uid, const compressed_op& op) {
                       return inst_uid < op.inst_uid;
                     }),
    fetched_ops_.end());
}

void Fake_Scarab::refill_op_buffer() {
  ASSERT_TRUE(op_buffer_.empty());
  Scarab_To_Pin_Msg msg;
  msg.type      = FE_FETCH_OP;
  msg.inst_addr = 0;
  msg.inst_uid  = 0;

  server_communicator_->send(0, Message<Scarab_To_Pin_Msg>(msg));
  op_buffer_ = server_communicator_->receive<ScarabOpBuffer_type>(
    /*proc_id=*/0);
}

void Fake_Scarab::retire(uint64_t inst_uid) {
  Scarab_To_Pin_Msg msg;
  msg.type      = FE_RETIRE;
  msg.inst_addr = 0;
  msg.inst_uid  = inst_uid;

  server_communicator_->send(0, Message<Scarab_To_Pin_Msg>(msg));
}

}  // namespace testing
}  // namespace pin
}  // namespace scarab