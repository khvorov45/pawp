#include "cbuild.h"

#define function static
#define assert(x) prb_assert(x)
#define STR(x) prb_STR(x)
#define LIT(x) prb_LIT(x)

typedef prb_Arena Arena;
typedef prb_Str   Str;
typedef intptr_t  isize;
typedef uint8_t   u8;

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

function Str
decode(Arena* arena, prb_Bytes input) {
    assert(input.len > 0 && input.len % 2 == 0);

    prb_GrowingStr gstr = prb_beginStr(arena);
    prb_addStrSegment(&gstr, "bits 16\n");

    for (isize offset = 0; offset < input.len; offset += 2) {
        u8 byte1 = input.data[offset];
        u8 byte2 = input.data[offset + 1];

        u8 movMask = 0b10001000;
        assert((byte1 & movMask) == movMask);

        bool dbit = (byte1 & 0b10) != 0;
        bool wbit = (byte1 & 0b1) != 0;

        u8 mod = byte2 >> 6;
        assert(mod == 0b11);

        u8 reg = (byte2 & 0b00111000) >> 3;
        u8 rm = byte2 & 0b00000111;

        u8 destReg = reg;
        u8 srcReg = rm;
        if (!dbit) {
            destReg = rm;
            srcReg = reg;
        }

        char* regTable16[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
        char* regTable8[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};

        char** regTable = regTable16;
        if (!wbit) {
            regTable = regTable8;
        }

        prb_addStrSegment(&gstr, "mov %s, %s\n", regTable[destReg], regTable[srcReg]);
    }

    Str result = prb_endStr(&gstr);
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

    test_decode(arena, STR("bits 16\nmov cx, bx"));
    test_decode(
        arena,
        STR(
            "bits 16\n"
            "mov cx, bx\n"
            "mov ch, ah\n"
            "mov dx, bx\n"
            "mov si, bx\n"
            "mov bx, di\n"
            "mov al, cl\n"
            "mov ch, ch\n"
            "mov bx, ax\n"
            "mov bx, si\n"
            "mov sp, di\n"
            "mov bp, ax\n"
        )
    );

    return 0;
}
