#ifndef __CORE_H__
#define __CORE_H__

#include "tick.h"
#include "arch.h"
#include "mem_map.h"

typedef struct {
    Tick super; // inherit from parent class

    // internal states of core (includes memory map object)
    arch_state_t arch_state; // RISC-V architectural states
    reg_t new_pc;            // helper data member for next-pc calculation
    MemoryMap mem_map;       // memory map which contains all MMIO devices (with
                             // LOAD/STORE capability)
} Core;

extern void Core_ctor(Core *self);
extern void Core_dtor(Core *self);
extern int Core_add_device(Core *self, mmap_unit_t new_device);

#endif
