.section .text
.globl _start

_start:
      mov $2, %rax
loop:
      sub $1, %rax
      jne skip
      xor %rdi, %rdi
      mov $231, %rax
      syscall
skip: 
      jne loop # this branch should be redirected to fall thrugh, then
               # eventually the process reaches non-instrumented instructions
               # on the wrongpath.
      sub $1, %rax
      jne skip
      jne skip
      jne skip
      jne skip
      jne skip
      jne skip
      jne skip
loop2:
      jmp loop2

      

