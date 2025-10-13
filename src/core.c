#include "core.h"

#include "inst.h"
#include "tick.h"
#include "arch.h"
#include "mem_map.h"
#include "common.h"

#include <stddef.h>
#include <assert.h>
#include <stdint.h>

/* ------------------------- helpers -------------------------- */

/* 32-bit wrap-safe add: returns (a + b) mod 2^32 regardless of sign */
static inline reg_t add32(reg_t a, int32_t b) {
    return (reg_t)((uint32_t)a + (uint32_t)b);
}

/* Little macros for bit slicing and sign-extension */
#define GETBITS(x,hi,lo) (((x) >> (lo)) & ((uint32_t) ((1u << ((hi)-(lo)+1)) - 1u)))
#define SEXT(val,bits)   ((int32_t)((int32_t)((uint32_t)(val) << (32-(bits))) >> (32-(bits))))

/* --------------------------- Fetch --------------------------- */
static inst_fields_t Core_fetch(Core *self) {
    /* fetch instruction according to self->arch_state.current_pc */
    byte_t inst_in_bytes[4] = {0};
    MemoryMap_generic_load(&self->mem_map, self->arch_state.current_pc, 4, inst_in_bytes);

    /* little-endian pack into raw */
    inst_fields_t ret = {0};
    ret.raw |= (reg_t)inst_in_bytes[0];
    ret.raw |= (reg_t)inst_in_bytes[1] << 8;
    ret.raw |= (reg_t)inst_in_bytes[2] << 16;
    ret.raw |= (reg_t)inst_in_bytes[3] << 24;
    return ret;
}

/* --------------------------- Decode -------------------------- */
static inst_enum_t Core_decode(Core *self, inst_fields_t inst_fields) {
    (void)self;
    reg_t opcode = inst_fields.raw & 0x7F;
    inst_enum_t ret = (inst_enum_t)0;

    switch (opcode) {
    case OP:       /* 0x33 */ ret = (inst_enum_t)OP;       break; // R-type
    case OP_IMM:   /* 0x13 */ ret = (inst_enum_t)OP_IMM;   break; // I-type ALU
    case LOAD:     /* 0x03 */ ret = (inst_enum_t)LOAD;     break;
    case STORE:    /* 0x23 */ ret = (inst_enum_t)STORE;    break;
    case BRANCH:   /* 0x63 */ ret = (inst_enum_t)BRANCH;   break;
    case JAL:                 ret = inst_jal;              break;
    case JALR:                ret = inst_jalr;             break;
    case AUIPC:               ret = inst_auipc;            break;
    case LUI:                 ret = inst_lui;              break;
    default:                  ret = (inst_enum_t)0;        break; // illegal/unused
    }
    return ret;
}

/* -------------------- Execute + Commit ----------------------- */
static void Core_execute(Core *self, inst_fields_t inst_fields, inst_enum_t inst_enum) {
    (void)inst_enum; /* we re-decode from raw below */

    /* default next PC = PC + 4 (mod 2^32) */
    self->new_pc = add32(self->arch_state.current_pc, 4);

    reg_t raw    = inst_fields.raw;
    reg_t opcode = GETBITS(raw, 6, 0);
    reg_t rd     = GETBITS(raw, 11, 7);
    reg_t funct3 = GETBITS(raw, 14, 12);
    reg_t rs1    = GETBITS(raw, 19, 15);
    reg_t rs2    = GETBITS(raw, 24, 20);
    reg_t funct7 = GETBITS(raw, 31, 25);

    reg_t *x = self->arch_state.x;
    reg_t pc = self->arch_state.current_pc;

    /* immediates */
    int32_t imm_i = SEXT(GETBITS(raw, 31, 20), 12);
    int32_t imm_s = SEXT(((GETBITS(raw, 31, 25) << 5) | GETBITS(raw, 11, 7)), 12);
    int32_t imm_b = SEXT(
        ((GETBITS(raw, 31,31) << 12) |
         (GETBITS(raw, 7,7)   << 11) |
         (GETBITS(raw, 30,25) << 5 ) |
         (GETBITS(raw, 11,8)  << 1 )), 13);
    reg_t   imm_u = (raw & 0xFFFFF000u); /* upper 20 bits, low 12 zero */
    int32_t imm_j = SEXT(
        ((GETBITS(raw, 31,31) << 20) |
         (GETBITS(raw, 19,12) << 12) |
         (GETBITS(raw, 20,20) << 11) |
         (GETBITS(raw, 30,21) << 1 )), 21);

    switch (opcode) {

    /* -------------------------- R-type (OP) -------------------------- */
    case OP: { /* 0x33 */
        reg_t v1 = x[rs1], v2 = x[rs2];
        reg_t res = 0;

        switch (funct3) {
        case 0x0: /* ADD/SUB */
            if (funct7 == 0x20) { /* SUB: unsigned wrap-around subtraction */
                res = (reg_t)((uint32_t)v1 - (uint32_t)v2);
            } else {               /* ADD: unsigned wrap-around addition */
                res = (reg_t)((uint32_t)v1 + (uint32_t)v2);
            }
            break;
        case 0x1: /* SLL  */
            res = v1 << (v2 & 31u);
            break;
        case 0x2: /* SLT  */
            res = ((int32_t)v1 < (int32_t)v2) ? 1u : 0u;
            break;
        case 0x3: /* SLTU */
            res = (v1 < v2) ? 1u : 0u;
            break;
        case 0x4: /* XOR  */
            res = v1 ^ v2;
            break;
        case 0x5: /* SRL/SRA */
            if (funct7 == 0x20) res = (reg_t)((int32_t)v1 >> (v2 & 31u)); /* SRA */
            else                 res = v1 >> (v2 & 31u);                  /* SRL */
            break;
        case 0x6: /* OR   */
            res = v1 | v2;
            break;
        case 0x7: /* AND  */
            res = v1 & v2;
            break;
        default:
            res = 0;
            break;
        }

        if (rd != 0 && rd < 32) x[rd] = res;
        break;
    }

    /* ------------------------ I-type (OP-IMM) ------------------------ */
    case OP_IMM: { /* 0x13 */
        reg_t v1 = x[rs1];
        reg_t res = 0;

        switch (funct3) {
        case 0x0: /* ADDI (wrap-around) */
            res = add32(v1, imm_i);
            break;
        case 0x2: /* SLTI  */
            res = ((int32_t)v1 < imm_i) ? 1u : 0u;
            break;
        case 0x3: /* SLTIU: compare as unsigned */
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
            if (GETBITS(raw, 31, 25) == 0x20) /* SRAI */
                res = (reg_t)((int32_t)v1 >> shamt);
            else                               /* SRLI */
                res = v1 >> shamt;
            break;
        }
        default:
            res = 0;
            break;
        }

        if (rd != 0 && rd < 32) x[rd] = res;
        break;
    }

    /* --------------------------- LOAD (I) ---------------------------- */
    case LOAD: { /* 0x03 */
        reg_t addr = add32(x[rs1], imm_i); /* wrap-safe */
        switch (funct3) {
        case 0x0: { /* LB */
            byte_t b[1];
            MemoryMap_generic_load(&self->mem_map, addr, 1, b);
            reg_t v = (reg_t)b[0];
            if (rd != 0 && rd < 32) x[rd] = (reg_t)SEXT(v & 0xFFu, 8);
            break;
        }
        case 0x1: { /* LH */
            byte_t b[2];
            MemoryMap_generic_load(&self->mem_map, addr, 2, b);
            reg_t v = (reg_t)b[0] | ((reg_t)b[1] << 8);
            if (rd != 0 && rd < 32) x[rd] = (reg_t)SEXT(v & 0xFFFFu, 16);
            break;
        }
        case 0x2: { /* LW */
            byte_t b[4];
            MemoryMap_generic_load(&self->mem_map, addr, 4, b);
            reg_t v = (reg_t)b[0] | ((reg_t)b[1] << 8) | ((reg_t)b[2] << 16) | ((reg_t)b[3] << 24);
            if (rd != 0 && rd < 32) x[rd] = v;
            break;
        }
        case 0x4: { /* LBU */
            byte_t b[1];
            MemoryMap_generic_load(&self->mem_map, addr, 1, b);
            reg_t v = (reg_t)b[0];
            if (rd != 0 && rd < 32) x[rd] = (v & 0xFFu);
            break;
        }
        case 0x5: { /* LHU */
            byte_t b[2];
            MemoryMap_generic_load(&self->mem_map, addr, 2, b);
            reg_t v = (reg_t)b[0] | ((reg_t)b[1] << 8);
            if (rd != 0 && rd < 32) x[rd] = (v & 0xFFFFu);
            break;
        }
        default:
            break;
        }
        break;
    }

    /* --------------------------- STORE (S) --------------------------- */
    case STORE: { /* 0x23 */
        reg_t addr = add32(x[rs1], imm_s); /* wrap-safe */
        reg_t v2   = x[rs2];

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
        default:
            break;
        }
        break;
    }

    /* -------------------------- BRANCH (B) --------------------------- */
    case BRANCH: { /* 0x63 */
        reg_t v1 = x[rs1], v2 = x[rs2];
        int take = 0;

        switch (funct3) {
        case 0x0: take = (v1 == v2); break;                          /* BEQ  */
        case 0x1: take = (v1 != v2); break;                          /* BNE  */
        case 0x4: take = ((int32_t)v1 <  (int32_t)v2); break;        /* BLT  */
        case 0x5: take = ((int32_t)v1 >= (int32_t)v2); break;        /* BGE  */
        case 0x6: take = (v1 <  v2); break;                          /* BLTU */
        case 0x7: take = (v1 >= v2); break;                          /* BGEU */
        default: break;
        }
        if (take) self->new_pc = add32(pc, imm_b);
        break;
    }

    /* ----------------------------- JAL ------------------------------- */
    case JAL: { /* 0x6F */
        if (rd != 0 && rd < 32) x[rd] = add32(pc, 4);
        self->new_pc = add32(pc, imm_j);
        break;
    }

    /* ----------------------------- JALR ------------------------------ */
    case JALR: { /* 0x67 */
        reg_t target = add32(x[rs1], imm_i);
        target &= ~1u; /* clear bit 0 */
        if (rd != 0 && rd < 32) x[rd] = add32(pc, 4);
        self->new_pc = target;
        break;
    }

    /* ----------------------------- AUIPC ----------------------------- */
    case AUIPC: { /* 0x17 */
        if (rd != 0 && rd < 32) x[rd] = add32(pc, (int32_t)imm_u);
        break;
    }

    /* ------------------------------ LUI ------------------------------ */
    case LUI: { /* 0x37 */
        if (rd != 0 && rd < 32) x[rd] = imm_u;
        break;
    }

    default:
        /* illegal/unsupported -> do nothing; PC already advanced by +4 */
        break;
    }

    /* enforce x0 == 0 */
    x[0] = 0;

    /* clean up local macros */
    #undef GETBITS
    #undef SEXT
}

/* -------------------------- PC update ------------------------- */
static void Core_update_pc(Core *self) {
    self->arch_state.current_pc = self->new_pc;
}

/* ---------------------------- Tick ---------------------------- */
DECLARE_TICK_TICK(Core) {
    Core *self_               = container_of(self, Core, super);
    inst_fields_t inst_fields = Core_fetch(self_);
    inst_enum_t inst_enum     = Core_decode(self_, inst_fields);
    Core_execute(self_, inst_fields, inst_enum);
    Core_update_pc(self_);
}

/* ------------------------ ctor / dtor ------------------------- */
void Core_ctor(Core *self) {
    assert(self != NULL);

    /* initialize memory map */
    MemoryMap_ctor(&self->mem_map);

    /* initialize base class (Tick) */
    Tick_ctor(&self->super);
    static struct TickVtbl const vtbl = { .tick = SIGNATURE_TICK_TICK(Core) };
    self->super.vtbl                  = &vtbl;
}

void Core_dtor(Core *self) {
    assert(self != NULL);
    MemoryMap_dtor(&self->mem_map);
}

int Core_add_device(Core *self, mmap_unit_t new_device) {
    return MemoryMap_add_device(&self->mem_map, new_device);
}
