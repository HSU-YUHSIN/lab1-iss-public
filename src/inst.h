#ifndef __RISCV_H__
#define __RISCV_H__

#include <stdint.h>

/* define 6 (4 base + 2 extended) types of formats of instructions in RV64I */
typedef union {
    struct {
        uint32_t opcode : 7;
        uint32_t rd : 5;
        uint32_t func3 : 3;
        uint32_t rs1 : 5;
        uint32_t rs2 : 5;
        uint32_t func7 : 7;
    } R_TYPE;
    struct {
        uint32_t opcode : 7;
        uint32_t rd : 5;
        uint32_t func3 : 3;
        uint32_t rs1 : 5;
        int32_t imm_11_0 : 12;
    } I_TYPE;
    struct {
        uint32_t opcode : 7;
        uint32_t imm_4_0 : 5;
        uint32_t func3 : 3;
        uint32_t rs1 : 5;
        uint32_t rs2 : 5;
        int32_t imm_11_5 : 7;
    } S_TYPE;
    struct {
        uint32_t opcode : 7;
        uint32_t rd : 5;
        int32_t imm_31_12 : 20;
    } U_TYPE;
    struct {
        uint32_t opcode : 7;
        uint32_t imm_11 : 1;
        uint32_t imm_4_1 : 4;
        uint32_t func3 : 3;
        uint32_t rs1 : 5;
        uint32_t rs2 : 5;
        uint32_t imm_10_5 : 6;
        int32_t imm_12 : 1; // highest imm
    } B_TYPE;
    struct {
        uint32_t opcode : 7;
        uint32_t rd : 5;
        uint32_t imm_19_12 : 8;
        uint32_t imm_11 : 1;
        uint32_t imm_10_1 : 10;
        int32_t imm_20 : 1; // highest imm
    } J_TYPE;
    uint32_t raw;
} inst_fields_t;

/*
 * The name without any suffix (e.g., _FUNC3) represents the OPCODE type
 */
typedef enum {
    /* 12 types in total */
    OP     = 0b0110011,
    OP_IMM = 0b0010011,
    LOAD   = 0b0000011,
    STORE  = 0b0100011,
    BRANCH = 0b1100011,
    JAL    = 0b1101111,
    JALR   = 0b1100111,
    AUIPC  = 0b0010111,
    LUI    = 0b0110111,
    // SYSTEM = 0b1110011,
} OPCODE;

typedef enum {
    ADD_SUB_FUNC3 = 0b000,
    SLL_FUNC3     = 0b001,
    SLT_FUNC3     = 0b010,
    SLTU_FUNC3    = 0b011,
    XOR_FUNC3     = 0b100,
    SRL_SRA_FUNC3 = 0b101,
    OR_FUNC3      = 0b110,
    AND_FUNC3     = 0b111,
} ARITHMETIC_FUNC3;

typedef enum {
    BEQ_FUNC3  = 0b000,
    BNE_FUNC3  = 0b001,
    BLT_FUNC3  = 0b100,
    BGE_FUNC3  = 0b101,
    BLTU_FUNC3 = 0b110,
    BGEU_FUNC3 = 0b111,
} BRANCH_FUNC3;

typedef enum {
    SB_FUNC3 = 0b000,
    SH_FUNC3 = 0b001,
    SW_FUNC3 = 0b010,
    SD_FUNC3 = 0b011,
} STORE_FUNC3;

typedef enum {
    LB_FUNC3  = 0b000,
    LH_FUNC3  = 0b001,
    LW_FUNC3  = 0b010,
    LD_FUNC3  = 0b011,
    LBU_FUNC3 = 0b100,
    LHU_FUNC3 = 0b101,
    LWU_FUNC3 = 0b110,
} LOAD_FUNC3;

/*
// Note that the SYSTEM type instructions use the I-Type format
typedef enum {
    ECALL_FUNC12 = 0b000000000000,
    EBREAK_FUNC12 = 0b000000000001,
} SYSTEM_FUNC12;
 */


/*
 * Enumerate 37 instructions in total
 * It should be generated in ISS_decode() stage
 */
typedef enum {
    // OP
    inst_add,
    inst_sub,
    inst_sll,
    inst_slt,
    inst_sltu,
    inst_xor,
    inst_srl,
    inst_sra,
    inst_or,
    inst_and,
    // OP-IMM
    inst_addi,
    inst_slti,
    inst_sltiu,
    inst_xori,
    inst_ori,
    inst_andi,
    inst_slli,
    inst_srli,
    inst_srai,
    // LOAD
    isnt_lb,
    inst_lh,
    inst_lw,
    inst_lbu,
    inst_lhu,
    // STORE
    inst_sb,
    inst_sh,
    inst_sw,
    // BRANCH
    inst_beq,
    inst_bne,
    inst_blt,
    inst_bge,
    inst_bltu,
    inst_bgeu,
    // JAL
    inst_jal,
    // JALR
    inst_jalr,
    // AUIPC
    inst_auipc,
    // LUI
    inst_lui,
} inst_enum_t;

#endif
