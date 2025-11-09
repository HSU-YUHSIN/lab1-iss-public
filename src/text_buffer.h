#ifndef __TEXT_BUFFER_H__
#define __TEXT_BUFFER_H__

#include "arch.h"
#include "abstract_mem.h"
#include "tick.h"

#include <stdbool.h>

#define TEXT_BUFFER_MMAP_BASE 0xffffffe0
#define TEXT_BUFFER_SIZE 0x1

typedef struct {
    // derived base class
    AbstractMem abstract_mem_super;
    Tick tick_super;
    // buffer for "one" character (one byte)
    bool valid;
    byte_t buffer;
} TextBuffer;

void TextBuffer_ctor(TextBuffer *self);

#endif
