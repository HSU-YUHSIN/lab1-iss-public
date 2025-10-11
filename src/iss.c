#include "iss.h"

#include "common.h"
#include "arch.h"
#include "core.h"
#include "load_elf.h"
#include "mem_map.h"
#include "tick.h"
#include "main_mem.h"
#include "rom.h"
#include "halt.h"
#include "text_buffer.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct iss {
    // core part (RISC-V processor)
    Core core;

    // MMIO devices
    ROM rom_mmio;
    MainMem main_mem_mmio;
    TextBuffer text_buffer_mmio;
    Halt halt_mmio;
};

int ISS_ctor(ISS **self, const char *elf_file_name) {
    assert(self != NULL);
    if (NULL == (*self = calloc(1, sizeof(struct iss)))) {
        return -1;
    }

    ISS *self_ = *self;
    // call constructors
    Core_ctor(&self_->core);
    ROM_ctor(&self_->rom_mmio);
    MainMem_ctor(&self_->main_mem_mmio);
    TextBuffer_ctor(&self_->text_buffer_mmio);
    Halt_ctor(&self_->halt_mmio);

    // add ROM into core's mmap
    mmap_unit_t ROM_mmap_unit = { .addr_bound = { .first = ROM_MMAP_BASE,
                                                  .second = ROM_MMAP_BASE + ROM_SIZE },
                                  .device_ptr = (AbstractMem *)&self_->rom_mmio };
    Core_add_device(&self_->core, ROM_mmap_unit);

    // add main memory into core's mmap
    mmap_unit_t main_mem_mmap_unit = {
        .addr_bound = { .first = MAIN_MEM_MMAP_BASE, .second = MAIN_MEM_MMAP_BASE + MAIN_MEM_SIZE },
        .device_ptr = (AbstractMem *)&self_->main_mem_mmio
    };
    Core_add_device(&self_->core, main_mem_mmap_unit);

    // add text buffer into core's mmap
    mmap_unit_t text_buffer_mmap_unit = {
        .addr_bound = { .first  = TEXT_BUFFER_MMAP_BASE,
                        .second = TEXT_BUFFER_MMAP_BASE + TEXT_BUFFER_SIZE },
        .device_ptr = (AbstractMem *)&self_->text_buffer_mmio
    };
    Core_add_device(&self_->core, text_buffer_mmap_unit);

    // add halt flag into core's mmap
    mmap_unit_t halt_mmap_unit = { .addr_bound = { .first = HALT_MMAP_BASE,
                                                   .second = HALT_MMAP_BASE + HALT_SIZE },
                                   .device_ptr = (AbstractMem *)&self_->halt_mmio };
    Core_add_device(&self_->core, halt_mmap_unit);

    // load ELF into main memory, and initialize PC
    load_elf(elf_file_name, self_->main_mem_mmio.mem, MAIN_MEM_SIZE,
             &self_->core.arch_state.current_pc);

    return 0;
}

void ISS_dtor(ISS *self) {
    LOG("Calling ISS_dtor to clean up things...");

    // core destructor
    Core_dtor(&self->core);
    free(self);

    /*
     * self->core is the only one data member whose destructor is must be called
     */
}

void ISS_step(ISS *self, unsigned long n_step) {
    for (unsigned i = 0; i < n_step; i++) {
        // check halt flag
        if (unlikely(self->halt_mmio.halt_flag == true)) {
            return;
        }
        // tick all tickable devices (includes core itself)
        Tick_tick(&self->core.super);
        Tick_tick(&self->text_buffer_mmio.tick_super);
    }
}

arch_state_t ISS_get_arch_state(const ISS *self) {
    Assert(self != NULL, "self should not be NULL!");
    arch_state_t ret = {};
    memcpy(&ret, &self->core.arch_state, sizeof(arch_state_t));
    return ret;
}

void ISS_set_arch_state(ISS *self, const arch_state_t ref_arch_state) {
    Assert(self != NULL, "self should not be NULL!");
    memcpy(&self->core.arch_state, &ref_arch_state, sizeof(arch_state_t));
}

void ISS_get_main_memory(const ISS *self,
                         const addr_t base_addr,
                         const unsigned int length,
                         byte_t *buffer) {
    //
}

void ISS_set_main_memory(ISS *self,
                         const addr_t base_addr,
                         const unsigned int length,
                         const byte_t *ref_data) {
    //
}

bool ISS_get_halt(ISS *self) {
    return self->halt_mmio.halt_flag;
}

