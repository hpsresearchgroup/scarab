.section .text
.globl _start

_start:
      xor %rax, %rax
redirect_target:
      test %rax, %rax
mispredicting_branch:
      jnz far_target
      mov $1, %rax
      jmp done  # test should redirect this jmp to redirect_target
done: 
      xor %rdi, %rdi
      mov $231, %rax
      syscall

gap:
      .skip 4096, 0
far_target:
      jmp far_target

