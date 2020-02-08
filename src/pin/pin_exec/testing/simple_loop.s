.section .text
.globl _start

// eax: loop variable
// ecx: counts of odd loop variables
_start:
      xor %eax, %eax 
      xor %ecx, %ecx 

loop:
      // first, test if loop variable is odd
      mov %eax, %ebx 
      and $1, %ebx

      // only increment ecx if eax is odd
      je  end_loop
      add $1, %ecx

end_loop: 
      // loop for 10 iterations
      add $1, %eax
      cmp $10, %eax
      jl loop

      // exit the program
      xor %edi, %edi
      mov $231, %eax
      syscall

