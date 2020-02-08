#include "utils.h"

#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <vector>

namespace scarab {
namespace pin {
namespace testing {

std::string create_pin_exec_cmd(const std::string& binary_path,
                                const std::string& socket_path) {
  return std::string(getenv("PIN_ROOT")) + "/pin -t " + PIN_EXEC_TOOL_PATH +
         " -socket_path " + socket_path + " -- ./" + binary_path;
}

std::string get_new_tmpdir_path() {
  char tmpdir_path_template[] = "/tmp/scarab_test_rundir_XXXXXX";

  const auto mkdtemp_ret = mkdtemp(tmpdir_path_template);
  if(mkdtemp_ret == NULL) {
    throw std::runtime_error("mkstempt failed with errno: " +
                             std::to_string(errno));
  }
  return tmpdir_path_template;
}

std::string execute_cmd_and_get_output(const char* command) {
  std::string                   output;
  constexpr int                 BUFFER_SIZE = 4096;
  std::array<char, BUFFER_SIZE> buffer;

  FILE* pipe_fptr = popen(command, "r");
  if(!pipe_fptr) {
    throw std::runtime_error(std::string("Could not open a pipe to execute: ") +
                             command);
  }

  while(fgets(buffer.data(), BUFFER_SIZE, pipe_fptr) != NULL) {
    output += buffer.data();
  }

  pclose(pipe_fptr);
  return output;
}

Parsed_Binary get_instructions_in_binary(const std::string& binary_path) {
  Parsed_Binary parsed_output;

  const auto objdump_cmd = "objdump -d --no-show-raw-insn " + binary_path +
                           " 2>&1";
  const auto objdump_output = execute_cmd_and_get_output(objdump_cmd.c_str());


  std::istringstream output_stream(objdump_output);
  std::regex         search_expr("([0-9a-f]+):\\s+([a-z]+)");
  std::smatch        matches;
  for(std::string line; std::getline(output_stream, line);) {
    const auto found_matches = std::regex_search(line, matches, search_expr);
    if(!found_matches || matches.size() != 3) {
      continue;
    }

    parsed_output.emplace_back(strtoul(matches[1].str().c_str(), NULL, 16),
                               matches[2]);
  }

  return parsed_output;
}

void Process_Runner::start() {
  if(running_) {
    std::cerr << "start_process() called when a process is already running";
    return;
  }

  const auto fork_result = fork();
  assert(fork_result >= 0);
  if(fork_result == 0) {
    execute_cmd();
  } else {
    child_pid_ = fork_result;
  }

  running_ = true;
}

bool Process_Runner::is_running() {
  if(running_) {
    int        status;
    const auto wait_result = waitpid(child_pid_, &status, WNOHANG);
    assert(wait_result >= 0);
    if(wait_result > 0) {
      if(wait_result != child_pid_) {
        std::cout << "waitpid() returned a different pid (" << wait_result
                  << ") from the child pid (" << child_pid_ << ")";
        assert(false);
      }
      if(WIFEXITED(status) || WIFSIGNALED(status)) {
        running_ = false;
      }
    }
  }

  return running_;
}

void Process_Runner::stop() {
  if(!running_)
    return;

  kill(child_pid_, SIGTERM);
  wait(NULL);
  running_ = false;
}

void Process_Runner::execute_cmd() {
  const auto array_size = run_cmd_.size() + 1;
  auto       argv_array = std::make_unique<char[]>(array_size);
  memcpy(argv_array.get(), run_cmd_.c_str(), array_size);
  std::vector<char*> argv_ptrs;

  char* next_token = strtok(argv_array.get(), " ");
  argv_ptrs.push_back(next_token);
  while(next_token != NULL) {
    next_token = strtok(NULL, " ");
    argv_ptrs.push_back(next_token);
  }

  execv(argv_ptrs[0], argv_ptrs.data());

  throw std::runtime_error("Command could not be executed properly: " +
                           run_cmd_);
}

}  // namespace testing
}  // namespace pin
}  // namespace scarab