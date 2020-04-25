.section .text
.globl _start

_start:
      jmp done
done:
      xor %rdi, %rdi
      mov $231, %rax
      syscall

.rept   0x550
      nopl (%rax)
.endr
      .byte 0x0f
      .byte 0x1f
