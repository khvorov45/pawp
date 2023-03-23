#include "cbuild.h"

#define function static
#define assert(x) prb_assert(x)
#define STR(x) prb_STR(x)
#define LIT(x) prb_LIT(x)

typedef prb_Arena Arena;
typedef prb_Str   Str;
typedef intptr_t  isize;
typedef uint8_t   u8;
typedef int8_t    i8;
typedef uint16_t  u16;
typedef int16_t   i16;
typedef int32_t   i32;

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

typedef enum InstrKind {
    InstrKind_mov_RegisterMemory_ToFrom_Register,
    InstrKind_mov_Immediate_To_RegisterMemory,
    InstrKind_mov_Immediate_To_Register,
    InstrKind_mov_Memory_To_Accumulator,
    InstrKind_mov_Accumulator_To_Memory,
    InstrKind_add_RegisterMemory_With_Register_To_Either,
    InstrKind_add_Immediate_To_RegisterMemory,
    InstrKind_add_Immediate_To_Accumulator,
} InstrKind;

typedef enum RegisterID {
    RegisterID_AX,
    RegisterID_CX,
    RegisterID_DX,
    RegisterID_BX,
    RegisterID_SP,
    RegisterID_BP,
    RegisterID_SI,
    RegisterID_DI,
} RegisterID;

typedef enum FormulaID {
    FormulaID_BX_SI,
    FormulaID_BX_DI,
    FormulaID_BP_SI,
    FormulaID_BP_DI,
    FormulaID_SI,
    FormulaID_DI,
    FormulaID_BP,
    FormulaID_BX,
} FormulaID;

typedef enum OpID {
    OpID_Register,
    OpID_Memory,
    OpID_Immediate,
} OpID;

typedef struct Operand {
    OpID kind;
    union {
        struct {
            RegisterID id;
            u8         bytes;
            u8         offset;
        } reg;
        struct {
            FormulaID id;
            bool      direct;
            i16       disp;
        } mem;
        struct {
            u16 val;
            u8  bytes;
        } immediate;
    };
} Operand;

typedef struct Instr {
    InstrKind kind;
    Operand   op1;
    Operand   op2;
} Instr;

typedef struct InstrArray {
    Instr* ptr;
    isize  len;
} InstrArray;

function InstrArray
decode(Arena* arena, prb_Bytes input) {
    // NOTE(khvorov) This arena won't be allocating anything other than instructions here
    prb_arenaAlignFreePtr(arena, alignof(Instr));
    Instr* instrs = prb_arenaFreePtr(arena);

    i32 instrCount = 0;
    i32 offset = 0;
    while (offset < input.len) {
        Instr* instr = prb_arenaAllocStruct(arena, Instr);
        instrCount += 1;

        // clang-format off
        // @codegen
        u8 first4Bits = input.data[offset] >> 4;
        switch (first4Bits) {

            case 0b0000: {
                u8 first6Bits = input.data[offset] >> 2;
                switch (first6Bits) {

                    // add(RegisterMemory_With_Register_To_Either)
                    case 0b000000: {
                        instr->kind = InstrKind_add_RegisterMemory_With_Register_To_Either;

                        u8 byte0bit0_literal = (input.data[offset] >> 2) & 0b00111111;
                        assert(byte0bit0_literal == 0b000000);
                        u8 d = (input.data[offset] >> 1) & 0b00000001;
                        u8 w = (input.data[offset] >> 0) & 0b00000001;
                        offset += 1;

                        u8 mod = (input.data[offset] >> 6) & 0b00000011;
                        u8 reg = (input.data[offset] >> 3) & 0b00000111;
                        u8 r_m = (input.data[offset] >> 0) & 0b00000111;
                        offset += 1;

                        Operand rmOp = {};
                        switch (mod) {
                            case 0b00: {
                                rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m};
                                if (r_m == 0b110) {
                                    rmOp.mem.direct = true;
                                    rmOp.mem.disp = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset]);
                                    offset += 2;
                                }
                            } break;
                            case 0b01: {
                                rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m, .mem.disp = *((i8*)&input.data[offset])};
                                offset += 1;
                            } break;
                            case 0b10: {
                                rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m, .mem.disp = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset])};
                                offset += 2;
                            } break;
                            case 0b11: {
                                rmOp = (Operand) {.kind = OpID_Register, .reg.id = w ? r_m : r_m % 4, .reg.bytes = w ? 2 : 1, .reg.offset = w == 0 && r_m > 0b11};
                            } break;
                        }

                        Operand regOp = {.kind = OpID_Register, .reg.id = w ? reg : reg % 4, .reg.bytes = w ? 2 : 1, .reg.offset = w == 0 && reg > 0b11};

                        if (d) {
                            instr->op1 = regOp;
                            instr->op2 = rmOp;
                        } else {
                            instr->op1 = rmOp;
                            instr->op2 = regOp;
                        }
                    } break;

                    // add(Immediate_To_Accumulator)
                    case 0b000001: {
                        instr->kind = InstrKind_add_Immediate_To_Accumulator;

                        u8 byte0bit0_literal = (input.data[offset] >> 1) & 0b01111111;
                        assert(byte0bit0_literal == 0b0000010);
                        u8 w = (input.data[offset] >> 0) & 0b00000001;
                        offset += 1;

                        u16 data = input.data[offset];
                        offset += 1;
                        if (w == 1) {
                            data = ((u16)input.data[offset] << 8) | data;
                            offset += 1;
                        }

                        instr->op1 = (Operand) {.kind = OpID_Register, .reg.id = RegisterID_AX, .reg.bytes = w ? 2 : 1};
                        instr->op2 = (Operand) {.kind = OpID_Immediate, .immediate.val = data, .immediate.bytes = w ? 2 : 1};
                    } break;
                
                    default: assert(!"unimplemented"); break;
                }
            } break;

            case 0b1000: {
                u8 first6Bits = input.data[offset] >> 2;
                switch (first6Bits) {

                    // add(Immediate_To_RegisterMemory)
                    case 0b100000: {
                        instr->kind = InstrKind_add_Immediate_To_RegisterMemory;

                        u8 byte0bit0_literal = (input.data[offset] >> 2) & 0b00111111;
                        assert(byte0bit0_literal == 0b100000);
                        u8 s = (input.data[offset] >> 1) & 0b00000001;
                        u8 w = (input.data[offset] >> 0) & 0b00000001;
                        offset += 1;

                        u8 mod = (input.data[offset] >> 6) & 0b00000011;
                        u8 byte1bit1_literal = (input.data[offset] >> 3) & 0b00000111;
                        assert(byte1bit1_literal == 0b000);
                        u8 r_m = (input.data[offset] >> 0) & 0b00000111;
                        offset += 1;

                        Operand rmOp = {};
                        switch (mod) {
                            case 0b00: {
                                rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m};
                                if (r_m == 0b110) {
                                    rmOp.mem.direct = true;
                                    rmOp.mem.disp = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset]);
                                    offset += 2;
                                }
                            } break;
                            case 0b01: {
                                rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m, .mem.disp = *((i8*)&input.data[offset])};
                                offset += 1;
                            } break;
                            case 0b10: {
                                rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m, .mem.disp = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset])};
                                offset += 2;
                            } break;
                            case 0b11: {
                                rmOp = (Operand) {.kind = OpID_Register, .reg.id = w ? r_m : r_m % 4, .reg.bytes = w ? 2 : 1, .reg.offset = w == 0 && r_m > 0b11};
                            } break;
                        }

                        u16 data = input.data[offset];
                        offset += 1;
                        if (w == 1 && s == 0) {
                            data = ((u16)input.data[offset] << 8) | data;
                            offset += 1;
                        }

                        instr->op1 = rmOp;
                        instr->op2 = (Operand) {.kind = OpID_Immediate, .immediate.val = data, .immediate.bytes = w ? 2 : 1};
                    } break;

                    // mov(RegisterMemory_ToFrom_Register)
                    case 0b100010: {
                        instr->kind = InstrKind_mov_RegisterMemory_ToFrom_Register;

                        u8 byte0bit0_literal = (input.data[offset] >> 2) & 0b00111111;
                        assert(byte0bit0_literal == 0b100010);
                        u8 d = (input.data[offset] >> 1) & 0b00000001;
                        u8 w = (input.data[offset] >> 0) & 0b00000001;
                        offset += 1;

                        u8 mod = (input.data[offset] >> 6) & 0b00000011;
                        u8 reg = (input.data[offset] >> 3) & 0b00000111;
                        u8 r_m = (input.data[offset] >> 0) & 0b00000111;
                        offset += 1;

                        Operand rmOp = {};
                        switch (mod) {
                            case 0b00: {
                                rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m};
                                if (r_m == 0b110) {
                                    rmOp.mem.direct = true;
                                    rmOp.mem.disp = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset]);
                                    offset += 2;
                                }
                            } break;
                            case 0b01: {
                                rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m, .mem.disp = *((i8*)&input.data[offset])};
                                offset += 1;
                            } break;
                            case 0b10: {
                                rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m, .mem.disp = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset])};
                                offset += 2;
                            } break;
                            case 0b11: {
                                rmOp = (Operand) {.kind = OpID_Register, .reg.id = w ? r_m : r_m % 4, .reg.bytes = w ? 2 : 1, .reg.offset = w == 0 && r_m > 0b11};
                            } break;
                        }

                        Operand regOp = {.kind = OpID_Register, .reg.id = w ? reg : reg % 4, .reg.bytes = w ? 2 : 1, .reg.offset = w == 0 && reg > 0b11};

                        if (d) {
                            instr->op1 = regOp;
                            instr->op2 = rmOp;
                        } else {
                            instr->op1 = rmOp;
                            instr->op2 = regOp;
                        }
                    } break;
                
                    default: assert(!"unimplemented"); break;
                }
            } break;

            case 0b1010: {
                u8 first7Bits = input.data[offset] >> 1;
                switch (first7Bits) {

                    // mov(Memory_To_Accumulator)
                    case 0b1010000: {
                        instr->kind = InstrKind_mov_Memory_To_Accumulator;

                        u8 byte0bit0_literal = (input.data[offset] >> 1) & 0b01111111;
                        assert(byte0bit0_literal == 0b1010000);
                        u8 w = (input.data[offset] >> 0) & 0b00000001;
                        offset += 1;

                        u16 addr = input.data[offset];
                        offset += 1;
                        if (w == 1) {
                            addr = ((u16)input.data[offset] << 8) | addr;
                            offset += 1;
                        }

                        instr->op1 = (Operand) {.kind = OpID_Register, .reg.id = RegisterID_AX, .reg.bytes = w ? 2 : 1};
                        instr->op2 = (Operand) {.kind = OpID_Memory, .mem.direct = true, .mem.disp = addr};
                    } break;

                    // mov(Accumulator_To_Memory)
                    case 0b1010001: {
                        instr->kind = InstrKind_mov_Accumulator_To_Memory;

                        u8 byte0bit0_literal = (input.data[offset] >> 1) & 0b01111111;
                        assert(byte0bit0_literal == 0b1010001);
                        u8 w = (input.data[offset] >> 0) & 0b00000001;
                        offset += 1;

                        u16 addr = input.data[offset];
                        offset += 1;
                        if (w == 1) {
                            addr = ((u16)input.data[offset] << 8) | addr;
                            offset += 1;
                        }

                        instr->op1 = (Operand) {.kind = OpID_Memory, .mem.direct = true, .mem.disp = addr};
                        instr->op2 = (Operand) {.kind = OpID_Register, .reg.id = RegisterID_AX, .reg.bytes = w ? 2 : 1};
                    } break;
                
                    default: assert(!"unimplemented"); break;
                }
            } break;

            // mov(Immediate_To_Register)
            case 0b1011: {
                instr->kind = InstrKind_mov_Immediate_To_Register;

                u8 byte0bit0_literal = (input.data[offset] >> 4) & 0b00001111;
                assert(byte0bit0_literal == 0b1011);
                u8 w = (input.data[offset] >> 3) & 0b00000001;
                u8 reg = (input.data[offset] >> 0) & 0b00000111;
                offset += 1;

                u16 data = input.data[offset];
                offset += 1;
                if (w == 1) {
                    data = ((u16)input.data[offset] << 8) | data;
                    offset += 1;
                }

                Operand regOp = {.kind = OpID_Register, .reg.id = w ? reg : reg % 4, .reg.bytes = w ? 2 : 1, .reg.offset = w == 0 && reg > 0b11};

                instr->op1 = regOp;
                instr->op2 = (Operand) {.kind = OpID_Immediate, .immediate.val = data, .immediate.bytes = w ? 2 : 1};
            } break;

            // mov(Immediate_To_RegisterMemory)
            case 0b1100: {
                instr->kind = InstrKind_mov_Immediate_To_RegisterMemory;

                u8 byte0bit0_literal = (input.data[offset] >> 1) & 0b01111111;
                assert(byte0bit0_literal == 0b1100011);
                u8 w = (input.data[offset] >> 0) & 0b00000001;
                offset += 1;

                u8 mod = (input.data[offset] >> 6) & 0b00000011;
                u8 byte1bit1_literal = (input.data[offset] >> 3) & 0b00000111;
                assert(byte1bit1_literal == 0b000);
                u8 r_m = (input.data[offset] >> 0) & 0b00000111;
                offset += 1;

                Operand rmOp = {};
                switch (mod) {
                    case 0b00: {
                        rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m};
                        if (r_m == 0b110) {
                            rmOp.mem.direct = true;
                            rmOp.mem.disp = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset]);
                            offset += 2;
                        }
                    } break;
                    case 0b01: {
                        rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m, .mem.disp = *((i8*)&input.data[offset])};
                        offset += 1;
                    } break;
                    case 0b10: {
                        rmOp = (Operand) {.kind = OpID_Memory, .mem.id = r_m, .mem.disp = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset])};
                        offset += 2;
                    } break;
                    case 0b11: {
                        rmOp = (Operand) {.kind = OpID_Register, .reg.id = w ? r_m : r_m % 4, .reg.bytes = w ? 2 : 1, .reg.offset = w == 0 && r_m > 0b11};
                    } break;
                }

                u16 data = input.data[offset];
                offset += 1;
                if (w == 1) {
                    data = ((u16)input.data[offset] << 8) | data;
                    offset += 1;
                }

                instr->op1 = rmOp;
                instr->op2 = (Operand) {.kind = OpID_Immediate, .immediate.val = data, .immediate.bytes = w ? 2 : 1};
            } break;
        
            default: assert(!"unimplemented"); break;
        }
        // @end
        // clang-format on

        assert(offset <= input.len);
    }

    InstrArray result = {instrs, instrCount};
    return result;
}

function void
addOpStr(prb_GrowingStr* gstr, Operand op) {
    switch (op.kind) {
        case OpID_Register: {
            char* regTable16[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
            char* regTable8[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
            assert(prb_arrayCount(regTable8) == prb_arrayCount(regTable16));
            char** regTable = regTable16;
            if (op.reg.bytes == 1) {
                regTable = regTable8;
            }
            i32 index = op.reg.id;
            if (op.reg.offset == 1) {
                assert(op.reg.bytes == 1);
                index += 4;
            }
            assert(index < prb_arrayCount(regTable8));
            prb_addStrSegment(gstr, "%s", regTable[index]);
        } break;

        case OpID_Memory: {
            if (op.mem.direct) {
                prb_addStrSegment(gstr, "[%d]", op.mem.disp);
            } else {
                char* memTable[] = {"bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx"};
                prb_addStrSegment(gstr, "[%s + %d]", memTable[op.mem.id], op.mem.disp);
            }
        } break;

        case OpID_Immediate: {
            char* sizes[] = {"byte", "word"};
            prb_addStrSegment(gstr, "%s %d", sizes[op.immediate.bytes - 1], op.immediate.val);
        } break;
    }
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

    InstrArray decodedArray = decode(arena, ref);

    prb_GrowingStr reincode = prb_beginStr(arena);
    prb_addStrSegment(&reincode, "bits 16\n");
    for (i32 instrInd = 0; instrInd < decodedArray.len; instrInd++) {
        Instr instr = decodedArray.ptr[instrInd];
        switch (instr.kind) {
            case InstrKind_mov_RegisterMemory_ToFrom_Register:
            case InstrKind_mov_Immediate_To_Register:
            case InstrKind_mov_Immediate_To_RegisterMemory:
            case InstrKind_mov_Memory_To_Accumulator:
            case InstrKind_mov_Accumulator_To_Memory: {
                prb_addStrSegment(&reincode, "mov ");
            } break;

            case InstrKind_add_RegisterMemory_With_Register_To_Either:
            case InstrKind_add_Immediate_To_RegisterMemory:
            case InstrKind_add_Immediate_To_Accumulator: {
                prb_addStrSegment(&reincode, "add ");
            } break;
        }

        addOpStr(&reincode, instr.op1);
        prb_addStrSegment(&reincode, ", ");
        addOpStr(&reincode, instr.op2);
        prb_addStrSegment(&reincode, "\n");
    }
    Str reincodeAsm = prb_endStr(&reincode);

    assert(prb_writeEntireFile(arena, decodeAsm, reincodeAsm.ptr, reincodeAsm.len));
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
