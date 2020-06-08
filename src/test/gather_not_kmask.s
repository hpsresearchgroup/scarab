.section .text
.globl _start

_start:
      // mov $-32, %ebx
      // vmovdqa int_indices,%ymm0
      // vmovdqa mask1_2,%ymm2
      // vpgatherdd %ymm2,output_ints(%ebx,%ymm0,4),%ymm3
      // vmovdqa %ymm3, output_ints

      // vmovdqa int_indices,%ymm0
      // vmovdqa mask3_4,%ymm7
      // vpgatherdq %ymm7,input_ints(,%xmm0,8),%ymm4
      // vmovdqa %ymm4, output_ints

      // vmovdqa quad_indices_for_ints,%ymm0
      // vmovdqa mask5_6,%ymm5
      // vpgatherqd %xmm5,input_ints(,%ymm0,4),%xmm6
      // vmovdqa %xmm6, output_ints

      vmovdqa quad_indices_for_quads,%ymm0
      vmovdqa mask7_8,%ymm5
      vpgatherqq %ymm5,input_ints(,%ymm0,8),%ymm6
      vmovdqa %ymm6, output_ints

      xor %edi, %edi
      xor %rax, %rax
LOOP: add output_ints(, %rax, 4), %edi 
      add $1, %rax
      cmp $7, %rax
      jle LOOP
      
      mov $231, %eax
      syscall

.data
.align 32
mask1_2: .int 0x80000000,0x80000000,0,0,0,0,0,0
.align 32
mask3_4: .quad 0,0x8000000000000000,0,0
.align 32
mask5_6: .int 0x80000000,0x80000000,0,0
.align 32
mask7_8: .quad 0,0,0,0x8000000000000000
.align 32
int_indices:      .int 0,1,2,3,4,5,6,7
.align 32
quad_indices_for_ints:      .quad 4,5,6,7
.align 32
quad_indices_for_quads:      .quad 0,1,2,3
.align 32
input_ints:   .int 1,2,3,4,5,6,7,8
.align 32
output_ints:  .fill 8, 4, 0
