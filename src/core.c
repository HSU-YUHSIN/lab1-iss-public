#include "core.h"

#include "inst.h"
#include "tick.h"
#include "arch.h"
#include "mem_map.h"
#include "common.h"

#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

/* ================================================================
 *                  TRACE: ENABLED BY DEFAULT
 *   (No CMake flags required; comment out the define to mute.)
 * ================================================================ */
#define ISS_TRACE 1

#if ISS_TRACE
  #define TRACE(fmt, ...) do { fprintf(stderr, "[TRACE] " fmt, ##__VA_ARGS__); fflush(stderr); } while (0)
#else
  #define TRACE(fmt, ...) do{}while(0)
#endif

/* Sanity: RV32 expects 32-bit reg_t */
typedef char static_assert_reg_t_is_32[(sizeof(reg_t) == 4) ? 1 : -1];

/* --------------------------- helpers --------------------------- */

/* 32-bit wrap-safe add: (a + b) mod 2^32 regardless of sign */
static inline reg_t add32(reg_t a, int32_t b) {
    return (reg_t)((uint32_t)a + (uint32_t)b);
}

/* bit slicing & sign-extension; operate on uint32_t explicitly */
#define GETBITS32(x,hi,lo) ( ((uint32_t)(x) >> (lo)) & (uint32_t)((1u << ((hi)-(lo)+1)) - 1u) )
#define SEXT(val,bits)     ((int32_t)((int32_t)((uint32_t)(val) << (32-(bits))) >> (32-(bits))))

/* ----------------------------- Fetch ---------------------------- */
static inst_fields_t Core_fetch(Core *self) {
    byte_t inst_in_bytes[4] = {0};
    MemoryMap_generic_load(&self->mem_map, self->arch_state.current_pc, 4, inst_in_bytes);

    inst_fields_t ret = (inst_fields_t){0};
    /* little-endian pack -> raw */
    ret.raw |= (reg_t)inst_in_bytes[0];
    ret.raw |= (reg_t)inst_in_bytes[1] << 8;
    ret.raw |= (reg_t)inst_in_bytes[2] << 16;
    ret.raw |= (reg_t)inst_in_bytes[3] << 24;
    return ret;
}

/* ----------------------------- Decode --------------------------- */
static inst_enum_t Core_decode(Core *self, inst_fields_t inst_fields) {
    (void)self;
    uint32_t raw32 = (uint32_t)inst_fields.raw;
    uint32_t opcode = GETBITS32(raw32, 6, 0);
    inst_enum_t ret = (inst_enum_t)0;

    switch (opcode) {
    case OP:       ret = (inst_enum_t)OP;       break; // 0x33 R-type
    case OP_IMM:   ret = (inst_enum_t)OP_IMM;   break; // 0x13 I-type ALU
    case LOAD:     ret = (inst_enum_t)LOAD;     break; // 0x03
    case STORE:    ret = (inst_enum_t)STORE;    break; // 0x23
    case BRANCH:   ret = (inst_enum_t)BRANCH;   break; // 0x63
    case JAL:      ret = inst_jal;              break; // 0x6F
    case JALR:     ret = inst_jalr;             break; // 0x67
    case AUIPC:    ret = inst_auipc;            break; // 0x17
    case LUI:      ret = inst_lui;              break; // 0x37
    default:       ret = (inst_enum_t)0;        break; // illegal/unused
    }
    return ret;
}

/* ------------------------- Execute + Commit ---------------------- */
static void Core_execute(Core *self, inst_fields_t inst_fields, inst_enum_t inst_enum) {
    (void)inst_enum; /* re-decode from raw for simplicity */

    reg_t pc = self->arch_state.current_pc;

    /* default next PC = PC + 4 (mod 2^32) */
    self->new_pc = add32(pc, 4);

    /* Always work from a uint32_t copy for field extraction */
    uint32_t raw32 = (uint32_t)inst_fields.raw;
    uint32_t opcode = GETBITS32(raw32, 6, 0);
    uint32_t rd     = GETBITS32(raw32, 11, 7);
    uint32_t funct3 = GETBITS32(raw32, 14, 12);
    uint32_t rs1    = GETBITS32(raw32, 19, 15);
    uint32_t rs2    = GETBITS32(raw32, 24, 20);
    uint32_t funct7 = GETBITS32(raw32, 31, 25);

    reg_t *x = self->arch_state.x;

    /* immediates (RV32I) from raw32 */
    int32_t imm_i = SEXT(GETBITS32(raw32, 31, 20), 12);
    int32_t imm_s = SEXT(((GETBITS32(raw32,31,25) << 5) | GETBITS32(raw32,11,7)), 12);
    int32_t imm_b = SEXT(((GETBITS32(raw32,31,31) << 12) |
                          (GETBITS32(raw32, 7, 7) << 11) |
                          (GETBITS32(raw32,30,25) << 5 ) |
                          (GETBITS32(raw32,11, 8) << 1 )), 13);
    reg_t   imm_u = (reg_t)(raw32 & 0xFFFFF000u);
    int32_t imm_j = SEXT(((GETBITS32(raw32,31,31) << 20) |
                          (GETBITS32(raw32,19,12) << 12) |
                          (GETBITS32(raw32,20,20) << 11) |
                          (GETBITS32(raw32,30,21) << 1 )), 21);

    /* one-line opcode trace to confirm we’re executing this file */
    TRACE("pc=%08x raw=%08x opc=%02x\n", (unsigned)pc, (unsigned)raw32, (unsigned)opcode);

    switch (opcode) {

    /* --------------------------- R-type (OP) -------------------------- */
    case OP: {
        reg_t v1 = x[rs1], v2 = x[rs2];
        reg_t res = 0;

        switch (funct3) {
        case 0x0: /* ADD/SUB (wrap) */
            if (funct7 == 0x20) res = (reg_t)((uint32_t)v1 - (uint32_t)v2); /* SUB */
            else                 res = (reg_t)((uint32_t)v1 + (uint32_t)v2); /* ADD */
            break;
        case 0x1: /* SLL */
            res = v1 << (v2 & 31u);
            break;
        case 0x2: /* SLT */
            res = ((int32_t)v1 < (int32_t)v2) ? 1u : 0u;
            break;
        case 0x3: /* SLTU */
            res = ((uint32_t)v1 < (uint32_t)v2) ? 1u : 0u;
            break;
        case 0x4: /* XOR */
            res = v1 ^ v2;
            break;
        case 0x5: /* SRL/SRA */
            if (funct7 == 0x20) res = (reg_t)((int32_t)v1   >> (v2 & 31u));  /* SRA (arith) */
            else                 res = (reg_t)((uint32_t)v1 >> (v2 & 31u));  /* SRL (logical) */
            break;
        case 0x6: /* OR  */
            res = v1 | v2;
            break;
        case 0x7: /* AND */
            res = v1 & v2;
            break;
        default: break;
        }

        if (rd != 0 && rd < 32) x[rd] = res;
        break;
    }

    /* ------------------------- I-type (OP-IMM) ------------------------ */
    case OP_IMM: {
        reg_t v1 = x[rs1];
        reg_t res = 0;

        switch (funct3) {
        case 0x0: { /* ADDI (wrap) */
            reg_t old = v1;
            res = add32(v1, imm_i);
            if (rd == 3) /* gp */
                TRACE("       ADDI  gp: old=%08x imm=%d -> new=%08x\n",
                      (unsigned)old, (int)imm_i, (unsigned)res);
            break;
        }
        case 0x2: /* SLTI  */
            res = ((int32_t)v1 < imm_i) ? 1u : 0u;
            break;
        case 0x3: /* SLTIU */
            res = ((uint32_t)v1 < (uint32_t)imm_i) ? 1u : 0u;
            break;
        case 0x4: /* XORI  */
            res = v1 ^ (reg_t)imm_i;
            break;
        case 0x6: /* ORI   */
            res = v1 | (reg_t)imm_i;
            break;
        case 0x7: /* ANDI  */
            res = v1 & (reg_t)imm_i;
            break;
        case 0x1: { /* SLLI */
            reg_t shamt = (reg_t)(imm_i & 0x1F);
            res = v1 << shamt;
            break;
        }
        case 0x5: { /* SRLI/SRAI */
            reg_t shamt = (reg_t)(imm_i & 0x1F);
            if (GETBITS32(raw32,31,25) == 0x20) res = (reg_t)((int32_t)v1   >> shamt); /* SRAI */
            else                                 res = (reg_t)((uint32_t)v1 >> shamt); /* SRLI (logical) */
            break;
        }
        default: break;
        }

        if (rd != 0 && rd < 32) x[rd] = res;
        break;
    }

    /* ---------------------------- LOAD (I) ---------------------------- */
    case LOAD: {
        reg_t addr = add32(x[rs1], imm_i);
        switch (funct3) {
        case 0x0: { /* LB */
            byte_t b[1];
            MemoryMap_generic_load(&self->mem_map, addr, 1, b);
            if (rd != 0 && rd < 32) x[rd] = (reg_t)SEXT((uint32_t)b[0], 8);
            break;
        }
        case 0x1: { /* LH */
            byte_t b[2];
            MemoryMap_generic_load(&self->mem_map, addr, 2, b);
            uint32_t v = (uint32_t)b[0] | ((uint32_t)b[1] << 8);
            if (rd != 0 && rd < 32) x[rd] = (reg_t)SEXT(v, 16);
            break;
        }
        case 0x2: { /* LW */
            byte_t b[4];
            MemoryMap_generic_load(&self->mem_map, addr, 4, b);
            uint32_t v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
                         ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
            if (rd != 0 && rd < 32) x[rd] = (reg_t)v;
            break;
        }
        case 0x4: { /* LBU */
            byte_t b[1];
            MemoryMap_generic_load(&self->mem_map, addr, 1, b);
            if (rd != 0 && rd < 32) x[rd] = (reg_t)((uint32_t)b[0] & 0xFFu);
            break;
        }
        case 0x5: { /* LHU */
            byte_t b[2];
            MemoryMap_generic_load(&self->mem_map, addr, 2, b);
            uint32_t v = (uint32_t)b[0] | ((uint32_t)b[1] << 8);
            if (rd != 0 && rd < 32) x[rd] = (reg_t)(v & 0xFFFFu);
            break;
        }
        default: break;
        }
        break;
    }

    /* ---------------------------- STORE (S) --------------------------- */
    case STORE: {
        reg_t base = x[rs1];
        reg_t addr = add32(base, imm_s);
        reg_t v2   = x[rs2];

        TRACE("       STORE rs1=x%u=%08x imm_s=%d -> addr=%08x funct3=%u\n",
              (unsigned)rs1, (unsigned)base, (int)imm_s, (unsigned)addr, (unsigned)funct3);

        switch (funct3) {
        case 0x0: { /* SB */
            byte_t b[1] = { (byte_t)(v2 & 0xFF) };
            MemoryMap_generic_store(&self->mem_map, addr, 1, b);
            break;
        }
        case 0x1: { /* SH */
            byte_t b[2] = {
                (byte_t)(v2 & 0xFF),
                (byte_t)((v2 >> 8) & 0xFF)
            };
            MemoryMap_generic_store(&self->mem_map, addr, 2, b);
            break;
        }
        case 0x2: { /* SW */
            byte_t b[4] = {
                (byte_t)(v2 & 0xFF),
                (byte_t)((v2 >> 8) & 0xFF),
                (byte_t)((v2 >> 16) & 0xFF),
                (byte_t)((v2 >> 24) & 0xFF)
            };
            MemoryMap_generic_store(&self->mem_map, addr, 4, b);
            break;
        }
        default: break;
        }
        break;
    }

    /* --------------------------- BRANCH (B) --------------------------- */
    case BRANCH: {
        reg_t v1 = x[rs1], v2 = x[rs2];
        int take = 0;
        switch (funct3) {
        case 0x0: take = (v1 == v2); break;                           /* BEQ  */
        case 0x1: take = (v1 != v2); break;                           /* BNE  */
        case 0x4: take = ((int32_t)v1 <  (int32_t)v2); break;         /* BLT  */
        case 0x5: take = ((int32_t)v1 >= (int32_t)v2); break;         /* BGE  */
        case 0x6: take = ((uint32_t)v1 <  (uint32_t)v2); break;       /* BLTU */
        case 0x7: take = ((uint32_t)v1 >= (uint32_t)v2); break;       /* BGEU */
        default: break;
        }
        if (take) self->new_pc = add32(pc, imm_b);
        break;
    }

    /* ------------------------------ JAL ------------------------------- */
    case JAL: {
        if (rd != 0 && rd < 32) x[rd] = add32(pc, 4);
        self->new_pc = add32(pc, imm_j);
        break;
    }

    /* ------------------------------ JALR ------------------------------ */
    case JALR: {
        reg_t target = add32(x[rs1], imm_i);
        target &= ~1u; /* clear bit 0 */
        if (rd != 0 && rd < 32) x[rd] = add32(pc, 4);
        self->new_pc = target;
        break;
    }

    /* ------------------------------ AUIPC ----------------------------- */
    case AUIPC: {
        reg_t res = add32(pc, (int32_t)imm_u);
        if (rd == 3) /* gp */
            TRACE("       AUIPC gp: pc=%08x imm_u=%08x -> new=%08x\n",
                  (unsigned)pc, (unsigned)imm_u, (unsigned)res);
        if (rd != 0 && rd < 32) x[rd] = res;
        break;
    }

    /* ------------------------------- LUI ------------------------------ */
    case LUI: {
        if (rd == 3) /* gp */
            TRACE("       LUI   gp: imm_u=%08x -> new=%08x\n",
                  (unsigned)imm_u, (unsigned)imm_u);
        if (rd != 0 && rd < 32) x[rd] = imm_u;
        break;
    }

    default:
        /* illegal/unsupported → NOP; PC already advanced by +4 */
        break;
    }

    /* enforce x0 == 0 */
    x[0] = 0;
}

/* ---------------------------- PC update --------------------------- */
static void Core_update_pc(Core *self) {
    self->arch_state.current_pc = self->new_pc;
}

/* ------------------------------- Tick ------------------------------ */
DECLARE_TICK_TICK(Core) {
    Core *self_               = container_of(self, Core, super);
    inst_fields_t inst_fields = Core_fetch(self_);
    inst_enum_t inst_enum     = Core_decode(self_, inst_fields);
    Core_execute(self_, inst_fields, inst_enum);
    Core_update_pc(self_);
}

/* ---------------------------- ctor / dtor --------------------------- */
void Core_ctor(Core *self) {
    assert(self != NULL);

    /* initialize memory map (no devices yet) */
    MemoryMap_ctor(&self->mem_map);

    /* init base class (Tick) */
    Tick_ctor(&self->super);
    static struct TickVtbl const vtbl = { .tick = SIGNATURE_TICK_TICK(Core) };
    self->super.vtbl = &vtbl;
}

void Core_dtor(Core *self) {
    assert(self != NULL);
    MemoryMap_dtor(&self->mem_map);
}

int Core_add_device(Core *self, mmap_unit_t new_device) {
    return MemoryMap_add_device(&self->mem_map, new_device);
}
