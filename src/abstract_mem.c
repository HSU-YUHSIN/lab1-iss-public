#include "abstract_mem.h"

#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

static void _load(const AbstractMem *self, addr_t base_addr, unsigned length, byte_t *buffer) {
    assert(false);
    fprintf(stderr, "%s\n", "Please implement the virtual function!");
}

static void _store(AbstractMem *self, addr_t base_addr, unsigned length, const byte_t *ref_data) {
    assert(false);
    fprintf(stderr, "%s\n", "Please implement the virtual function!");
}

void AbstractMem_ctor(AbstractMem *self) {
    assert(self != NULL);
    static struct AbstractMemVtbl const vtbl = { .load = &_load, .store = &_store };
    self->vtbl = &vtbl;
}

void AbstractMem_load(const AbstractMem *self, addr_t base_addr, unsigned length, byte_t *buffer) {
    assert((self != NULL) && (self->vtbl != NULL));
    self->vtbl->load(self, base_addr, length, buffer);
}

void AbstractMem_store(AbstractMem *self, addr_t base_addr, unsigned length, const byte_t *ref_data) {
    assert((self != NULL) && (self->vtbl != NULL));
    self->vtbl->store(self, base_addr, length, ref_data);
}
