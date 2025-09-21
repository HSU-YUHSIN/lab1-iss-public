#ifndef __LOAD_ELF_H__
#define __LOAD_ELF_H__

#include "arch.h"

extern void
load_elf(const char *file_name, byte_t *buffer, unsigned long buffer_size, reg_t *entry_pc);

#endif
