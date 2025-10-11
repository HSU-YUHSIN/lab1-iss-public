#include "core.h"
#include "inst.h"
#include "tick.h"
#include "arch.h"
#include "mem_map.h"
#include "common.h"

#include <stddef.h>
#include <assert.h>
#include <string.h>

static inline reg_t sign_extend(reg_t value, int bits) {
    reg_t mask = 1U << (bits - 1);
    return (value ^ mask) - mask;
}

// ---- portable field extractors: avoid non-portable bitfields ----
static inline uint32_t OP_OF(uint32_t insn)    { return insn & 0x7F; }
static inline uint32_t RD_OF(uint32_t insn)    { return (insn >> 7) & 0x1F; }
static inline uint32_t FUNCT3_OF(uint32_t insn){ return (insn >> 12) & 0x7; }
static inline uint32_t RS1_OF(uint32_t insn)   { return (insn >> 15) & 0x1F; }
static inline uint32_t RS2_OF(uint32_t insn)   { return (insn >> 20) & 0x1F; }
static inline uint32_t FUNCT7_OF(uint32_t insn){ return (insn >> 25) & 0x7F; }

static inline int32_t IMM_I_OF(uint32_t insn) {
    // sign-extended imm[31:20]
    return (int32_t)insn >> 20;
}
static inline int32_t IMM_S_OF(uint32_t insn) {
    // imm[11:5]=[31:25], imm[4:0]=[11:7]
    uint32_t imm = ((insn >> 25) << 5) | ((insn >> 7) & 0x1F);
    return (int32_t)((imm ^ (1u << 11)) - (1u << 11)); // sign-extend 12-bit
}
static inline int32_t IMM_B_OF(uint32_t insn) {
    // imm[12|10:5|4:1|11]
    uint32_t imm = (((insn >> 31) & 0x1) << 12) |
                   (((insn >> 25) & 0x3F) << 5)  |
                   (((insn >> 8)  & 0xF)  << 1)  |
                   (((insn >> 7)  & 0x1)  << 11);
    return (int32_t)((imm ^ (1u << 12)) - (1u << 12)); // sign-extend 13-bit
}
static inline uint32_t IMM_U_OF(uint32_t insn) { return insn & 0xFFFFF000u; }
static inline int32_t IMM_J_OF(uint32_t insn) {
    // imm[20|10:1|11|19:12]
    uint32_t imm = (((insn >> 31) & 0x1)   << 20) |
                   (((insn >> 21) & 0x3FF) << 1)  |
                   (((insn >> 20) & 0x1)   << 11) |
                   (((insn >> 12) & 0xFF)  << 12);
    return (int32_t)((imm ^ (1u << 20)) - (1u << 20)); // sign-extend 21-bit
}

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

static inst_enum_t Core_decode(Core *self, inst_fields_t inst_fields) {
    inst_enum_t ret = inst_invalid;

    uint32_t insn = (uint32_t)inst_fields.raw;
    reg_t opcode = OP_OF(insn);
    reg_t func3  = FUNCT3_OF(insn);
    reg_t func7  = FUNCT7_OF(insn);

    switch (opcode) {
    case OP: {
        if (func3 == 0x0 && func7 == 0x00) ret = inst_add;
        else if (func3 == 0x0 && func7 == 0x20) ret = inst_sub;
        else if (func3 == 0x1 && func7 == 0x00) ret = inst_sll;
        else if (func3 == 0x2 && func7 == 0x00) ret = inst_slt;
        else if (func3 == 0x3 && func7 == 0x00) ret = inst_sltu;
        else if (func3 == 0x4 && func7 == 0x00) ret = inst_xor;
        else if (func3 == 0x5 && func7 == 0x00) ret = inst_srl;
        else if (func3 == 0x5 && func7 == 0x20) ret = inst_sra;
        else if (func3 == 0x6 && func7 == 0x00) ret = inst_or;
        else if (func3 == 0x7 && func7 == 0x00) ret = inst_and;
        break;
    }
    case OP_IMM: // I-type
        switch (func3) {
        case 0x0: ret = inst_addi; break;
        case 0x1: // SLLI requires funct7 == 0x00 in RV32I
            ret = (FUNCT7_OF(insn) == 0x00) ? inst_slli : inst_invalid;
            break;
        case 0x2: ret = inst_slti; break;
        case 0x3: ret = inst_sltiu; break;
        case 0x4: ret = inst_xori; break;
        case 0x5: // SRLI/SRAI: bit 30 distinguishes
            ret = ((insn >> 30) & 1u) ? inst_srai : inst_srli;
            break;
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
    case JAL:   ret = inst_jal;   break;
    case JALR:  ret = inst_jalr;  break;
    case AUIPC: ret = inst_auipc; break;
    case LUI:   ret = inst_lui;   break;
    default:
        ret = inst_invalid;
        break;
    }
    return ret;
}

// ISS execute and commit stage (two-in-one)
static void Core_execute(Core *self, inst_fields_t inst_fields, inst_enum_t inst_enum) {
    // set self->new_pc to a default value by PC+4
    // it might be overridden when there is a branch instruction
    self->new_pc = self->arch_state.current_pc + 4;

    reg_t pc   = self->arch_state.current_pc;

    uint32_t insn = (uint32_t)inst_fields.raw;
    reg_t rd   = RD_OF(insn);
    reg_t rs1  = RS1_OF(insn);
    reg_t rs2  = RS2_OF(insn);

    int32_t imm_i = IMM_I_OF(insn);
    int32_t imm_s = IMM_S_OF(insn);
    int32_t imm_b = IMM_B_OF(insn);
    reg_t  imm_u  = IMM_U_OF(insn);   // U-type stays zero-extended
    int32_t imm_j = IMM_J_OF(insn);

    switch (inst_enum) {
    // R-type OP instructions
    case inst_add:
        if (rd != 0)
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] + self->arch_state.gpr[rs2];
        break;
    case inst_sub:
        if (rd != 0)
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] - self->arch_state.gpr[rs2];
        break;
    case inst_sll:
        if (rd != 0)
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] << (self->arch_state.gpr[rs2] & 0x1F);
        break;
    case inst_slt:
        if (rd != 0)
            self->arch_state.gpr[rd] = (int32_t)self->arch_state.gpr[rs1] < (int32_t)self->arch_state.gpr[rs2];
        break;
    case inst_sltu:
        if (rd != 0)
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] < self->arch_state.gpr[rs2];
        break;
    case inst_xor:
        if (rd != 0)
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] ^ self->arch_state.gpr[rs2];
        break;
    case inst_srl:
        if (rd != 0)
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] >> (self->arch_state.gpr[rs2] & 0x1F);
        break;
    case inst_sra:
        if (rd != 0)
            self->arch_state.gpr[rd] = ((int32_t)self->arch_state.gpr[rs1]) >> (self->arch_state.gpr[rs2] & 0x1F);
        break;
    case inst_or:
        if (rd != 0)
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] | self->arch_state.gpr[rs2];
        break;
    case inst_and:
        if (rd != 0)
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] & self->arch_state.gpr[rs2];
        break;

    // I-type
    case inst_addi:
        if (rd != 0)
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] + imm_i;
        break;
    case inst_slli:
        if (rd != 0) {
            uint32_t shamt = insn >> 20; // lower 5 bits are shamt in RV32I
            shamt &= 0x1F;
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] << shamt;
        }
        break;
    case inst_slti:
        if (rd != 0)
            self->arch_state.gpr[rd] = (int32_t)self->arch_state.gpr[rs1] < (int32_t)imm_i;
        break;
    case inst_sltiu:
        if (rd != 0)
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] < imm_i;
        break;
    case inst_xori:
        if (rd != 0)
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] ^ imm_i;
        break;
    case inst_srli:
        if (rd != 0) {
            uint32_t shamt = insn >> 20; // lower 5 bits are shamt in RV32I
            shamt &= 0x1F;
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] >> shamt;
        }
        break;
    case inst_srai:
        if (rd != 0) {
            uint32_t shamt = insn >> 20; // lower 5 bits are shamt in RV32I
            shamt &= 0x1F;
            self->arch_state.gpr[rd] = ((int32_t)self->arch_state.gpr[rs1]) >> shamt;
        }
        break;
    case inst_ori:
        if (rd != 0)
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] | imm_i;
        break;
    case inst_andi:
        if (rd != 0)
            self->arch_state.gpr[rd] = self->arch_state.gpr[rs1] & imm_i;
        break;

    // LOAD
    case inst_lb: {
        byte_t b;
        reg_t addr = (reg_t)((int32_t)self->arch_state.gpr[rs1] + imm_i);
        MemoryMap_generic_load(&self->mem_map, addr, 1, &b);
        if (rd != 0)
            self->arch_state.gpr[rd] = sign_extend(b, 8);
        break;
    }
    case inst_lh: {
        byte_t buf[2];
        reg_t addr = (reg_t)((int32_t)self->arch_state.gpr[rs1] + imm_i);
        MemoryMap_generic_load(&self->mem_map, addr, 2, buf);
        if (rd != 0)
            self->arch_state.gpr[rd] = sign_extend((reg_t)buf[0] | ((reg_t)buf[1] << 8), 16);
        break;
    }
    case inst_lw: {
        byte_t buf[4];
        reg_t addr = (reg_t)((int32_t)self->arch_state.gpr[rs1] + imm_i);
        MemoryMap_generic_load(&self->mem_map, addr, 4, buf);
        if (rd != 0)
            self->arch_state.gpr[rd] = (reg_t)buf[0]
                                     | ((reg_t)buf[1] << 8)
                                     | ((reg_t)buf[2] << 16)
                                     | ((reg_t)buf[3] << 24);
        break;
    }
    case inst_lbu: {
        byte_t b;
        reg_t addr = (reg_t)((int32_t)self->arch_state.gpr[rs1] + imm_i);
        MemoryMap_generic_load(&self->mem_map, addr, 1, &b);
        if (rd != 0)
            self->arch_state.gpr[rd] = b;
        break;
    }
    case inst_lhu: {
        byte_t buf[2];
        reg_t addr = (reg_t)((int32_t)self->arch_state.gpr[rs1] + imm_i);
        MemoryMap_generic_load(&self->mem_map, addr, 2, buf);
        if (rd != 0)
            self->arch_state.gpr[rd] = (reg_t)buf[0] | ((reg_t)buf[1] << 8);
        break;
    }

    // STORE
    case inst_sb: {
        byte_t b = self->arch_state.gpr[rs2] & 0xFF;
        reg_t addr = (reg_t)((int32_t)self->arch_state.gpr[rs1] + imm_s);
        MemoryMap_generic_store(&self->mem_map, addr, 1, &b);
        break;
    }
    case inst_sh: {
        byte_t buf[2] = { self->arch_state.gpr[rs2] & 0xFF, (self->arch_state.gpr[rs2] >> 8) & 0xFF };
        reg_t addr = (reg_t)((int32_t)self->arch_state.gpr[rs1] + imm_s);
        MemoryMap_generic_store(&self->mem_map, addr, 2, buf);
        break;
    }
    case inst_sw: {
        byte_t buf[4] = {
            self->arch_state.gpr[rs2] & 0xFF, (self->arch_state.gpr[rs2] >> 8) & 0xFF,
            (self->arch_state.gpr[rs2] >> 16) & 0xFF, (self->arch_state.gpr[rs2] >> 24) & 0xFF
        };
        reg_t addr = (reg_t)((int32_t)self->arch_state.gpr[rs1] + imm_s);
        MemoryMap_generic_store(&self->mem_map, addr, 4, buf);
        break;
    }

    // BRANCH
    case inst_beq:
        if (self->arch_state.gpr[rs1] == self->arch_state.gpr[rs2])
            self->new_pc = (reg_t)((int32_t)pc + imm_b);
        break;
    case inst_bne:
        if (self->arch_state.gpr[rs1] != self->arch_state.gpr[rs2])
            self->new_pc = (reg_t)((int32_t)pc + imm_b);
        break;
    case inst_blt:
        if ((int32_t)self->arch_state.gpr[rs1] < (int32_t)self->arch_state.gpr[rs2])
            self->new_pc = (reg_t)((int32_t)pc + imm_b);
        break;
    case inst_bge:
        if ((int32_t)self->arch_state.gpr[rs1] >= (int32_t)self->arch_state.gpr[rs2])
            self->new_pc = (reg_t)((int32_t)pc + imm_b);
        break;
    case inst_bltu:
        if (self->arch_state.gpr[rs1] < self->arch_state.gpr[rs2])
            self->new_pc = (reg_t)((int32_t)pc + imm_b);
        break;
    case inst_bgeu:
        if (self->arch_state.gpr[rs1] >= self->arch_state.gpr[rs2])
            self->new_pc = (reg_t)((int32_t)pc + imm_b);
        break;

    // JAL
    case inst_jal:
        if (rd != 0)
            self->arch_state.gpr[rd] = pc + 4;
        self->new_pc = (reg_t)((int32_t)pc + imm_j);
        break;
    // JALR
    case inst_jalr:
        if (rd != 0)
            self->arch_state.gpr[rd] = pc + 4;
        self->new_pc = ((reg_t)((int32_t)self->arch_state.gpr[rs1] + imm_i)) & ~1u;
        break;

    // LUI
    case inst_lui:
        if (rd != 0)
            self->arch_state.gpr[rd] = imm_u;
        break;

    // AUIPC
    case inst_auipc:
        if (rd != 0)
            self->arch_state.gpr[rd] = pc + imm_u;
        break;

    default:
        // NOP or invalid
        break;
    }
    // Note: x0 protection is handled by checking (rd != 0) before each write

    // Ensure x0 is always zero regardless of any path
    self->arch_state.gpr[0] = 0;
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
    
    // Initialize PC and register file (malloc doesn't zero memory)
    self->arch_state.current_pc = 0;
    self->new_pc = 0;
    memset(self->arch_state.gpr, 0, sizeof(self->arch_state.gpr));
    // Initialize stack pointer (x2) to top of RAM so prologue stores are valid.
    // Try multiple common macro names from mem_map.h used in lab templates.
    {
        reg_t ram_base = 0;
        reg_t ram_size = 0;
        #if defined(MEM_DRAM_BASE) && defined(MEM_DRAM_SIZE)
            ram_base = (reg_t)MEM_DRAM_BASE;
            ram_size = (reg_t)MEM_DRAM_SIZE;
        #elif defined(MEM_RAM_BASE) && defined(MEM_RAM_SIZE)
            ram_base = (reg_t)MEM_RAM_BASE;
            ram_size = (reg_t)MEM_RAM_SIZE;
        #elif defined(DRAM_BASE) && defined(DRAM_SIZE)
            ram_base = (reg_t)DRAM_BASE;
            ram_size = (reg_t)DRAM_SIZE;
        #elif defined(RAM_BASE) && defined(RAM_SIZE)
            ram_base = (reg_t)RAM_BASE;
            ram_size = (reg_t)RAM_SIZE;
        #endif
        if (ram_size != 0) {
            // Leave a small redzone (16B) and set sp to top of RAM
            self->arch_state.gpr[2] = (reg_t)(ram_base + ram_size - 16);
        }
    }
}

void Core_dtor(Core *self) {
    assert(self != NULL);
    MemoryMap_dtor(&self->mem_map);
}

int Core_add_device(Core *self, mmap_unit_t new_device) {
    return MemoryMap_add_device(&self->mem_map, new_device);
}














































































































