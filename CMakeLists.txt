# CMake version requirements
cmake_minimum_required(VERSION 3.21...3.28)
if(${CMAKE_VERSION} VERSION_LESS 3.28)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION} ${CMAKE_MINOR_VERSION})
endif()

# project information
project(RISCV-ISS VERSION 0.1.0
        DESCRIPTION "A RISC-V ISA Simulator"
        LANGUAGES C)

# set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# add src subdirectory
add_subdirectory(src)

# # Testing
if (PROJECT_IS_TOP_LEVEL)
    include(CTest)
    if (BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
