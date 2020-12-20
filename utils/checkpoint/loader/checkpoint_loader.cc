/* Copyright 2020 HPS/SAFARI Research Groups
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*Date: 4/22/19*/

#include <getopt.h>
#include <iomanip>
#include <set>
#include <sstream>
#include <sys/utsname.h>

#include "checkpoint_reader.h"
#include "cpuinfo.h"
#include "ptrace_interface.h"
#include "utils.h"

void execute_tracee(const char* application, char* const argv[],
                    char* const envp[], bool print_argv_envp);
void execute_tracer(pid_t child_pid, bool running_with_pin);
int  attach_pin_to_child(pid_t child_pid);
void load_fp_state(pid_t pid);
void jump_to_infinite_loop(pid_t pid);
void usage(const char* name_of_loader_exe, int longest_option_length);
void parse_options(int argc, char* const argv[], int& run_natively_without_pin,
                   int& print_argv_envp, int& force_even_if_wrong_kernel,
                   int& force_even_if_wrong_cpu, int& longest_option_length);
void parse_positional_arguments(int argc, char* const argv[],
                                int run_natively_without_pin,
                                int longest_option_length);
void print_high_level_difference(const char* what_was_different);
void print_flag_to_force(const char* flag_to_force);
void print_os_info_difference_then_exit(const char* what_was_different,
                                        std::string checkpoint_version,
                                        std::string loader_version);
std::set<std::string> split_cpuinfo_flags(std::string all_flags);
void                  check_os_info();
void                  check_cpuinfo();

void execute_tracee(const char* application, char* const argv[],
                    char* const envp[], bool print_argv_envp) {
  debug("Inside tracee");
  open_file_descriptors();
  change_working_directory();
  ptrace(PTRACE_TRACEME, 0, 0, 0);
  if(print_argv_envp) {
    print_string_array("argv", argv);
    print_string_array("envp", envp);
  }
  turn_aslr_off();
  execve(application, argv, envp);
}

void execute_tracer(pid_t child_pid, bool running_with_pin) {
  debug("Inside tracer: child_pid=%d", child_pid);

  set_child_pid(child_pid);

  int status;
  waitpid(child_pid, &status, 0);
  assertm(WIFSTOPPED(status), "Child process did not stop\n");

  allocate_new_regions(child_pid);
  write_data_to_regions(child_pid);
  update_region_protections(child_pid);
  load_fp_state(child_pid);
  load_registers(child_pid);

  if(running_with_pin) {
    jump_to_infinite_loop(child_pid);
  }

  fflush(stdout);
  fflush(stderr);

  detach_process(child_pid);

  if(running_with_pin) {
    attach_pin_to_child(child_pid);
  }

  waitpid(child_pid, &status, 0);
  assertm(WIFEXITED(status), "Child process did not terminate normally\n");
}

static std::string socket_path;
static std::string pintool_path;
static int         core_id;

static const char* run_natively_without_pin_option = "run_natively_without_pin";
static const char* print_argv_envp_option          = "print_argv_envp";
static const char* force_even_if_wrong_kernel_option =
  "force_even_if_wrong_kernel";
static const char* force_even_if_wrong_cpu_option = "force_even_if_wrong_cpu";

void usage(const char* name_of_loader_exe, int longest_option_length) {
  assert(NULL != name_of_loader_exe);
  assert(-1 != longest_option_length);

  std::cerr << "usage for running with PIN: " << name_of_loader_exe
            << " [OPTION]... "
               "<checkpoint_dir> <socket_path> <core_id> <pintool_path>\n"
               "usage for running natively without PIN: "
            << name_of_loader_exe << " --" << run_natively_without_pin_option
            << " [OPTION]... <checkpoint_dir>\n\n";

  std::string option_prefix = "  --";
  int         text_width = longest_option_length + option_prefix.length() + 5;

  std::cerr << "Options:\n";
  std::cerr << std::left << std::setw(text_width)
            << option_prefix + run_natively_without_pin_option
            << "run from the checkpoint natively without launching PIN\n";
  std::cerr << std::left << std::setw(text_width)
            << option_prefix + print_argv_envp_option
            << "print the contents of argv and envp that we pass to execve\n";
  std::cerr << std::left << std::setw(text_width)
            << option_prefix + force_even_if_wrong_kernel_option
            << "try loading the checkpoint anyways, even if the current kernel "
               "version does not match"
               " the kernel version recorded during checkpoint creation\n";
  std::cerr << std::left << std::setw(text_width)
            << option_prefix + force_even_if_wrong_cpu_option
            << "try loading the checkpoint anyways, even if certain CPU "
               "features (e.g., avx512f) were available"
               " during checkpoint creation, but not on the current machine \n";

  exit(EXIT_FAILURE);
}

void parse_options(int argc, char* const argv[], int& run_natively_without_pin,
                   int& print_argv_envp, int& force_even_if_wrong_kernel,
                   int& force_even_if_wrong_cpu, int& longest_option_length) {
  while(1) {
    int                  getopt_retval;
    int                  option_index   = 0;
    static struct option long_options[] = {
      {run_natively_without_pin_option, no_argument, &run_natively_without_pin,
       true},
      {print_argv_envp_option, no_argument, &print_argv_envp, true},
      {force_even_if_wrong_kernel_option, no_argument,
       &force_even_if_wrong_kernel, true},
      {force_even_if_wrong_cpu_option, no_argument, &force_even_if_wrong_cpu,
       true},
      {"help", no_argument, NULL, 'h'},
      {0, 0, 0, 0}};
    longest_option_length = count_longest_option_length(long_options);

    getopt_retval = getopt_long_only(argc, argv, "h", long_options,
                                     &option_index);
    if(getopt_retval == -1) /* reached the end of all options */
      break;

    switch(getopt_retval) {
      case 'h':
        usage(argv[0], longest_option_length);
      case 0: /* successfully parsed option, moving onto next option */
        break;

      case '?': /* unrecognized option, but moving onto next option anyways */
        break;

      default: /* something else went wrong while parsing options */
        std::cerr
          << "getopt_long_only error, returned unexpected character code "
          << getopt_retval << " for argument at option_index " << option_index
          << std::endl;
        exit(EXIT_FAILURE);
    }
  }
}

void parse_positional_arguments(int argc, char* const argv[],
                                int run_natively_without_pin,
                                int longest_option_length) {
  int num_positional_args = (argc - optind);
  if(4 != num_positional_args) {
    if(!run_natively_without_pin) {
      usage(argv[0], longest_option_length);
    } else if(1 == num_positional_args) {
      read_checkpoint(argv[optind++]);
    } else {
      usage(argv[0], longest_option_length);
    }
  } else {
    read_checkpoint(argv[optind++]);
    socket_path  = argv[optind++];
    core_id      = atoi(argv[optind++]);
    pintool_path = argv[optind++];
  }
}

void print_high_level_difference(const char* what_was_different) {
  std::cerr << "Error! " << what_was_different
            << " during checkpoint creation is different from "
               "current "
            << what_was_different << ":" << std::endl;
}

void print_flag_to_force(const char* flag_to_force) {
  std::cerr << "run with the --" << flag_to_force
            << " flag to force the loader to"
               " load the checkpoint anyways"
            << std::endl;
}

void print_os_info_difference_then_exit(const char* what_was_different,
                                        std::string checkpoint_version,
                                        std::string loader_version) {
  print_high_level_difference(what_was_different);
  std::cerr << '\t' << checkpoint_version << " (" << what_was_different
            << " used during checkpoint creation)" << std::endl
            << '\t' << loader_version << " (current " << what_was_different
            << ")" << std::endl;
  print_flag_to_force(force_even_if_wrong_kernel_option);
  exit(EXIT_FAILURE);
}

std::set<std::string> split_cpuinfo_flags(std::string all_flags) {
  std::stringstream     ss(all_flags);
  std::string           flag;
  std::set<std::string> set_of_flags;

  while(std::getline(ss, flag, ' ')) {
    set_of_flags.insert(flag);
  }

  return set_of_flags;
}

void check_os_info() {
  std::string checkpoint_kernel_release, checkpoint_os_version;
  bool        os_info_exists = get_checkpoint_os_info(checkpoint_kernel_release,
                                               checkpoint_os_version);

  if(os_info_exists) {
    struct utsname loader_os_info;
    int            ret_val = uname(&loader_os_info);
    assertm(0 == ret_val, "uname failed while determining loader OS info");
    if(std::string(loader_os_info.release) != checkpoint_kernel_release) {
      print_os_info_difference_then_exit(
        "kernel release", checkpoint_kernel_release, loader_os_info.release);
    }
    if(std::string(loader_os_info.version) != checkpoint_os_version) {
      print_os_info_difference_then_exit("OS version", checkpoint_os_version,
                                         loader_os_info.version);
    }
  }
}

void check_cpuinfo() {
  std::string checkpoint_cpuinfo_flags;
  bool        cpuinfo_exists = get_checkpoint_cpuinfo(checkpoint_cpuinfo_flags);

  if(cpuinfo_exists) {
    std::string current_cpuinfo_flags = getCPUflags();

    std::set<std::string> checkpoint_flags_set = split_cpuinfo_flags(
      checkpoint_cpuinfo_flags);
    std::set<std::string> current_flags_set = split_cpuinfo_flags(
      current_cpuinfo_flags);

    std::string differences;
    for(auto it = checkpoint_flags_set.crbegin();
        it != checkpoint_flags_set.crend(); ++it) {
      if(0 == current_flags_set.count(*it)) {
        differences += (*it + " ");
      }
    }

    if(!differences.empty()) {
      print_high_level_difference("/proc/cpuinfo flags");
      std::cerr << "The following flags were present during checkpoint "
                   "creation but missing"
                   " from the current machine:"
                << std::endl
                << "\t" << differences << std::endl;
      print_flag_to_force(force_even_if_wrong_cpu_option);
      exit(EXIT_FAILURE);
    }
  }
}


int main(int argc, char* const argv[], char* const envp[]) {
  int run_natively_without_pin   = false;
  int print_argv_envp            = false;
  int force_even_if_wrong_kernel = false;
  int force_even_if_wrong_cpu    = false;
  int longest_option_length      = -1;

  parse_options(argc, argv, run_natively_without_pin, print_argv_envp,
                force_even_if_wrong_kernel, force_even_if_wrong_cpu,
                longest_option_length);
  parse_positional_arguments(argc, argv, run_natively_without_pin,
                             longest_option_length);

  if(!force_even_if_wrong_kernel) {
    check_os_info();
  }

  if(!force_even_if_wrong_cpu) {
    check_cpuinfo();
  }

  pid_t fork_pid = fork();

  if(fork_pid == 0) {
    std::vector<char*> checkpoint_argv_vector = get_checkpoint_argv_vector();
    std::vector<char*> checkpoint_envp_vector = get_checkpoint_envp_vector();

    execute_tracee(
      get_checkpoint_exe_path(),
      checkpoint_argv_vector.empty() ? argv : checkpoint_argv_vector.data(),
      checkpoint_envp_vector.empty() ? envp : checkpoint_envp_vector.data(),
      print_argv_envp);
  } else {
    execute_tracer(fork_pid, !run_natively_without_pin);
  }

  return 0;
}

void jump_to_infinite_loop(pid_t pid) {
  char* infinite_loop_page_address = (char*)NULL;

  // Note: it does not matter if we get the address we requested or a different
  // address.
  infinite_loop_page_address = (char*)execute_mmap(
    pid, infinite_loop_page_address, FPSTATE_SIZE, PROT_READ | PROT_EXEC,
    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if(infinite_loop_page_address == ((void*)-1)) {
    perror("jump_to_inifinite_loop (mmap failed)");
    kill_and_exit(pid);
  }

  debug("jump_to_infinite_loop: mmapped address: %llx",
        infinite_loop_page_address);

  debug("jump_to_infinite_loop: execute jump to loop: pid: %d, addr: %llx", pid,
        infinite_loop_page_address);
  execute_jump_to_loop(pid, infinite_loop_page_address);
}

void load_fp_state(pid_t pid) {
  std::cout << "Loading the floating-point state ..." << std::endl;
  char* fp_page_address = (char*)NULL;

  // Note: it does not matter if we get the address we requested or a different
  // address.
  debug("load_fp_state: mmapp for fp_state: %llx", fp_page_address);
  fp_page_address = (char*)execute_mmap(pid, fp_page_address, FPSTATE_SIZE,
                                        PROT_READ | PROT_EXEC,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if(fp_page_address == ((void*)-1)) {
    perror("load_fp_state (mmap failed)");
    kill_and_exit(pid);
  }

  debug("load_fp_state: mmapped address: %llx", fp_page_address);

  debug("load_fp_state: initiating memcpy %llx -> %d:%llx", fpstate_buffer, pid,
        fp_page_address);
  execute_memcpy(pid, fp_page_address, fpstate_buffer, FPSTATE_SIZE);

  debug("load_fp_state: execute xrstor: pid: %d, addr: %llx, edx: %x, eax: %x",
        pid, fp_page_address, 0x0, 0xff);
  execute_xrstor(pid, fp_page_address, 0x0, 0xff);

  debug("load_fp_state: munmap: pid: %d, addr: %llx, size: %d", pid,
        fp_page_address, FPSTATE_SIZE);
  int munmap_ret = execute_munmap(pid, fp_page_address, FPSTATE_SIZE);

  if(munmap_ret) {
    perror("load_fp_state (munmap failed)");
    kill_and_exit(pid);
  }

  debug("load_fp_state: DONE");
}

int attach_pin_to_child(pid_t child_pid) {
  std::string       pin_path = std::string(getenv("PIN_ROOT")) + "/pin";
  std::stringstream pin_command_ss;
  pin_command_ss << pin_path << " -mt 0"
                 << " -pid " << std::dec << child_pid << " -t " << pintool_path
                 << " -rip 0x" << std::hex << get_checkpoint_start_rip()
                 << " -socket_path " << socket_path << " -core_id " << std::dec
                 << core_id;
  std::string pin_command_str = pin_command_ss.str();

  debug("PIN COMMAND: %s", pin_command_str.c_str());
  return system(pin_command_str.c_str());
}
