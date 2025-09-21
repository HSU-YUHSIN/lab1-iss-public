#ifndef __MAIN_MEM_H__
#define __MAIN_MEM_H__

#include "abstract_mem.h"
#include "arch.h"

#define MAIN_MEM_MMAP_BASE 0x80000000
#define MAIN_MEM_SIZE 0x10000

typedef struct {
    AbstractMem super;
    byte_t mem[MAIN_MEM_SIZE];
} MainMem;

extern void MainMem_ctor(MainMem *self);

#endif
