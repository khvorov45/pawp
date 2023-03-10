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

        bool isRegRegMov = (byte1 & 0b11111100) == 0b10001000;
        bool isRegRegArithmetic = (byte1 & 0b11000100) == 0b00000000;
        bool isImmToRmMov = (byte1 & 0b11111110) == 0b11000110;
        bool isImmToRmArithmetic = (byte1 & 0b11111100) == 0b10000000;
        bool isImmToAxMov = (byte1 & 0b11111110) == 0b10100000;
        bool isImmToAxArithmetic = (byte1 & 0b11000110) == 0b00000100;

        char* jmpStr = "";
        switch (byte1) {
            case 0b01110100: jmpStr = "je"; break;
            case 0b01111100: jmpStr = "jl"; break;
            case 0b01111110: jmpStr = "jle"; break;
            case 0b01110010: jmpStr = "jb"; break;
            case 0b01110110: jmpStr = "jbe"; break;
            case 0b01111010: jmpStr = "jp"; break;
            case 0b01110000: jmpStr = "jo"; break;
            case 0b01111000: jmpStr = "js"; break;
            case 0b01110101: jmpStr = "jnz"; break;
            case 0b01111101: jmpStr = "jnl"; break;
            case 0b01111111: jmpStr = "jnle"; break;
            case 0b01110011: jmpStr = "jnb"; break;
            case 0b01110111: jmpStr = "jnbe"; break;
            case 0b01111011: jmpStr = "jnp"; break;
            case 0b01110001: jmpStr = "jno"; break;
            case 0b01111001: jmpStr = "jns"; break;
            case 0b11100010: jmpStr = "loop"; break;
            case 0b11100001: jmpStr = "loopz"; break;
            case 0b11100000: jmpStr = "loopnz"; break;
            case 0b11100011: jmpStr = "jcxz"; break;
        }

        if (isRegRegMov || isRegRegArithmetic) {
            bool dbit = (byte1 & 0b10) != 0;
            bool wbit = (byte1 & 0b1) != 0;

            u8 byte2 = input.data[offset + 1];
            u8 mod = byte2 >> 6;
            u8 reg = (byte2 & 0b00111000) >> 3;
            u8 rm = byte2 & 0b00000111;

            char* regStr = getRegStr(reg, wbit);
            char* eqStr = eqs[rm];

            if (isRegRegArithmetic) {
                u8 instr = byte1 & 0b00111000;
                switch (instr) {
                    case 0b00000000: prb_addStrSegment(&gstr, "add "); break;
                    case 0b00101000: prb_addStrSegment(&gstr, "sub "); break;
                    case 0b00111000: prb_addStrSegment(&gstr, "cmp "); break;
                    default: assert(!"unimplemented");
                }
            } else {
                prb_addStrSegment(&gstr, "mov ");
            }

            switch (mod) {
                case 0b00: {
                    if (rm == 0b110) {
                        u8  byte3 = input.data[offset + 2];
                        u8  byte4 = input.data[offset + 3];
                        u16 address = ((u16)byte4 << 8) | (u16)byte3;

                        if (dbit) {
                            prb_addStrSegment(&gstr, "%s, [%d]\n", regStr, address);
                        } else {
                            prb_addStrSegment(&gstr, "[%d], %s,\n", address, regStr);
                        }

                        offset += 4;
                    } else {
                        if (dbit) {
                            prb_addStrSegment(&gstr, "%s, [%s]\n", regStr, eqStr);
                        } else {
                            prb_addStrSegment(&gstr, "[%s], %s\n", eqStr, regStr);
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
                        prb_addStrSegment(&gstr, "%s, [%s + %d]\n", regStr, eqStr, val);
                    } else {
                        prb_addStrSegment(&gstr, "[%s + %d], %s\n", eqStr, val, regStr);
                    }

                    offset += 3;
                } break;

                case 0b10: {
                    u8  byte3 = input.data[offset + 2];
                    u8  byte4 = input.data[offset + 3];
                    u16 val = ((u16)byte4 << 8) | (u16)byte3;

                    if (dbit) {
                        prb_addStrSegment(&gstr, "%s, [%s + %d]\n", regStr, eqStr, val);
                    } else {
                        prb_addStrSegment(&gstr, "[%s + %d], %s\n", eqStr, val, regStr);
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

                    prb_addStrSegment(&gstr, "%s, %s\n", getRegStr(destReg, wbit), getRegStr(srcReg, wbit));
                    offset += 2;
                } break;

                default: assert(!"unreachable"); break;
            }
        } else if (isImmToRmMov || isImmToRmArithmetic) {
            bool wbit = (byte1 & 0b1) != 0;

            u8 byte2 = input.data[offset + 1];

            u8 mod = byte2 >> 6;

            u8 reg = (byte2 & 0b00111000) >> 3;

            if (isImmToRmArithmetic) {
                u8 instr = byte2 & 0b00111000;
                switch (instr) {
                    case 0b00000000: prb_addStrSegment(&gstr, "add "); break;
                    case 0b00101000: prb_addStrSegment(&gstr, "sub "); break;
                    case 0b00111000: prb_addStrSegment(&gstr, "cmp "); break;
                    default: assert(!"unimplemented");
                }
            } else {
                assert(reg == 0);
                prb_addStrSegment(&gstr, "mov ");
            }

            u8    rm = byte2 & 0b00000111;
            char* eqStr = eqs[rm];

            switch (mod) {
                case 0b00: {
                    bool sbit = (byte1 & 0b10) != 0;

                    if (rm == 0b110) {
                        u8  byte3 = input.data[offset + 2];
                        u8  byte4 = input.data[offset + 3];
                        u16 disp = ((u16)byte4 << 8) | (u16)byte3;
                        prb_addStrSegment(&gstr, "[%d], ", disp);
                        offset += 2;
                    } else {
                        prb_addStrSegment(&gstr, "[%s], ", eqStr);
                    }

                    u8    byte3 = input.data[offset + 2];
                    u16   val = byte3;
                    char* size = "byte";

                    bool readByte4 = wbit && (!isImmToRmArithmetic || !sbit);
                    if (readByte4) {
                        u8 byte4 = input.data[offset + 3];
                        val = ((u16)byte4 << 8) | val;
                    }

                    if (wbit) {
                        size = "word";
                    }

                    prb_addStrSegment(&gstr, "%s %d\n", size, val);

                    offset += 3;
                    if (readByte4) {
                        offset += 1;
                    }
                } break;

                case 0b01: {
                    assert(!"unimplemented");
                } break;

                case 0b10: {
                    bool sbit = (byte1 & 0b10) != 0;

                    u8  byte3 = input.data[offset + 2];
                    u8  byte4 = input.data[offset + 3];
                    u16 disp = ((u16)byte4 << 8) | (u16)byte3;

                    u8    byte5 = input.data[offset + 4];
                    u16   val = byte5;
                    char* size = "byte";
                    bool  readByte6 = wbit && (!isImmToRmArithmetic || !sbit);
                    if (readByte6) {
                        u8 byte6 = input.data[offset + 5];
                        val = ((u16)byte6 << 8) | val;
                    }

                    if (wbit) {
                        size = "word";
                    }

                    prb_addStrSegment(&gstr, "[%s + %d], %s %d\n", eqStr, disp, size, val);

                    offset += 5;
                    if (readByte6) {
                        offset += 1;
                    }
                } break;

                case 0b11: {
                    assert(isImmToRmArithmetic);

                    char* regStr = getRegStr(rm, wbit);
                    prb_addStrSegment(&gstr, "%s, ", regStr);

                    bool sbit = (byte1 & 0b10) != 0;
                    u8   byte3 = input.data[offset + 2];
                    if (!sbit && wbit) {
                        u16 val = byte3;
                        u8  byte4 = input.data[offset + 3];
                        val = ((u16)byte4 << 8) | val;
                        prb_addStrSegment(&gstr, "%d\n", val);
                    } else if (sbit) {
                        assert(wbit);
                        i16 val = byte3;
                        prb_addStrSegment(&gstr, "%d\n", val);
                    }

                    offset += 3;
                    if (wbit && !sbit) {
                        offset += 1;
                    }
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
        } else if (isImmToAxMov || isImmToAxArithmetic) {
            bool wbit = (byte1 & 0b1) != 0;
            u8   byte2 = input.data[offset + 1];
            u16  address = byte2;

            if (wbit) {
                u8 byte3 = input.data[offset + 2];
                address = ((u16)byte3 << 8) | address;
            }

            char* regName = "ax";
            if (!wbit) {
                regName = "al";
            }

            if (isImmToAxArithmetic) {
                u8 instr = byte1 & 0b00111000;
                switch (instr) {
                    case 0b00000000: prb_addStrSegment(&gstr, "add "); break;
                    case 0b00101000: prb_addStrSegment(&gstr, "sub "); break;
                    case 0b00111000: prb_addStrSegment(&gstr, "cmp "); break;
                    default: assert(!"unimplemented");
                }
                i16 value = address;
                if (!wbit & (byte2 & 0b10000000)) {
                    value = 0b1111111100000000 | (u16)byte2;
                }
                prb_addStrSegment(&gstr, "%s, %d\n", regName, value);
            } else {
                prb_addStrSegment(&gstr, "mov %s, [%d]\n", regName, address);
            }

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
        } else if (jmpStr[0] != '\0') {
            u8 byte2 = input.data[offset + 1];
            i16 jumpOffset = byte2;
            if (byte2 & 0b10000000) {
                jumpOffset = 0b1111111100000000 | jumpOffset;
            }
            prb_addStrSegment(&gstr, "%s $+2+%d\n", jmpStr, jumpOffset);
            offset += 2;
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
            "mov cl, 12\n"
            "mov ch, -12\n"
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

            "add bx, [bx+si]\n"
            "add bx, [bp]\n"
            "add si, 2\n"
            "add bp, 2\n"
            "add cx, 8\n"
            "add bx, [bp + 0]\n"
            "add cx, [bx + 2]\n"
            "add bh, [bp + si + 4]\n"
            "add di, [bp + di + 6]\n"
            "add [bx+si], bx\n"
            "add [bp], bx\n"
            "add [bp + 0], bx\n"
            "add [bx + 2], cx\n"
            "add [bp + si + 4], bh\n"
            "add [bp + di + 6], di\n"
            "add byte [bx], 34\n"
            "add word [bp + si + 1000], 29\n"
            "add ax, [bp]\n"
            "add al, [bx + si]\n"
            "add ax, bx\n"
            "add al, ah\n"
            "add ax, 1000\n"
            "add al, -30\n"
            "add al, 9\n"

            "sub bx, [bx+si]\n"
            "sub bx, [bp]\n"
            "sub si, 2\n"
            "sub bp, 2\n"
            "sub cx, 8\n"
            "sub bx, [bp + 0]\n"
            "sub cx, [bx + 2]\n"
            "sub bh, [bp + si + 4]\n"
            "sub di, [bp + di + 6]\n"
            "sub [bx+si], bx\n"
            "sub [bp], bx\n"
            "sub [bp + 0], bx\n"
            "sub [bx + 2], cx\n"
            "sub [bp + si + 4], bh\n"
            "sub [bp + di + 6], di\n"
            "sub byte [bx], 34\n"
            "sub word [bx + di], 29\n"
            "sub ax, [bp]\n"
            "sub al, [bx + si]\n"
            "sub ax, bx\n"
            "sub al, ah\n"
            "sub ax, 1000\n"
            "sub al, -30\n"
            "sub al, 9\n"

            "cmp bx, [bx+si]\n"
            "cmp bx, [bp]\n"
            "cmp si, 2\n"
            "cmp bp, 2\n"
            "cmp cx, 8\n"
            "cmp bx, [bp + 0]\n"
            "cmp cx, [bx + 2]\n"
            "cmp bh, [bp + si + 4]\n"
            "cmp di, [bp + di + 6]\n"
            "cmp [bx+si], bx\n"
            "cmp [bp], bx\n"
            "cmp [bp + 0], bx\n"
            "cmp [bx + 2], cx\n"
            "cmp [bp + si + 4], bh\n"
            "cmp [bp + di + 6], di\n"
            "cmp byte [bx], 34\n"
            "cmp word [4834], 29\n"
            "cmp ax, [bp]\n"
            "cmp al, [bx + si]\n"
            "cmp ax, bx\n"
            "cmp al, ah\n"
            "cmp ax, 1000\n"
            "cmp al, -30\n"
            "cmp al, 9\n"

            "test_label0:\n"
            "jnz test_label1\n"
            "jnz test_label0\n"
            "test_label1:\n"
            "jnz test_label0\n"
            "jnz test_label1\n"

            "label:\n"
            "je label\n"
            "jl label\n"
            "jle label\n"
            "jb label\n"
            "jbe label\n"
            "jp label\n"
            "jo label\n"
            "js label\n"
            "jne label\n"
            "jnl label\n"
            "jg label\n"
            "jnb label\n"
            "ja label\n"
            "jnp label\n"
            "jno label\n"
            "jns label\n"
            "loop label\n"
            "loopz label\n"
            "loopnz label\n"
            "jcxz label\n"
        )
    );

    return 0;
}
