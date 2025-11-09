#include "iss.h"
#include "arch.h"
#include "common.h"

#include <stdio.h>

int main(int argc, char **argv) {
    // check argc
    Assert(argc == 2, "The number of arguments should be 2");

    // main body
    ISS *iss_ptr;
    Assert(ISS_ctor(&iss_ptr, argv[1]) == 0, "ISS_ctor failed!");
    
    // run until halt
    printf("\n========== Running ArraySort with merge.S ==========\n");
    printf("Program output:\n");
    printf("----------------------------------------\n");
    fflush(stdout);
    unsigned long inst_count = 0;
    while (!ISS_get_halt(iss_ptr)) {
        ISS_step(iss_ptr, 1);
        fflush(stdout);  // flush after each step to see output immediately
        inst_count++;
        // prevent infinite loop
        if (inst_count > 10000000) {
            printf("\nWarning: Exceeded 10M instructions, stopping...\n");
            break;
        }
    }
    printf("\n----------------------------------------\n");
    
    printf("\n========== Execution Complete ==========\n");
    printf("Total instructions executed: %lu\n", inst_count);
    
    // get final state
    arch_state_t state = ISS_get_arch_state(iss_ptr);
    printf("\n========== Final Register State ==========\n");
    printf("PC: 0x%08x\n", state.current_pc);
    printf("a0 (x10, return value): %d (0x%08x)\n", (int)state.gpr[10], state.gpr[10]);
    printf("sp (x2, stack pointer): 0x%08x\n", state.gpr[2]);
    printf("ra (x1, return address): 0x%08x\n", state.gpr[1]);
    
    printf("\n========== Test Result ==========\n");
    if (state.gpr[10] == 0) {
        printf("✓ SUCCESS: Array sort verification passed (return value = 0)\n");
    } else {
        printf("✗ FAILED: Array sort verification failed (return value = %d)\n", (int)state.gpr[10]);
    }

    // end of main
    ISS_dtor(iss_ptr);
    
    return (state.gpr[10] == 0) ? 0 : 1;
}

