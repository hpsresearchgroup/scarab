.section .text
.globl _start

_start:
      mov $0, %rax
      mov $0, %rbx
      add %rax, %rbx
      mov var(,1), %rcx
      add $1, var(,1)
      add %eax, var(,1)
      add var(,1), %rdx
      lea var(,1), %rbx
      mov (%rbx, %rax), %rbp
      mov (%rbx, %rax, 1), %rsp
      mov var(, %rax, 4), %rdi
      mov %ebp, (%rbx, %rax)
      mov %esp, (%rbx, %rax, 4)
      mov %di, var(, %rax, 4)

      addpd %xmm0, %xmm1
      vaddpd %ymm2, %ymm3, %ymm4
      vaddps %ymm5, %ymm6, %ymm7
      addss %xmm8, %xmm9            # <======
      addsd var(,1), %xmm11         # <======

      addsd var(, %rax, 1), %xmm12  # <======
      addsd (%rbx, %rax, 1), %xmm12 # <======

      bsf %rsi, %r8             # <======
      bsf var(, %rax, 1), %r9   # <======
      bswap %r10

      btc %rax, var(, %rax, 1)
      lea stack2, %rsp
      lea stack2, %rbp
      push %r13
      push %r14
      call func1 



      xor %edi, %edi
      mov $231, %eax
      syscall

func1: ret

.data
.align 32
var: .long 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
stack: .long 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
stack2: .long 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
