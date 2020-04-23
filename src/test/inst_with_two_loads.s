.section .text
.globl _start

_start:
      cld
      lea my_str1(,1), %rsi
      lea my_str2(,1), %rdi
LOOP: cmpsb
      je LOOP
      xor %edi, %edi
      mov $231, %eax
      syscall

.data
.align 32
my_str1:   .string "12345678a"
my_str2:   .string "12345678b"
