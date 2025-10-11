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
    inst_enum_t ret = {};

    // helper local variables
    reg_t opcode = inst_fields.R_TYPE.opcode;
    reg_t rs1    = inst_fields.R_TYPE.rs1;
    reg_t rs2    = inst_fields.R_TYPE.rs2;
    reg_t rd     = inst_fields.R_TYPE.rd;
    reg_t func3  = inst_fields.R_TYPE.func3;
    reg_t func7  = inst_fields.R_TYPE.func7;

    // decode common part
    // TODO
    switch (opcode) {
    case OP: {
        break;
    }
    case OP_IMM: {
        break;
    }
    case LOAD: {
        break;
    }
    case STORE: {
        break;
    }
    case BRANCH: {
        break;
    }
    case JAL: {
        ret = inst_jal;
        break;
    }
    case JALR: {
        ret = inst_jalr;
        break;
    }
    case AUIPC: {
        ret = inst_auipc;
        break;
    }
    case LUI: {
        ret = inst_lui;
        break;
    }
    }

    return ret;
}

// ISS execute and commit stage (two-in-one)
static void Core_execute(Core *self, inst_fields_t inst_fields, inst_enum_t inst_enum) {
    // set self->new_pc to a default value by PC+4
    // it might be overridden when there is a branch instruction
    self->new_pc = self->arch_state.current_pc + 4;

    // TODO
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














