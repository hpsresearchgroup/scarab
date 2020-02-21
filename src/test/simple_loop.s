.section .text
.globl _start

_start:
      xor %eax, %eax
LOOP: add $1, %eax
      cmp $10, %eax
      jle LOOP
      xor %edi, %edi
      mov $231, %eax
      syscall
