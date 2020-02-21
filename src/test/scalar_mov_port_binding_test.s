.section .text
.globl _start

_start:
      mov $0, %rax
LOOP: add $1, %rax

      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      mov $1, %rbx
      
      cmp $10000, %rax
      jle LOOP

      xor %edi, %edi
      mov $231, %eax
      syscall

.data
.align 32
var: .long 0, 0, 0, 0, 0, 0
