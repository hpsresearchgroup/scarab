.section .text
.globl _start

_start:
      mov $0, %rax
LOOP: add $1, %rax

      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      movmskps %xmm0, %rbx
      
      cmp $10000, %rax
      jle LOOP

      xor %edi, %edi
      mov $231, %eax
      syscall

.data
.align 32
var: .long 0, 0, 0, 0, 0, 0
