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

/*Date Created: 4/23/19*/

#ifndef __PTRACE_INTERFACE_H__
#define __PTRACE_INTERFACE_H__

#include <cassert>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

void  execute_jump_to_loop(pid_t pid, void* loop_address);
void  execute_xrstor(pid_t pid, void* fpstate_address,
                     unsigned long long mask_edx, unsigned long long mask_eax);
void  execute_memcpy(pid_t pid, void* dest, const void* src, size_t n);
int   execute_munmap(pid_t pid, void* addr, size_t length);
void* execute_mmap(pid_t pid, void* addr, size_t length, int prot, int flags,
                   int fd, off_t offset);
int   execute_mprotect(pid_t pid, void* addr, size_t length, int prot);
void  execute_instruction(pid_t pid, char instruction_bytes[],
                          size_t                   instruction_size,
                          struct user_regs_struct* newregs,
                          struct user_regs_struct* oldregs);
int   execute_open(pid_t pid, const char* pathname, int flags);
int   execute_close(pid_t pid, int fd);
void* execute_mremap(pid_t pid, void* old_addr, size_t old_size,
                     size_t new_size, int flags, void* new_addr);
void* execute_brk(pid_t pid, void* addr);
void* execute_shmat(pid_t pid, int shmid, const void* shmaddr, int shmflg);
void  assert_equal_mem(pid_t pid, char* tracer_addr, const char* tracee_addr,
                       size_t n);

unsigned long long execute_syscall(
  pid_t pid, unsigned long long syscall_number, unsigned long long arg1,
  unsigned long long arg2, unsigned long long arg3, unsigned long long arg4,
  unsigned long long arg5, unsigned long long arg6);
void kill_and_exit(pid_t pid);
void do_wait(pid_t pid, const char* name);
void singlestep(pid_t pid);
void ptrace_continue(pid_t pid);
void poke_text(pid_t pid, char* where, const char* new_text, char* old_text,
               size_t len);
void restore(pid_t pid, struct user_regs_struct oldregs, char* old_word,
             size_t old_word_size);
void detach_process(pid_t pid);

std::pair<void*, void*> allocate_shared_memory(pid_t child_pid);
void shared_memory_memcpy(pid_t pid, void* dest, void* src, int64_t n,
                          void* sharedmem_tracer_addr,
                          void* sharedmem_tracee_addr);

#endif
