global MOVAllBytesAsm
global NOP3x1AllBytesAsm
global NOP1x3AllBytesAsm
global NOP1x9AllBytesAsm
global CMPAllBytesAsm
global DECAllBytesAsm

section .text

MOVAllBytesAsm:
    xor rax, rax
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
