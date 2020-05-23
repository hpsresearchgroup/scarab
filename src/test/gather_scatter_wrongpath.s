.section .text
.globl _start

_start:
      kmovw full_mask, %k1
      vmovdqa64 neg_indices,%zmm0
      vpgatherdd output_ints(,%zmm0,4),%zmm1{%k1}
      kmovw full_mask, %k1
      lea output_ints, %rcx
      vmovdqa64 pos_indices,%zmm0
      vpscatterdd %zmm1,(%rcx,%zmm0,4){%k1}
  
      vmovdqa64 input_ints_2,%zmm1
      kmovw missing_one_mask, %k1

      cmp $0, %rcx
      jne skip
      vpscatterdd %zmm1,(%rcx,%zmm0,4){%k1}
      movl $3, output_ints

 skip:
      xor %edi, %edi
      xor %rax, %rax
LOOP: add (%rcx, %rax, 4), %edi 
      add $1, %rax
      cmp $15, %rax
      jle LOOP
      
      mov %edi, %ebx
 LOOP2:         
      cmp $0, %ebx
      je EXIT
      add $-1, %ebx
      jmp LOOP2

 EXIT: mov $231, %eax
      syscall

.data
full_mask:   .word 0xFFFF
missing_one_mask: .word 0xFFFE
.align 64
pos_indices:      .int  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
neg_indices:      .int -1,-2,-3,-4,-5,-6,-7,-8,-9,-10,-11,-12,-13,-14,-15,-16
input_ints_2:   .fill 16, 4, 2
input_ints_1:   .fill 16, 4, 1
output_ints:  .fill 16, 4, 0
