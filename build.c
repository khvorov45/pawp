#include "cbuild.h"

#define function static
#define assert(x) prb_assert(x)
#define STR(x) prb_STR(x)
#define LIT(x) prb_LIT(x)
#define createStrScanner prb_createStrScanner
#define strTrim prb_strTrim

typedef prb_Arena      Arena;
typedef prb_Str        Str;
typedef prb_StrScanner StrScanner;
typedef intptr_t       isize;
typedef uint8_t        u8;
typedef uint16_t       u16;
typedef int16_t        i16;
typedef int32_t        i32;

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
readFileStr(Arena* arena, Str path) {
    prb_Bytes bytes = readFile(arena, path);
    Str       str = prb_strFromBytes(bytes);
    return str;
}

function prb_Status
scanForward(StrScanner* scanner, Str pattern) {
    prb_Status result = prb_strScannerMove(scanner, (prb_StrFindSpec) {.pattern = pattern}, prb_StrScannerSide_AfterMatch);
    return result;
}

function u8
parseU8(Str str) {
    u8 result = 0;
    u8 pow2 = 1;
    for (i32 ind = str.len - 1; ind >= 0; ind--) {
        char digit = str.ptr[ind];
        switch (digit) {
            case '0': break;
            case '1': result += pow2; break;
            default: assert(!"unreachable"); break;
        }
        pow2 *= 2;
    }
    return result;
}

function void
addBinary(prb_GrowingStr* gstr, u8 binary, u8 bitCount) {
    for (i32 bitInd = bitCount - 1; bitInd >= 0; bitInd--) {
        u8 mask = 1 << bitInd;
        if (binary & mask) {
            prb_addStrSegment(gstr, "1");
        } else {
            prb_addStrSegment(gstr, "0");
        }
    }
}

typedef enum BitDescKind {
    BitDescKind_Literal,
    BitDescKind_Named,
} BitDescKind;

typedef struct BitDesc {
    BitDescKind kind;
    u8          bitCount;
    union {
        u8  literal;
        Str name;
    };
} BitDesc;

typedef struct ByteDesc {
    BitDesc* bits;
} ByteDesc;

typedef struct Instr {
    Str       name;
    Str       desc;
    ByteDesc* bytes;
} Instr;

function void
addIndent(prb_GrowingStr* gstr, i32 indentLevel) {
    while (indentLevel--) {
        prb_addStrSegment(gstr, "    ");
    }
}

function void
prb_ATTRIBUTE_FORMAT(3, 4)
    addLine(prb_GrowingStr* gstr, i32 indentLevel, char* fmt, ...) {
    addIndent(gstr, indentLevel);
    prb_assert(gstr->arena->lockedForStr);
    va_list args;
    va_start(args, fmt);
    prb_Str seg = prb_vfmtCustomBuffer((uint8_t*)prb_arenaFreePtr(gstr->arena), (int32_t)prb_min(prb_arenaFreeSize(gstr->arena), INT32_MAX), fmt, args);
    prb_arenaChangeUsed(gstr->arena, seg.len);
    gstr->str.len += seg.len;
    va_end(args);
    prb_addStrSegment(gstr, "\n");
}

function void
codegen(prb_GrowingStr* gstr, Instr* instrs, i32 indentLevel) {
    assert(arrlen(instrs) > 0);
    if (arrlen(instrs) > 1) {
        i32 minBitsInFirstLiteral = 8;
        for (i32 instrInd = 0; instrInd < arrlen(instrs); instrInd++) {
            Instr   instr = instrs[instrInd];
            BitDesc firstBit = instr.bytes[0].bits[0];
            assert(firstBit.kind == BitDescKind_Literal);
            minBitsInFirstLiteral = prb_min(minBitsInFirstLiteral, firstBit.bitCount);
        }

        addIndent(gstr, indentLevel);
        prb_addStrSegment(gstr, "u8 first%dBits = input.data[offset] >> %d;\n", minBitsInFirstLiteral, 8 - minBitsInFirstLiteral);
        addIndent(gstr, indentLevel);
        prb_addStrSegment(gstr, "switch (first%dBits) {\n", minBitsInFirstLiteral);

        Instr** cases = 0;
        i32     totalPossibleCases = 1 << minBitsInFirstLiteral;
        arrsetlen(cases, totalPossibleCases);
        for (i32 instrInd = 0; instrInd < arrlen(instrs); instrInd++) {
            Instr   instr = instrs[instrInd];
            BitDesc firstBit = instr.bytes[0].bits[0];
            u8      literal = firstBit.literal;
            u8      shift = firstBit.bitCount - minBitsInFirstLiteral;
            u8      switchLiteral = literal >> shift;
            assert(switchLiteral < arrlen(cases));
            arrput(cases[switchLiteral], instr);
        }

        for (u8 caseid = 0; caseid < arrlen(cases); caseid++) {
            Instr* caseInstrs = cases[caseid];
            if (arrlen(caseInstrs) > 0) {
                assert(arrlen(caseInstrs) < arrlen(instrs));
                prb_addStrSegment(gstr, "\n");
                if (arrlen(caseInstrs) == 1) {
                    Instr instr = caseInstrs[0];
                    addLine(gstr, indentLevel + 1, "// %.*s(%.*s)", LIT(instr.name), LIT(instr.desc));
                }
                addIndent(gstr, indentLevel + 1);
                prb_addStrSegment(gstr, "case 0b");
                addBinary(gstr, caseid, minBitsInFirstLiteral);
                prb_addStrSegment(gstr, ": {\n");
                codegen(gstr, caseInstrs, indentLevel + 2);
                addIndent(gstr, indentLevel + 1);
                prb_addStrSegment(gstr, "} break;\n");
            }
        }

        addLine(gstr, indentLevel, "");
        addLine(gstr, indentLevel + 1, "default: assert(!\"unimplemented\"); break;");
        addLine(gstr, indentLevel, "}");
    } else {
        Instr instr = instrs[0];
        addIndent(gstr, indentLevel);
        prb_addStrSegment(gstr, "instr->kind = InstrKind_%.*s_%.*s;\n\n", LIT(instr.name), LIT(instr.desc));
        bool regField = false;
        bool rmField = false;
        bool dField = false;
        bool wField = false;
        bool sField = false;
        bool dataField = false;
        bool memoryToAccumulator = false;
        bool accumulatorToMemory = false;
        bool op1IsAccumulator = false;

        for (i32 byteInd = 0; byteInd < arrlen(instr.bytes); byteInd++) {
            ByteDesc byte = instr.bytes[byteInd];

            if (arrlen(byte.bits) > 1) {
                assert(byteInd == 0 || byteInd == 1);

                i32 bitsLeft = 8;
                for (i32 bitInd = 0; bitInd < arrlen(byte.bits); bitInd++) {
                    BitDesc bit = byte.bits[bitInd];

                    if (byteInd == 0 && bitInd == 0) {
                        assert(bit.kind == BitDescKind_Literal);
                        memoryToAccumulator = memoryToAccumulator || bit.literal == 0b1010000;
                        accumulatorToMemory = accumulatorToMemory || bit.literal == 0b1010001;
                        op1IsAccumulator = op1IsAccumulator || bit.literal == 0b0000010;
                    }

                    addIndent(gstr, indentLevel);
                    switch (bit.kind) {
                        case BitDescKind_Literal: {
                            prb_addStrSegment(gstr, "u8 byte%dbit%d_literal = ", byteInd, bitInd);
                        } break;
                        case BitDescKind_Named: {
                            prb_addStrSegment(gstr, "u8 %.*s = ", LIT(bit.name));
                            regField = regField || prb_streq(bit.name, STR("reg"));
                            rmField = rmField || prb_streq(bit.name, STR("r_m"));
                            dField = dField || prb_streq(bit.name, STR("d"));
                            wField = wField || prb_streq(bit.name, STR("w"));
                            sField = sField || prb_streq(bit.name, STR("s"));
                        } break;
                    }

                    u8 mask = ((1 << bit.bitCount) - 1);
                    prb_addStrSegment(gstr, "(input.data[offset] >> %d) & 0b", bitsLeft - bit.bitCount);
                    addBinary(gstr, mask, 8);
                    prb_addStrSegment(gstr, ";\n");

                    if (bit.kind == BitDescKind_Literal) {
                        addIndent(gstr, indentLevel);
                        prb_addStrSegment(gstr, "assert(byte%dbit%d_literal == 0b", byteInd, bitInd);
                        addBinary(gstr, bit.literal, bit.bitCount);
                        prb_addStrSegment(gstr, ");\n");
                    }

                    bitsLeft -= bit.bitCount;
                }

                addIndent(gstr, indentLevel);
                prb_addStrSegment(gstr, "offset += 1;\n\n");
            } else {
                assert(arrlen(byte.bits) == 1);
                BitDesc bit = byte.bits[0];
                assert(bit.kind == BitDescKind_Named);
                if (prb_streq(bit.name, STR("disp_lo"))) {
                    addLine(gstr, indentLevel, "Operand rmOp = {};");
                    addLine(gstr, indentLevel, "switch (mod) {");

                    addLine(gstr, indentLevel + 1, "case 0b00: {");
                    addLine(gstr, indentLevel + 1 + 1, "rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m};");
                    addLine(gstr, indentLevel + 1 + 1, "if (r_m == 0b110) {");
                    addLine(gstr, indentLevel + 1 + 1 + 1, "rmOp.mem.direct = true;");
                    addLine(gstr, indentLevel + 1 + 1 + 1, "rmOp.mem.disp = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset]);");
                    addLine(gstr, indentLevel + 1 + 1 + 1, "offset += 2;");
                    addLine(gstr, indentLevel + 1 + 1, "}");
                    addLine(gstr, indentLevel + 1, "} break;");

                    addLine(gstr, indentLevel + 1, "case 0b01: {");
                    addLine(gstr, indentLevel + 1 + 1, "rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m, .mem.disp = *((i8*)&input.data[offset])};");
                    addLine(gstr, indentLevel + 1 + 1, "offset += 1;");
                    addLine(gstr, indentLevel + 1, "} break;");

                    addLine(gstr, indentLevel + 1, "case 0b10: {");
                    addLine(gstr, indentLevel + 1 + 1, "rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m, .mem.disp = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset])};");
                    addLine(gstr, indentLevel + 1 + 1, "offset += 2;");
                    addLine(gstr, indentLevel + 1, "} break;");

                    addLine(gstr, indentLevel + 1, "case 0b11: {");
                    addLine(gstr, indentLevel + 1 + 1, "rmOp = (Operand) {.kind = OpID_Register, .reg.id = w ? r_m : r_m %% 4, .reg.bytes = w ? 2 : 1, .reg.offset = w == 0 && r_m > 0b11};");
                    addLine(gstr, indentLevel + 1, "} break;");

                    addLine(gstr, indentLevel, "}\n");
                } else if (prb_streq(bit.name, STR("data_lo"))) {
                    dataField = true;
                    addLine(gstr, indentLevel, "u16 data = input.data[offset];");
                    addLine(gstr, indentLevel, "offset += 1;");
                    if (sField) {
                        addLine(gstr, indentLevel, "if (w == 1 && s == 0) {");
                    } else {
                        addLine(gstr, indentLevel, "if (w == 1) {");
                    }
                    addLine(gstr, indentLevel + 1, "data = ((u16)input.data[offset] << 8) | data;");
                    addLine(gstr, indentLevel + 1, "offset += 1;");
                    addLine(gstr, indentLevel, "}\n");
                } else if (prb_streq(bit.name, STR("addr_lo"))) {
                    addLine(gstr, indentLevel, "u16 addr = input.data[offset];");
                    addLine(gstr, indentLevel, "offset += 1;");
                    addLine(gstr, indentLevel, "if (w == 1) {");
                    addLine(gstr, indentLevel + 1, "addr = ((u16)input.data[offset] << 8) | addr;");
                    addLine(gstr, indentLevel + 1, "offset += 1;");
                    addLine(gstr, indentLevel, "}\n");
                } else {
                    assert(!"unrecognized");
                }

                if (prb_strEndsWith(bit.name, STR("_lo"))) {
                    assert(byteInd < arrlen(instr.bytes) - 1);
                    ByteDesc nextByte = instr.bytes[byteInd + 1];
                    assert(arrlen(nextByte.bits) == 1);
                    BitDesc nextByteBit = nextByte.bits[0];
                    assert(nextByteBit.kind == BitDescKind_Named && prb_strEndsWith(nextByteBit.name, STR("_hi")));
                    byteInd += 1;
                }
            }
        }

        if (regField) {
            addLine(gstr, indentLevel, "Operand regOp = {.kind = OpID_Register, .reg.id = w ? reg : reg %% 4, .reg.bytes = w ? 2 : 1, .reg.offset = w == 0 && reg > 0b11};\n");
        }

        if (regField && rmField) {
            addLine(gstr, indentLevel, "if (d) {");
            addLine(gstr, indentLevel + 1, "instr->op1 = regOp;");
            addLine(gstr, indentLevel + 1, "instr->op2 = rmOp;");
            addLine(gstr, indentLevel, "} else {");
            addLine(gstr, indentLevel + 1, "instr->op1 = rmOp;");
            addLine(gstr, indentLevel + 1, "instr->op2 = regOp;");
            addLine(gstr, indentLevel, "}");
        } else if (regField) {
            addLine(gstr, indentLevel, "instr->op1 = regOp;");
        } else if (rmField) {
            addLine(gstr, indentLevel, "instr->op1 = rmOp;");
        } else if (memoryToAccumulator) {
            addLine(gstr, indentLevel, "instr->op1 = (Operand) {.kind = OpID_Register, .reg.id = RegisterID_AX, .reg.bytes = w ? 2 : 1};");
            addLine(gstr, indentLevel, "instr->op2 = (Operand) {.kind = OpID_Memory, .mem.direct = true, .mem.disp = addr};");
        } else if (accumulatorToMemory) {
            addLine(gstr, indentLevel, "instr->op1 = (Operand) {.kind = OpID_Memory, .mem.direct = true, .mem.disp = addr};");
            addLine(gstr, indentLevel, "instr->op2 = (Operand) {.kind = OpID_Register, .reg.id = RegisterID_AX, .reg.bytes = w ? 2 : 1};");
        } else if (op1IsAccumulator) {
            addLine(gstr, indentLevel, "instr->op1 = (Operand) {.kind = OpID_Register, .reg.id = RegisterID_AX, .reg.bytes = w ? 2 : 1};");
        }

        if (dataField) {
            addLine(gstr, indentLevel, "instr->op2 = (Operand) {.kind = OpID_Immediate, .immediate.val = data, .immediate.bytes = w ? 2 : 1};");
        }
    }
}

int
main() {
    Arena  arena_ = prb_createArenaFromVmem(1 * prb_GIGABYTE);
    Arena* arena = &arena_;

    Str rootDir = prb_getParentDir(arena, STR(__FILE__));
    Str x86InstructionPath = prb_pathJoin(arena, rootDir, STR("x86.inl"));
    Str x86Instructions = readFileStr(arena, x86InstructionPath);

    // NOTE(khvorov) Parse out instruction
    Instr* instrs = 0;
    {
        StrScanner lineIter = createStrScanner(x86Instructions);
        while (prb_strScannerMove(&lineIter, (prb_StrFindSpec) {.mode = prb_StrFindMode_LineBreak}, prb_StrScannerSide_AfterMatch)) {
            Str        line = strTrim(lineIter.betweenLastMatches);
            StrScanner fieldIter = createStrScanner(line);

            Instr instr = {};

            {
                assert(scanForward(&fieldIter, STR("|")));
                Str        firstField = strTrim(fieldIter.betweenLastMatches);
                StrScanner firstscan = createStrScanner(firstField);
                assert(scanForward(&firstscan, STR("(")));
                instr.name = strTrim(firstscan.betweenLastMatches);
                assert(scanForward(&firstscan, STR(")")));
                instr.desc = strTrim(firstscan.betweenLastMatches);
            }

            while (prb_strScannerMove(&fieldIter, (prb_StrFindSpec) {.pattern = STR("|"), .alwaysMatchEnd = true}, prb_StrScannerSide_AfterMatch)) {
                ByteDesc byte = {};

                Str        byteDesc = strTrim(fieldIter.betweenLastMatches);
                StrScanner byteScan = createStrScanner(byteDesc);
                while ((prb_strScannerMove(&byteScan, (prb_StrFindSpec) {.pattern = STR(" "), .alwaysMatchEnd = true}, prb_StrScannerSide_AfterMatch))) {
                    Str bitsDescStr = strTrim(byteScan.betweenLastMatches);
                    assert(bitsDescStr.len > 0);

                    BitDesc desc = {};
                    if (bitsDescStr.ptr[0] == '1' || bitsDescStr.ptr[0] == '0') {
                        desc.kind = BitDescKind_Literal;
                        desc.literal = parseU8(bitsDescStr);
                        desc.bitCount = bitsDescStr.len;
                    } else {
                        desc.kind = BitDescKind_Named;
                        {
                            StrScanner scan = createStrScanner(bitsDescStr);
                            if (scanForward(&scan, STR("("))) {
                                desc.name = strTrim(scan.betweenLastMatches);
                                assert(scanForward(&scan, STR(")")));
                                Str                 valueStr = strTrim(scan.betweenLastMatches);
                                prb_ParseUintResult res = prb_parseUint(valueStr, 10);
                                assert(res.success);
                                assert(res.number < UINT8_MAX);
                                desc.bitCount = res.number;
                            } else {
                                desc.name = bitsDescStr;
                                desc.bitCount = 8;
                            }
                        }
                    }

                    arrput(byte.bits, desc);
                }

                arrput(instr.bytes, byte);
            }

            arrput(instrs, instr);
        }
    }

    // NOTE(khvorov) Verify instructions make sense
    {
        for (i32 instrInd = 0; instrInd < arrlen(instrs); instrInd++) {
            Instr instr = instrs[instrInd];
            assert(arrlen(instr.bytes) > 0);
            for (i32 byteInd = 0; byteInd < arrlen(instr.bytes); byteInd++) {
                ByteDesc byte = instr.bytes[byteInd];
                assert(arrlen(byte.bits) > 0);

                i32 byteBitCount = 0;
                for (i32 bitInd = 0; bitInd < arrlen(byte.bits); bitInd++) {
                    BitDesc bit = byte.bits[bitInd];
                    byteBitCount += bit.bitCount;
                }
                assert(byteBitCount == 8);
            }
        }
    }

    // NOTE(khvorov) Write out formatted instructions
    {
        i32 colCount = 0;
        for (i32 instrInd = 0; instrInd < arrlen(instrs); instrInd++) {
            Instr instr = instrs[instrInd];
            colCount = prb_max(arrlen(instr.bytes) + 1, colCount);
        }
        i32* longestCols = prb_arenaAllocArray(arena, i32, colCount);

        for (i32 instrInd = 0; instrInd < arrlen(instrs); instrInd++) {
            Instr instr = instrs[instrInd];
            i32   firstColLen = instr.name.len + instr.desc.len + 2;
            longestCols[0] = prb_max(longestCols[0], firstColLen);
        }

        prb_GrowingStr gstr = prb_beginStr(arena);
        for (i32 instrInd = 0; instrInd < arrlen(instrs); instrInd++) {
            Instr instr = instrs[instrInd];

            {
                i32 thisColLen = 0;
                {
                    i32 before = gstr.str.len;
                    prb_addStrSegment(&gstr, "%.*s(%.*s)", LIT(instr.name), LIT(instr.desc));
                    thisColLen = gstr.str.len - before;
                }
                i32 spaces = longestCols[0] - thisColLen + 1;
                while (spaces--) {
                    prb_addStrSegment(&gstr, " ");
                }
                prb_addStrSegment(&gstr, "| ");
            }

            for (i32 byteInd = 0; byteInd < arrlen(instr.bytes); byteInd++) {
                ByteDesc byte = instr.bytes[byteInd];
                for (i32 bitInd = 0; bitInd < arrlen(byte.bits); bitInd++) {
                    BitDesc bit = byte.bits[bitInd];
                    switch (bit.kind) {
                        case BitDescKind_Literal: addBinary(&gstr, bit.literal, bit.bitCount); break;
                        case BitDescKind_Named: {
                            prb_addStrSegment(&gstr, "%.*s", LIT(bit.name));
                            if (bit.bitCount < 8) {
                                prb_addStrSegment(&gstr, "(%d)", bit.bitCount);
                            }
                        } break;
                    }

                    if (bitInd < arrlen(byte.bits) - 1) {
                        prb_addStrSegment(&gstr, " ");
                    }
                }
                if (byteInd < arrlen(instr.bytes) - 1) {
                    prb_addStrSegment(&gstr, " | ");
                }
            }
            prb_addStrSegment(&gstr, "\n");
        }
        Str str = prb_endStr(&gstr);
        assert(prb_writeEntireFile(arena, x86InstructionPath, str.ptr, str.len));
    }

    // NOTE(khvorov) Insert codegen into the main file
    Str hmpath = prb_pathJoin(arena, rootDir, STR("hm.c"));
    {
        Str            hmcontent = readFileStr(arena, hmpath);
        prb_GrowingStr hmnew = prb_beginStr(arena);

        StrScanner hmscan = createStrScanner(hmcontent);

        assert(scanForward(&hmscan, STR("typedef enum InstrKind {")));
        prb_addStrSegment(&hmnew, "%.*s%.*s\n", LIT(hmscan.betweenLastMatches), LIT(hmscan.match));
        for (i32 instrInd = 0; instrInd < arrlen(instrs); instrInd++) {
            Instr instr = instrs[instrInd];
            prb_addStrSegment(&hmnew, "    InstrKind_%.*s_%.*s,\n", LIT(instr.name), LIT(instr.desc));
        }
        assert(scanForward(&hmscan, STR("} InstrKind;")));
        prb_addStrSegment(&hmnew, "%.*s", LIT(hmscan.match));

        assert(scanForward(&hmscan, STR("// @codegen")));
        prb_addStrSegment(&hmnew, "%.*s%.*s\n", LIT(hmscan.betweenLastMatches), LIT(hmscan.match));
        codegen(&hmnew, instrs, 2);
        assert(scanForward(&hmscan, STR("// @end")));
        prb_addStrSegment(&hmnew, "        %.*s%.*s", LIT(hmscan.match), LIT(hmscan.afterMatch));

        Str hmnewContent = prb_endStr(&hmnew);
        assert(prb_writeEntireFile(arena, hmpath, hmnewContent.ptr, hmnewContent.len));
    }

    // NOTE(khvorov) Compile and run the main file
    {
        Str hmout = prb_replaceExt(arena, hmpath, STR("exe"));
        execCmd(arena, prb_fmt(arena, "clang -g -Wall -Wextra %.*s -o %.*s", LIT(hmpath), LIT(hmout)));
        execCmd(arena, hmout);
    }

    return 0;
}
