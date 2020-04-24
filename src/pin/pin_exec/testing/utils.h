#ifndef __PIN_PIN_EXEC_TESTING_UTILS_H__
#define __PIN_PIN_EXEC_TESTING_UTILS_H__

#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace scarab {
namespace pin {
namespace testing {

// Representation of the parsed binary.
using Parsed_Binary = std::vector<std::pair<uint64_t, std::string>>;

// Creates a command for executing a pin_exec pintool process for the given
// arguments.
std::string create_pin_exec_cmd(const std::string& binary_path,
                                const std::string& socket_path);

// Creates a new temporary directory and returns its path. The directory is
// guaranteed to be new through Linux API.
std::string get_new_tmpdir_path();


// Executes command and returns its stdout as a string.
// Hint: you can use "2>&1" to redirect stderr to stdout as part of the command.
std::string execute_cmd_and_get_output(const char* command);

// Uses Linux objdump to get a list of PC-Opcode pairs in the binary.
Parsed_Binary get_instructions_in_binary(const std::string& binary_path);

template <typename T>
::testing::AssertionResult CHECK_EQUAL_IN_HEX(
  T actual, T expected, const std::string& variable_name = {}) {
  static_assert(std::is_integral<T>::value,
                "T should be an integral (integer) type");
  if(actual == expected) {
    return ::testing::AssertionSuccess();
  } else {
    ::testing::Message msg;
    if(!variable_name.empty()) {
      msg << "Mismatch in Variable " << variable_name << ".";
    }
    msg << "Actual: " << std::hex << actual << ".";
    msg << "Expected: " << std::hex << expected << ".";
    return ::testing::AssertionFailure(msg);
  }
}

// A class for parsing binaries and getting the addresses of their instructions
// using opcodes
class Binary_Info {
 public:
  Binary_Info(const char* binary_path);

  uint64_t find_addr(const char* opcode, int n = 1);

 private:
  Parsed_Binary binary_;
};

// A class for asynchronously running a Linux command through a forked process.
class Process_Runner {
 public:
  Process_Runner(const std::string& run_cmd) : run_cmd_(run_cmd) {}
  ~Process_Runner() { stop(); }

  // If the process in not running, forks a new process and runs the command in
  // the child process.
  void start();

  // Probes whether the process is running.
  bool is_running();

  // Kills the child process.
  void stop();

 private:
  void execute_cmd();

  std::string run_cmd_;
  pid_t       child_pid_;
  bool        running_ = false;
};


}  // namespace testing
}  // namespace pin
}  // namespace scarab

#endif  //__PIN_PIN_EXEC_TESTING_UTILS_H__