#ifndef __ISS_H__
#define __ISS_H__

#include "arch.h"

#include <stdbool.h>

// forward declaration
typedef struct iss ISS;

// for initializetion and finalization
extern int ISS_ctor(ISS **self, const char *elf_file_name);
extern void ISS_dtor(ISS *self);

// for Reference-Model-Based Verification with RTL model
extern void ISS_get_main_memory(const ISS *self,
                                const addr_t base_addr,
                                const unsigned length,
                                byte_t *const buffer);
extern void ISS_set_main_memory(ISS *self,
                                const addr_t base_addr,
                                const unsigned length,
                                const byte_t *const ref_data);
extern arch_state_t ISS_get_arch_state(const ISS *self);
extern void ISS_set_arch_state(ISS *self, const arch_state_t ref_arch_state);
extern void ISS_step(ISS *self, unsigned long n_step);
extern bool ISS_get_halt(ISS *self);

#endif
