.section .text
.globl _start

_start:
      xor %rax, %rax
      je done       # redirect this to fall-through for testing
                    # wrong-path exceptions

      movq $0, done  # code region is not writable, so this should cause an
                     # exception, however, since it is on the wrongpath,
                     # the pintool should just skip executing it and continue

      ud2           # illegal instruction should stop fetch (ifetch_barrier)
done:
      xor %rdi, %rdi
      mov $231, %rax
      syscall

