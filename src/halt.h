#ifndef __HALT_H__
#define __HALT_H__

#include "abstract_mem.h"

#include <stdbool.h>

#define HALT_MMAP_BASE 0xfffffffc
#define HALT_SIZE 0x1

typedef struct {
    // super classes
    AbstractMem super;
    // internal data
    bool halt_flag;
} Halt;

void Halt_ctor(Halt *self);

#endif
