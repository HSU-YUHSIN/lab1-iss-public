#include "text_buffer.h"

#include "tick.h"
#include "abstract_mem.h"
#include "common.h"

#include <stdio.h>
#include <assert.h>

DECLARE_ABSTRACT_MEM_LOAD(TextBuffer) {
    Assert(self != NULL, "");
    Assert(base_addr + length <= TEXT_BUFFER_SIZE, "");
    Assert(length == 1, "");

    TextBuffer *self_ = container_of(self, TextBuffer, abstract_mem_super);
    buffer[0]         = self_->buffer;
}

DECLARE_ABSTRACT_MEM_STORE(TextBuffer) {
    Assert(self != NULL, "");
    Assert(base_addr + length <= TEXT_BUFFER_SIZE, "");
    Assert(length == 1, "");

    TextBuffer *self_ = container_of(self, TextBuffer, abstract_mem_super);
    self_->buffer     = ref_data[0];
    self_->valid      = true;
}

DECLARE_TICK_TICK(TextBuffer) {
    TextBuffer *self_ = container_of(self, TextBuffer, tick_super);
    if (self_->valid) {
        printf("%c", self_->buffer);
        self_->valid  = false;
        self_->buffer = 0;
    }
}

void TextBuffer_ctor(TextBuffer *self) {
    assert(self != NULL);

    // Tick vtable initialization
    Tick_ctor(&self->tick_super);
    static struct TickVtbl const tickable_vtbl = { .tick = &SIGNATURE_TICK_TICK(TextBuffer) };
    self->tick_super.vtbl = &tickable_vtbl;

    // AbstractMem vtable initialization
    AbstractMem_ctor(&self->abstract_mem_super);
    static struct AbstractMemVtbl const abstract_mem_vtbl = {
        .load  = &SIGNATURE_ABSTRACT_MEM_LOAD(TextBuffer),
        .store = &SIGNATURE_ABSTRACT_MEM_STORE(TextBuffer)
    };
    self->abstract_mem_super.vtbl = &abstract_mem_vtbl;

    // initialize buffer and valid
    self->buffer = 0;
    self->valid  = false;
}
