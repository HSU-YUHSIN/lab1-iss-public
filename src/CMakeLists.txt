add_library(iss)
add_executable(main)
add_executable(test_merge)

set(LIB_SRCS
    iss.c
    core.c
    main_mem.c
    rom.c
    halt.c
    text_buffer.c
    mem_map.c
    load_elf.c
    tick.c
    abstract_mem.c
)
target_sources(iss PRIVATE ${LIB_SRCS})
target_sources(main PRIVATE main.c)
target_sources(test_merge PRIVATE test_merge.c)

target_link_libraries(main PRIVATE iss)
target_link_libraries(test_merge PRIVATE iss)

target_include_directories(iss
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(main
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
target_include_directories(test_merge
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(iss
    PRIVATE
        # -Wall -Wextra -Wpedantic -Werror
        -Wall -Werror
)
target_compile_options(main
    PRIVATE
        # -Wall -Wextra -Wpedantic -Werror
        -Wall -Werror
)
target_compile_options(test_merge
    PRIVATE
        -Wall -Werror
)
