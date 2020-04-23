.section .text
.globl _start

_start:
      lea done, %rax
      lea ret_inst, %rbx
      sub $0x10000, %rbx
      pushq %rbx  # Pushing nonsense address on the stack. If an extra retq
                  # is executed on the wrongpath, we expect to go into
                  # wronpath nop mode.
      pushq %rax
ret_inst:
      retq
done: 
      xor %rdi, %rdi
      mov $231, %rax
      syscall

