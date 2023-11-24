global MOVAllBytesAsm
global NOPAllBytesAsm
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

NOPAllBytesAsm:
    xor rax, rax
.loop:
    db 0x0f, 0x1f, 0x00
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
