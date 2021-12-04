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

#include "checkpoint_reader.h"

#include <cassert>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <vector>

#include "ptrace_interface.h"
#include "read_mem_map.h"

static const int MAX_MEMORY_REGIONS = 256;

const hconfig_t* process_config;

std::string  checkpoint_dir;
static pid_t child_pid = 0;

char fpstate_buffer[FPSTATE_SIZE];

struct Checkpoint_Memory_Region {
  RegionInfo  region_info;
  bool        already_mapped;
  std::string data_file;
};

static Checkpoint_Memory_Region memory_regions[MAX_MEMORY_REGIONS];
int                             heap_region_id     = -1;
int                             stack_region_id    = -1;
int                             vdso_region_id     = -1;
int                             vsyscall_region_id = -1;
int                             vvar_region_id     = -1;

void* checkpoint_brk;

int num_valid_memory_regions = 0;

struct X86_Registers {
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rbp;
  uint64_t rsp;
  uint64_t rbx;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rax;
  uint64_t r8;
  uint64_t r9;
  uint64_t r10;
  uint64_t r11;
  uint64_t r12;
  uint64_t r13;
  uint64_t r14;
  uint64_t r15;
  uint64_t cs;
  uint64_t ss;
  uint64_t ds;
  uint64_t es;
  uint64_t fs;
  uint64_t gs;
  uint64_t fs_base;
  uint64_t gs_base;
  uint64_t rflags;
  uint64_t rip;
};

static X86_Registers registers;

static const char* cwd;
static const char* exe_path;

static const char* HEX_PREFIX = "0x";

static const char* require_str(const struct hconfig_t* config,
                               const char*             name);

static uint64_t require_uint64(const struct hconfig_t* config,
                               const char*             name);
static int64_t  require_int64(const struct hconfig_t* config, const char* name);

static const struct hconfig_t* subconfig(const struct hconfig_t* config,
                                         const char*             name);

static const struct hconfig_t* read_checkpoint_config(
  const char* checkpoint_dir);

void read_fpstate(const struct hconfig_t* registers_config);

bool is_pin_library(std::string filename);

static const char* require_str(const struct hconfig_t* config,
                               const char*             name) {
  const char* str = hconfig_value(config, name);
  if(!str)
    fatal_and_kill_child(child_pid, "Could not find \"%s\"", name);
  return str;
}

static uint64_t require_uint64(const struct hconfig_t* config,
                               const char*             name) {
  const char* str = require_str(config, name);
  uint64_t    ans;
  int         rc;
  if(!strncmp(str, HEX_PREFIX, strlen(HEX_PREFIX))) {
    rc = sscanf(str + strlen(HEX_PREFIX), "%" PRIx64, &ans);
  } else {
    rc = sscanf(str, "%" PRIu64, &ans);
  }
  if(rc != 1)
    fatal_and_kill_child(
      child_pid, "Could not parse \"%s\" as a 64 bit unsigned integer", str);
  return ans;
}

static int64_t require_int64(const struct hconfig_t* config, const char* name) {
  const char* str = require_str(config, name);
  int64_t     ans;
  int         rc;
  if(!strncmp(str, HEX_PREFIX, strlen(HEX_PREFIX))) {
    rc = sscanf(str + strlen(HEX_PREFIX), "%" PRIx64, &ans);
  } else {
    rc = sscanf(str, "%" PRId64, &ans);
  }
  if(rc != 1)
    fatal_and_kill_child(
      child_pid, "Could not parse \"%s\" as a 64 bit signed integer", str);
  return ans;
}

static const struct hconfig_t* subconfig(const struct hconfig_t* config,
                                         const char*             name) {
  const struct hconfig_t* sub = hconfig_descend(config, name);
  if(!sub) {
    switch(hconfig_error()) {
      case HCONFIG_NAME_NOT_FOUND:
        fatal_and_kill_child(child_pid, "Subconfig \"%s\" not found", name);
        break;
      case HCONFIG_MULTIPLE_NAMES:
        fatal_and_kill_child(child_pid, "Too many subconfigs named \"%s\"",
                             name);
        break;
      default:
        fatal_and_kill_child(
          child_pid, "Unknown error looking for subconfig \"%s\"", name);
        break;
    }
  }
  return sub;
}

static void read_byte_array(char* buffer, const char* str, int buffer_size) {
  if(strncmp(str, HEX_PREFIX, 2)) {
    fatal_and_kill_child(
      child_pid,
      "Cannot read byte array: \"%s\"; only hex representation is "
      "currently supported",
      str);
  }
  str += strlen(HEX_PREFIX);
  int num_chars = strlen(str);
  if(num_chars != 2 * buffer_size) {
    fatal_and_kill_child(
      child_pid, "Mismatch between number of bytes in the checkpoint file and "
                 "register sizes");
  }
  for(int buf_idx = 0; buf_idx < buffer_size; ++buf_idx) {
    int byte;
    sscanf(str + num_chars - buf_idx * 2 - 2, "%2x", &byte);
    buffer[buf_idx] = (char)byte;
  }
}

static const struct hconfig_t* read_checkpoint_config(
  const char* checkpoint_dir) {
  std::string filepath = std::string(checkpoint_dir) + "/main";
  FILE*       file     = fopen(filepath.c_str(), "r");
  if(!file) {
    fatal_and_kill_child(child_pid, "Could not open checkpoint file %s",
                         filepath.c_str());
  }
  const struct hconfig_t* config           = hconfig_load(file);
  const struct hconfig_t* processes_config = subconfig(config, "processes");
  fclose(file);
  return subconfig(processes_config, "process");
}

static void read_memory_regions() {
  const struct hconfig_t* memory_config = subconfig(process_config, "memory");
  checkpoint_brk = (void*)require_uint64(process_config, "brk");

  if(hconfig_num_children(memory_config) > MAX_MEMORY_REGIONS) {
    fatal_and_kill_child(
      child_pid, "More memory regions in the checkpoint than the maximum size");
  }
  for(unsigned int i = 0; i < hconfig_num_children(memory_config); ++i) {
    const struct hconfig_t* range_config = hconfig_children(memory_config)[i];
    memory_regions[i].region_info.range.inclusive_lower_bound = require_uint64(
      range_config, "start");
    memory_regions[i].region_info.range.exclusive_upper_bound = require_uint64(
      range_config, "end");
    const char* permissions = hconfig_value(range_config, "permissions");
    memory_regions[i].region_info.prot = 0;
    if(permissions) {
      for(unsigned int j = 0; j < strlen(permissions); ++j) {
        switch(permissions[j]) {
          case 'r':
            memory_regions[i].region_info.prot |= PROT_READ;
            break;
          case 'w':
            memory_regions[i].region_info.prot |= PROT_WRITE;
            break;
          case 'x':
            memory_regions[i].region_info.prot |= PROT_EXEC;
            break;
          default:
            fatal_and_kill_child(child_pid, "Unknown permission '%c'",
                                 permissions[j]);
            break;
        }
      }
    }
    memory_regions[i].already_mapped         = false;
    const struct hconfig_t* mapped_to_config = hconfig_descend(range_config,
                                                               "mapped_to");
    if(mapped_to_config) {
      memory_regions[i].region_info.offset = require_uint64(mapped_to_config,
                                                            "offset");
      const char* path = hconfig_value(mapped_to_config, "path");
      memory_regions[i].region_info.file_name = std::string(path);
      if(!strcmp(path, "[heap]")) {
        heap_region_id = i;
      } else if(!strncmp(path, "[stack", 6)) {
        if(stack_region_id != -1) {
          fatal_and_kill_child(child_pid, "Found multiple stack regions");
        }
        stack_region_id = i;
      } else if(!strcmp(path, "[vdso]")) {
        vdso_region_id = i;
      } else if(!strcmp(path, "[vsyscall]")) {
        vsyscall_region_id = i;
      } else if(!strcmp(path, "[vvar]")) {
        vvar_region_id = i;
      }
    }

    memory_regions[i].data_file = std::string(
      require_str(range_config, "data"));

    num_valid_memory_regions += 1;
  }

  if(heap_region_id == -1) {
    fatal_and_kill_child(child_pid,
                         "Did not find the heap region in the checkpoint");
  }
  if(stack_region_id == -1) {
    fatal_and_kill_child(child_pid,
                         "Did not find the stack region in the checkpoint");
  }
}

void read_fpstate(const struct hconfig_t* registers_config) {
  const char* str = hconfig_value(registers_config, "FPSTATE");
  if(!str) {
    fatal_and_kill_child(child_pid, "Could not find FPSTATE in the checkpoint");
  }
  read_byte_array(fpstate_buffer, str, FPSTATE_SIZE);

  //__builtin_ia32_xrstor64(fpstate_buffer, 0xff);
}

static void read_integer_regs(const struct hconfig_t* registers_config) {
  registers.rax     = require_uint64(registers_config, "rax");
  registers.rbx     = require_uint64(registers_config, "rbx");
  registers.rcx     = require_uint64(registers_config, "rcx");
  registers.rdx     = require_uint64(registers_config, "rdx");
  registers.rsi     = require_uint64(registers_config, "rsi");
  registers.rdi     = require_uint64(registers_config, "rdi");
  registers.rsp     = require_uint64(registers_config, "rsp");
  registers.rbp     = require_uint64(registers_config, "rbp");
  registers.r8      = require_uint64(registers_config, "r8");
  registers.r9      = require_uint64(registers_config, "r9");
  registers.r10     = require_uint64(registers_config, "r10");
  registers.r11     = require_uint64(registers_config, "r11");
  registers.r12     = require_uint64(registers_config, "r12");
  registers.r13     = require_uint64(registers_config, "r13");
  registers.r14     = require_uint64(registers_config, "r14");
  registers.r15     = require_uint64(registers_config, "r15");
  registers.rip     = require_uint64(registers_config, "rip");
  registers.cs      = require_uint64(registers_config, "cs");
  registers.ss      = require_uint64(registers_config, "ss");
  registers.ds      = require_uint64(registers_config, "ds");
  registers.es      = require_uint64(registers_config, "es");
  registers.fs      = require_uint64(registers_config, "fs");
  registers.gs      = require_uint64(registers_config, "gs");
  registers.fs_base = require_uint64(registers_config, "seg_fs_base");
  registers.gs_base = require_uint64(registers_config, "seg_gs_base");
  registers.rflags  = require_uint64(registers_config, "rflags");
}

static std::vector<char*> read_null_delimited_data(const char* name) {
  std::vector<char*> addrsOfIndivStrs;

  const char* relativepath = hconfig_value(process_config, name);
  if(relativepath) {
    std::string fullpath = checkpoint_dir + "/" + std::string(relativepath);
    int         fd       = open(fullpath.c_str(), O_RDONLY);
    assert(-1 != fd);
    off_t totalLength = lseek(fd, 0, SEEK_END);
    assert((off_t)-1 != totalLength);
    void* data = mmap(0, totalLength, PROT_READ, MAP_PRIVATE, fd, 0);
    assert((void*)-1 != data);

    char*  ptr = (char*)data;
    size_t len = strlen(ptr);
    while((0 != len) && (ptr < ((char*)data + totalLength))) {
      addrsOfIndivStrs.push_back(ptr);
      ptr += (len + 1);
      len = strlen(ptr);
    }
    addrsOfIndivStrs.push_back(NULL);
    int ret = close(fd);
    if(0 != ret) {
      vfatal("Clsoing a file failed: %s", fullpath.c_str());
    }
  }
  return addrsOfIndivStrs;
}

static void read_process_state() {
  cwd      = require_str(process_config, "cwd");
  exe_path = require_str(process_config, "exe");
}

static void read_registers() {
  const struct hconfig_t* threads_config = subconfig(process_config, "threads");
  const struct hconfig_t* thread_config  = subconfig(threads_config, "thread");
  const struct hconfig_t* registers_config = subconfig(thread_config,
                                                       "registers");

  read_fpstate(registers_config);
  read_integer_regs(registers_config);
}

static void read_signals() {
  const struct hconfig_t* threads_config = subconfig(process_config, "threads");
  const struct hconfig_t* thread_config  = subconfig(threads_config, "thread");
  const struct hconfig_t* signals_config = subconfig(thread_config, "signals");

  const char* blocked = hconfig_value(signals_config, "blocked");
  if(blocked) {
    fatal_and_kill_child(
      child_pid,
      "Checkpoint loader currently does not support blocked signals at "
      "the checkpoint time");
  }
  const char* pending = hconfig_value(signals_config, "pending");
  if(pending) {
    fatal_and_kill_child(
      child_pid,
      "Checkpoint loader currently does not support pending signals at "
      "the checkpoint time");
  }
}

static void read_tls() {
  const struct hconfig_t* threads_config = subconfig(process_config, "threads");
  const struct hconfig_t* thread_config  = subconfig(threads_config, "thread");
  const struct hconfig_t* tls_config     = subconfig(thread_config,
                                                 "thread_local_storage");
  if(hconfig_num_children(tls_config)) {
    fatal_and_kill_child(
      child_pid, "Checkpoint loader currently does not support thread local "
                 "storage");
  }
}

static void resize_heap(pid_t child_pid, const RegionInfo& child_region,
                        const RegionInfo& checkpoint_region) {
  if(child_region.range.inclusive_lower_bound !=
       checkpoint_region.range.inclusive_lower_bound ||
     child_region.prot != checkpoint_region.prot || child_region.offset != 0 ||
     checkpoint_region.offset != 0) {
    std::cerr << "Child region: " << child_region << std::endl;
    std::cerr << "Checkpoint region: " << checkpoint_region << std::endl;
    fatal_and_kill_child(
      child_pid,
      "Mismatch in the heap region of the tracee and the checkpoint");
  }
  void* brk_ret = execute_brk(child_pid, checkpoint_brk);
  if(brk_ret != checkpoint_brk) {
    std::cerr << "Child region: " << child_region << std::endl;
    std::cerr << "Checkpoint region: " << checkpoint_region << std::endl;
    std::cerr << "brk() return value: " << brk_ret << std::endl;
    fatal_and_kill_child(child_pid, "brk() syscall on the tracee failed");
  }
}

static void resize_stack(pid_t child_pid, const RegionInfo& child_region,
                         const RegionInfo& checkpoint_region) {
  ADDR checkpoint_start = checkpoint_region.range.inclusive_lower_bound;
  ADDR checkpoint_end   = checkpoint_region.range.exclusive_upper_bound;
  ADDR child_start      = child_region.range.inclusive_lower_bound;
  ADDR child_end        = child_region.range.exclusive_upper_bound;

  if(child_end != checkpoint_end ||
     child_region.prot != checkpoint_region.prot || child_region.offset != 0 ||
     checkpoint_region.offset != 0) {
    std::cerr << "Child region: " << child_region << std::endl;
    std::cerr << "Checkpoint region: " << checkpoint_region << std::endl;
    fatal_and_kill_child(
      child_pid,
      "Mismatch in the stack region of the tracee and the checkpoint");
  }

  int munmap_ret = execute_munmap(child_pid, (void*)child_start,
                                  (size_t)(child_end - child_start));
  if(munmap_ret) {
    fatal_and_kill_child(child_pid,
                         "munmap() syscall on the tracee stack failed");
  }

  void* mapped_addr = execute_mmap(
    child_pid, (void*)checkpoint_start,
    (size_t)(checkpoint_end - checkpoint_start), checkpoint_region.prot,
    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_GROWSDOWN | MAP_STACK, -1, 0);

  if((ADDR)mapped_addr != checkpoint_start) {
    fatal_and_kill_child(
      child_pid, "mmap() syscall to create a new stack for the tracee failed");
  }
}

static int verify_generic_region(pid_t             child_pid,
                                 const RegionInfo& child_region) {
  int found_region_id = -1;
  for(int i = 0; i < num_valid_memory_regions; ++i) {
    const RegionInfo& checkpoint_region = memory_regions[i].region_info;
    if(child_region.range.inclusive_lower_bound ==
       checkpoint_region.range.inclusive_lower_bound) {
      if(child_region.range.exclusive_upper_bound !=
           checkpoint_region.range.exclusive_upper_bound ||
         child_region.prot != checkpoint_region.prot ||
         child_region.offset != checkpoint_region.offset ||
         child_region.file_name != checkpoint_region.file_name) {
        std::cerr << "Child region: " << child_region << std::endl;
        std::cerr << "Checkpoint region: " << checkpoint_region << std::endl;
        fatal_and_kill_child(
          child_pid, "Mismatch in a region in the tracee and the checkpoint");
      }
      found_region_id = i;
      break;
    }
  }
  if(found_region_id == -1) {
    std::cerr << "Child region: " << child_region << std::endl;
    fatal_and_kill_child(child_pid,
                         "Could not find a region starting at 0x%" PRIx64
                         " in the checkpoint",
                         child_region.range.inclusive_lower_bound);
  }
  return found_region_id;
}

void read_checkpoint(const char* cdir) {
  std::cout << "Reading checkpoint from " << cdir << std::endl;
  process_config = read_checkpoint_config(cdir);
  checkpoint_dir = std::string(cdir);
  read_process_state();
  read_registers();
  read_memory_regions();
  read_tls();
  read_signals();
}

void set_child_pid(pid_t pid) {
  assert(0 != pid);
  child_pid = pid;
}

void open_file_descriptors() {
  const struct hconfig_t* fd_parent_config = subconfig(process_config,
                                                       "file_descriptors");

  const char* path;
  off_t       offset;
  int         flags;

  int       num_open_dummies = 0;
  const int MAX_TMP_FILES    = 128;
  FILE*     dummy_files[MAX_TMP_FILES];

  for(int i = 0; i < (int)hconfig_num_children(fd_parent_config); ++i) {
    const struct hconfig_t* fd_config  = hconfig_children(fd_parent_config)[i];
    const char*             fd_num_str = hconfig_name(fd_config);
    int                     fd_num     = std::stoi(fd_num_str);

    path   = require_str(fd_config, "path");
    offset = require_int64(fd_config, "offset");
    flags  = require_int64(fd_config, "flags");

    if(fd_num == 0) {  // stdin
      if(i != fd_num) {
        fatal_and_kill_child(
          child_pid, "The file descriptor for stdin (fd = 0) should be first "
                     "in the checkpoint");
      }
      if(!strncmp(path, "pipe:", 5) || !strncmp(path, "socket:", 7)) {
        fatal_and_kill_child(
          child_pid, "stdin of the checkpoint cannot be a pipe or a socket");
      } else if(!strncmp(path, "/dev", 4)) {
        // continue using the default stdin
      } else {
        FILE* opened_file = freopen(path, "r", stdin);
        if(opened_file == 0) {
          fatal_and_kill_child(child_pid,
                               "Could not open the input file for stdin");
        }
        int ret = fcntl(fd_num, F_SETFL, flags);
        if(ret != 0) {
          fatal_and_kill_child(child_pid,
                               "Could not change the flags of stdin");
        }
        off_t ret_offset = lseek(0, offset, SEEK_SET);
        if(ret_offset != offset) {
          fatal_and_kill_child(child_pid,
                               "Could not set the offset of stdin properly");
        }
      }
    } else if(fd_num < 3) {  // stdout or stderr
      // We allow stdin (file descriptor 0) to be omitted from
      // the checkpoint. This occurs when the program is run
      // with 0<&- from the shell, e.g.,

      // "./a.out input 0<&-"

      // as is commonly done for for SPEC applications. For
      // stdout and stderr, we assume they exist as file
      // descriptors 1 and 2 in the checkpoint, and we simply do
      // nothing, which means the existing stdout and stderr of
      // the checkpoint loader will be used. Thus if stdout/err
      // was linked to another file during checkpoint creation,
      // the link will not be maintained after checkpoint
      // loading. For example, if we linked stdout to output.txt
      // by running as

      //"./a.out > output.txt"

      // then after checkpoint loading, all outputs from a.out
      // will go to the stdout of the checkpoint loader, and not
      // to output.txt. This allows for flexible piping of
      // stdout/stderr after checkpoint loading.

      if(fd_num > (i + 1)) {
        fatal_and_kill_child(
          child_pid,
          "The 2nd and 3rd file descriptors in the checkpoint should "
          "be stdout and stderr (fd = 1,2)");
      }
    } else {
      if((i + num_open_dummies) > fd_num) {
        fatal_and_kill_child(
          child_pid, "File descriptors in the checkpoint are not sorted");
      }
      while((i + num_open_dummies) < fd_num) {
        if(num_open_dummies >= MAX_TMP_FILES) {
          fatal_and_kill_child(child_pid, "MAX_TMP_FILES is too small");
        }
        dummy_files[num_open_dummies] = tmpfile();
        num_open_dummies += 1;
      }
      int opened_fd = open(path, flags);
      if(opened_fd == -1) {
        fatal_and_kill_child(child_pid, "Could not open the file descriptor %d",
                             fd_num);
      }
      if(opened_fd != fd_num) {
        fatal_and_kill_child(
          child_pid, "Got unexpected file descriptor (%d instead of %d)",
          opened_fd, fd_num);
      }
      off_t ret_offset = lseek(fd_num, offset, SEEK_SET);
      if(ret_offset != offset) {
        fatal_and_kill_child(
          child_pid, "Could not set the offset of file descriptor %d properly",
          fd_num);
      }
    }
  }

  for(int i = 0; i < num_open_dummies; ++i) {
    fclose(dummy_files[i]);
  }
}

void change_working_directory() {
  int res = chdir(cwd);
  if(0 != res) {
    fatal_and_kill_child(child_pid, "Could not change working directory to %s",
                         cwd);
  }
}


// DEPRACTED: This function is used to filter out memory regions in the
// checkpoint that correspond to the checkpoint creator pintool, which are not
// actually part of the checkpointed process. Later, the checkpoint creator
// tool was changed to filter out these regions at creation time, so this is
// not necessary anymore. However, we keep this function in the loader to
// remain compatible with the checkpoints created before the creator was
// updated.
bool is_pin_library(std::string filename) {
  std::string pin_root = std::string(std::getenv("PIN_ROOT"));

  if(filename.find(pin_root) != std::string::npos)
    return true;
  else {
    std::string create_checkpoint = std::string("create_checkpoint.so");
    if(filename.find(create_checkpoint) != std::string::npos)
      return true;
    else
      return false;
  }
}

void allocate_new_regions(pid_t child_pid) {
  std::cout << "Allocating all regions in the child process ..." << std::endl;
  std::vector<RegionInfo> child_regions = read_proc_maps_file(child_pid);

  for(auto& child_region : child_regions) {
    if(child_region.file_name == std::string("[heap]")) {
      resize_heap(child_pid, child_region,
                  memory_regions[heap_region_id].region_info);
      memory_regions[heap_region_id].already_mapped = true;

    } else if(!strncmp(child_region.file_name.c_str(), "[stack", 6)) {
      resize_stack(child_pid, child_region,
                   memory_regions[stack_region_id].region_info);
      memory_regions[stack_region_id].already_mapped = true;

    } else {
      if((child_region.file_name == std::string("[vvar]") &&
          vvar_region_id == -1) ||
         (child_region.file_name == std::string("[vdso]") &&
          vdso_region_id == -1) ||
         (child_region.file_name == std::string("[vsyscall]") &&
          vsyscall_region_id == -1)) {
        std::cout << " Found the " << child_region.file_name
                  << " in the binary, but not in the checkpoint. This is "
                     "probably fine and a is a result of a bug in PIN or "
                     "ptrace during checkpoint creation. So we'll skip sanity "
                     "checks for this region.\n";
        continue;
      }

      int checkpoint_region_id = verify_generic_region(child_pid, child_region);
      memory_regions[checkpoint_region_id].already_mapped = true;
    }
  }

  for(int i = 0; i < num_valid_memory_regions; ++i) {
    const RegionInfo& checkpoint_region = memory_regions[i].region_info;

    if(is_pin_library(checkpoint_region.file_name)) {
      // Don't allocate pin library regions
      continue;
    }

    void*  addr   = (void*)checkpoint_region.range.inclusive_lower_bound;
    size_t length = checkpoint_region.range.exclusive_upper_bound -
                    checkpoint_region.range.inclusive_lower_bound;
    int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
    if(memory_regions[i].already_mapped == false) {
      int flags;
      int fd;
      int offset;
      if(checkpoint_region.file_name.empty()) {
        flags  = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED;
        fd     = -1;
        offset = 0;
      } else {
        flags = MAP_PRIVATE | MAP_FIXED;
        fd    = execute_open(child_pid, checkpoint_region.file_name.c_str(), 0);
        offset = checkpoint_region.offset;
      }
      void* mapped_addr = execute_mmap(child_pid, addr, length, prot, flags, fd,
                                       offset);
      if(mapped_addr != addr) {
        std::cerr << "Checkpoint region: " << checkpoint_region << std::endl;
        std::cerr << "mmap return value: " << mapped_addr << "\n";
        fatal_and_kill_child(child_pid,
                             "mmap() did not map the region correctly");
      }
      if(fd >= 0) {
        if(execute_close(child_pid, fd)) {
          std::cerr << "Checkpoint region: " << checkpoint_region << std::endl;
          fatal_and_kill_child(child_pid,
                               "close() failed after mapping this region");
        }
      }
    } else {
      if(i != vsyscall_region_id && i != vdso_region_id &&
         i != vvar_region_id) {
        int mprotect_ret = execute_mprotect(child_pid, addr, length, prot);
        if(mprotect_ret != 0) {
          std::cerr << "Checkpoint region: " << checkpoint_region << std::endl;
          std::cerr << "mprotect return value: " << mprotect_ret << "\n";
          fatal_and_kill_child(
            child_pid,
            "mprotect() did not change the region protection correctly");
        }
      }
    }
  }
}

void write_data_to_regions(pid_t child_pid) {
  std::cout << "Writing data to all regions ..." << std::endl;
  auto[sharedmem_tracer_addr, sharedmem_tracee_addr] = allocate_shared_memory(
    child_pid);
  constexpr int INJECTION_REGION_SIZE = 4096;
  void* injection_site = execute_mmap(child_pid, NULL, INJECTION_REGION_SIZE,
                                      PROT_EXEC | PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(injection_site == (void*)-1) {
    fatal_and_kill_child(
      child_pid, "Could not map a new region for code injection. errno: %s",
      std::strerror(errno));
  }
  struct user_regs_struct oldregs;
  if(ptrace(PTRACE_GETREGS, child_pid, NULL, &oldregs)) {
    perror("PTRACE_GETREGS");
    kill_and_exit(child_pid);
  }

  struct user_regs_struct newregs;
  memcpy(&newregs, &oldregs, sizeof(newregs));
  newregs.rip = (unsigned long long)injection_site;
  if(ptrace(PTRACE_SETREGS, child_pid, NULL, &newregs)) {
    perror("PTRACE_SETREGS");
    kill_and_exit(child_pid);
  }

  for(int i = 0; i < num_valid_memory_regions; ++i) {
    const RegionInfo& checkpoint_region = memory_regions[i].region_info;
    size_t region_size = checkpoint_region.range.exclusive_upper_bound -
                         checkpoint_region.range.inclusive_lower_bound;
    assert(region_size % 8 == 0);
    if(is_pin_library(checkpoint_region.file_name)) {
      // Don't allocate pin library regions
      continue;
    }

    char*       temp_buffer = new char[region_size];
    std::string cmd         = std::string("bzip2 -dc ") + checkpoint_dir + "/" +
                      memory_regions[i].data_file;
    DEBUG(cmd);
    FILE* data_file = popen(cmd.c_str(), "r");
    if(!data_file) {
      fatal_and_kill_child(child_pid, "Error opening a dat file: %s",
                           memory_regions[i].data_file.c_str());
    }
    size_t bytes_read = fread(temp_buffer, 1, region_size, data_file);
    if(bytes_read != region_size) {
      fatal_and_kill_child(child_pid,
                           "dat file did not have enough bytes: %s. "
                           "bytes_read: %d, region_size: %d",
                           memory_regions[i].data_file.c_str(), bytes_read,
                           region_size);
    }

    char temp_byte;
    bytes_read = fread(&temp_byte, 1, 1, data_file);
    if(bytes_read == 1 || !feof(data_file)) {
      fatal_and_kill_child(child_pid, "dat file has too many bytes: %s",
                           memory_regions[i].data_file.c_str());
    }
    fclose(data_file);

    if(i == vsyscall_region_id || i == vdso_region_id || i == vvar_region_id) {
      DEBUG("asserting regions are equal: start");
      assert_equal_mem(child_pid, temp_buffer,
                       (char*)checkpoint_region.range.inclusive_lower_bound,
                       region_size);
      DEBUG("asserting regions are equal: done");
    } else {
      DEBUG("doing a ptrace memcpy: start");
      // execute_memcpy(child_pid,
      //               (void*)checkpoint_region.range.inclusive_lower_bound,
      //               temp_buffer, region_size);
      shared_memory_memcpy(
        child_pid, (void*)checkpoint_region.range.inclusive_lower_bound,
        temp_buffer, region_size, sharedmem_tracer_addr, sharedmem_tracee_addr);
      DEBUG("doing a ptrace memcpy: end");
    }

    delete[] temp_buffer;
  }

  if(ptrace(PTRACE_SETREGS, child_pid, NULL, &oldregs)) {
    perror("PTRACE_SETREGS");
    kill_and_exit(child_pid);
  }
  int munmap_ret = execute_munmap(child_pid, injection_site,
                                  INJECTION_REGION_SIZE);
  if(munmap_ret) {
    fatal_and_kill_child(
      child_pid, "munmap() for deallocating the code injection site failed");
  }
}

void update_region_protections(pid_t child_pid) {
  std::cout << "Updating region protection fields ..." << std::endl;
  for(int i = 0; i < num_valid_memory_regions; ++i) {
    const RegionInfo& checkpoint_region = memory_regions[i].region_info;

    if(is_pin_library(checkpoint_region.file_name)) {
      // Don't allocate pin library regions
      continue;
    }

    void*  addr   = (void*)checkpoint_region.range.inclusive_lower_bound;
    size_t length = checkpoint_region.range.exclusive_upper_bound -
                    checkpoint_region.range.inclusive_lower_bound;
    int prot = checkpoint_region.prot;
    if(i != vsyscall_region_id && i != vdso_region_id) {
      DEBUG("Running mprotect for region start: " << checkpoint_region);
      int mprotect_ret = execute_mprotect(child_pid, addr, length, prot);
      if(mprotect_ret != 0) {
        std::cerr << "Checkpoint region: " << checkpoint_region << std::endl;
        std::cerr << "mprotect return value: " << mprotect_ret << "\n";
        fatal_and_kill_child(
          child_pid,
          "mprotect() did not change the region protection correctly");
      }
    }
  }
}

void load_registers(pid_t child_pid) {
  std::cout << "Loading the architectural registers ..." << std::endl;
  struct user_regs_struct newregs;
  DEBUG("About to GETREGS for load_registers()");
  if(ptrace(PTRACE_GETREGS, child_pid, NULL, &newregs)) {
    perror("PTRACE_GETREGS");
    kill_and_exit(child_pid);
  }

  newregs.rdi     = registers.rdi;
  newregs.rsi     = registers.rsi;
  newregs.rbp     = registers.rbp;
  newregs.rsp     = registers.rsp;
  newregs.rbx     = registers.rbx;
  newregs.rdx     = registers.rdx;
  newregs.rcx     = registers.rcx;
  newregs.rax     = registers.rax;
  newregs.r8      = registers.r8;
  newregs.r9      = registers.r9;
  newregs.r10     = registers.r10;
  newregs.r11     = registers.r11;
  newregs.r12     = registers.r12;
  newregs.r13     = registers.r13;
  newregs.r14     = registers.r14;
  newregs.r15     = registers.r15;
  newregs.cs      = registers.cs;
  newregs.ss      = registers.ss;
  newregs.ds      = registers.ds;
  newregs.es      = registers.es;
  newregs.fs      = registers.fs;
  newregs.gs      = registers.gs;
  newregs.fs_base = registers.fs_base;
  newregs.gs_base = registers.gs_base;
  newregs.eflags  = registers.rflags;
  newregs.rip     = registers.rip;

  DEBUG("About to SETREGS for load_registers()");

  // set the new registers with our syscall arguments
  if(ptrace(PTRACE_SETREGS, child_pid, NULL, &newregs)) {
    perror("PTRACE_SETREGS");
    kill_and_exit(child_pid);
  }
  DEBUG("load_registers() DONE");
}

uint64_t get_checkpoint_start_rip() {
  return registers.rip;
}

const char* get_checkpoint_exe_path() {
  return exe_path;
}

std::vector<char*> get_checkpoint_envp_vector() {
  return read_null_delimited_data("environ");
}
std::vector<char*> get_checkpoint_argv_vector() {
  return read_null_delimited_data("cmdline");
}

bool get_checkpoint_os_info(std::string& kernel_release,
                            std::string& os_version) {
  const struct hconfig_t* os_info_config = hconfig_descend(process_config,
                                                           "os_info");
  if(NULL != os_info_config) {
    kernel_release = std::string(require_str(os_info_config, "release"));
    os_version     = std::string(require_str(os_info_config, "version"));
    return true;
  } else {
    return false;
  }
}

bool get_checkpoint_cpuinfo(std::string& flags) {
  const struct hconfig_t* cpuinfo_config = hconfig_descend(process_config,
                                                           "cpuinfo");
  if(NULL != cpuinfo_config) {
    flags = std::string(require_str(cpuinfo_config, "flags"));
    return true;
  } else {
    return false;
  }
}
