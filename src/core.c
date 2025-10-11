#include "core.h"

#include "inst.h"
#include "tick.h"
#include "arch.h"
#include "mem_map.h"
#include "common.h"

#include <stddef.h>
#include <assert.h>

// ISS Fetch stage
static inst_fields_t Core_fetch(Core *self) {
    // fetch instruction according to self->arch_state.current_pc
    byte_t inst_in_bytes[4] = {};
    MemoryMap_generic_load(&self->mem_map, self->arch_state.current_pc, 4, inst_in_bytes);
    // transformation
    inst_fields_t ret = {};
    ret.raw |= (reg_t)inst_in_bytes[0];
    ret.raw |= (reg_t)inst_in_bytes[1] << 8;
    ret.raw |= (reg_t)inst_in_bytes[2] << 16;
    ret.raw |= (reg_t)inst_in_bytes[3] << 24;
    return ret;
}

// ISS decode stage
static inst_enum_t Core_decode(Core *self, inst_fields_t inst_fields) {
    (void)self; // unused for now
    inst_enum_t ret = (inst_enum_t)0;

    // helper local variables
    reg_t opcode = inst_fields.R_TYPE.opcode;
    reg_t func3  = inst_fields.R_TYPE.func3;
    reg_t func7  = inst_fields.R_TYPE.func7;

    // Minimal decode to tag special control‑flow/upper‑imm ops that need the enum elsewhere.
    // For all other instructions we will directly use opcode/func3/func7 in Core_execute.
    switch (opcode) {
    case 0x6F: // JAL
        ret = inst_jal;
        break;
    case 0x67: // JALR
        ret = inst_jalr;
        break;
    case 0x17: // AUIPC
        ret = inst_auipc;
        break;
    case 0x37: // LUI
        ret = inst_lui;
        break;
    default:
        (void)func3; (void)func7; // silence unused warnings
        break;
    }

    return ret;
}

// ISS execute and commit stage (two-in-one)
static void Core_execute(Core *self, inst_fields_t inst_fields, inst_enum_t inst_enum) {
    // Default: next sequential PC
    self->new_pc = self->arch_state.current_pc + 4;

    // Shorthand to registers
    reg_t *x = self->arch_state.x;

    // Extract raw fields
    uint32_t raw   = (uint32_t)inst_fields.raw;
    uint32_t opcode= raw & 0x7F;
    uint32_t rd    = (raw >> 7)  & 0x1F;
    uint32_t func3 = (raw >> 12) & 0x7;
    uint32_t rs1   = (raw >> 15) & 0x1F;
    uint32_t rs2   = (raw >> 20) & 0x1F;
    uint32_t func7 = (raw >> 25) & 0x7F;

    uint32_t rs1v = x[rs1];
    uint32_t rs2v = x[rs2];

    // ----- Immediate construction helpers (no new functions/macros; fully inlined) -----
    // I‑type (12‑bit signed)
    int32_t imm_i = (int32_t)((int32_t)raw >> 20);
    // S‑type (12‑bit signed): bits [31:25|11:7]
    uint32_t imm_s_u = ((raw >> 7) & 0x1F) | (((raw >> 25) & 0x7F) << 5);
    int32_t  imm_s   = (int32_t)((int32_t)(imm_s_u << 20) >> 20);
    // B‑type (13‑bit signed, LSB=0): bits [31|7|30:25|11:8|0]
    uint32_t imm_b_u = (((raw >> 31) & 0x1) << 12) |
                       (((raw >> 7)  & 0x1) << 11) |
                       (((raw >> 25) & 0x3F) << 5) |
                       (((raw >> 8)  & 0xF) << 1);
    int32_t  imm_b   = (int32_t)((int32_t)(imm_b_u << 19) >> 19);
    // U‑type (upper 20 bits)
    int32_t  imm_u   = (int32_t)(raw & 0xFFFFF000);
    // J‑type (21‑bit signed, LSB=0): bits [31|19:12|20|30:21|0]
    uint32_t imm_j_u = (((raw >> 31) & 0x1) << 20) |
                       (((raw >> 12) & 0xFF) << 12) |
                       (((raw >> 20) & 0x1) << 11) |
                       (((raw >> 21) & 0x3FF) << 1);
    int32_t  imm_j   = (int32_t)((int32_t)(imm_j_u << 11) >> 11);

    // Write helper: x0 is hardwired to 0
    auto write_rd = [&](uint32_t value) {
        if (rd != 0) x[rd] = (reg_t)value;
    };

    switch (opcode) {
    case 0x33: { // OP (R‑type)
        uint32_t shamt = rs2v & 0x1F;
        uint32_t res = 0;
        switch (func3) {
        case 0x0: // ADD/SUB
            res = (func7 == 0x20) ? (uint32_t)((int32_t)rs1v - (int32_t)rs2v)
                                   : (rs1v + rs2v);
            write_rd(res);
            break;
        case 0x1: // SLL
            write_rd(rs1v << shamt);
            break;
        case 0x2: // SLT
            write_rd(((int32_t)rs1v < (int32_t)rs2v) ? 1u : 0u);
            break;
        case 0x3: // SLTU
            write_rd((rs1v < rs2v) ? 1u : 0u);
            break;
        case 0x4: // XOR
            write_rd(rs1v ^ rs2v);
            break;
        case 0x5: // SRL/SRA
            if (func7 == 0x20) {
                write_rd((uint32_t)((int32_t)rs1v >> shamt));
            } else {
                write_rd(rs1v >> shamt);
            }
            break;
        case 0x6: // OR
            write_rd(rs1v | rs2v);
            break;
        case 0x7: // AND
            write_rd(rs1v & rs2v);
            break;
        }
        break;
    }
    case 0x13: { // OP‑IMM (I‑type ALU)
        uint32_t shamt = (uint32_t)imm_i & 0x1F;
        switch (func3) {
        case 0x0: // ADDI
            write_rd((uint32_t)((int32_t)rs1v + imm_i));
            break;
        case 0x2: // SLTI
            write_rd(((int32_t)rs1v < imm_i) ? 1u : 0u);
            break;
        case 0x3: // SLTIU
            write_rd((rs1v < (uint32_t)imm_i) ? 1u : 0u);
            break;
        case 0x4: // XORI
            write_rd(rs1v ^ (uint32_t)imm_i);
            break;
        case 0x6: // ORI
            write_rd(rs1v | (uint32_t)imm_i);
            break;
        case 0x7: // ANDI
            write_rd(rs1v & (uint32_t)imm_i);
            break;
        case 0x1: // SLLI
            write_rd(rs1v << shamt);
            break;
        case 0x5: // SRLI/SRAI
            if (((imm_i >> 10) & 0x3F) == 0x10) { // imm[11:5]==0b010000 -> SRAI
                write_rd((uint32_t)((int32_t)rs1v >> shamt));
            } else { // SRLI
                write_rd(rs1v >> shamt);
            }
            break;
        }
        break;
    }
    case 0x03: { // LOAD
        uint32_t addr = rs1v + (uint32_t)imm_i;
        byte_t buf[4] = {0,0,0,0};
        switch (func3) {
        case 0x0: // LB
            MemoryMap_generic_load(&self->mem_map, addr, 1, buf);
            write_rd((uint32_t)(int32_t)((int8_t)buf[0]));
            break;
        case 0x1: // LH
            MemoryMap_generic_load(&self->mem_map, addr, 2, buf);
            {
                uint32_t u = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8);
                write_rd((uint32_t)(int32_t)((int16_t)u));
            }
            break;
        case 0x2: // LW
            MemoryMap_generic_load(&self->mem_map, addr, 4, buf);
            {
                uint32_t u = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                             ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
                write_rd(u);
            }
            break;
        case 0x4: // LBU
            MemoryMap_generic_load(&self->mem_map, addr, 1, buf);
            write_rd((uint32_t)buf[0]);
            break;
        case 0x5: // LHU
            MemoryMap_generic_load(&self->mem_map, addr, 2, buf);
            {
                uint32_t u = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8);
                write_rd(u);
            }
            break;
        }
        break;
    }
    case 0x23: { // STORE
        uint32_t addr = rs1v + (uint32_t)imm_s;
        byte_t buf[4];
        switch (func3) {
        case 0x0: // SB
            buf[0] = (byte_t)(rs2v & 0xFF);
            MemoryMap_generic_store(&self->mem_map, addr, 1, buf);
            break;
        case 0x1: // SH
            buf[0] = (byte_t)(rs2v & 0xFF);
            buf[1] = (byte_t)((rs2v >> 8) & 0xFF);
            MemoryMap_generic_store(&self->mem_map, addr, 2, buf);
            break;
        case 0x2: // SW
            buf[0] = (byte_t)(rs2v & 0xFF);
            buf[1] = (byte_t)((rs2v >> 8) & 0xFF);
            buf[2] = (byte_t)((rs2v >> 16) & 0xFF);
            buf[3] = (byte_t)((rs2v >> 24) & 0xFF);
            MemoryMap_generic_store(&self->mem_map, addr, 4, buf);
            break;
        }
        break;
    }
    case 0x63: { // BRANCH
        int take = 0;
        switch (func3) {
        case 0x0: take = (rs1v == rs2v); break;              // BEQ
        case 0x1: take = (rs1v != rs2v); break;              // BNE
        case 0x4: take = ((int32_t)rs1v <  (int32_t)rs2v); break;  // BLT
        case 0x5: take = ((int32_t)rs1v >= (int32_t)rs2v); break;  // BGE
        case 0x6: take = (rs1v <  rs2v); break;              // BLTU
        case 0x7: take = (rs1v >= rs2v); break;              // BGEU
        default: break;
        }
        if (take) {
            self->new_pc = self->arch_state.current_pc + (uint32_t)imm_b;
        }
        break;
    }
    case 0x6F: { // JAL
        write_rd(self->arch_state.current_pc + 4);
        self->new_pc = self->arch_state.current_pc + (uint32_t)imm_j;
        break;
    }
    case 0x67: { // JALR
        write_rd(self->arch_state.current_pc + 4);
        uint32_t t = (rs1v + (uint32_t)imm_i) & ~1u; // clear LSB
        self->new_pc = t;
        break;
    }
    case 0x17: { // AUIPC
        write_rd(self->arch_state.current_pc + (uint32_t)imm_u);
        break;
    }
    case 0x37: { // LUI
        write_rd((uint32_t)imm_u);
        break;
    }
    default:
        (void)inst_enum; // unused; execution handled via opcode
        break;
    }

    // x0 must always read as 0
    x[0] = 0;
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

    // initialize memory map object
    // prepare for being added new MMIO devices
    MemoryMap_ctor(&self->mem_map);

    // initialize base class (Tick)
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













