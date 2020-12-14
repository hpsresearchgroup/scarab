.section .text
.globl _start

_start:
      xor %rax, %rax
redirect_target:
      test %rax, %rax
mispredicting_branch:
      jne far_target
      mov $1, %rax
      # To test NOP mode because of jmp to untraced addresses, test should redirect this jmp to redirect_target 
      # To test NOP mode because of redirect to untraced addresses, test should redirect this jmp to far_target
      jmp done  
      nop
      nop
      nop
      nop
      nop
done: 
      xor %rdi, %rdi
      mov $231, %rax
      syscall

gap:
      .skip 4096, 0
far_target:
      jmp far_target

