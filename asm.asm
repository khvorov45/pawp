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
