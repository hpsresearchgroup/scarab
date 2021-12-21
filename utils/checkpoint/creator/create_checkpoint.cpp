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

#include <asm/ldt.h>
#include <dirent.h>
#include <iostream>
#include <linux/unistd.h>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include "../loader/cpuinfo.h"
#include "control_manager.H"
#include "instlib.H"
#include "pin.H"

using namespace INSTLIB;
using namespace CONTROLLER;

KNOB<string> KnobOutputDir(KNOB_MODE_WRITEONCE, "pintool", "o", "checkpoint",
                           "Checkpoint dir name");
KNOB<bool>   KnobDebug(KNOB_MODE_WRITEONCE, "pintool", "d", "0", "Debug mode");

#define DEBUG(...)                  \
  do {                              \
    if(KnobDebug.Value()) {         \
      fprintf(stderr, __VA_ARGS__); \
    }                               \
  } while(0)

void controlHandler(EVENT_TYPE ev, VOID* val, CONTEXT* ctxt, VOID* ip,
                    THREADID tid);
void syscallEntryHandler(THREADID tid, CONTEXT* ctxt, SYSCALL_STANDARD std,
                         VOID* v);
void syscallExitHandler(THREADID tid, CONTEXT* ctxt, SYSCALL_STANDARD std,
                        VOID* v);
void takeCheckpoint(CONTEXT* ctxt, THREADID tid);

/* File descriptor dumping functions */
void        dumpFDs(FILE* out, UINT pid);
void        dumpFdInfo(FILE* out, ADDRINT fd);
std::string getPathFromFd(ADDRINT fd);

/* General process info dumping function */
void dumpProcessInfo(FILE* out, CONTEXT* ctxt);
void dumpProcFileRawContent(FILE* out, std::string fileName, UINT pid);
void dumpThread(FILE* out, CONTEXT* ctxt);
void dumpOSinfo(FILE* out);
void dumpCPUinfo(FILE* out);

/* Register dumping functions */
void dumpRegs(FILE* out, CONTEXT* ctxt);
void dumpRegRange(FILE* out, CONTEXT* ctxt, REG start_reg, REG end_reg,
                  int width);
void dumpExtReg(FILE* out, std::string name, UINT8* data, UINT size);
void dumpFpState(FILE* out, CONTEXT* ctxt);

/* Memory dumping functions */
int  nextDataFileId = 0;
void dumpMemory(FILE* out, UINT pid);
void processMapsLine(FILE* out, const std::string& line);
int  dumpMemoryData(const char* path, UINT8* start, UINT8* end);

/* Signal dumping functions */
void dumpSignals(FILE* out);
void dumpSigSet(FILE* out, sigset_t* sigset);

/* Thread local storage */
void dumpTLS(FILE* out);

// Intercepted TLS
const UINT NUM_TLS = 32;
user_desc  tls[NUM_TLS];
ADDRINT    syscallNum;
ADDRINT    syscallArg;

/* Simple hierarchical configuration emitter functions */
int       treeDepth        = 0;
bool      newLineStarted   = true;
const int OGDL_INDENT_SIZE = 4;
void      startChild(FILE* out, const char* name);
void      startInlineChild(FILE* out, const char* name);
void      endChild(FILE* out);
#define INLINE_CHILD(out, name, ...) \
  do {                               \
    startInlineChild((out), (name)); \
    fprintf((out), __VA_ARGS__);     \
    endChild((out));                 \
  } while(0)

CONTROL_MANAGER control("controller_");

void syscallEntryHandler(THREADID tid, CONTEXT* ctxt, SYSCALL_STANDARD std,
                         VOID* v) {
  syscallNum = PIN_GetSyscallNumber(ctxt, std);
  syscallArg = PIN_GetSyscallArgument(ctxt, std, 0);
}

void syscallExitHandler(THREADID tid, CONTEXT* ctxt, SYSCALL_STANDARD std,
                        VOID* v) {
  if(syscallNum == SYS_set_thread_area) {
    DEBUG("Inside set_thread_area Syscall\n");
    user_desc tmp;
    PIN_SafeCopy(&tmp, (void*)syscallArg, sizeof(user_desc));
    if(tmp.entry_number < NUM_TLS) {
      memcpy(&tls[tmp.entry_number], &tmp, sizeof(user_desc));
    }
  }
}

void controlHandler(EVENT_TYPE ev, VOID* val, CONTEXT* ctxt, VOID* ip,
                    THREADID tid, BOOL bcast) {
  std::cout << "Entered control handler\n";
  switch(ev) {
    case EVENT_START:
      std::cout << " event start\n";
      ASSERTX(ctxt);
      takeCheckpoint(ctxt, tid);
      PIN_ExitApplication(0);
      break;
    case EVENT_STOP:
      std::cout << "Stop" << endl;
      break;

    case EVENT_THREADID:
      std::cout << "ThreadID" << endl;
      break;

    default:
      ASSERTX(false);
      break;
  }
}

void startChild(FILE* out, const char* name) {
  if(!newLineStarted)
    fprintf(out, "\n");
  for(int i = 0; i < treeDepth * OGDL_INDENT_SIZE; ++i)
    fprintf(out, " ");
  treeDepth++;
  newLineStarted = false;

  fprintf(out, "%s", name);
}

void startInlineChild(FILE* out, const char* name) {
  startChild(out, name);
  fprintf(out, " ");
}

void endChild(FILE* out) {
  ASSERTX(treeDepth > 0);
  treeDepth--;
  if(!newLineStarted)
    fprintf(out, "\n");
  newLineStarted = true;
}

void dumpProcessInfo(FILE* out, CONTEXT* ctxt) {
  DEBUG("Dumping process %d\n", PIN_GetPid());
  startChild(out, "process");
  INLINE_CHILD(out, "pid", "%d", PIN_GetPid());
  // char *cwd = get_current_dir_name();
  char* cwd = getcwd(0, 0);
  INLINE_CHILD(out, "cwd", "%s", cwd);
  free(cwd);
  cwd = NULL;

  std::stringstream pidSS;
  pidSS << PIN_GetPid();
  std::string proc_exe_path = "/proc/" + pidSS.str() + "/exe";

  const int MAX_PATH_SIZE = 4096;
  char      exe_path[MAX_PATH_SIZE + 1];
  memset(exe_path, 0, MAX_PATH_SIZE + 1);
  readlink(proc_exe_path.c_str(), exe_path, MAX_PATH_SIZE);
  INLINE_CHILD(out, "exe", "%s", exe_path);

  INLINE_CHILD(out, "brk", "%p", sbrk(0));
  {
    DEBUG("Dumping standard streams\n");
    startChild(out, "standard_streams");
    INLINE_CHILD(out, "stdin", "%lu", fileno(stdin));
    INLINE_CHILD(out, "stdout", "%lu", fileno(stdout));
    INLINE_CHILD(out, "stderr", "%lu", fileno(stderr));
    endChild(out);
  }
  dumpFDs(out, PIN_GetPid());
  startChild(out, "threads");
  dumpThread(out, ctxt);
  endChild(out);
  dumpProcFileRawContent(out, "environ", PIN_GetPid());
  dumpProcFileRawContent(out, "cmdline", PIN_GetPid());
  dumpOSinfo(out);
  dumpCPUinfo(out);
  dumpMemory(out, PIN_GetPid());
  endChild(out);
  DEBUG("End of Dumping process %d\n", PIN_GetPid());
}


std::string escapeQuotes(const char* orig) {
  std::stringstream ss;
  for(const char* cPtr = orig; *cPtr; ++cPtr) {
    if(*cPtr == '"')
      ss << '\\';
    ss << *cPtr;
  }
  return ss.str();
}

void dumpProcFileRawContent(FILE* out, std::string fileName, UINT pid) {
  std::stringstream pidSS;
  pidSS << pid;
  std::ifstream procFileInputStream;
  procFileInputStream.open(("/proc/" + pidSS.str() + "/" + fileName).c_str());

  std::string   relativeOutputDatFilePath = (fileName + ".dat");
  std::ofstream outputDatFileStream;
  outputDatFileStream.open(
    (KnobOutputDir.Value() + "/" + relativeOutputDatFilePath).c_str());

  while(procFileInputStream.peek() != EOF) {
    std::string line;
    getline(procFileInputStream, line);
    outputDatFileStream << line;
  }

  procFileInputStream.close();
  outputDatFileStream.close();

  INLINE_CHILD(out, fileName.c_str(), "%s", relativeOutputDatFilePath.c_str());
}

void dumpOSinfo(FILE* out) {
  const USIZE buf_size = 128;
  char        kernel_release[buf_size];
  char        os_version[buf_size];

  OS_RETURN_CODE ret_code = OS_GetKernelRelease(kernel_release, buf_size);
  ASSERTX(OS_RETURN_CODE_NO_ERROR == ret_code);
  ret_code = OS_GetOSVersion(os_version, buf_size);
  ASSERTX(OS_RETURN_CODE_NO_ERROR == ret_code);

  startChild(out, "os_info");
  INLINE_CHILD(out, "release", "\"%s\"", kernel_release);
  INLINE_CHILD(out, "version", "\"%s\"", os_version);
  endChild(out);
}

void dumpCPUinfo(FILE* out) {
  std::string cpuinfo_flags = getCPUflags();
  startChild(out, "cpuinfo");
  INLINE_CHILD(out, "flags", "\"%s\"", cpuinfo_flags.c_str());
  endChild(out);
}

void dumpMemory(FILE* out, UINT pid) {
  DEBUG("Dumping memory\n");
  startChild(out, "memory");

  // Using C++ I/O for simplicity
  std::ifstream     maps;
  std::stringstream pidSS;
  pidSS << pid;
  maps.open(("/proc/" + pidSS.str() + "/maps").c_str());
  maps >> std::hex;
  while(maps.peek() != EOF) {
    std::string line;
    getline(maps, line);
    processMapsLine(out, line);
  }
  maps.close();

  endChild(out);
}

bool is_pin_library(const std::string& path) {
  auto pin_root = std::string(std::getenv("PIN_ROOT"));

  if(path.find(pin_root) != std::string::npos) {
    return true;

  } else {
    auto create_checkpoint = std::string("create_checkpoint.so");
    if(path.find(create_checkpoint) != std::string::npos)
      return true;
    else
      return false;
  }
}

void processMapsLine(FILE* out, const std::string& line) {
  std::stringstream ss(line);
  // maps format ([xy] means either x or y, numbers except inode are in hex,
  // path me be omitted): addr1-addr2 [r-][w-][x-][ps] offset dev1:dev2 inode
  // path
  UINT64      addr1;
  char        dash;
  UINT64      addr2;
  char        permR, permW, permX, shareMode;
  ADDRINT     offset;
  std::string device;
  std::string inode;  // string to avoid switching between dec/hex
  std::string path;
  ss >> std::hex >> addr1;
  ss >> dash;
  ASSERTX(dash == '-');
  ss >> addr2;
  ASSERTX(addr2 >= addr1);
  ss >> permR;
  ASSERTX(permR == 'r' || permR == '-');
  ss >> permW;
  ASSERTX(permW == 'w' || permW == '-');
  ss >> permX;
  ASSERTX(permX == 'x' || permX == '-');
  ss >> shareMode;
  ASSERTX(shareMode == 'p');  // Scarab does not support shared mapping
  ss >> offset;
  ss >> device;
  ss >> std::dec >> inode;
  ss >> path;

  if(permR == '-') {  // no read permission
    if(mprotect(reinterpret_cast<void*>(addr1), addr2 - addr1, PROT_READ)) {
      // could not change protection to read
      std::cerr << "Warning: ignoring memory range " << line << std::endl;
      return;
    }
  }
  std::stringstream dataIdSS;
  dataIdSS << nextDataFileId;
  std::cout << "============================" << std::endl;
  std::cout << "page addr: " << std::hex << addr1 << " " << addr2 << std::endl;
  std::cout << "path: " << path << std::endl;
  OS_MEMORY_AT_ADDR_INFORMATION memory_info;
  OS_RETURN_CODE                query = OS_QueryMemory(
    PIN_GetPid(), reinterpret_cast<void*>(addr1), &memory_info);
  std::cout << "Pin query results: " << query.generic_err << std::endl;
  std::cout << "Pin query base addr: " << memory_info.BaseAddress
            << ", page size: " << memory_info.MapSize << std::endl;

  if(is_pin_library(path)) {
    std::cout << "Skipping the page because it corresponds to a PIN library"
              << std::endl;
    return;
  }

  if(dumpMemoryData(
       (KnobOutputDir.Value() + "/" + dataIdSS.str() + ".dat").c_str(),
       (UINT8*)addr1, (UINT8*)addr2)) {
    startChild(out, "range");
    INLINE_CHILD(out, "start", "0x%lx", addr1);
    INLINE_CHILD(out, "end", "0x%lx", addr2);
    {
      startInlineChild(out, "permissions");
      if(permR != '-')
        fprintf(out, "r");
      if(permW != '-')
        fprintf(out, "w");
      if(permX != '-')
        fprintf(out, "x");
      endChild(out);
    }
    if(!path.empty()) {
      startChild(out, "mapped_to");
      INLINE_CHILD(out, "path", "%s", path.c_str());
      INLINE_CHILD(out, "offset", "0x%lx", offset);
      endChild(out);
    }
    INLINE_CHILD(out, "data", "%d.dat", nextDataFileId);
    nextDataFileId++;
    endChild(out);
  } else {
    std::cerr << "Ignoring memory region: " << line << std::endl;
  }
}

int dumpMemoryData(const char* path, UINT8* start, UINT8* end) {
  //    FILE * out = fopen(path, "w");
  std::stringstream bzip_cmd;
  bzip_cmd << "bzip2 > " << path;
  FILE* out = popen(bzip_cmd.str().c_str(), "w");

  const UINT64 BUF_SIZE = 4096;
  char         buf[BUF_SIZE];
  UINT64       total_bytes_written = 0;
  for(UINT8* addr = start; addr < end; addr += BUF_SIZE) {
    UINT64         bytes_left = end - addr;
    UINT64         num_bytes  = bytes_left < BUF_SIZE ? bytes_left : BUF_SIZE;
    EXCEPTION_INFO ex;
    UINT64         bytes_copied = PIN_SafeCopyEx(buf, addr, num_bytes, &ex);
    if(bytes_copied != num_bytes) {
      std::cerr << "Could not copy data at " << start << ": "
                << PIN_ExceptionToString(&ex) << std::endl;
      pclose(out);
      return 0;
    }
    UINT64 bytes_written = fwrite(buf, 1, num_bytes, out);
    if(bytes_written != num_bytes) {
      perror(0);
      exit(1);
    }
    total_bytes_written += bytes_written;
  }
  UINT64 region_size = (UINT64)(end - start);
  INT64  delta       = total_bytes_written - region_size;
  if(delta != 0) {
    std::cerr << "ERROR: Saving the content of a region to file " << path
              << " failed!" << std::endl;
    std::cerr << "Bytes written: " << std::dec << total_bytes_written
              << ", region size: " << region_size << ", delta: " << delta
              << std::endl;
    pclose(out);
    exit(1);
  }
  pclose(out);
  return 1;
}

void dumpFDs(FILE* out, UINT pid) {
  DEBUG("Dumping file descriptors\n");
  startChild(out, "file_descriptors");

  // we first get the list of file descriptors from /proc/pid/fdinfo
  const int MAX_FDS = 1024;
  ADDRINT   fds[MAX_FDS];
  int       numFds = 0;

  std::stringstream pidSS;
  pidSS << pid;
  DIR* dir = opendir(("/proc/" + pidSS.str() + "/fdinfo").c_str());

  while(dir) {
    struct dirent* entry = readdir(dir);
    if(entry) {
      if(strcmp(entry->d_name, ".") != 0 &&
         strcmp(entry->d_name, "..") != 0) {  // not a "." or ".."
        ASSERTX(numFds < MAX_FDS);
        fds[numFds] = atoi(entry->d_name);
        ++numFds;
      }
    } else {
      break;
    }
  }

  // by closing the directory, we invalidate the FDs opened by opendir/readdir
  closedir(dir);

  for(int i = 0; i < numFds; ++i) {
    if(fcntl(fds[i], F_GETFL, 0) != -1) {
      // valid FD (not related to /proc/pid/fd traversal)
      if(fileno(out) != fds[i]) {  // not the checkpoint fd
        dumpFdInfo(out, fds[i]);

        // ensure the data is out to disk
        if(fcntl(fds[i], F_GETFL, 0) & (O_WRONLY | O_RDWR)) {
          fsync(fds[i]);
        }
      }
    } else {
      ASSERTX(errno == EBADF);
      errno = 0;
    }
  }

  endChild(out);
}

void dumpFdInfo(FILE* out, ADDRINT fd) {
  std::stringstream fdSS;
  fdSS << fd;
  startChild(out, fdSS.str().c_str());
  std::string path = getPathFromFd(fd);
  INLINE_CHILD(out, "path", "%s", path.c_str());
  INLINE_CHILD(out, "offset", "%lld", (long long int)lseek(fd, 0, SEEK_CUR));
  INLINE_CHILD(out, "flags", "0x%x", fcntl(fd, F_GETFL, 0));
  endChild(out);
}

std::string getPathFromFd(ADDRINT fd) {
  // using a constant max size because lstat does not give me the
  // correct path size
  const int         MAX_PATH_SIZE = 4096;
  std::stringstream pidSS, fdSS;
  pidSS << PIN_GetPid();
  fdSS << fd;
  std::string fdPath = "/proc/" + pidSS.str() + "/fd/" + fdSS.str();
  char        path[MAX_PATH_SIZE + 1];
  memset(path, 0, MAX_PATH_SIZE + 1);
  readlink(fdPath.c_str(), path, MAX_PATH_SIZE);
  return std::string(path);
}

void dumpThread(FILE* out, CONTEXT* ctxt) {
  DEBUG("Dumping thread %d\n", PIN_GetTid());
  startChild(out, "thread");  // currently single thread
  INLINE_CHILD(out, "tid", "%d", PIN_GetTid());
  dumpRegs(out, ctxt);
  dumpSignals(out);
  dumpTLS(out);
  endChild(out);
}

void dumpTLS(FILE* out) {
  DEBUG("Dumping TLS\n");
  startChild(out, "thread_local_storage");
  UINT num = 0;
  while(num < NUM_TLS) {
    if(tls[num].useable) {
      std::cerr << "We currently do not support checkpoints with TLS\n";
      ASSERTX(false);
      DEBUG("Useable TLS %d\n", num);
      user_desc*        u_info = &tls[num];
      std::stringstream numSS;
      numSS << num;
      startChild(out, numSS.str().c_str());
      INLINE_CHILD(out, "entry_number", "%d", u_info->entry_number);
      INLINE_CHILD(out, "base_addr", "0x%u", u_info->base_addr);
      INLINE_CHILD(out, "limit", "%d", u_info->limit);
      INLINE_CHILD(out, "seg_32bit", "%d", u_info->seg_32bit);
      INLINE_CHILD(out, "contents", "%d", u_info->contents);
      INLINE_CHILD(out, "read_exec_only", "%d", u_info->read_exec_only);
      INLINE_CHILD(out, "limit_in_pages", "%d", u_info->limit_in_pages);
      INLINE_CHILD(out, "seg_not_present", "%d", u_info->seg_not_present);
      INLINE_CHILD(out, "useable", "%d", u_info->useable);
      endChild(out);
    }
    ++num;
  }
  endChild(out);
}


void dumpSignals(FILE* out) {
  startChild(out, "signals");
  sigset_t blockedSignals;
  sigset_t pendingSignals;
  if(sigprocmask(SIG_BLOCK, NULL, &blockedSignals) == 0) {
    startChild(out, "blocked");
    dumpSigSet(out, &blockedSignals);
    endChild(out);
  }
  if(sigpending(&pendingSignals) == 0) {
    startChild(out, "pending");
    dumpSigSet(out, &pendingSignals);
    endChild(out);
  }
  endChild(out);
}

void dumpSigSet(FILE* out, sigset_t* sigset) {
  for(int sig = 0; sig < NSIG; ++sig) {
    if(sigismember(sigset, sig) == 1) {
      std::stringstream sigSS;
      sigSS << sig;
      startChild(out, sigSS.str().c_str());
      endChild(out);
    }
  }
}

void dumpRegs(FILE* out, CONTEXT* ctxt) {
  DEBUG("Dumping registers\n");
  startChild(out, "registers");
  dumpRegRange(out, ctxt, REG_GR_BASE, REG_GR_LAST, 8);
  dumpRegRange(out, ctxt, REG_SEG_BASE, REG_SEG_LAST, 8);
  dumpRegRange(out, ctxt, REG_SEG_GS_BASE, REG_SEG_FS_BASE, 8);
  dumpRegRange(out, ctxt, REG_RFLAGS, REG_RFLAGS, 8);
  dumpRegRange(out, ctxt, REG_RIP, REG_RIP, 8);
  dumpFpState(out, ctxt);

  if(KnobDebug.Value()) {
    if(PIN_ContextContainsState(ctxt, PROCESSOR_STATE_X87)) {
      dumpRegRange(out, ctxt, REG_FPST_BASE, REG_FPST_LAST, 8);
    }

    if(PIN_ContextContainsState(ctxt, PROCESSOR_STATE_ZMM)) {
      dumpRegRange(out, ctxt, REG_ZMM_BASE, REG_ZMM_LAST, 64);
      dumpRegRange(out, ctxt, REG_K0, REG_K_LAST, 8);
    } else if(PIN_ContextContainsState(ctxt, PROCESSOR_STATE_YMM)) {
      dumpRegRange(out, ctxt, REG_YMM_BASE, REG_YMM_AVX_LAST, 32);
    } else if(PIN_ContextContainsState(ctxt, PROCESSOR_STATE_XMM)) {
      dumpRegRange(out, ctxt, REG_XMM_BASE, REG_XMM_SSE_LAST, 16);
    }
  }


  endChild(out);
}

void dumpRegRange(FILE* out, CONTEXT* ctxt, REG start_reg, REG end_reg,
                  int width) {
  PIN_REGISTER buffer_reg;
  for(int i = start_reg; i <= end_reg; ++i) {
    REG reg = (REG)i;
    PIN_GetContextRegval(ctxt, reg, buffer_reg.byte);
    dumpExtReg(out, REG_StringShort(reg), buffer_reg.byte, width);
  }
}

void dumpExtReg(FILE* out, std::string name, UINT8* data, UINT size) {
  startInlineChild(out, name.c_str());
  fprintf(out, "0x");
  for(UINT i = 0; i < size; ++i) {
    fprintf(out, "%02x", (UINT)data[size - i - 1]);  // MSB first!
  }
  endChild(out);
}

void dumpFpState(FILE* out, CONTEXT* ctxt) {
  FPSTATE fpstate;
  PIN_GetContextFPState(ctxt, &fpstate);
  startInlineChild(out, "FPSTATE");
  std::cout << "xsave header mask: 0x" << std::hex
            << fpstate._xstate._extendedHeader._mask << std::endl;
  std::cout << "xsave header xcomp: 0x" << std::hex
            << fpstate._xstate._extendedHeader._xcomp_bv << std::endl;
  fprintf(out, "0x");
  for(UINT i = 0; i < sizeof(fpstate); ++i) {
    fprintf(out, "%02x", (UINT)((UINT8*)&fpstate)[sizeof(fpstate) - 1 - i]);
  }
  endChild(out);
}

void takeCheckpoint(CONTEXT* ctxt, THREADID tid) {
  DEBUG("Taking checkpoint\n");
  ASSERTX(ctxt);
  mkdir(KnobOutputDir.Value().c_str(), S_IRWXU);
  FILE* out = fopen((KnobOutputDir.Value() + "/main").c_str(), "w");
  INLINE_CHILD(out, "generator", "pincpt");
  startChild(out, "processes");
  dumpProcessInfo(out, ctxt);
  endChild(out);
  fclose(out);
  // exit(0);
}

VOID Fini(INT32 code, VOID* v) {
  std::cout << "Fini\n";
}

int main(int argc, char* argv[]) {
  if(PIN_Init(argc, argv)) {
    std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
  }
  for(UINT i = 0; i < NUM_TLS; ++i) {
    tls[i].useable = 0;
  }
  PIN_AddSyscallEntryFunction(syscallEntryHandler, 0);
  PIN_AddSyscallExitFunction(syscallExitHandler, 0);

  // Activate alarm, must be done before PIN_StartProgram
  control.RegisterHandler(controlHandler, 0, TRUE);
  control.Activate();

  PIN_AddFiniFunction(Fini, 0);

  // Never returns
  PIN_StartProgram();

  return 0;
}
