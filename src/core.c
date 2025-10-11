#include "core.h"

#include "inst.h"
#include "tick.h"
#include "arch.h"
#include "mem_map.h"
#include "common.h"

#include <stddef.h>
#include <assert.h>
#include <stdint.h>

/* ---------------- helpers ---------------- */
static inline int32_t sext32(uint32_t x, int bits) {
    int shift = 32 - bits;
    return (int32_t)(x << shift) >> shift;
}

static inline void write_x(Core *self, uint32_t rd, uint32_t val) {
    if (rd != 0 && rd < 32) self->arch_state.gpr[rd] = val; // x0 is hardwired to 0
}

/* ---------------- macros for instruction field extraction ---------------- */
#define OPCODE(raw)   ((raw) & 0x7F)
#define RD(raw)       (((raw) >> 7) & 0x1F)
#define FUNCT3(raw)   (((raw) >> 12) & 0x7)
#define RS1(raw)      (((raw) >> 15) & 0x1F)
#define RS2(raw)      (((raw) >> 20) & 0x1F)
#define FUNCT7(raw)   (((raw) >> 25) & 0x7F)
#define IMM_I(raw)    (sext32((raw) >> 20, 12))
#define IMM_S(raw)    (sext32((((raw) >> 25) << 5) | (((raw) >> 7) & 0x1F), 12))
#define IMM_B(raw)    (sext32((((raw) >> 31) << 12) | \
                              (((raw) >> 7) & 0x1) << 11 | \
                              (((raw) >> 25) & 0x3F) << 5 | \
                              (((raw) >> 8) & 0xF) << 1, 13))
#define IMM_U(raw)    ((raw) & 0xFFFFF000)
#define IMM_J(raw)    (sext32((((raw) >> 31) << 20) | \
                              (((raw) >> 12) & 0xFF) << 12 | \
                              (((raw) >> 20) & 0x1) << 11 | \
                              (((raw) >> 21) & 0x3FF) << 1, 21))

/* ---------------- fetch ---------------- */
static uint32_t Core_fetch(Core *self) {
    // fetch instruction according to self->arch_state.current_pc
    // Enforce 4-byte alignment for instruction fetch
    if (self->arch_state.current_pc & 0x3u) {
        self->arch_state.current_pc &= ~0x3u; // silently align to word boundary
    }
    byte_t inst_in_bytes[4] = {0,0,0,0};
    MemoryMap_generic_load(&self->mem_map, self->arch_state.current_pc, 4, inst_in_bytes);

    // little-endian pack
    uint32_t inst = ((uint32_t)inst_in_bytes[0]) |
                    ((uint32_t)inst_in_bytes[1] << 8) |
                    ((uint32_t)inst_in_bytes[2] << 16) |
                    ((uint32_t)inst_in_bytes[3] << 24);
    return inst;
}

/* ---------------- decode ---------------- */
static inst_enum_t Core_decode(uint32_t inst) {
    inst_enum_t ret = (inst_enum_t)0; // unknown by default

    uint32_t opcode = OPCODE(inst);
    uint32_t func3  = FUNCT3(inst);
    uint32_t func7  = FUNCT7(inst);

    switch (opcode) {
    case OP: { /* R-type */
        switch (func3) {
            case 0x0: ret = (func7 == 0x20) ? inst_sub : inst_add; break;
            case 0x1: ret = inst_sll;  break;
            case 0x2: ret = inst_slt;  break;
            case 0x3: ret = inst_sltu; break;
            case 0x4: ret = inst_xor;  break;
            case 0x5: ret = (func7 == 0x20) ? inst_sra : inst_srl; break;
            case 0x6: ret = inst_or;   break;
            case 0x7: ret = inst_and;  break;
        }
        break;
    }
    case OP_IMM: { /* I-type ALU */
        switch (func3) {
            case 0x0: ret = inst_addi;  break;
            case 0x2: ret = inst_slti;  break;
            case 0x3: ret = inst_sltiu; break;
            case 0x4: ret = inst_xori;  break;
            case 0x6: ret = inst_ori;   break;
            case 0x7: ret = inst_andi;  break;
            case 0x1: { // SLLI legality: imm[11:5]==0
                uint32_t imm = (inst >> 20) & 0xFFF;
                if ((imm & ~0x1Fu) == 0) ret = inst_slli;
                break;
            }
            case 0x5: { // SRLI/SRAI legality
                uint32_t imm = (inst >> 20) & 0xFFF;
                uint32_t hi  = imm & ~0x1Fu;
                if (hi == 0x000u)      ret = inst_srli;
                else if (hi == 0x400u) ret = inst_srai;
                break;
            }
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
    default: break; // unknown stays 0
    }

    return ret;
}

/* ---------------- execute & commit ---------------- */
static void Core_execute(Core *self, uint32_t inst, inst_enum_t e) {
    // default next PC = PC + 4
    self->new_pc = self->arch_state.current_pc + 4;

    uint32_t rs1 = RS1(inst);
    uint32_t rs2 = RS2(inst);
    uint32_t rd  = RD(inst);

    uint32_t x1 = self->arch_state.gpr[rs1];
    uint32_t x2 = self->arch_state.gpr[rs2];

    switch (e) {
        /* R-type ALU */
        case inst_add:  write_x(self, rd, x1 + x2); break;
        case inst_sub:  write_x(self, rd, x1 - x2); break;
        case inst_sll:  write_x(self, rd, x1 << (x2 & 31)); break;
        case inst_slt:  write_x(self, rd, (int32_t)x1 <  (int32_t)x2); break;
        case inst_sltu: write_x(self, rd, x1 < x2); break;
        case inst_xor:  write_x(self, rd, x1 ^ x2); break;
        case inst_srl:  write_x(self, rd, x1 >> (x2 & 31)); break;
        case inst_sra:  write_x(self, rd, (uint32_t)((int32_t)x1 >> (x2 & 31))); break;
        case inst_or:   write_x(self, rd, x1 | x2); break;
        case inst_and:  write_x(self, rd, x1 & x2); break;

        /* I-type ALU */
        case inst_addi:  { int32_t imm = IMM_I(inst); write_x(self, rd, x1 + imm); break; }
        case inst_slti:  { int32_t imm = IMM_I(inst); write_x(self, rd, (int32_t)x1 < imm); break; }
        case inst_sltiu: { int32_t imm = IMM_I(inst); write_x(self, rd, x1 < (uint32_t)imm); break; }
        case inst_xori:  { int32_t imm = IMM_I(inst); write_x(self, rd, x1 ^ (uint32_t)imm); break; }
        case inst_ori:   { int32_t imm = IMM_I(inst); write_x(self, rd, x1 | (uint32_t)imm); break; }
        case inst_andi:  { int32_t imm = IMM_I(inst); write_x(self, rd, x1 & (uint32_t)imm); break; }
        case inst_slli:  { uint32_t sh = (inst >> 20) & 0x1F; write_x(self, rd, x1 << sh); break; }
        case inst_srli:  { uint32_t sh = (inst >> 20) & 0x1F; write_x(self, rd, x1 >> sh); break; }
        case inst_srai:  { uint32_t sh = (inst >> 20) & 0x1F; write_x(self, rd, (uint32_t)((int32_t)x1 >> sh)); break; }

        /* LOADs */
        case inst_lb: case inst_lbu: case inst_lh: case inst_lhu: case inst_lw: {
            int32_t imm = IMM_I(inst);
            uint32_t addr = x1 + imm;
            byte_t buf[4] = {0,0,0,0};
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
                    write_x(self, rd, (uint32_t)sext32((uint32_t)((uint16_t)(uint8_t)buf[0] |
                                                                  ((uint16_t)(uint8_t)buf[1] << 8)), 16));
                    break;
                case inst_lhu:
                    MemoryMap_generic_load(&self->mem_map, addr, 2, buf);
                    write_x(self, rd, (uint32_t)((uint16_t)(uint8_t)buf[0] |
                                                 ((uint16_t)(uint8_t)buf[1] << 8)));
                    break;
                case inst_lw:
                    MemoryMap_generic_load(&self->mem_map, addr, 4, buf);
                    write_x(self, rd, ((uint32_t)(uint8_t)buf[0]) |
                                      ((uint32_t)(uint8_t)buf[1] << 8) |
                                      ((uint32_t)(uint8_t)buf[2] << 16) |
                                      ((uint32_t)(uint8_t)buf[3] << 24));
                    break;
                default: break;
            }
            break;
        }

        /* STOREs */
        case inst_sb: case inst_sh: case inst_sw: {
            int32_t imm = IMM_S(inst);
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

        /* BRANCHes */
        case inst_beq: case inst_bne: case inst_blt: case inst_bge: case inst_bltu: case inst_bgeu: {
            int32_t imm = IMM_B(inst);
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
            if (take) self->new_pc = (self->arch_state.current_pc + imm) & ~0x3u;
            break;
        }

        /* JUMPs */
        case inst_jal: {
            int32_t imm = IMM_J(inst);
            write_x(self, rd, self->arch_state.current_pc + 4);
            self->new_pc = (self->arch_state.current_pc + imm) & ~0x3u;
            break;
        }
        case inst_jalr: {
            int32_t imm = IMM_I(inst);
            uint32_t target = (x1 + imm) & ~3u;
            write_x(self, rd, self->arch_state.current_pc + 4);
            self->new_pc = target;
            break;
        }

        /* U-type */
        case inst_lui:
            write_x(self, rd, IMM_U(inst));
            break;
        case inst_auipc:
            write_x(self, rd, self->arch_state.current_pc + IMM_U(inst));
            break;

        default:
            /* unknown: do nothing */
            break;
    }

    /* Hardwire x0 to 0 */
    self->arch_state.gpr[0] = 0;
}

static void Core_update_pc(Core *self) {
    self->arch_state.current_pc = self->new_pc;
}

/* ---------------- tick ---------------- */
DECLARE_TICK_TICK(Core) {
    Core *self_               = container_of(self, Core, super);
    uint32_t inst             = Core_fetch(self_);
    inst_enum_t   inst_enum   = Core_decode(inst);
    Core_execute(self_, inst, inst_enum);
    Core_update_pc(self_);
}

/* ---------------- ctor/dtor ---------------- */
void Core_ctor(Core *self) {
    assert(self != NULL);

    // initialize memory map object first
    MemoryMap_ctor(&self->mem_map);

    // initialize architectural state
    for (int i = 0; i < 32; i++) self->arch_state.gpr[i] = 0;

    // Choose reset PC: prefer known bases if defined, else 0x80000000u for lab harness
    #if defined(MEM_TEXT_BASE)
        const uint32_t reset_pc = (uint32_t)MEM_TEXT_BASE;
    #elif defined(DRAM_BASE)
        const uint32_t reset_pc = (uint32_t)DRAM_BASE;
    #elif defined(TEXT_BASE)
        const uint32_t reset_pc = (uint32_t)TEXT_BASE;
    #elif defined(ROM_BASE)
        const uint32_t reset_pc = (uint32_t)ROM_BASE;
    #else
        const uint32_t reset_pc = 0x80000000u;  /* common RISC-V DRAM base */
    #endif
    self->arch_state.current_pc = reset_pc;
    self->new_pc                = reset_pc;

    // initialize base class (Tick)
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







