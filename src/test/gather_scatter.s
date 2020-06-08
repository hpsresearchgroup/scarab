.section .text
.globl _start

_start:
      kmovw mask, %k1
      lea input_ints, %rbx
      lea output_ints, %rcx
      vmovdqa64 indices,%zmm0
      vpgatherdd (%rbx,%zmm0,4),%zmm1{%k1}
      kmovw mask, %k1
      vpscatterdd %zmm1,(%rcx,%zmm0,4){%k1}

      xor %edi, %edi
      xor %rax, %rax
LOOP: add (%rcx, %rax, 4), %edi 
      add $1, %rax
      cmp $15, %rax
      jle LOOP
      
      mov $231, %eax
      syscall

.data
mask:   .word 0x8001
.align 64
indices:      .int 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
input_ints:   .int 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16
output_ints:  .fill 16, 4, 0
