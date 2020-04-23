.section .text
.globl _start

_start:
      lea gvar, %rax
      movq $1, (%rax)
      mov %rsp, %rax # since we do not write anything to the stack in this
                     # program, writing to the stack on the wrong path should
                     # trigger wrong path nop mode
      sub $8, %rsp
      jmp done       # redirect this to the first mov
done:
      xor %rdi, %rdi
      mov $231, %rax
      syscall

.section .data
gvar: .skip 8, 0
