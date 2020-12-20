#include <cassert>
#include <iostream>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ptrace_interface.h"

namespace {

char data[]      = "Test Failed !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
char repl_data[] = "Test Succeeded !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";

void execute_tracer(pid_t child_pid) {
  int status;
  waitpid(child_pid, &status, 0);
  if(!WIFSTOPPED(status)) {
    std::cerr << "Child process did not stop" << std::endl;
    std::exit(1);
  }


  auto[tracer_addr, tracee_addr] = allocate_shared_memory(child_pid);

  shared_memory_memcpy(child_pid, &data[0], &repl_data[0],
                       (sizeof(data) / 8) * 8, tracer_addr, tracee_addr);
  // execute_memcpy(child_pid, &data[0], &repl_data[0], (sizeof(data) / 8) * 8);

  if(ptrace(PTRACE_DETACH, child_pid, NULL, NULL)) {
    perror("PTRACE_DETACH");
  }

  waitpid(child_pid, &status, 0);
  if(!WIFEXITED(status)) {
    std::cerr << "Child process did not terminate normally" << std::endl;
    std::exit(1);
  }
}

void execute_tracee_wrapper(char* argv0) {
  ptrace(PTRACE_TRACEME, 0, 0, 0);
  char  dummy_arg[] = "dummy";
  char* argv[]      = {argv0, dummy_arg, nullptr};
  execv(argv0, argv);
}

void execute_tracee() {
  std::cout << data << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
  if(argc > 1) {
    execute_tracee();

  } else {
    auto fork_pid = fork();
    if(fork_pid == 0) {
      execute_tracee_wrapper(argv[0]);
    } else {
      execute_tracer(fork_pid);
    }
  }
}