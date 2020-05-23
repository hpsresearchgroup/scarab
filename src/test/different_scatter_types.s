.section .text
.globl _start

_start:
      vmovdqa64 input_ints,%zmm1
      lea output_ints, %rcx
      
      #vmovdqa64 indices, %zmm0
      #kmovw mask_0, %k1
      #vscatterdps %zmm1,(%rcx,%zmm0,4){%k1}

      #vmovdqa64 indices, %zmm0
      #kmovw mask_1, %k1
      #vscatterdps %zmm1,output_ints(,%zmm0,4){%k1}

      #kmovw mask_0, %k7
      #vmovdqa32 indices_for_scale1_for_quads_vals, %ymm0
      #vpscatterdq %zmm1,output_ints(,%ymm0,1){%k7}

      #kmovw mask_1, %k7
      #vmovdqa32 neg_indices_for_scale1_for_quads_vals, %ymm0
      #vpscatterdq %ymm1,end(,%xmm0,1){%k7}
      
      #kmovw mask_all, %k7
      #vmovdqa32 indices, %ymm0
      #lea end, %r15
      #vpscatterdq %ymm1,-0x40(%r15,%xmm0,8){%k7}

      #kmovw mask_all, %k7
      #vmovdqa32 indices, %ymm0
      #lea end, %r15
      #vpscatterdq %ymm1,-0x40(%r15,%xmm0,8){%k7}

      #kmovw mask_all, %k7
      #vmovdqa32 indices, %ymm0
      #mov -0x40, %eax
      #vpscatterdq %ymm1,end(%eax,%xmm0,8){%k7}

      kmovw mask_3, %k1
      vmovdqa32 indices, %ymm0
      lea input_ints, %eax
      vpgatherdq (%eax,%xmm0,8),%ymm1{%k1}

      kmovw mask_0, %k1
      vmovdqa32 indices, %ymm0
      mov $-0x40, %eax
      vpgatherdq output_ints(%eax,%xmm0,8),%ymm1{%k1}

      kmovw mask_3, %k7
      vmovdqa32 indices, %ymm0
      mov $-0x40, %r8d
      vpscatterdq %ymm1,end(%r8d,%xmm0,8){%k7}
      kmovw mask_0, %k7
      vpscatterdq %ymm1,end(%r8d,%xmm0,8){%k7}

      kmovw mask_1, %k1
      vmovdqa32 indices, %ymm0
      mov $-0x40, %rax
      vpgatherdq output_ints(%rax,%xmm0,8),%xmm1{%k1}
      lea output_ints,%rax
      kmovw mask_1, %k1
      vpscatterdq %xmm1,(%rax,%xmm0,8){%k1}
#
      #kmovw mask_0, %k7
      #vmovdqa32 neg_indices_for_scale1_for_quads_vals, %ymm0
      #vpscatterdq %xmm1,end(,%xmm0,1){%k7}

      #vmovdqa64 quad_indices, %zmm0
      #kmovw mask_all, %k1
      #vscatterqps %xmm1,output_ints(,%ymm0,4){%k1}

      #vscatterdps %ymm1,(%rcx,%ymm0,4){%k1}
      #vpscatterdd %zmm1,(%rcx,%zmm0,4){%k1}

      #vmovdqa64 indices, %zmm0
      #kmovw mask_all, %k1
      #lea output_ints, %r8d
      #vpscatterdd %zmm1,(%r8d,%zmm0,4){%k1}

      vmovdqa64 input_ints,%zmm1
      vmovdqa64 quad_indices, %zmm0
      kmovw mask_8, %k2
      xor %r8, %r8
      vpscatterqq %zmm1,output_ints(%r8,%zmm0,8){%k2}

      lea output_ints, %rcx
      xor %edi, %edi
      xor %rax, %rax
LOOP: add (%rcx, %rax, 4), %edi 
      add $1, %rax
      cmp $15, %rax
      jle LOOP
      
      mov $231, %eax
      syscall

.data
mask_all: .word 0xFFFF
mask_0:   .word 0x0001
mask_1:   .word 0x0002
mask_2:   .word 0x0004
mask_3:   .word 0x0008
mask_4:   .word 0x0010
mask_5:   .word 0x0020
mask_6:   .word 0x0040
mask_7:   .word 0x0080
mask_8:   .word 0x0100
mask_9:   .word 0x0200
mask_10:   .word 0x0400
mask_11:   .word 0x0800
mask_12:   .word 0x1000
mask_13:   .word 0x2000
mask_14:   .word 0x4000
mask_15:   .word 0x8000
.align 64
indices:      .int 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
.align 64
indices_for_scale1: .int 0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60
.align 64
indices_for_scale1_for_quads_vals: .int 0, 8, 16, 24, 32, 40, 48, 56
.align 64
neg_indices_for_scale1_for_quads_vals: .int -64, -56, -48, -40, -32, -24, -16, -8
.align 64
quad_indices:  .quad 0,1,2,3,4,5,6,7
.align 64
r_indices:     .int 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
.align 64
same_indices:  .fill 16, 4, 0
.align 64
input_ints:   .int 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16
.align 64
output_ints:  .fill 16, 4, 0
end:
#input_quads:  .fill 8, 8, 2
#output_quads: .fill 8, 8, 0
