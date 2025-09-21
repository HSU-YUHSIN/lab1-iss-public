#include "halt.h"

#include "abstract_mem.h"
#include "common.h"

#include <stddef.h>
#include <assert.h>

DECLARE_ABSTRACT_MEM_LOAD(Halt) {
    // assertions
    Assert(self != NULL, "");
    Assert(base_addr + length <= HALT_SIZE, "");
    Assert(length == 1, "");

    // load into buffer
    Halt *self_ = container_of(self, Halt, super);
    buffer[0]   = (byte_t)self_->halt_flag;
}

DECLARE_ABSTRACT_MEM_STORE(Halt) {
    // assertions
    Assert(self != NULL, "");
    Assert(base_addr + length <= HALT_SIZE, "");
    Assert(length == 1, "");

    // load ref_data into Halt internal flag
    Halt *self_      = container_of(self, Halt, super);
    self_->halt_flag = (bool)(ref_data[0] & 0x1);
}

void Halt_ctor(Halt *self) {
    assert((self != NULL) && "Halt *self should not be null ptr");

    AbstractMem_ctor(&self->super);
    static struct AbstractMemVtbl const vtbl = {
        .load = &SIGNATURE_ABSTRACT_MEM_LOAD(Halt), .store = &SIGNATURE_ABSTRACT_MEM_STORE(Halt)
    };
    self->super.vtbl = &vtbl;

    // initialize jalt_flag
    self->halt_flag = false;
}
