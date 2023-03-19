#include "cbuild.h"

#define function static
#define assert(x) prb_assert(x)
#define STR(x) prb_STR(x)
#define LIT(x) prb_LIT(x)

typedef prb_Arena Arena;
typedef prb_Str   Str;
typedef intptr_t  isize;
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef int16_t   i16;

function void
execCmd(Arena* arena, Str cmd) {
    prb_Process proc = prb_createProcess(cmd, (prb_ProcessSpec) {});
    assert(prb_launchProcesses(arena, &proc, 1, prb_Background_No));
}

function prb_Bytes
readFile(Arena* arena, Str path) {
    prb_ReadEntireFileResult res = prb_readEntireFile(arena, path);
    assert(res.success);
    prb_Bytes result = res.content;
    return result;
}

function char*
getRegStr(u8 reg, bool wbit) {
    char* regTable16[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
    char* regTable8[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
    assert(prb_arrayCount(regTable8) == prb_arrayCount(regTable16));
    assert(reg < prb_arrayCount(regTable16));

    char** regTable = regTable16;
    if (!wbit) {
        regTable = regTable8;
    }

    char* result = regTable[reg];
    return result;
}

typedef struct Instr {
    InstrKind kind;
    union {
        union {
            u16 u;
            i16 i;
        } disp;
    };
} Instr;

typedef struct InstrArray {
    Instr* ptr;
    isize len;
} InstrArray;

function InstrArray
decode(Arena* arena, prb_Bytes input) {
    Instr* result = prb_arenaAllocStruct(arena, Instr);

    i32 offset = 0;
    while (offset < input.len) {

        // @codegen
        // @end

        assert(offset <= input.len);
    }

    return result;
}

function void
test_decode(Arena* arena, Str input) {
    Str rootDir = prb_getParentDir(arena, STR(__FILE__));
    Str inputAsm = prb_pathJoin(arena, rootDir, STR("input.asm"));
    Str inputBin = prb_pathJoin(arena, rootDir, STR("input.bin"));
    Str decodeAsm = prb_pathJoin(arena, rootDir, STR("decode.asm"));
    Str decodeBin = prb_pathJoin(arena, rootDir, STR("decode.bin"));

    assert(prb_writeEntireFile(arena, inputAsm, input.ptr, input.len));
    execCmd(arena, prb_fmt(arena, "nasm %.*s -o %.*s", LIT(inputAsm), LIT(inputBin)));
    prb_Bytes ref = readFile(arena, inputBin);

    prb_Str decoded = decode(arena, ref);

    assert(prb_writeEntireFile(arena, decodeAsm, decoded.ptr, decoded.len));
    execCmd(arena, prb_fmt(arena, "nasm %.*s -o %.*s", LIT(decodeAsm), LIT(decodeBin)));
    prb_Bytes mine = readFile(arena, decodeBin);
    assert(ref.len == mine.len);
    assert(prb_memeq(ref.data, mine.data, ref.len));
}

int
main() {
    Arena  arena_ = prb_createArenaFromVmem(1 * prb_GIGABYTE);
    Arena* arena = &arena_;

    test_decode(
        arena,
        // NOTE(khvorov) This random asm is compiled from the various listings here
        // https://github.com/cmuratori/computer_enhance
        STR(
            "bits 16\n"

            "mov cx, bx\n"
            // "mov ch, ah\n"
            // "mov dx, bx\n"
            // "mov si, bx\n"
            // "mov bx, di\n"
            // "mov al, cl\n"
            // "mov ch, ch\n"
            // "mov bx, ax\n"
            // "mov bx, si\n"
            // "mov sp, di\n"
            // "mov bp, ax\n"

            // "mov si, bx\n"
            // "mov dh, al\n"
            // "mov cl, 12\n"
            // "mov ch, -12\n"
            // "mov cx, 12\n"
            // "mov cx, -12\n"
            // "mov dx, 3948\n"
            // "mov dx, -3948\n"
            // "mov al, [bx + si]\n"
            // "mov bx, [bp + di]\n"
            // "mov dx, [bp]\n"
            // "mov ah, [bx + si + 4]\n"
            // "mov al, [bx + si + 4999]\n"
            // "mov [bx + di], cx\n"
            // "mov [bp + si], cl\n"
            // "mov [bp], ch\n"

            // "mov ax, [bx + di - 37]\n"
            // "mov [si - 300], cx\n"
            // "mov dx, [bx - 32]\n"
            // "mov [bp + di], byte 7\n"
            // "mov [di + 901], word 347\n"
            // "mov bp, [5]\n"
            // "mov bx, [3458]\n"
            // "mov ax, [2555]\n"
            // "mov ax, [16]\n"
            // "mov [2554], ax\n"
            // "mov [15], ax\n"

            // "add bx, [bx+si]\n"
            // "add bx, [bp]\n"
            // "add si, 2\n"
            // "add bp, 2\n"
            // "add cx, 8\n"
            // "add bx, [bp + 0]\n"
            // "add cx, [bx + 2]\n"
            // "add bh, [bp + si + 4]\n"
            // "add di, [bp + di + 6]\n"
            // "add [bx+si], bx\n"
            // "add [bp], bx\n"
            // "add [bp + 0], bx\n"
            // "add [bx + 2], cx\n"
            // "add [bp + si + 4], bh\n"
            // "add [bp + di + 6], di\n"
            // "add byte [bx], 34\n"
            // "add word [bp + si + 1000], 29\n"
            // "add ax, [bp]\n"
            // "add al, [bx + si]\n"
            // "add ax, bx\n"
            // "add al, ah\n"
            // "add ax, 1000\n"
            // "add al, -30\n"
            // "add al, 9\n"

            // "sub bx, [bx+si]\n"
            // "sub bx, [bp]\n"
            // "sub si, 2\n"
            // "sub bp, 2\n"
            // "sub cx, 8\n"
            // "sub bx, [bp + 0]\n"
            // "sub cx, [bx + 2]\n"
            // "sub bh, [bp + si + 4]\n"
            // "sub di, [bp + di + 6]\n"
            // "sub [bx+si], bx\n"
            // "sub [bp], bx\n"
            // "sub [bp + 0], bx\n"
            // "sub [bx + 2], cx\n"
            // "sub [bp + si + 4], bh\n"
            // "sub [bp + di + 6], di\n"
            // "sub byte [bx], 34\n"
            // "sub word [bx + di], 29\n"
            // "sub ax, [bp]\n"
            // "sub al, [bx + si]\n"
            // "sub ax, bx\n"
            // "sub al, ah\n"
            // "sub ax, 1000\n"
            // "sub al, -30\n"
            // "sub al, 9\n"

            // "cmp bx, [bx+si]\n"
            // "cmp bx, [bp]\n"
            // "cmp si, 2\n"
            // "cmp bp, 2\n"
            // "cmp cx, 8\n"
            // "cmp bx, [bp + 0]\n"
            // "cmp cx, [bx + 2]\n"
            // "cmp bh, [bp + si + 4]\n"
            // "cmp di, [bp + di + 6]\n"
            // "cmp [bx+si], bx\n"
            // "cmp [bp], bx\n"
            // "cmp [bp + 0], bx\n"
            // "cmp [bx + 2], cx\n"
            // "cmp [bp + si + 4], bh\n"
            // "cmp [bp + di + 6], di\n"
            // "cmp byte [bx], 34\n"
            // "cmp word [4834], 29\n"
            // "cmp ax, [bp]\n"
            // "cmp al, [bx + si]\n"
            // "cmp ax, bx\n"
            // "cmp al, ah\n"
            // "cmp ax, 1000\n"
            // "cmp al, -30\n"
            // "cmp al, 9\n"

            // "test_label0:\n"
            // "jnz test_label1\n"
            // "jnz test_label0\n"
            // "test_label1:\n"
            // "jnz test_label0\n"
            // "jnz test_label1\n"

            // "label:\n"
            // "je label\n"
            // "jl label\n"
            // "jle label\n"
            // "jb label\n"
            // "jbe label\n"
            // "jp label\n"
            // "jo label\n"
            // "js label\n"
            // "jne label\n"
            // "jnl label\n"
            // "jg label\n"
            // "jnb label\n"
            // "ja label\n"
            // "jnp label\n"
            // "jno label\n"
            // "jns label\n"
            // "loop label\n"
            // "loopz label\n"
            // "loopnz label\n"
            // "jcxz label\n"
        )
    );

    return 0;
}
