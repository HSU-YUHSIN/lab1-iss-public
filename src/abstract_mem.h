#ifndef __ABSTRACT_MEM_H__
#define __ABSTRACT_MEM_H__

#include "arch.h"

struct AbstractMemVtbl; // forward declaration
typedef struct {
    struct AbstractMemVtbl const *vtbl; // vtable ptr
} AbstractMem;

// define virtual table attribute
struct AbstractMemVtbl {
    void (*load)(const AbstractMem *self, addr_t base_addr, unsigned length, byte_t *buffer);
    void (*store)(AbstractMem *self, addr_t base_addr, unsigned length, const byte_t *ref_data);
};

// define public APIs
extern void AbstractMem_ctor(AbstractMem *self);
extern void
AbstractMem_load(const AbstractMem *self, addr_t base_addr, unsigned length, byte_t *buffer);
extern void
AbstractMem_store(AbstractMem *self, addr_t base_addr, unsigned length, const byte_t *ref_data);

// define helper macros
// clang-format off
#define SIGNATURE_ABSTRACT_MEM_LOAD(cls) cls##_AbstractMem_load
#define DECLARE_ABSTRACT_MEM_LOAD(cls)                                          \
    void (SIGNATURE_ABSTRACT_MEM_LOAD(cls))(const AbstractMem *self,            \
                                           addr_t base_addr, unsigned length,   \
                                           byte_t *buffer)
#define SIGNATURE_ABSTRACT_MEM_STORE(cls) cls##_AbstractMem_store
#define DECLARE_ABSTRACT_MEM_STORE(cls)                                         \
    void (SIGNATURE_ABSTRACT_MEM_STORE(cls))(AbstractMem * self,                \
                                            addr_t base_addr, unsigned length,  \
                                            const byte_t *ref_data)
// clang-format on

#endif
