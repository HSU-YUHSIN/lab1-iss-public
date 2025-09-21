#include "tick.h"

#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

static void _tick(Tick *const self) {
    assert(false);
    fprintf(stderr, "%s\n", "Please implement the virtual function!");
}

void Tick_ctor(Tick *const self) {
    assert(self != NULL);
    static struct TickVtbl const vtbl = { .tick = &_tick };
    self->vtbl                        = &vtbl;
}

void Tick_tick(Tick *const self) {
    assert((self != NULL) && (self->vtbl != NULL));
    self->vtbl->tick(self);
}
