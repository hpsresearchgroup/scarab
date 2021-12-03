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
#include <string_view>
#include <sys/utsname.h>

#include "checkpoint_reader.h"
#include "cpuinfo.h"
#include "ptrace_interface.h"
#include "utils.h"

void execute_tracee(const char* application, char* const argv[],
                    char* const envp[], bool print_argv_envp);
void execute_tracer(pid_t child_pid, bool running_with_pin,
                    bool external_pintool);
int  attach_pin_to_child(pid_t child_pid, bool external_pintool);
void load_fp_state(pid_t pid);
void jump_to_infinite_loop(pid_t pid);
void usage(const char* name_of_loader_exe, int longest_option_length);
void parse_options(int argc, char* const argv[], int& run_natively_without_pin,
                   int& run_external_pintool, int& print_argv_envp,
                   int& force_even_if_wrong_kernel,
                   int& force_even_if_wrong_cpu, int& longest_option_length);
void parse_positional_arguments(int argc, char* const argv[],
                                int run_natively_without_pin,
                                int run_external_pintool,
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

void execute_tracer(pid_t child_pid, bool running_with_pin,
                    bool external_pintool) {
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
    attach_pin_to_child(child_pid, external_pintool);
  }

  waitpid(child_pid, &status, 0);
  assertm(WIFEXITED(status), "Child process did not terminate normally\n");
}

static std::string socket_path;
static std::string pintool_path;
static std::string pintool_args;
static int         core_id;

static const char* run_natively_without_pin_option = "run_natively_without_pin";
static const char* run_external_pintool_option     = "run_external_pintool";
static const char* print_argv_envp_option          = "print_argv_envp";
static const char* force_even_if_wrong_kernel_option =
  "force_even_if_wrong_kernel";
static const char* force_even_if_wrong_cpu_option = "force_even_if_wrong_cpu";
static const char* pintool_args_option            = "pintool_args";

namespace {

template <typename T>
std::string hex_str(T x) {
  std::stringstream ss;
  ss << "0x" << std::hex << x;
  return ss.str();
}

template <typename F>
void for_each_extra_pintool_arg(F f) {
  for(size_t pos = 0; pos < pintool_args.size();) {
    size_t delim_pos = pintool_args.find(' ', pos);
    if(delim_pos == std::string::npos) {
      delim_pos = pintool_args.size();
    }

    if(delim_pos > pos) {
      f(std::string_view(&pintool_args[pos], delim_pos - pos));
    }

    pos = delim_pos + 1;
  }
}


std::vector<std::vector<char>> create_pin_cmd_argv_vectors(
  pid_t child_pid, bool external_pintool) {
  std::vector<std::vector<char>> argv;

  auto add_arg = [&argv](std::string_view str) {
    argv.emplace_back(str.begin(), str.end());
    auto& new_arg = argv[argv.size() - 1];
    new_arg.push_back('\0');
  };

  add_arg(std::string(getenv("PIN_ROOT")) + "/pin");
  add_arg("-mt");
  add_arg("0");
  add_arg("-pid");
  add_arg(std::to_string(child_pid));
  add_arg("-t");
  add_arg(pintool_path);
  if(!external_pintool) {
    add_arg("-rip");
    add_arg(hex_str(get_checkpoint_start_rip()));
    add_arg("-socket_path");
    add_arg(socket_path);
    add_arg("-core_id");
    add_arg(std::to_string(core_id));
  }

  for_each_extra_pintool_arg(add_arg);
  return argv;
}

std::vector<char*> convert_argv_vectors_to_charptr(
  std::vector<std::vector<char>>& argv_vectors) {
  std::vector<char*> ptrs(argv_vectors.size() + 1);
  for(size_t i = 0; i < argv_vectors.size(); ++i) {
    ptrs[i] = argv_vectors[i].data();
  }
  ptrs[ptrs.size() - 1] = nullptr;
  return ptrs;
}

}  // namespace

void usage(const char* name_of_loader_exe, int longest_option_length) {
  assert(NULL != name_of_loader_exe);
  assert(-1 != longest_option_length);

  std::cerr << "usage for running with PIN: " << name_of_loader_exe
            << " [OPTION]... "
               "<checkpoint_dir> <socket_path> <core_id> <pintool_path>\n"
            << "usage for preparing for PIN, but not actually attaching PIN: "
            << name_of_loader_exe << " --" << run_external_pintool_option
            << " [OPTION]... <checkpoint_dir> <pintool_path> <pintool_args>\n"
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
            << option_prefix + run_external_pintool_option
            << "Run any pintool(can be external to Scarab)\n";
  std::cerr << std::left << std::setw(text_width)
            << option_prefix + pintool_args_option
            << "pass extra arguments to the pintool\n";
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
                   int& run_external_pintool, int& print_argv_envp,
                   int& force_even_if_wrong_kernel,
                   int& force_even_if_wrong_cpu, int& longest_option_length) {
  static struct option long_options[] = {
    {run_natively_without_pin_option, no_argument, &run_natively_without_pin,
     true},
    {run_external_pintool_option, no_argument, &run_external_pintool, true},
    {print_argv_envp_option, no_argument, &print_argv_envp, true},
    {force_even_if_wrong_kernel_option, no_argument,
     &force_even_if_wrong_kernel, true},
    {force_even_if_wrong_cpu_option, no_argument, &force_even_if_wrong_cpu,
     true},
    {pintool_args_option, required_argument, NULL, 'p'},
    {"help", no_argument, NULL, 'h'},
    {0, 0, 0, 0}};

  while(1) {
    longest_option_length = count_longest_option_length(long_options);

    int option_index  = 0;
    int getopt_retval = getopt_long_only(argc, argv, "h", long_options,
                                         &option_index);
    if(getopt_retval == -1) /* reached the end of all options */
      break;

    switch(getopt_retval) {
      case 'h':
        usage(argv[0], longest_option_length);
      case 0: /* successfully parsed option, moving onto next option */
        break;

      case 'p':
        pintool_args = optarg;

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

  if(run_natively_without_pin && run_external_pintool) {
    std::cerr << "At most one of --" << run_natively_without_pin_option
              << " and --" << run_external_pintool_option << " must be set."
              << std::endl;
    exit(EXIT_FAILURE);
  }
}

void parse_positional_arguments(int argc, char* const argv[],
                                int run_natively_without_pin,
                                int run_external_pintool,
                                int longest_option_length) {
  int  num_positional_args     = (argc - optind);
  bool run_scarab_exec_pintool = !run_natively_without_pin &&
                                 !run_external_pintool;
  if(run_natively_without_pin && num_positional_args == 1) {
    read_checkpoint(argv[optind++]);
  } else if(run_external_pintool && 2 == num_positional_args) {
    read_checkpoint(argv[optind++]);
    pintool_path = argv[optind++];
  } else if(run_scarab_exec_pintool && num_positional_args == 4) {
    read_checkpoint(argv[optind++]);
    socket_path  = argv[optind++];
    core_id      = atoi(argv[optind++]);
    pintool_path = argv[optind++];
  } else {
    usage(argv[0], longest_option_length);
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
  int run_external_pintool       = false;
  int print_argv_envp            = false;
  int force_even_if_wrong_kernel = false;
  int force_even_if_wrong_cpu    = false;
  int longest_option_length      = -1;

  parse_options(argc, argv, run_natively_without_pin, run_external_pintool,
                print_argv_envp, force_even_if_wrong_kernel,
                force_even_if_wrong_cpu, longest_option_length);
  parse_positional_arguments(argc, argv, run_natively_without_pin,
                             run_external_pintool, longest_option_length);

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
    execute_tracer(fork_pid, !run_natively_without_pin, run_external_pintool);
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

int attach_pin_to_child(pid_t child_pid, bool external_pintool) {
  auto pin_cmd_argv_vectors = create_pin_cmd_argv_vectors(child_pid,
                                                          external_pintool);
  auto pin_cmd_argv_ptrs    = convert_argv_vectors_to_charptr(
    pin_cmd_argv_vectors);

  for(auto& arg : pin_cmd_argv_vectors) {
    debug("PIN COMMAND ARGV: %s", arg.data());
  }

  return execv(pin_cmd_argv_ptrs[0], pin_cmd_argv_ptrs.data());
}
