global MOVAllBytesAsm
global NOP3x1AllBytesAsm
global NOP1x3AllBytesAsm
global NOP1x9AllBytesAsm
global CMPAllBytesAsm
global DECAllBytesAsm
global ConditionalNopAsm
global MOVAllBytesAsmAlign64
global MOVAllBytesAsmAlign64Nop
global StoreManyTimesX1
global StoreManyTimesX2
global StoreManyTimesX3
global StoreManyTimesX4
global LoadManyTimesX1
global LoadManyTimesX2
global LoadManyTimesX3
global LoadManyTimesX4
global StoreManyTimesX2_64
global StoreManyTimesX2_128
global LoadManyTimesX2_64
global LoadManyTimesX2_128
global LoadManyTimesX2_256
global LoadManyTimesX2_512
global BandwidthTest

section .text

MOVAllBytesAsm:
    xor rax, rax
.loop:
    mov byte [rcx + rax], al
    inc rax
    cmp rax, rdx
jne .loop
    ret

MOVAllBytesAsmAlign64:
    xor rax, rax
align 64
.loop:
    mov byte [rcx + rax], al
    inc rax
    cmp rax, rdx
jne .loop
    ret

MOVAllBytesAsmAlign64Nop:
    xor rax, rax
align 64
%rep 59
nop
%endrep
.loop:
    mov byte [rcx + rax], al
    inc rax
    cmp rax, rdx
jne .loop
    ret

NOP3x1AllBytesAsm:
    xor rax, rax
.loop:
    db 0x0f, 0x1f, 0x00
    inc rax
    cmp rax, rdx
jne .loop
    ret

NOP1x3AllBytesAsm:
    xor rax, rax
.loop:
    nop
    nop
    nop
    inc rax
    cmp rax, rdx
jne .loop
    ret

NOP1x9AllBytesAsm:
    xor rax, rax
.loop:
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    inc rax
    cmp rax, rdx
jne .loop
    ret

CMPAllBytesAsm:
    xor rax, rax
.loop:
    inc rax
    cmp rax, rdx
jne .loop
    ret

DECAllBytesAsm:
.loop:
    dec rdx
jnz .loop
    ret

ConditionalNopAsm:
    xor rax, rax
.loop:
    mov r10, [rcx + rax]
    inc rax
    test r10, 1
    jnz .skip
    nop
.skip:
    cmp rax, rdx
    jne .loop
    ret

StoreManyTimesX1:
    xor rax, rax
.loop:
    mov [rcx], rax
    inc rax
    cmp rax, rdx
jne .loop
    ret

StoreManyTimesX2:
    xor rax, rax
.loop:
    mov [rcx], rax
    mov [rcx], rax
    inc rax
    cmp rax, rdx
jne .loop
    ret

StoreManyTimesX3:
    xor rax, rax
.loop:
    mov [rcx], rax
    mov [rcx], rax
    mov [rcx], rax
    inc rax
    cmp rax, rdx
jne .loop
    ret

StoreManyTimesX4:
    xor rax, rax
.loop:
    mov [rcx], rax
    mov [rcx], rax
    mov [rcx], rax
    mov [rcx], rax
    inc rax
    cmp rax, rdx
jne .loop
    ret

LoadManyTimesX1:
    xor rax, rax
.loop:
    mov r8, [rcx]
    inc rax
    cmp rax, rdx
jne .loop
    ret

LoadManyTimesX2:
    xor rax, rax
.loop:
    mov r8, [rcx]
    mov r8, [rcx]
    inc rax
    cmp rax, rdx
jne .loop
    ret

LoadManyTimesX3:
    xor rax, rax
.loop:
    mov r8, [rcx]
    mov r8, [rcx]
    mov r8, [rcx]
    inc rax
    cmp rax, rdx
jne .loop
    ret

LoadManyTimesX4:
    xor rax, rax
.loop:
    mov r8, [rcx]
    mov r8, [rcx]
    mov r8, [rcx]
    mov r8, [rcx]
    inc rax
    cmp rax, rdx
jne .loop
    ret

StoreManyTimesX2_64:
    xor rax, rax
.loop:
    mov [rcx], r8
    mov [rcx + 8], r8
    add rax, 8
    cmp rax, rdx
jne .loop
    ret

StoreManyTimesX2_128:
    xor rax, rax
.loop:
    movdqu [rcx], xmm0
    movdqu [rcx + 16], xmm0
    add rax, 16
    cmp rax, rdx
jne .loop
    ret

LoadManyTimesX2_64:
    xor rax, rax
.loop:
    mov r8, [rcx]
    mov r8, [rcx + 8]
    add rax, 8
    cmp rax, rdx
jne .loop
    ret

LoadManyTimesX2_128:
    xor rax, rax
.loop:
    movdqu xmm0, [rcx]
    movdqu xmm0, [rcx + 16]
    add rax, 16
    cmp rax, rdx
jl .loop
    ret

LoadManyTimesX2_256:
    xor rax, rax
.loop:
    vmovdqu ymm0, [rcx]
    vmovdqu ymm0, [rcx + 32]
    add rax, 32
    cmp rax, rdx
jl .loop
    ret

LoadManyTimesX2_512:
    xor rax, rax
.loop:
    vmovdqu64 zmm0, [rcx]
    vmovdqu64 zmm0, [rcx + 64]
    add rax, 64
    cmp rax, rdx
jl .loop
    ret

BandwidthTest:
    xor rax, rax
    mov r9, rcx
.loop:
    vmovdqu ymm0, [r9]
    vmovdqu ymm0, [r9 + 32]
    vmovdqu ymm0, [r9 + 32 + 32]
    vmovdqu ymm0, [r9 + 32 + 32 + 32]
    add rax, 128
    mov r9, rax
    and r9, r8
    add r9, rcx
    cmp rax, rdx
jl .loop
    ret
