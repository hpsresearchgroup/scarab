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

/* Copyright 2016, Evan Klitzke
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* HPS Research Group
 * Code was originally found here:
 * https://github.com/eklitzke/ptrace-call-userspace
 *
 * Modified original source and added wrapper functions to meet our needs.
 */

/*Date Created: 4/23/19*/

#include "ptrace_interface.h"

#include <cstdarg>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

#include "utils.h"

#define OPEN_SYSCALL 2
#define CLOSE_SYSCALL 3
#define MMAP_SYSCALL 9
#define MPROTECT_SYSCALL 10
#define MUNMAP_SYSCALL 11
#define BRK_SYSCALL 12
#define MREMAP_SYSCALL 25
#define SHMAT_SYSCALL 30

static constexpr int64_t SHARED_MEMORY_SIZE = 2 * 1024 * 1024;

void execute_jump_to_loop(pid_t pid, void* loop_address) {
  struct user_regs_struct regs;
  if(ptrace(PTRACE_GETREGS, pid, NULL, &regs)) {
    perror("PTRACE_GETREGS");
    kill_and_exit(pid);
  }

  char infinite_loop[8];
  infinite_loop[0] = 0xeb;  // jmp 0x-2
  infinite_loop[1] = 0xfe;  // eb fe

  poke_text(pid, (char*)loop_address, infinite_loop, NULL,
            sizeof(infinite_loop));
  regs.rip = (unsigned long long)loop_address;

  if(ptrace(PTRACE_SETREGS, pid, NULL, &regs)) {
    perror("PTRACE_SETREGS");
    kill_and_exit(pid);
  }
}

void execute_xrstor(pid_t pid, void* fpstate_address,
                    unsigned long long mask_rdx, unsigned long long mask_rax) {
  struct user_regs_struct newregs;
  struct user_regs_struct oldregs;

  if(ptrace(PTRACE_GETREGS, pid, NULL, &oldregs)) {
    perror("PTRACE_GETREGS");
    kill_and_exit(pid);
  }

  memmove(&newregs, &oldregs, sizeof(newregs));
  newregs.rax = mask_rax;
  newregs.rdx = mask_rdx;
  newregs.rcx = (unsigned long long)fpstate_address;

  char xrstor_rcx[8];
  xrstor_rcx[0] = 0x0f;
  xrstor_rcx[1] = 0xae;
  xrstor_rcx[2] = 0x29;

  execute_instruction(pid, xrstor_rcx, sizeof(xrstor_rcx), &newregs, &oldregs);
}

void execute_memcpy(pid_t pid, void* dest, const void* src, size_t n) {
  poke_text(pid, (char*)dest, (const char*)src, NULL, n);
}

void assert_equal_mem(pid_t pid, char* tracer_addr, const char* tracee_addr,
                      size_t n) {
  if(n % sizeof(void*) != 0) {
    printf("invalid len, not a multiple of %zd\n", sizeof(void*));
    kill_and_exit(pid);
  }

  long peek_data;
  assert(sizeof(peek_data) == sizeof(void*));
  for(size_t i = 0; i < n; i += sizeof(peek_data)) {
    errno     = 0;
    peek_data = ptrace(PTRACE_PEEKTEXT, pid, tracee_addr + i, NULL);
    if(peek_data == -1 && errno) {
      perror("PTRACE_PEEKTEXT");
      kill_and_exit(pid);
    }
    if(*((long*)(tracer_addr + i)) != peek_data) {
      printf("Mismatch in tracee (address %p) and tracer (address %p) data \n",
             tracee_addr + i, tracer_addr + i);
      kill_and_exit(pid);
    }
  }
}

void detach_process(pid_t pid) {
  std::cout << "Detaching ptrace from the child process ..." << std::endl;
  if(ptrace(PTRACE_DETACH, pid, NULL, NULL)) {
    perror("PTRACE_DETACH");
  }
}

void kill_and_exit(pid_t pid) {
  kill(pid, SIGKILL);
  exit(1);
}

void do_wait(pid_t pid, const char* name) {
  int status;
  if(wait(&status) == -1) {
    perror("wait");
    kill_and_exit(pid);
  }
  if(WIFSTOPPED(status)) {
    if(WSTOPSIG(status) == SIGTRAP) {
      return;
    }
    printf("%s unexpectedly got status %s\n", name,
           strsignal(WSTOPSIG(status)));
    kill_and_exit(pid);
  }

  if(WIFEXITED(status)) {
    printf("child exited with status %d\n", WEXITSTATUS(status));
  } else if(WIFSIGNALED(status)) {
    printf("child terminated by a signal %d\n", WTERMSIG(status));
  } else if(WCOREDUMP(status)) {
    printf("child core dump\n");
  } else if(WIFCONTINUED(status)) {
    printf("child continued\n");
  }
  printf("%s got unexpected status %d\n", name, status);
  kill_and_exit(pid);
}

void singlestep(pid_t pid) {
  if(ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL)) {
    perror("PTRACE_SINGLESTEP");
    kill_and_exit(pid);
  }
  do_wait(pid, "PTRACE_SINGLESTEP");
}

void ptrace_continue(pid_t pid) {
  if(ptrace(PTRACE_CONT, pid, NULL, NULL)) {
    perror("PTRACE_CONT");
    kill_and_exit(pid);
  }
  do_wait(pid, "PTRACE_CONT");
}

// Update the text area of pid at the area starting at where. The data copied
// should be in the new_text buffer whose size is given by len. If old_text is
// not null, the original text data will be copied into it. Therefore old_text
// must have the same size as new_text.
void poke_text(pid_t pid, char* where, const char* new_text, char* old_text,
               size_t len) {
  if(len % sizeof(void*) != 0) {
    printf("invalid len, not a multiple of %zd\n", sizeof(void*));
    kill_and_exit(pid);
  }

  assert(sizeof(long) == sizeof(void*));
  long poke_data;
  for(size_t copied = 0; copied < len; copied += sizeof(poke_data)) {
    memmove(&poke_data, new_text + copied, sizeof(poke_data));

    if(old_text != NULL) {
      errno          = 0;
      long peek_data = ptrace(PTRACE_PEEKTEXT, pid, where + copied, NULL);
      if(peek_data == -1 && errno) {
        perror("PTRACE_PEEKTEXT");
        kill_and_exit(pid);
      }
      memmove(old_text + copied, &peek_data, sizeof(peek_data));
    }

    if(ptrace(PTRACE_POKETEXT, pid, where + copied, (void*)poke_data) < 0) {
      perror("PTRACE_POKETEXT");
      kill_and_exit(pid);
    }
  }
}

void restore(pid_t pid, struct user_regs_struct oldregs, char* old_word,
             size_t old_word_size) {
  poke_text(pid, (char*)oldregs.rip, old_word, NULL, old_word_size);
  if(ptrace(PTRACE_SETREGS, pid, NULL, &oldregs)) {
    perror("PTRACE_SETREGS");
    kill_and_exit(pid);
  }
}

void execute_instruction(pid_t pid, char instruction_bytes[],
                         size_t                   instruction_size,
                         struct user_regs_struct* newregs,
                         struct user_regs_struct* oldregs) {
  // Inject instruction
  char* rip      = (char*)oldregs->rip;
  char* old_word = (char*)malloc(instruction_size);
  poke_text(pid, rip, instruction_bytes, old_word, instruction_size);

  // Set the new registers
  if(ptrace(PTRACE_SETREGS, pid, NULL, newregs)) {
    perror("PTRACE_SETREGS");
    kill_and_exit(pid);
  }

  // Run the instruction in tracee address space
  singlestep(pid);

  // read the new register state, so we can see where the mmap went
  if(ptrace(PTRACE_GETREGS, pid, NULL, newregs)) {
    perror("PTRACE_GETREGS");
    kill_and_exit(pid);
  }

  // Restore the tracee
  restore(pid, *oldregs, old_word, instruction_size);

  free(old_word);
}

unsigned long long execute_syscall(
  pid_t pid, unsigned long long syscall_number, unsigned long long arg1,
  unsigned long long arg2, unsigned long long arg3, unsigned long long arg4,
  unsigned long long arg5, unsigned long long arg6) {
  // save the register state of the remote process
  struct user_regs_struct oldregs;
  if(ptrace(PTRACE_GETREGS, pid, NULL, &oldregs)) {
    perror("PTRACE_GETREGS");
    kill_and_exit(pid);
  }
  char* rip = (char*)oldregs.rip;

  struct user_regs_struct newregs;
  memmove(&newregs, &oldregs, sizeof(newregs));
  newregs.rax = syscall_number;
  newregs.rdi = arg1;
  newregs.rsi = arg2;
  newregs.rdx = arg3;
  newregs.r10 = arg4;
  newregs.r8  = arg5;
  newregs.r9  = arg6;

  char old_word[8];
  char new_word[8];
  new_word[0] = 0x0f;  // SYSCALL
  new_word[1] = 0x05;  // SYSCALL
  new_word[2] = 0xff;  // JMP %rax
  new_word[3] = 0xe0;  // JMP %rax

  // insert the SYSCALL instruction into the process, and save the old word
  poke_text(pid, rip, new_word, old_word, sizeof(new_word));

  // set the new registers with our syscall arguments
  if(ptrace(PTRACE_SETREGS, pid, NULL, &newregs)) {
    perror("PTRACE_SETREGS");
    kill_and_exit(pid);
  }

  // invoke mmap(2)
  DEBUG("About to single-step for syscall. RIP: " << std::hex << oldregs.rip
                                                  << ", Arg1: " << arg1);
  singlestep(pid);

  // read the new register state, so we can see where the mmap went
  if(ptrace(PTRACE_GETREGS, pid, NULL, &newregs)) {
    perror("PTRACE_GETREGS");
    kill_and_exit(pid);
  }

  // this is the address of the memory we allocated
  restore(pid, oldregs, old_word, sizeof(old_word));
  return newregs.rax;
}

void* execute_mmap(pid_t pid, void* addr, size_t length, int prot, int flags,
                   int fd, off_t offset) {
  DEBUG("Calling mmap, Addr: " << addr);
  return (void*)execute_syscall(
    pid, MMAP_SYSCALL, (unsigned long long int)addr,
    (unsigned long long int)length, (unsigned long long int)prot,
    (unsigned long long int)flags, (unsigned long long int)fd,
    (unsigned long long int)offset);
}

int execute_mprotect(pid_t pid, void* addr, size_t length, int prot) {
  return (int)execute_syscall(
    pid, MPROTECT_SYSCALL, (unsigned long long int)addr,
    (unsigned long long int)length, (unsigned long long int)prot, 0, 0, 0);
}

int execute_munmap(pid_t pid, void* addr, size_t length) {
  return (int)execute_syscall(pid, MUNMAP_SYSCALL, (unsigned long long int)addr,
                              (unsigned long long int)length, 0, 0, 0, 0);
}

void* execute_mremap(pid_t pid, void* old_addr, size_t old_size,
                     size_t new_size, int flags, void* new_addr) {
  return (void*)execute_syscall(
    pid, MREMAP_SYSCALL, (unsigned long long int)old_addr,
    (unsigned long long int)old_size, (unsigned long long int)new_size,
    (unsigned long long int)flags, (unsigned long long int)new_addr, 0);
}

void* execute_brk(pid_t pid, void* addr) {
  return (void*)execute_syscall(pid, BRK_SYSCALL, (unsigned long long int)addr,
                                0, 0, 0, 0, 0);
}

int execute_open(pid_t pid, const char* pathname, int flags) {
  void* temp_addr;
  // execute_memcpy requires the size to be a multiple of 8
  size_t rounded_strlen = (strlen(pathname) + 1 + 7) & ~0x7;

  // we need a new temporary buffer to make sure there is space
  // for the added padding.
  char* temp_pathname = new char[rounded_strlen];

  strcpy(temp_pathname, pathname);

  temp_addr = execute_mmap(pid, NULL, rounded_strlen, PROT_READ,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(temp_addr == (void*)-1) {
    std::cerr << "Could not map a temporary page for the path of open()"
              << std::endl;
    kill_and_exit(pid);
  }

  execute_memcpy(pid, (char*)temp_addr, temp_pathname, rounded_strlen);
  int ret_val = execute_syscall(pid, OPEN_SYSCALL,
                                (unsigned long long int)temp_addr,
                                (unsigned long long int)flags, 0, 0, 0, 0);

  if(execute_munmap(pid, temp_addr, rounded_strlen)) {
    std::cerr << "Could not unmap a temporary page used for the path of open()"
              << std::endl;
    kill_and_exit(pid);
  }
  return ret_val;
}

int execute_close(pid_t pid, int fd) {
  return execute_syscall(pid, CLOSE_SYSCALL, (unsigned long long int)fd, 0, 0,
                         0, 0, 0);
}

void* execute_shmat(pid_t pid, int shmid, const void* shmaddr, int shmflg) {
  return (void*)execute_syscall(
    pid, SHMAT_SYSCALL, (unsigned long long int)shmid,
    (unsigned long long int)shmaddr, (unsigned long long int)shmflg, 0, 0, 0);
}

std::pair<void*, void*> allocate_shared_memory(pid_t pid) {
  int  USER_READ_WRITE  = 0600;
  auto shared_memory_id = shmget(IPC_PRIVATE, SHARED_MEMORY_SIZE,
                                 IPC_CREAT | IPC_EXCL | USER_READ_WRITE);
  if(shared_memory_id == -1) {
    fatal_and_kill_child(pid,
                         "Could not create a shared memory region. errno: %s",
                         std::strerror(errno));
  }

  void* tracer_addr = shmat(shared_memory_id, NULL, 0);
  if(tracer_addr == (void*)-1) {
    auto shmat_errno = errno;
    if(shmctl(shared_memory_id, IPC_RMID, NULL) == -1) {
      fatal_and_kill_child(
        pid,
        "Could not attach the shared memory region to the "
        "tracer. Marking the region to be destroyed also "
        "failed. shmat_errno: %s, shmctl_errno: %s\n\n!!!!!! DO NOT "
        "IGNORE THIS ERROR. This could be a SYSTEM-LEVEL memory leak. \n\n",
        std::strerror(shmat_errno), std::strerror(errno));
    } else {
      fatal_and_kill_child(pid,
                           "Could not attach the shared memory region to the "
                           "tracer. errno: %s",
                           std::strerror(errno));
    }
  }

  // Immediately mark the shared region to be destroyed. This is safe because
  // Linux guarantees the region will exist until no process is attached to
  // the region.
  if(shmctl(shared_memory_id, IPC_RMID, NULL) == -1) {
    fatal_and_kill_child(pid,
                         "Could not mark the shared memory region to be "
                         "destroyed. errno: %s",
                         std::strerror(errno));
  }

  void* tracee_addr = execute_shmat(pid, shared_memory_id, NULL, 0);
  if(tracer_addr == (void*)-1) {
    fatal_and_kill_child(pid,
                         "Could not attach the shared memory region to the "
                         "tracer.");
  }

  return {tracer_addr, tracee_addr};
}

void shared_memory_memcpy(pid_t pid, void* dest, void* src, int64_t n,
                          void* sharedmem_tracer_addr,
                          void* sharedmem_tracee_addr) {
  if(n % 8 != 0) {
    fatal_and_kill_child(
      pid,
      "Cannot do a shared memory copy for a block that is not a multiple of 8");
  }

  struct user_regs_struct oldregs;
  if(ptrace(PTRACE_GETREGS, pid, NULL, &oldregs)) {
    perror("PTRACE_GETREGS");
    kill_and_exit(pid);
  }
  char* rip = (char*)oldregs.rip;

  struct user_regs_struct newregs;
  memmove(&newregs, &oldregs, sizeof(newregs));
  newregs.ds = 0;
  newregs.es = 0;

  char old_word[8];
  char new_word[8];
  new_word[0] = 0xf3;  // REP MOVSQ
  new_word[1] = 0x48;  // REP MOVSQ
  new_word[2] = 0xa5;  // REP MOVSQ
  new_word[3] = 0xcc;  // int3 (breakpoint)

  // insert the REP-MOVSQ instruction into the process, and save the old word
  poke_text(pid, rip, new_word, old_word, sizeof(new_word));

  for(int64_t i = 0; i < n; i += SHARED_MEMORY_SIZE) {
    auto block_size = std::min(n - i, SHARED_MEMORY_SIZE);

    std::memcpy(sharedmem_tracer_addr, (const char*)src + i, block_size);

    // Is casting void* to an int type undefined behavior?
    newregs.rdi = (unsigned long long)(dest) + i;
    // newregs.rsi = (unsigned long long)(dest) + i;
    // newregs.rdi = (unsigned long long)sharedmem_tracee_addr;
    newregs.rsi = (unsigned long long)sharedmem_tracee_addr;
    newregs.rcx = (unsigned long long)block_size / 8;

    // set the new registers with our syscall arguments
    if(ptrace(PTRACE_SETREGS, pid, NULL, &newregs)) {
      perror("PTRACE_SETREGS");
      kill_and_exit(pid);
    }

    DEBUG("About to continue the tracee for REP MOVSQ. RIP: "
          << std::hex << newregs.rip << ", rdi: " << newregs.rdi
          << ", rsi: " << newregs.rsi << ", rcx: " << newregs.rcx);
    ptrace_continue(pid);
    // singlestep(pid);

    struct user_regs_struct tmpregs;
    if(ptrace(PTRACE_GETREGS, pid, NULL, &tmpregs)) {
      perror("PTRACE_GETREGS");
      kill_and_exit(pid);
    }
  }

  // this is the address of the memory we allocated
  restore(pid, oldregs, old_word, sizeof(old_word));
}