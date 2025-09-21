#include "arch.h"
#include "iss.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    ISS *iss_ptr = NULL;
    ISS_ctor(&iss_ptr, argv[1]);
    while (!ISS_get_halt(iss_ptr)) {
        ISS_step(iss_ptr, 1);
    }

    // checl value in register x3 ($gp)
    arch_state_t state = ISS_get_arch_state(iss_ptr);
    printf("gp: %x\n", state.gpr[3]);
    if (state.gpr[3] == 1) {
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
