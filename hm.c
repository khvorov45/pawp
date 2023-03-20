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

typedef enum InstrKind {
    InstrKind_mov_RegisterMemory_ToFrom_Register,
    InstrKind_mov_Immediate_To_RegisterMemory,
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

typedef enum OpID {
    OpID_Register,
} OpID;

typedef struct Operand {
    OpID kind;
    union {
        struct {
            RegisterID id;
            u8         bytes;
            u8         offset;
        } reg;
    };
} Operand;

typedef struct Instr {
    InstrKind kind;
    union {
        u16 u;
        i16 i;
    } disp;
    Operand op1;
    Operand op2;
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
        u8 first6Bits = input.data[offset] >> 2;
        switch (first6Bits) {

            // mov(RegisterMemory_ToFrom_Register)
            case 0b100010: {
                instr->kind = InstrKind_mov_RegisterMemory_ToFrom_Register;

                u8 byte0bit0_literal = (input.data[offset] >> 2) & 0b00111111;
                u8 d = (input.data[offset] >> 1) & 0b00000001;
                u8 w = (input.data[offset] >> 0) & 0b00000001;
                offset += 1;

                u8 mod = (input.data[offset] >> 6) & 0b00000011;
                u8 reg = (input.data[offset] >> 3) & 0b00000111;
                u8 r_m = (input.data[offset] >> 0) & 0b00000111;
                offset += 1;

                switch (mod) {
                    case 0b00: {
                        if (r_m == 0b110) {
                            instr->disp.u = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset]);
                            offset += 2;
                        }
                    } break;
                    case 0b01: {
                        instr->disp.i = *((i8*)&input.data[offset]);
                        offset += 1;
                    } break;
                    case 0b10: {
                        instr->disp.u = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset]);
                        offset += 2;
                    } break;
                    case 0b11: break;
                }

                i32 regBytes = w ? 2 : 1;

                instr->op1 = (Operand) {.kind = OpID_Register, .reg.id = reg, .reg.bytes = regBytes};
                if (!w) {
                    instr->op1.reg.offset = reg > 0b11;
                    instr->op1.reg.id = reg % 4;
                }

                instr->op2 = (Operand) {.kind = OpID_Register, .reg.id = r_m, .reg.bytes = regBytes};
                if (!d) {
                    Operand temp = instr->op1;
                    instr->op1 = instr->op2;
                    instr->op2 = temp;
                }
            } break;

            // mov(Immediate_To_RegisterMemory)
            case 0b110001: {
                instr->kind = InstrKind_mov_Immediate_To_RegisterMemory;

                u8 byte0bit0_literal = (input.data[offset] >> 1) & 0b01111111;
                u8 w = (input.data[offset] >> 0) & 0b00000001;
                offset += 1;

                u8 mod = (input.data[offset] >> 6) & 0b00000011;
                u8 byte1bit1_literal = (input.data[offset] >> 3) & 0b00000111;
                u8 r_m = (input.data[offset] >> 0) & 0b00000111;
                offset += 1;

                switch (mod) {
                    case 0b00: {
                        if (r_m == 0b110) {
                            instr->disp.u = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset]);
                            offset += 2;
                        }
                    } break;
                    case 0b01: {
                        instr->disp.i = *((i8*)&input.data[offset]);
                        offset += 1;
                    } break;
                    case 0b10: {
                        instr->disp.u = (((u16)input.data[offset + 1]) << 8) | ((u16)input.data[offset]);
                        offset += 2;
                    } break;
                    case 0b11: break;
                }

                u16 data = input.data[offset];
                offset += 1;
                if (w == 1) {
                    data = ((u16)input.data[offset] << 8) | data;
                    offset += 1;
                }

                i32 regBytes = w ? 2 : 1;

                instr->op1 = (Operand) {.kind = OpID_Register, .reg.id = r_m, .reg.bytes = regBytes};
            } break;
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
            case InstrKind_mov_RegisterMemory_ToFrom_Register: {
                prb_addStrSegment(&reincode, "mov ");
                addOpStr(&reincode, instr.op1);
                prb_addStrSegment(&reincode, ", ");
                addOpStr(&reincode, instr.op2);
                prb_addStrSegment(&reincode, "\n");
            } break;
            case InstrKind_mov_Immediate_To_RegisterMemory: {
            } break;
        }
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
