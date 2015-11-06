;
; jdsample.asm - upsampling (64-bit AVX2)
;
; Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright 2009 D. R. Commander
; Copyright 2015 Intel Corporation
;
; Based on
; x86 SIMD extension for IJG JPEG library
; Copyright (C) 1999-2006, MIYASAKA Masaru.
; For conditions of distribution and use, see copyright notice in jsimdext.inc
;
; This file should be assembled with NASM (Netwide Assembler),
; can *not* be assembled with Microsoft's MASM or any compatible
; assembler (including Borland's Turbo Assembler).
; NASM is available from http://nasm.sourceforge.net/ or
; http://sourceforge.net/project/showfiles.php?group_id=6208
;
; [TAB8]

%include "jsimdext.inc"

; --------------------------------------------------------------------------
        SECTION SEG_CONST

        alignz  32
        global  EXTN(jconst_fancy_upsample_avx2)

EXTN(jconst_fancy_upsample_avx2):

PW_ONE          times 16 dw  1
PW_TWO          times 16 dw  2
PW_THREE        times 16 dw  3
PW_SEVEN        times 16 dw  7
PW_EIGHT        times 16 dw  8

        alignz  32

; --------------------------------------------------------------------------
        SECTION SEG_TEXT
        BITS    64
;
; Fancy processing for the common case of 2:1 horizontal and 1:1 vertical.
;
; The upsampling algorithm is linear interpolation between pixel centers,
; also known as a "triangle filter".  This is a good compromise between
; speed and visual quality.  The centers of the output pixels are 1/4 and 3/4
; of the way between input pixel centers.
;
; GLOBAL(void)
; jsimd_h2v1_fancy_upsample_avx2 (int max_v_samp_factor,
;                                 JDIMENSION downsampled_width,
;                                 JSAMPARRAY input_data,
;                                 JSAMPARRAY * output_data_ptr);
;

; r10 = int max_v_samp_factor
; r11 = JDIMENSION downsampled_width
; r12 = JSAMPARRAY input_data
; r13 = JSAMPARRAY * output_data_ptr

        align   32
        global  EXTN(jsimd_h2v1_fancy_upsample_avx2)

EXTN(jsimd_h2v1_fancy_upsample_avx2):
        push    rbp
        mov     rax,rsp
        mov     rbp,rsp
        collect_args

        mov     rax, r11  ; colctr
        test    rax,rax
        jz      near .return

        mov     rcx, r10        ; rowctr
        test    rcx,rcx
        jz      near .return

        mov     rsi, r12        ; input_data
        mov     rdi, r13
        mov     rdi, JSAMPARRAY [rdi]                   ; output_data

        vpxor    ymm0,ymm0,ymm0               ; ymm0=(all 0's)
        vpcmpeqb xmm14,xmm14,xmm14
        vpsrldq  xmm15,xmm14,(SIZEOF_XMMWORD-1); (ff -- -- -- ... -- --); LSB is ff

        vpslldq xmm14,xmm14,(SIZEOF_XMMWORD-1)
        vperm2i128 ymm14,ymm14,ymm14,1 ;(---- ---- ... ---- ---- ff) MSB is ff

.rowloop:
        push    rax                     ; colctr
        push    rdi
        push    rsi

        mov     rsi, JSAMPROW [rsi]     ; inptr
        mov     rdi, JSAMPROW [rdi]     ; outptr

        test    rax, SIZEOF_YMMWORD-1
        jz      short .skip
        mov     dl, JSAMPLE [rsi+(rax-1)*SIZEOF_JSAMPLE]
        mov     JSAMPLE [rsi+rax*SIZEOF_JSAMPLE], dl    ; insert a dummy sample
.skip:
        vpand    ymm7,ymm15, YMMWORD [rsi+0*SIZEOF_YMMWORD]

        add     rax, byte SIZEOF_YMMWORD-1
        and     rax, byte -SIZEOF_YMMWORD
        cmp     rax, byte SIZEOF_YMMWORD
        ja      short .columnloop

.columnloop_last:
        vpand    ymm6,ymm14, YMMWORD [rsi+0*SIZEOF_YMMWORD]
        jmp     short .upsample

.columnloop:
        vmovdqu  ymm6, YMMWORD [rsi+1*SIZEOF_YMMWORD]
        vperm2i128 ymm8,ymm0,ymm6,0x20  
        vpslldq ymm6,ymm8,15      

.upsample:
        vmovdqu  ymm1, YMMWORD [rsi+0*SIZEOF_YMMWORD]
        vmovdqa  ymm2,ymm1
        vmovdqa  ymm3,ymm1               

        vperm2i128 ymm8,ymm0,ymm2,0x20  
        vpalignr ymm2,ymm2,ymm8,15

        vperm2i128 ymm8,ymm0,ymm3,0x03  
        vpalignr ymm3,ymm8,ymm3,1

        vpor     ymm2,ymm2,ymm7               
        vpor     ymm3,ymm3,ymm6               

        vpsrldq  ymm7,ymm8,(SIZEOF_XMMWORD-1) 

        vpunpckhbw ymm4,ymm1,ymm0             
        vpunpcklbw ymm8,ymm1,ymm0             
        vperm2i128 ymm1,ymm8,ymm4,0x20
        vperm2i128 ymm4,ymm8,ymm4,0x31

        vpunpckhbw ymm5,ymm2,ymm0             
        vpunpcklbw ymm8,ymm2,ymm0             
        vperm2i128 ymm2,ymm8,ymm5,0x20
        vperm2i128 ymm5,ymm8,ymm5,0x31

        vpunpckhbw ymm6,ymm3,ymm0             
        vpunpcklbw ymm8,ymm3,ymm0             
        vperm2i128 ymm3,ymm8,ymm6,0x20
        vperm2i128 ymm6,ymm8,ymm6,0x31

        vpmullw  ymm1,ymm1,[rel PW_THREE]
        vpmullw  ymm4,ymm4,[rel PW_THREE]
        vpaddw   ymm2,ymm2,[rel PW_ONE]
        vpaddw   ymm5,ymm5,[rel PW_ONE]
        vpaddw   ymm3,ymm3,[rel PW_TWO]
        vpaddw   ymm6,ymm6,[rel PW_TWO]

        vpaddw   ymm2,ymm2,ymm1
        vpaddw   ymm5,ymm5,ymm4
        vpsrlw   ymm2,ymm2,2                  
        vpsrlw   ymm5,ymm5,2                  
        vpaddw   ymm3,ymm3,ymm1
        vpaddw   ymm6,ymm6,ymm4
        vpsrlw   ymm3,ymm3,2                  
        vpsrlw   ymm6,ymm6,2                  

        vpsllw   ymm3,ymm3,BYTE_BIT
        vpsllw   ymm6,ymm6,BYTE_BIT
        vpor     ymm2,ymm2,ymm3               
        vpor     ymm5,ymm5,ymm6               

        vmovdqu  YMMWORD [rdi+0*SIZEOF_YMMWORD], ymm2
        vmovdqu  YMMWORD [rdi+1*SIZEOF_YMMWORD], ymm5

        sub     rax, byte SIZEOF_YMMWORD
        add     rsi, byte 1*SIZEOF_YMMWORD      ; inptr
        add     rdi, byte 2*SIZEOF_YMMWORD      ; outptr
        cmp     rax, byte SIZEOF_YMMWORD
        ja      near .columnloop
        test    eax,eax
        jnz     near .columnloop_last

        pop     rsi
        pop     rdi
        pop     rax

        add     rsi, byte SIZEOF_JSAMPROW       ; input_data
        add     rdi, byte SIZEOF_JSAMPROW       ; output_data
        dec     rcx                             ; rowctr
        jg      near .rowloop

.return:
        uncollect_args
        pop     rbp
        ret

; --------------------------------------------------------------------------
;
; Fancy processing for the common case of 2:1 horizontal and 2:1 vertical.
; Again a triangle filter; see comments for h2v1 case, above.
;
; GLOBAL(void)
; jsimd_h2v2_fancy_upsample_avx2 (int max_v_samp_factor,
;                                 JDIMENSION downsampled_width,
;                                 JSAMPARRAY input_data,
;                                 JSAMPARRAY * output_data_ptr);
;

; r10 = int max_v_samp_factor
; r11 = JDIMENSION downsampled_width
; r12 = JSAMPARRAY input_data
; r13 = JSAMPARRAY * output_data_ptr

%define wk(i)           rbp-(WK_NUM-(i))*SIZEOF_YMMWORD ; ymmword wk[WK_NUM]
%define WK_NUM          4

        align   32
        global  EXTN(jsimd_h2v2_fancy_upsample_avx2)

EXTN(jsimd_h2v2_fancy_upsample_avx2):
        push    rbp
        mov     rax,rsp                         ; rax = original rbp
        sub     rsp, byte 4
        and     rsp, byte (-SIZEOF_YMMWORD)     ; align to 128 bits
        mov     [rsp],rax
        mov     rbp,rsp                         ; rbp = aligned rbp
        lea     rsp, [wk(0)]
        collect_args
        push    rbx

        mov     rax, r11  ; colctr
        test    rax,rax
        jz      near .return

        mov     rcx, r10        ; rowctr
        test    rcx,rcx
        jz      near .return

        mov     rsi, r12        ; input_data
        mov     rdi, r13
        mov     rdi, JSAMPARRAY [rdi]                   ; output_data
.rowloop:
        push    rax                                     ; colctr
        push    rcx
        push    rdi
        push    rsi

        mov     rcx, JSAMPROW [rsi-1*SIZEOF_JSAMPROW]   ; inptr1(above)
        mov     rbx, JSAMPROW [rsi+0*SIZEOF_JSAMPROW]   ; inptr0
        mov     rsi, JSAMPROW [rsi+1*SIZEOF_JSAMPROW]   ; inptr1(below)
        mov     rdx, JSAMPROW [rdi+0*SIZEOF_JSAMPROW]   ; outptr0
        mov     rdi, JSAMPROW [rdi+1*SIZEOF_JSAMPROW]   ; outptr1

    vpxor ymm13,ymm13,ymm13
    vpcmpeqb xmm14,xmm14,xmm14
        vpsrldq  xmm15,xmm14,(SIZEOF_XMMWORD-2); (ffff ---- ---- ... ---- ----) LSB is ffff
    vpslldq  xmm14,xmm14,(SIZEOF_XMMWORD-2)
    vperm2i128 ymm14,ymm14,ymm14,1       ; (---- ---- ... ---- ---- ffff) MSB is ffff

        test    rax, SIZEOF_YMMWORD-1
        jz      short .skip
        push    rdx
        mov     dl, JSAMPLE [rcx+(rax-1)*SIZEOF_JSAMPLE]
        mov     JSAMPLE [rcx+rax*SIZEOF_JSAMPLE], dl
        mov     dl, JSAMPLE [rbx+(rax-1)*SIZEOF_JSAMPLE]
        mov     JSAMPLE [rbx+rax*SIZEOF_JSAMPLE], dl
        mov     dl, JSAMPLE [rsi+(rax-1)*SIZEOF_JSAMPLE]
        mov     JSAMPLE [rsi+rax*SIZEOF_JSAMPLE], dl    ; insert a dummy sample
        pop     rdx
.skip:
        ; -- process the first column block

        vmovdqu  ymm0, YMMWORD [rbx+0*SIZEOF_YMMWORD]    
        vmovdqu  ymm1, YMMWORD [rcx+0*SIZEOF_YMMWORD]    
        vmovdqu  ymm2, YMMWORD [rsi+0*SIZEOF_YMMWORD]    

        vpunpckhbw ymm4,ymm0,ymm13             
        vpunpcklbw ymm8,ymm0,ymm13             
        vperm2i128 ymm0,ymm8,ymm4,0x20
        vperm2i128 ymm4,ymm8,ymm4,0x31

        vpunpckhbw ymm5,ymm1,ymm13             
        vpunpcklbw ymm8,ymm1,ymm13             
        vperm2i128 ymm1,ymm8,ymm5,0x20
        vperm2i128 ymm5,ymm8,ymm5,0x31

        vpunpckhbw ymm6,ymm2,ymm13             
        vpunpcklbw ymm8,ymm2,ymm13             
        vperm2i128 ymm2,ymm8,ymm6,0x20
        vperm2i128 ymm6,ymm8,ymm6,0x31

        vpmullw  ymm0,ymm0,[rel PW_THREE]
        vpmullw  ymm4,ymm4,[rel PW_THREE]

        vpaddw   ymm1,ymm1,ymm0               
        vpaddw   ymm5,ymm5,ymm4               
        vpaddw   ymm2,ymm2,ymm0               
        vpaddw   ymm6,ymm6,ymm4               

        vmovdqu  YMMWORD [rdx+0*SIZEOF_YMMWORD], ymm1    
        vmovdqu  YMMWORD [rdx+1*SIZEOF_YMMWORD], ymm5    
        vmovdqu  YMMWORD [rdi+0*SIZEOF_YMMWORD], ymm2
        vmovdqu  YMMWORD [rdi+1*SIZEOF_YMMWORD], ymm6

        vpand    ymm1,ymm1,ymm15               
        vpand    ymm2,ymm2,ymm15               

        vmovdqa  YMMWORD [wk(0)], ymm1
        vmovdqa  YMMWORD [wk(1)], ymm2

        add     rax, byte SIZEOF_YMMWORD-1
        and     rax, byte -SIZEOF_YMMWORD
        cmp     rax, byte SIZEOF_YMMWORD
        ja      short .columnloop

        .columnloop_last:
        ; -- process the last column block

        vpand    ymm1,ymm14, YMMWORD [rdx+1*SIZEOF_YMMWORD]
        vpand    ymm2,ymm14, YMMWORD [rdi+1*SIZEOF_YMMWORD]

        vmovdqa  YMMWORD [wk(2)], ymm1   
        vmovdqa  YMMWORD [wk(3)], ymm2   

        jmp     near .upsample

        .columnloop:
        ; -- process the next column block

        vmovdqu  ymm0, YMMWORD [rbx+1*SIZEOF_YMMWORD]    
        vmovdqu  ymm1, YMMWORD [rcx+1*SIZEOF_YMMWORD]    
        vmovdqu  ymm2, YMMWORD [rsi+1*SIZEOF_YMMWORD]    

        vpunpckhbw ymm4,ymm0,ymm13             
        vpunpcklbw ymm8,ymm0,ymm13             
        vperm2i128 ymm0,ymm8,ymm4,0x20
        vperm2i128 ymm4,ymm8,ymm4,0x31

        vpunpckhbw ymm5,ymm1,ymm13             
        vpunpcklbw ymm8,ymm1,ymm13             
        vperm2i128 ymm1,ymm8,ymm5,0x20
        vperm2i128 ymm5,ymm8,ymm5,0x31

        vpunpckhbw ymm6,ymm2,ymm13             
        vpunpcklbw ymm8,ymm2,ymm13             
        vperm2i128 ymm2,ymm8,ymm6,0x20
        vperm2i128 ymm6,ymm8,ymm6,0x31

        vpmullw  ymm0,ymm0,[rel PW_THREE]
        vpmullw  ymm4,ymm4,[rel PW_THREE]

        vpaddw   ymm1,ymm1,ymm0               
        vpaddw   ymm5,ymm5,ymm4               
        vpaddw   ymm2,ymm2,ymm0               
        vpaddw   ymm6,ymm6,ymm4               

        vmovdqu  YMMWORD [rdx+2*SIZEOF_YMMWORD], ymm1    
        vmovdqu  YMMWORD [rdx+3*SIZEOF_YMMWORD], ymm5    
        vmovdqu  YMMWORD [rdi+2*SIZEOF_YMMWORD], ymm2
        vmovdqu  YMMWORD [rdi+3*SIZEOF_YMMWORD], ymm6

        vperm2i128 ymm1,ymm13,ymm1,0x20     
        vpslldq  ymm1,ymm1,14       
        vperm2i128 ymm2,ymm13,ymm2,0x20
        vpslldq  ymm2,ymm2,14

        vmovdqa  YMMWORD [wk(2)], ymm1
        vmovdqa  YMMWORD [wk(3)], ymm2

.upsample:
        ; -- process the upper row

        vmovdqu  ymm7, YMMWORD [rdx+0*SIZEOF_YMMWORD]
        vmovdqu  ymm3, YMMWORD [rdx+1*SIZEOF_YMMWORD]
        vmovdqa  ymm0,ymm7               
        vmovdqa  ymm4,ymm3               
        vperm2i128 ymm8,ymm13,ymm0,0x03  
        vpalignr ymm0,ymm8,ymm0,2    
        vperm2i128 ymm4,ymm13,ymm4,0x20  
        vpslldq   ymm4,ymm4,14       
        vmovdqa  ymm5,ymm7
        vmovdqa  ymm6,ymm3
        vperm2i128 ymm5,ymm13,ymm5,0x03
        vpsrldq   ymm5,ymm5,14
        vperm2i128 ymm8,ymm13,ymm6,0x20
        vpalignr ymm6,ymm6,ymm8,14

        vpor     ymm0,ymm0,ymm4               
        vpor     ymm5,ymm5,ymm6               

        vmovdqa  ymm1,ymm7
        vmovdqa  ymm2,ymm3
        vperm2i128 ymm8,ymm13,ymm1,0x20
        vpalignr ymm1,ymm1,ymm8,14
        vperm2i128 ymm8,ymm13,ymm2,0x03
        vpalignr ymm2,ymm8,ymm2,2
        vmovdqa  ymm4,ymm3
        vperm2i128 ymm4,ymm13,ymm4,0x03
        vpsrldq  ymm4,ymm4,14

        vpor     ymm1,ymm1, YMMWORD [wk(0)]   
        vpor     ymm2,ymm2, YMMWORD [wk(2)]   

        vmovdqa  YMMWORD [wk(0)], ymm4

        vpmullw  ymm7,ymm7,[rel PW_THREE]
        vpmullw  ymm3,ymm3,[rel PW_THREE]
        vpaddw   ymm1,ymm1,[rel PW_EIGHT]
        vpaddw   ymm5,ymm5,[rel PW_EIGHT]
        vpaddw   ymm0,ymm0,[rel PW_SEVEN]
        vpaddw   ymm2,[rel PW_SEVEN]

        vpaddw   ymm1,ymm1,ymm7
        vpaddw   ymm5,ymm5,ymm3
        vpsrlw   ymm1,ymm1,4                  
        vpsrlw   ymm5,ymm5,4                  
        vpaddw   ymm0,ymm0,ymm7
        vpaddw   ymm2,ymm2,ymm3
        vpsrlw   ymm0,ymm0,4                  
        vpsrlw   ymm2,ymm2,4                  

        vpsllw   ymm0,ymm0,BYTE_BIT
        vpsllw   ymm2,ymm2,BYTE_BIT
        vpor     ymm1,ymm1,ymm0               
        vpor     ymm5,ymm5,ymm2               

        vmovdqu  YMMWORD [rdx+0*SIZEOF_YMMWORD], ymm1
        vmovdqu  YMMWORD [rdx+1*SIZEOF_YMMWORD], ymm5

        ; -- process the lower row

        vmovdqu  ymm6, YMMWORD [rdi+0*SIZEOF_YMMWORD]
        vmovdqu  ymm4, YMMWORD [rdi+1*SIZEOF_YMMWORD]

        vmovdqa  ymm7,ymm6               
        vmovdqa  ymm3,ymm4               

        vperm2i128 ymm8,ymm13,ymm7,0x03
        vpalignr ymm7,ymm8,ymm7,2
        vperm2i128 ymm3,ymm13,ymm3,0x20
        vpslldq ymm3,ymm3,14

        vmovdqa  ymm0,ymm6
        vmovdqa  ymm2,ymm4

        vperm2i128 ymm0,ymm13,ymm0,0x03
        vpsrldq ymm0,ymm0,14
        vperm2i128 ymm8,ymm13,ymm2,0x20
        vpalignr ymm2,ymm2,ymm8,14

        vpor     ymm7,ymm7,ymm3               
        vpor     ymm0,ymm0,ymm2               

        vmovdqa  ymm1,ymm6
        vmovdqa  ymm5,ymm4
        vperm2i128 ymm8,ymm13,ymm1,0x20
        vpalignr ymm1,ymm1,ymm8,14
        vperm2i128 ymm8,ymm13,ymm5,0x03
        vpalignr ymm5,ymm8,ymm5,2

        vmovdqa  ymm3,ymm4

        vperm2i128 ymm3,ymm13,ymm3,0x03
        vpsrldq   ymm3,ymm3,14

        vpor     ymm1,ymm1, YMMWORD [wk(1)]   
        vpor     ymm5,ymm5, YMMWORD [wk(3)]   

        vmovdqa  YMMWORD [wk(1)], ymm3

        vpmullw  ymm6,ymm6,[rel PW_THREE]
        vpmullw  ymm4,ymm4,[rel PW_THREE]
        vpaddw   ymm1,ymm1,[rel PW_EIGHT]
        vpaddw   ymm0,ymm0,[rel PW_EIGHT]
        vpaddw   ymm7,ymm7,[rel PW_SEVEN]
        vpaddw   ymm5,ymm5,[rel PW_SEVEN]

        vpaddw   ymm1,ymm1,ymm6
        vpaddw   ymm0,ymm0,ymm4
        vpsrlw   ymm1,ymm1,4                  
        vpsrlw   ymm0,ymm0,4                  
        vpaddw   ymm7,ymm7,ymm6
        vpaddw   ymm5,ymm5,ymm4
        vpsrlw   ymm7,ymm7,4                  
        vpsrlw   ymm5,ymm5,4                  

        vpsllw   ymm7,ymm7,BYTE_BIT
        vpsllw   ymm5,ymm5,BYTE_BIT
        vpor     ymm1,ymm1,ymm7               
        vpor     ymm0,ymm0,ymm5               

        vmovdqu  YMMWORD [rdi+0*SIZEOF_YMMWORD], ymm1
        vmovdqu  YMMWORD [rdi+1*SIZEOF_YMMWORD], ymm0

        sub     rax, byte SIZEOF_YMMWORD
        add     rcx, byte 1*SIZEOF_YMMWORD      
        add     rbx, byte 1*SIZEOF_YMMWORD      
        add     rsi, byte 1*SIZEOF_YMMWORD      
        add     rdx, byte 2*SIZEOF_YMMWORD      
        add     rdi, byte 2*SIZEOF_YMMWORD      
        cmp     rax, byte SIZEOF_YMMWORD
        ja      near .columnloop
        test    rax,rax
        jnz     near .columnloop_last

        pop     rsi
        pop     rdi
        pop     rcx
        pop     rax

        add     rsi, byte 1*SIZEOF_JSAMPROW     ; input_data
        add     rdi, byte 2*SIZEOF_JSAMPROW     ; output_data
        sub     rcx, byte 2                     ; rowctr
        jg      near .rowloop

        .return:
        pop     rbx
        uncollect_args
        mov     rsp,rbp         ; rsp <- aligned rbp
        pop     rsp             ; rsp <- original rbp
        pop     rbp
        ret

        ; --------------------------------------------------------------------------
;
; Fast processing for the common case of 2:1 horizontal and 1:1 vertical.
; It's still a box filter.
;
; GLOBAL(void)
; jsimd_h2v1_upsample_avx2 (int max_v_samp_factor,
;                           JDIMENSION output_width,
;                           JSAMPARRAY input_data,
;                           JSAMPARRAY * output_data_ptr);
;

; r10 = int max_v_samp_factor
; r11 = JDIMENSION output_width
; r12 = JSAMPARRAY input_data
; r13 = JSAMPARRAY * output_data_ptr

        align   32
        global  EXTN(jsimd_h2v1_upsample_avx2)

EXTN(jsimd_h2v1_upsample_avx2):
        push    rbp
        mov     rax,rsp
        mov     rbp,rsp
        collect_args

        mov     rdx, r11
        add     rdx, byte (SIZEOF_YMMWORD-1)
        and     rdx, -SIZEOF_YMMWORD
        jz      near .return

        mov     rcx, r10        ; rowctr
        test    rcx,rcx
        jz      short .return

        mov     rsi, r12 ; input_data
        mov     rdi, r13
        mov     rdi, JSAMPARRAY [rdi]                   ; output_data
.rowloop:
        push    rdi
        push    rsi

        mov     rsi, JSAMPROW [rsi]             ; inptr
        mov     rdi, JSAMPROW [rdi]             ; outptr

        mov     rax,rdx                         ; colctr

.columnloop:

    cmp   rax, byte SIZEOF_YMMWORD
    ja    near .above_16

        vmovdqu  xmm0, XMMWORD [rsi+0*SIZEOF_YMMWORD]
        vpunpckhbw xmm1,xmm0,xmm0
        vpunpcklbw xmm0,xmm0,xmm0
        vmovdqu  XMMWORD [rdi+0*SIZEOF_XMMWORD], xmm0
        vmovdqu  XMMWORD [rdi+1*SIZEOF_XMMWORD], xmm1
    jmp   short .nextrow

.above_16:
        vmovdqu  ymm0, YMMWORD [rsi+0*SIZEOF_YMMWORD]

        vpermq     ymm0,ymm0,0xd8
        vpunpckhbw ymm1,ymm0,ymm0
        vpunpcklbw ymm0,ymm0,ymm0

        vmovdqu  YMMWORD [rdi+0*SIZEOF_YMMWORD], ymm0
        vmovdqu  YMMWORD [rdi+1*SIZEOF_YMMWORD], ymm1

        sub     rax, byte 2*SIZEOF_YMMWORD
        jz      short .nextrow

        add     rsi, byte SIZEOF_YMMWORD      ; inptr
        add     rdi, byte 2*SIZEOF_YMMWORD      ; outptr
        jmp     short .columnloop

.nextrow:
        pop     rsi
        pop     rdi

        add     rsi, byte SIZEOF_JSAMPROW       ; input_data
        add     rdi, byte SIZEOF_JSAMPROW       ; output_data
        dec     rcx                             ; rowctr
        jg      short .rowloop

.return:
        uncollect_args
        pop     rbp
        ret

; --------------------------------------------------------------------------
;
; Fast processing for the common case of 2:1 horizontal and 2:1 vertical.
; It's still a box filter.
;
; GLOBAL(void)
; jsimd_h2v2_upsample_avx2 (nt max_v_samp_factor,
;                           JDIMENSION output_width,
;                           JSAMPARRAY input_data,
;                           JSAMPARRAY * output_data_ptr);
;

; r10 = int max_v_samp_factor
; r11 = JDIMENSION output_width
; r12 = JSAMPARRAY input_data
; r13 = JSAMPARRAY * output_data_ptr

        align   32
        global  EXTN(jsimd_h2v2_upsample_avx2)

EXTN(jsimd_h2v2_upsample_avx2):
        push    rbp
        mov     rax,rsp
        mov     rbp,rsp
        collect_args
        push    rbx

        mov     rdx, r11
        add     rdx, byte (SIZEOF_YMMWORD-1)
        and     rdx, -SIZEOF_YMMWORD
        jz      near .return

        mov     rcx, r10        ; rowctr
        test    rcx,rcx
        jz      near .return

        mov     rsi, r12        ; input_data
        mov     rdi, r13
        mov     rdi, JSAMPARRAY [rdi]                   ; output_data
.rowloop:
        push    rdi
        push    rsi

        mov     rsi, JSAMPROW [rsi]                     ; inptr
        mov     rbx, JSAMPROW [rdi+0*SIZEOF_JSAMPROW]   ; outptr0
        mov     rdi, JSAMPROW [rdi+1*SIZEOF_JSAMPROW]   ; outptr1
        mov     rax,rdx                                 ; colctr

.columnloop:

        cmp rax, byte SIZEOF_YMMWORD
        ja    short .above_16

        vmovdqu  xmm0, XMMWORD [rsi+0*SIZEOF_XMMWORD]
        vpunpckhbw xmm1,xmm0,xmm0
        vpunpcklbw xmm0,xmm0,xmm0
        vmovdqu  XMMWORD [rbx+0*SIZEOF_XMMWORD], xmm0
        vmovdqu  XMMWORD [rbx+1*SIZEOF_XMMWORD], xmm1
        vmovdqu  XMMWORD [rdi+0*SIZEOF_XMMWORD], xmm0
        vmovdqu  XMMWORD [rdi+1*SIZEOF_XMMWORD], xmm1

        jmp   near .nextrow

.above_16:
        vmovdqu  ymm0, YMMWORD [rsi+0*SIZEOF_YMMWORD]
    
        vpermq     ymm0,ymm0,0xd8
        vpunpckhbw ymm1,ymm0,ymm0 
        vpunpcklbw ymm0,ymm0,ymm0 

        vmovdqu  YMMWORD [rbx+0*SIZEOF_YMMWORD], ymm0
        vmovdqu  YMMWORD [rbx+1*SIZEOF_YMMWORD], ymm1
        vmovdqu  YMMWORD [rdi+0*SIZEOF_YMMWORD], ymm0
        vmovdqu  YMMWORD [rdi+1*SIZEOF_YMMWORD], ymm1

        sub     rax, byte 2*SIZEOF_YMMWORD
        jz      short .nextrow

        add     rsi, byte SIZEOF_YMMWORD      ; inptr
        add     rbx, 2*SIZEOF_YMMWORD      ; outptr0
        add     rdi, 2*SIZEOF_YMMWORD      ; outptr1
        jmp     short .columnloop

.nextrow:
        pop     rsi
        pop     rdi

        add     rsi, byte 1*SIZEOF_JSAMPROW     ; input_data
        add     rdi, byte 2*SIZEOF_JSAMPROW     ; output_data
        sub     rcx, byte 2                     ; rowctr
        jg      near .rowloop

.return:
        pop     rbx
        uncollect_args
        pop     rbp
        ret

; For some reason, the OS X linker does not honor the request to align the
; segment unless we do this.
        align   32
