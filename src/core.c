#include "core.h"

#include "inst.h"
#include "tick.h"
#include "arch.h"
#include "mem_map.h"
#include "common.h"

#include <stddef.h>
#include <assert.h>
#include <stdint.h>

/* -------- helpers -------- */
static inline int32_t sext32(uint32_t x, int bits) {
    int shift = 32 - bits;
    return (int32_t)(x << shift) >> shift;
}

static inline void write_x(Core *self, uint32_t rd, uint32_t val) {
    if (rd != 0 && rd < 32) self->arch_state.x[rd] = val;
}

/* -------- fetch -------- */
static inst_fields_t Core_fetch(Core *self) {
    /* fetch instruction according to self->arch_state.current_pc */
    byte_t inst_in_bytes[4] = {0};
    MemoryMap_generic_load(&self->mem_map, self->arch_state.current_pc, 4, inst_in_bytes);

    /* little-endian pack */
    inst_fields_t ret = (inst_fields_t){0};
    ret.raw |= (reg_t)inst_in_bytes[0];
    ret.raw |= (reg_t)inst_in_bytes[1] << 8;
    ret.raw |= (reg_t)inst_in_bytes[2] << 16;
    ret.raw |= (reg_t)inst_in_bytes[3] << 24;
    return ret;
}

/* -------- decode -------- */
static inst_enum_t Core_decode(Core *self, inst_fields_t inst_fields) {
    (void)self; /* unused */
    inst_enum_t ret = (inst_enum_t)0;

    /* helper locals */
    reg_t opcode = inst_fields.R_TYPE.opcode;
    reg_t func3  = inst_fields.R_TYPE.func3;
    reg_t func7  = inst_fields.R_TYPE.func7;

    switch (opcode) {
    case OP: { /* R-type */
        switch (func3) {
            case 0x0: ret = (func7 == 0x20) ? inst_sub : inst_add; break; /* sub or add */
            case 0x1: ret = inst_sll; break;
            case 0x2: ret = inst_slt; break;
            case 0x3: ret = inst_sltu; break;
            case 0x4: ret = inst_xor; break;
            case 0x5: ret = (func7 == 0x20) ? inst_sra : inst_srl; break;
            case 0x6: ret = inst_or; break;
            case 0x7: ret = inst_and; break;
        }
        break;
    }
    case OP_IMM: { /* I-type */
        switch (func3) {
            case 0x0 : ret = inst_addi; break;
            case 0x2 : ret = inst_slti; break;
            case 0x3 : ret = inst_sltiu; break;
            case 0x4 : ret = inst_xori; break;
            case 0x6 : ret = inst_ori; break;
            case 0x7 : ret = inst_andi; break;
            case 0x1 : ret = inst_slli; break;
            case 0x5 : ret = (func7 == 0x20) ? inst_srai : inst_srli; break; /* shamt checked elsewhere if needed */
        }
        break;
    }
    case LOAD: {
        switch (func3) {
            case 0x0: ret = inst_lb;  break;
            case 0x1: ret = inst_lh;  break;
            case 0x2: ret = inst_lw;  break;
            case 0x4: ret = inst_lbu; break;
            case 0x5: ret = inst_lhu; break;
        }
        break;
    }
    case STORE: {
        switch (func3) {
            case 0x0: ret = inst_sb; break;
            case 0x1: ret = inst_sh; break;
            case 0x2: ret = inst_sw; break;
        }
        break;
    }
    case BRANCH: {
        switch (func3) {
            case 0x0: ret = inst_beq;  break;
            case 0x1: ret = inst_bne;  break;
            case 0x4: ret = inst_blt;  break;
            case 0x5: ret = inst_bge;  break;
            case 0x6: ret = inst_bltu; break;
            case 0x7: ret = inst_bgeu; break;
        }
        break;
    }
    case JAL:   ret = inst_jal;   break;
    case JALR:  ret = inst_jalr;  break;
    case AUIPC: ret = inst_auipc; break;
    case LUI:   ret = inst_lui;   break;
    default: /* leave as 0 (no-op / unknown) */ break;
    }

    return ret;
}

/* -------- execute+commit -------- */
static void Core_execute(Core *self, inst_fields_t f, inst_enum_t e) {
    /* default next PC */
    self->new_pc = self->arch_state.current_pc + 4;

    uint32_t rs1 = f.R_TYPE.rs1;
    uint32_t rs2 = f.R_TYPE.rs2;
    uint32_t rd  = f.R_TYPE.rd;
    uint32_t x1  = self->arch_state.x[rs1];
    uint32_t x2  = self->arch_state.x[rs2];

    switch (e) {
        /* R-type ALU */
        case inst_add: write_x(self, rd, x1 + x2); break;
        case inst_sub: write_x(self, rd, x1 - x2); break;
        case inst_sll: write_x(self, rd, x1 << (x2 & 31)); break;
        case inst_slt: write_x(self, rd, (int32_t)x1 < (int32_t)x2); break;
        case inst_sltu: write_x(self, rd, x1 < x2); break;
        case inst_xor: write_x(self, rd, x1 ^ x2); break;
        case inst_srl: write_x(self, rd, x1 >> (x2 & 31)); break;
        case inst_sra: write_x(self, rd, (uint32_t)((int32_t)x1 >> (x2 & 31))); break;
        case inst_or:  write_x(self, rd, x1 | x2); break;
        case inst_and: write_x(self, rd, x1 & x2); break;

        /* I-type ALU */
        case inst_addi: { int32_t imm = sext32(f.I_TYPE.imm, 12); write_x(self, rd, x1 + imm); break; }
        case inst_slti: { int32_t imm = sext32(f.I_TYPE.imm, 12); write_x(self, rd, (int32_t)x1 < imm); break; }
        case inst_sltiu:{ int32_t imm = sext32(f.I_TYPE.imm, 12); write_x(self, rd, x1 < (uint32_t)imm); break; }
        case inst_xori: { int32_t imm = sext32(f.I_TYPE.imm, 12); write_x(self, rd, x1 ^ (uint32_t)imm); break; }
        case inst_ori:  { int32_t imm = sext32(f.I_TYPE.imm, 12); write_x(self, rd, x1 | (uint32_t)imm); break; }
        case inst_andi: { int32_t imm = sext32(f.I_TYPE.imm, 12); write_x(self, rd, x1 & (uint32_t)imm); break; }
        case inst_slli: { uint32_t sh = f.I_TYPE.imm & 0x1F; write_x(self, rd, x1 << sh); break; }
        case inst_srli: { uint32_t sh = f.I_TYPE.imm & 0x1F; write_x(self, rd, x1 >> sh); break; }
        case inst_srai: { uint32_t sh = f.I_TYPE.imm & 0x1F; write_x(self, rd, (uint32_t)((int32_t)x1 >> sh)); break; }

        /* LOAD */
        case inst_lb: case inst_lh: case inst_lw:
        case inst_lbu: case inst_lhu: {
            int32_t imm = sext32(f.I_TYPE.imm, 12);
            uint32_t addr = x1 + imm;
            byte_t buf[4] = {0};

            switch (e) {
                case inst_lb:
                    MemoryMap_generic_load(&self->mem_map, addr, 1, buf);
                    write_x(self, rd, (uint32_t)sext32((uint32_t)(uint8_t)buf[0], 8));
                    break;
                case inst_lbu:
                    MemoryMap_generic_load(&self->mem_map, addr, 1, buf);
                    write_x(self, rd, (uint32_t)(uint8_t)buf[0]);
                    break;
                case inst_lh:
                    MemoryMap_generic_load(&self->mem_map, addr, 2, buf);
                    write_x(self, rd, (uint32_t)sext32(
                        (uint32_t)((uint16_t)(uint8_t)buf[0] | ((uint16_t)(uint8_t)buf[1] << 8)), 16));
                    break;
                case inst_lhu:
                    MemoryMap_generic_load(&self->mem_map, addr, 2, buf);
                    write_x(self, rd, (uint32_t)((uint16_t)(uint8_t)buf[0] |
                                                 ((uint16_t)(uint8_t)buf[1] << 8)));
                    break; /* FIX: avoid fallthrough */
                case inst_lw:
                    MemoryMap_generic_load(&self->mem_map, addr, 4, buf);
                    write_x(self, rd,
                        ((uint32_t)(uint8_t)buf[0])        |
                        ((uint32_t)(uint8_t)buf[1] << 8)   |
                        ((uint32_t)(uint8_t)buf[2] << 16)  |
                        ((uint32_t)(uint8_t)buf[3] << 24));
                    break;
                default: break;
            }
            break;
        }

        /* STORE */
        case inst_sb: case inst_sh: case inst_sw: {
            int32_t imm = sext32(((f.S_TYPE.imm_hi << 5) | f.S_TYPE.imm_lo), 12);
            uint32_t addr = x1 + imm;
            byte_t buf[4];

            switch (e) {
                case inst_sb:
                    buf[0] = (byte_t)(x2 & 0xFF);
                    MemoryMap_generic_store(&self->mem_map, addr, 1, buf);
                    break;
                case inst_sh:
                    buf[0] = (byte_t)(x2 & 0xFF);
                    buf[1] = (byte_t)((x2 >> 8) & 0xFF);
                    MemoryMap_generic_store(&self->mem_map, addr, 2, buf);
                    break;
                case inst_sw:
                    buf[0] = (byte_t)(x2 & 0xFF);
                    buf[1] = (byte_t)((x2 >> 8) & 0xFF);
                    buf[2] = (byte_t)((x2 >> 16) & 0xFF);
                    buf[3] = (byte_t)((x2 >> 24) & 0xFF);
                    MemoryMap_generic_store(&self->mem_map, addr, 4, buf);
                    break;
                default: break;
            }
            break;
        }

        /* BRANCH */
        case inst_beq: case inst_bne: case inst_blt:
        case inst_bge: case inst_bltu: case inst_bgeu: {
            int32_t imm = sext32(
                (f.B_TYPE.imm_12 << 12) |
                (f.B_TYPE.imm_11 << 11) |
                (f.B_TYPE.imm_10_5 << 5) |
                (f.B_TYPE.imm_4_1  << 1), 13);

            int take = 0;
            switch (e) {
                case inst_beq:  take = (x1 == x2); break;
                case inst_bne:  take = (x1 != x2); break;
                case inst_blt:  take = ((int32_t)x1 <  (int32_t)x2); break;
                case inst_bge:  take = ((int32_t)x1 >= (int32_t)x2); break;
                case inst_bltu: take = (x1 < x2); break;
                case inst_bgeu: take = (x1 >= x2); break;
                default: break;
            }
            if (take) self->new_pc = self->arch_state.current_pc + imm;
            break;
        }

        /* JUMPS */
        case inst_jal: {
            int32_t imm = sext32(
                (f.J_TYPE.imm_20     << 20) |
                (f.J_TYPE.imm_19_12  << 12) |
                (f.J_TYPE.imm_11     << 11) |
                (f.J_TYPE.imm_10_1   << 1), 21);
            write_x(self, rd, self->arch_state.current_pc + 4);
            self->new_pc = self->arch_state.current_pc + imm;
            break;
        }
        case inst_jalr: {
            int32_t imm = sext32(f.I_TYPE.imm, 12);
            uint32_t target = (x1 + imm) & ~1u;
            write_x(self, rd, self->arch_state.current_pc + 4);
            self->new_pc = target;
            break;
        }

        /* U-type */
        case inst_lui:
            write_x(self, rd, (f.U_TYPE.imm_31_12 << 12));
            break;
        case inst_auipc:
            write_x(self, rd, self->arch_state.current_pc + (f.U_TYPE.imm_31_12 << 12));
            break;

        default:
            /* unknown/none → do nothing */
            break;
    }

    /* x0 hard-wired to zero */
    self->arch_state.x[0] = 0;
}

/* -------- commit PC -------- */
static void Core_update_pc(Core *self) {
    self->arch_state.current_pc = self->new_pc;
}

/* -------- tick -------- */
DECLARE_TICK_TICK(Core) {
    Core *self_               = container_of(self, Core, super);
    inst_fields_t inst_fields = Core_fetch(self_);
    inst_enum_t  inst_enum    = Core_decode(self_, inst_fields);
    Core_execute(self_, inst_fields, inst_enum);
    Core_update_pc(self_);
}

/* -------- ctor/dtor -------- */
void Core_ctor(Core *self) {
    assert(self != NULL);
    MemoryMap_ctor(&self->mem_map);

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
