#include "iss.h"
#include "common.h"

int main(int argc, char **argv) {
    // check argc
    Assert(argc == 2, "The number of arguments should be 2");

    // main body
    ISS *iss_ptr;
    Assert(ISS_ctor(&iss_ptr, argv[1]) == 0, "ISS_ctor failed!");
    ISS_step(iss_ptr, -1);

    // end of main
    ISS_dtor(iss_ptr);
}
