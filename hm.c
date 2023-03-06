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

function Str
decode(Arena* arena, prb_Bytes input) {
    prb_GrowingStr gstr = prb_beginStr(arena);
    prb_addStrSegment(&gstr, "bits 16\n");

    for (isize offset = 0; offset < input.len;) {
        u8    byte1 = input.data[offset];
        char* eqs[] = {"bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx"};

        if ((byte1 & 0b11111100) == 0b10001000) {
            bool dbit = (byte1 & 0b10) != 0;
            bool wbit = (byte1 & 0b1) != 0;

            u8 byte2 = input.data[offset + 1];
            u8 mod = byte2 >> 6;
            u8 reg = (byte2 & 0b00111000) >> 3;
            u8 rm = byte2 & 0b00000111;

            char* regStr = getRegStr(reg, wbit);
            char* eqStr = eqs[rm];

            switch (mod) {
                case 0b00: {
                    if (rm == 0b110) {
                        u8  byte3 = input.data[offset + 2];
                        u8  byte4 = input.data[offset + 3];
                        u16 address = ((u16)byte4 << 8) | (u16)byte3;

                        if (dbit) {
                            prb_addStrSegment(&gstr, "mov %s, [%d]\n", regStr, address);
                        } else {
                            prb_addStrSegment(&gstr, "mov [%d], %s,\n", address, regStr);
                        }

                        offset += 4;
                    } else {
                        if (dbit) {
                            prb_addStrSegment(&gstr, "mov %s, [%s]\n", regStr, eqStr);
                        } else {
                            prb_addStrSegment(&gstr, "mov [%s], %s\n", eqStr, regStr);
                        }

                        offset += 2;
                    }
                } break;

                case 0b01: {
                    u8  byte3 = input.data[offset + 2];
                    i16 val = byte3;
                    if (byte3 & 0b10000000) {
                        val = 0b1111111100000000 | (u16)byte3;
                    }

                    if (dbit) {
                        prb_addStrSegment(&gstr, "mov %s, [%s + %d]\n", regStr, eqStr, val);
                    } else {
                        prb_addStrSegment(&gstr, "mov [%s + %d], %s\n", eqStr, val, regStr);
                    }

                    offset += 3;
                } break;

                case 0b10: {
                    u8  byte3 = input.data[offset + 2];
                    u8  byte4 = input.data[offset + 3];
                    u16 val = ((u16)byte4 << 8) | (u16)byte3;

                    if (dbit) {
                        prb_addStrSegment(&gstr, "mov %s, [%s + %d]\n", regStr, eqStr, val);
                    } else {
                        prb_addStrSegment(&gstr, "mov [%s + %d], %s\n", eqStr, val, regStr);
                    }

                    offset += 4;
                } break;

                case 0b11: {
                    u8 destReg = reg;
                    u8 srcReg = rm;
                    if (!dbit) {
                        destReg = rm;
                        srcReg = reg;
                    }

                    prb_addStrSegment(&gstr, "mov %s, %s\n", getRegStr(destReg, wbit), getRegStr(srcReg, wbit));
                    offset += 2;
                } break;

                default: assert(!"unreachable"); break;
            }
        } else if ((byte1 & 0b11111110) == 0b11000110) {
            bool wbit = (byte1 & 0b1) != 0;

            u8 byte2 = input.data[offset + 1];

            u8 mod = byte2 >> 6;

            u8 reg = (byte2 & 0b00111000) >> 3;
            assert(reg == 0);

            u8    rm = byte2 & 0b00000111;
            char* eqStr = eqs[rm];

            switch (mod) {
                case 0b00: {
                    assert(rm != 0b110);
                    u8    byte3 = input.data[offset + 2];
                    u16   val = byte3;
                    char* size = "byte";
                    if (wbit) {
                        u8 byte4 = input.data[offset + 3];
                        val = ((u16)byte4 << 8) | val;
                        size = "word";
                    }

                    prb_addStrSegment(&gstr, "mov [%s], %s %d\n", eqStr, size, val);

                    offset += 3;
                    if (wbit) {
                        offset += 1;
                    }
                } break;

                case 0b01: {
                    assert(!"unimplemented");
                } break;

                case 0b10: {
                    u8  byte3 = input.data[offset + 2];
                    u8  byte4 = input.data[offset + 3];
                    u16 disp = ((u16)byte4 << 8) | (u16)byte3;

                    u8    byte5 = input.data[offset + 4];
                    u16   val = byte5;
                    char* size = "byte";
                    if (wbit) {
                        u8 byte6 = input.data[offset + 5];
                        val = ((u16)byte6 << 8) | val;
                        size = "word";
                    }

                    prb_addStrSegment(&gstr, "mov [%s + %d], %s %d\n", eqStr, disp, size, val);

                    offset += 5;
                    if (wbit) {
                        offset += 1;
                    }
                } break;

                case 0b11: {
                    assert(!"unimplemented");
                } break;

                default: assert(!"unreachable"); break;
            }

        } else if ((byte1 & 0b11110000) == 0b10110000) {
            bool wbit = (byte1 & 0b1000) != 0;
            u16  value = (u16)input.data[offset + 1];
            if (wbit) {
                u16 byte3 = input.data[offset + 2];
                value = value | (byte3 << 8);
            }

            u8 reg = byte1 & 0b111;
            prb_addStrSegment(&gstr, "mov %s, %d\n", getRegStr(reg, wbit), value);

            offset += 2;
            if (wbit) {
                offset += 1;
            }
        } else if ((byte1 & 0b11111110) == 0b10100000) {
            bool wbit = (byte1 & 0b1) != 0;
            u8   byte2 = input.data[offset + 1];
            u16  address = byte2;
            if (wbit) {
                u8 byte3 = input.data[offset + 2];
                address = ((u16)byte3 << 8) | address;
            }

            prb_addStrSegment(&gstr, "mov ax, [%d]\n", address);
            offset += 2;
            if (wbit) {
                offset += 1;
            }
        } else if ((byte1 & 0b11111110) == 0b10100010) {
            bool wbit = (byte1 & 0b1) != 0;
            u8   byte2 = input.data[offset + 1];
            u16  address = byte2;
            if (wbit) {
                u8 byte3 = input.data[offset + 2];
                address = ((u16)byte3 << 8) | address;
            }

            prb_addStrSegment(&gstr, "mov [%d], ax\n", address);
            offset += 2;
            if (wbit) {
                offset += 1;
            }
        } else {
            assert(!"unimplemented");
        }

        assert(offset <= input.len);
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
            "mov si, bx\n"
            "mov dh, al\n"
            "mov cx, 12\n"
            "mov cx, -12\n"
            "mov dx, 3948\n"
            "mov dx, -3948\n"
            "mov al, [bx + si]\n"
            "mov bx, [bp + di]\n"
            "mov dx, [bp]\n"
            "mov ah, [bx + si + 4]\n"
            "mov al, [bx + si + 4999]\n"
            "mov [bx + di], cx\n"
            "mov [bp + si], cl\n"
            "mov [bp], ch\n"
            "mov ax, [bx + di - 37]\n"
            "mov [si - 300], cx\n"
            "mov dx, [bx - 32]\n"
            "mov [bp + di], byte 7\n"
            "mov [di + 901], word 347\n"
            "mov bp, [5]\n"
            "mov bx, [3458]\n"
            "mov ax, [2555]\n"
            "mov ax, [16]\n"
            "mov [2554], ax\n"
            "mov [15], ax\n"
        )
    );

    return 0;
}
