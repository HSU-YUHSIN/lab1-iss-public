#include "main_mem.h"

#include "abstract_mem.h"
#include "common.h"

#include <string.h>
#include <assert.h>

DECLARE_ABSTRACT_MEM_LOAD(MainMem) {
    Assert(self != NULL, "");
    Assert(base_addr + length <= MAIN_MEM_SIZE, "");

    MainMem *self_ = container_of(self, MainMem, super);
    for (int i = 0; i < length; i++) {
        buffer[i] = self_->mem[base_addr + i];
    }
}

DECLARE_ABSTRACT_MEM_STORE(MainMem) {
    Assert(self != NULL, "");
    Assert(base_addr + length <= MAIN_MEM_SIZE, "");

    MainMem *self_ = container_of(self, MainMem, super);
    for (int i = 0; i < length; i++) {
        self_->mem[base_addr + i] = ref_data[i];
    }
}

void MainMem_ctor(MainMem *self) {
    assert((self != NULL) && "MainMem *self ptr should not be NULL!");

    // initlaize base class
    AbstractMem_ctor(&self->super);
    static struct AbstractMemVtbl const vtbl = {
        .load  = &SIGNATURE_ABSTRACT_MEM_LOAD(MainMem),
        .store = &SIGNATURE_ABSTRACT_MEM_STORE(MainMem)
    };
    self->super.vtbl = &vtbl;
    // initialize self->mem
    memset(&self->mem, 0, sizeof(byte_t) * MAIN_MEM_SIZE);
}
