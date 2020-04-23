.section .text
.globl _start

_start:
      xor %rax, %rax
      lea next_instruction, %rbx
redirect_target:
      jmpq *%rbx
next_instruction:
      lea far_target, %rbx
      jmp done  # test should redirect this jmp to redirect_target
done: 
      xor %rdi, %rdi
      mov $231, %rax
      syscall

gap:
      .skip 4096, 0
far_target:
      jmp far_target

