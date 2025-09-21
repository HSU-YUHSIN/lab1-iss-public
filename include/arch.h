#ifndef __ARCH_H__
#define __ARCH_H__

#include <stdint.h>

// common types
typedef uint8_t byte_t;
typedef uint32_t reg_t;
typedef uint32_t addr_t;

// architectural states of a ISS
typedef struct arch_state {
    reg_t current_pc; // Program Counter
    reg_t gpr[32];    // General Purpose Registers (x0-x31)
} arch_state_t;

#endif
