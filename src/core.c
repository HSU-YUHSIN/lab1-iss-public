#include "core.h"

#include "inst.h"
#include "tick.h"
#include "arch.h"
#include "mem_map.h"
#include "common.h"

#include <stddef.h>
#include <assert.h>

// Helper for sign extension
static inline reg_t sign_extend(reg_t value, int bits) {
    reg_t mask = 1U << (bits - 1);
    return (value ^ mask) - mask;
}

// ISS Fetch stage
static inst_fields_t Core_fetch(Core *self) {
    byte_t inst_in_bytes[4] = {};
    MemoryMap_generic_load(&self->mem_map, self->arch_state.current_pc, 4, inst_in_bytes);
    inst_fields_t ret = {};
    ret.raw |= (reg_t)inst_in_bytes[0];
    ret.raw |= (reg_t)inst_in_bytes[1] << 8;
    ret.raw |= (reg_t)inst_in_bytes[2] << 16;
    ret.raw |= (reg_t)inst_in_bytes[3] << 24;
    return ret;
}

// ISS decode stage
static inst_enum_t Core_decode(Core *self, inst_fields_t inst_fields) {
    inst_enum_t ret = inst_invalid;

    reg_t opcode = inst_fields.R_TYPE.opcode;
    reg_t func3  = inst_fields.R_TYPE.func3;
    reg_t func7  = inst_fields.R_TYPE.func7;

    // decode main opcodes
    switch (opcode) {
    case OP: // R-type (add, sub, etc)
        switch (func3) {
        case 0x0: ret = (func7 == 0x00) ? inst_add : inst_sub; break;
        case 0x1: ret = inst_sll; break;
        case 0x2: ret = inst_slt; break;
        case 0x3: ret = inst_sltu; break;
        case 0x4: ret = inst_xor; break;
        case 0x5: ret = (func7 == 0x00) ? inst_srl : inst_sra; break;
        case 0x6: ret = inst_or; break;
        case 0x7: ret = inst_and; break;
        }
        break;
    case OP_IMM: // I-type immediate arithmetic
        switch (func3) {
        case 0x0: ret = inst_addi; break;
        case 0x1: ret = inst_slli; break;
        case 0x2: ret = inst_slti; break;
        case 0x3: ret = inst_sltiu; break;
        case 0x4: ret = inst_xori; break;
        case 0x5: ret = (func7 == 0x00) ? inst_srli : inst_srai; break;
        case 0x6: ret = inst_ori; break;
        case 0x7: ret = inst_andi; break;
        }
        break;
    case LOAD:
        switch (func3) {
        case 0x0: ret = inst_lb; break;
        case 0x1: ret = inst_lh; break;
        case 0x2: ret = inst_lw; break;
        case 0x4: ret = inst_lbu; break;
        case 0x5: ret = inst_lhu; break;
        }
        break;
    case STORE:
        switch (func3) {
        case 0x0: ret = inst_sb; break;
        case 0x1: ret = inst_sh; break;
        case 0x2: ret = inst_sw; break;
        }
        break;
    case BRANCH:
        switch (func3) {
        case 0x0: ret = inst_beq; break;
        case 0x1: ret = inst_bne; break;
        case 0x4: ret = inst_blt; break;
        case 0x5: ret = inst_bge; break;
        case 0x6: ret = inst_bltu; break;
        case 0x7: ret = inst_bgeu; break;
        }
        break;
    case JAL:
        ret = inst_jal;
        break;
    case JALR:
        ret = inst_jalr;
        break;
    case AUIPC:
        ret = inst_auipc;
        break;
    case LUI:
        ret = inst_lui;
        break;
    }
    return ret;
}

// ISS execute and commit stage (two-in-one)
static void Core_execute(Core *self, inst_fields_t inst_fields, inst_enum_t inst_enum) {
    reg_t *reg = self->arch_state.gpr;
    reg_t pc = self->arch_state.current_pc;
    reg_t rs1 = inst_fields.R_TYPE.rs1;
    reg_t rs2 = inst_fields.R_TYPE.rs2;
    reg_t rd = inst_fields.R_TYPE.rd;
    reg_t imm_i = sign_extend(inst_fields.I_TYPE.imm, 12);
    reg_t imm_s = sign_extend((inst_fields.S_TYPE.imm4_0 | (inst_fields.S_TYPE.imm11_5 << 5)), 12);
    reg_t imm_b = sign_extend((inst_fields.B_TYPE.imm11 << 11) | (inst_fields.B_TYPE.imm10_5 << 5) |
                             (inst_fields.B_TYPE.imm4_1 << 1) | (inst_fields.B_TYPE.imm12 << 12), 13);
    reg_t imm_u = inst_fields.U_TYPE.imm << 12;
    reg_t imm_j = sign_extend((inst_fields.J_TYPE.imm20 << 20) | (inst_fields.J_TYPE.imm10_1 << 1) |
                             (inst_fields.J_TYPE.imm11 << 11) | (inst_fields.J_TYPE.imm19_12 << 12), 21);

    self->new_pc = pc + 4; // default

    switch (inst_enum) {
    // OP
    case inst_add:   reg[rd] = reg[rs1] + reg[rs2]; break;
    case inst_sub:   reg[rd] = reg[rs1] - reg[rs2]; break;
    case inst_sll:   reg[rd] = reg[rs1] << (reg[rs2] & 0x1F); break;
    case inst_slt:   reg[rd] = (int32_t)reg[rs1] < (int32_t)reg[rs2]; break;
    case inst_sltu:  reg[rd] = reg[rs1] < reg[rs2]; break;
    case inst_xor:   reg[rd] = reg[rs1] ^ reg[rs2]; break;
    case inst_srl:   reg[rd] = reg[rs1] >> (reg[rs2] & 0x1F); break;
    case inst_sra:   reg[rd] = ((int32_t)reg[rs1]) >> (reg[rs2] & 0x1F); break;
    case inst_or:    reg[rd] = reg[rs1] | reg[rs2]; break;
    case inst_and:   reg[rd] = reg[rs1] & reg[rs2]; break;

    // OPIMM
    case inst_addi:  reg[rd] = reg[rs1] + imm_i; break;
    case inst_slli:  reg[rd] = reg[rs1] << (imm_i & 0x1F); break;
    case inst_slti:  reg[rd] = (int32_t)reg[rs1] < (int32_t)imm_i; break;
    case inst_sltiu: reg[rd] = reg[rs1] < imm_i; break;
    case inst_xori:  reg[rd] = reg[rs1] ^ imm_i; break;
    case inst_srli:  reg[rd] = reg[rs1] >> (imm_i & 0x1F); break;
    case inst_srai:  reg[rd] = ((int32_t)reg[rs1]) >> (imm_i & 0x1F); break;
    case inst_ori:   reg[rd] = reg[rs1] | imm_i; break;
    case inst_andi:  reg[rd] = reg[rs1] & imm_i; break;

    // LOAD
    case inst_lb: {
        byte_t b;
        MemoryMap_generic_load(&self->mem_map, reg[rs1] + imm_i, 1, &b);
        reg[rd] = sign_extend(b, 8);
        break;
    }
    case inst_lh: {
        byte_t buf[2];
        MemoryMap_generic_load(&self->mem_map, reg[rs1] + imm_i, 2, buf);
        reg[rd] = sign_extend(buf[0] | (buf[1] << 8), 16);
        break;
    }
    case inst_lw: {
        byte_t buf[4];
        MemoryMap_generic_load(&self->mem_map, reg[rs1] + imm_i, 4, buf);
        reg[rd] = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
        break;
    }
    case inst_lbu: {
        byte_t b;
        MemoryMap_generic_load(&self->mem_map, reg[rs1] + imm_i, 1, &b);
        reg[rd] = b;
        break;
    }
    case inst_lhu: {
        byte_t buf[2];
        MemoryMap_generic_load(&self->mem_map, reg[rs1] + imm_i, 2, buf);
        reg[rd] = buf[0] | (buf[1] << 8);
        break;
    }

    // STORE
    case inst_sb: {
        byte_t b = reg[rs2] & 0xFF;
        MemoryMap_generic_store(&self->mem_map, reg[rs1] + imm_s, 1, &b);
        break;
    }
    case inst_sh: {
        byte_t buf[2] = { reg[rs2] & 0xFF, (reg[rs2] >> 8) & 0xFF };
        MemoryMap_generic_store(&self->mem_map, reg[rs1] + imm_s, 2, buf);
        break;
    }
    case inst_sw: {
        byte_t buf[4] = {
            reg[rs2] & 0xFF, (reg[rs2] >> 8) & 0xFF,
            (reg[rs2] >> 16) & 0xFF, (reg[rs2] >> 24) & 0xFF
        };
        MemoryMap_generic_store(&self->mem_map, reg[rs1] + imm_s, 4, buf);
        break;
    }

    // BRANCH
    case inst_beq: if (reg[rs1] == reg[rs2]) self->new_pc = pc + imm_b; break;
    case inst_bne: if (reg[rs1] != reg[rs2]) self->new_pc = pc + imm_b; break;
    case inst_blt: if ((int32_t)reg[rs1] < (int32_t)reg[rs2]) self->new_pc = pc + imm_b; break;
    case inst_bge: if ((int32_t)reg[rs1] >= (int32_t)reg[rs2]) self->new_pc = pc + imm_b; break;
    case inst_bltu: if (reg[rs1] < reg[rs2]) self->new_pc = pc + imm_b; break;
    case inst_bgeu: if (reg[rs1] >= reg[rs2]) self->new_pc = pc + imm_b; break;

    // JAL
    case inst_jal:
        reg[rd] = pc + 4;
        self->new_pc = pc + imm_j;
        break;
    // JALR
    case inst_jalr:
        reg[rd] = pc + 4;
        self->new_pc = (reg[rs1] + imm_i) & ~1;
        break;

    // LUI
    case inst_lui:
        reg[rd] = imm_u;
        break;

    // AUIPC
    case inst_auipc:
        reg[rd] = pc + imm_u;
        break;

    default:
        // unknown/invalid, do nothing
        break;
    }

    // x0 is always zero
    reg[0] = 0;
}

static void Core_update_pc(Core *self) {
    self->arch_state.current_pc = self->new_pc;
}

DECLARE_TICK_TICK(Core) {
    Core *self_               = container_of(self, Core, super);
    inst_fields_t inst_fields = Core_fetch(self_);
    inst_enum_t inst_enum     = Core_decode(self_, inst_fields);
    Core_execute(self_, inst_fields, inst_enum);
    Core_update_pc(self_);
}

void Core_ctor(Core *self) {
    assert(self != NULL);
    MemoryMap_ctor(&self->mem_map);
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














