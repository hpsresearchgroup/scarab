.section .text
.globl _start

_start:
      kmovw mask, %k1
      knotw %k1,%k2
      lea input_ints, %rbx
      lea output_ints, %rcx
      vmovdqa64 indices,%zmm0
      vpgatherdd (%rbx,%zmm0,4),%zmm1{%k1}
      vpscatterdd %zmm1,(%rcx,%zmm0,4){%k2}
      xor %edi, %edi
      mov $231, %eax
      syscall

.data
mask:   .word 0x00FF
.align 64
indices:      .int 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
input_ints:   .int -1,-2,-3,-4,-5,-6,-7,-8,-9,-10,-11,-12,-13,-14,-15,-16
output_ints:  .fill 16, 4, 0
