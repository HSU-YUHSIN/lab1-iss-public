#ifndef __ROM_H__
#define __ROM_H__

#include "abstract_mem.h"
#include "arch.h"

#define ROM_MMAP_BASE 0x0
#define ROM_SIZE 0x1000

typedef struct {
    // parent class
    AbstractMem super;

    // the rom itself
    byte_t rom[ROM_SIZE];
} ROM;

extern void ROM_ctor(ROM *self);

#endif
