.section .text
.globl _start

# rax: loop variable
# rax: counts of odd loop variables
_start:
      xor %rax, %rax 
      xor %rcx, %rcx 

loop:
      # first, test if loop variable is odd
      mov %rax, %rbx 
      and $1, %rbx

      # only increment rax if rax is odd
      je  end_loop
      add $1, %rcx

end_loop: 
      # loop for 10 iterations
      add $1, %rax
      cmp $10, %rax
      jl loop

      # exit the program
      xor %rdi, %rdi
      mov $231, %rax
      syscall

wrongpath_loop:
      mov $99999, %rax
      mov $99999, %rax
      jmp wrongpath_loop
