add_executable(RiscvTestsTester riscv_tests_tester.c)
target_link_libraries(RiscvTestsTester iss)

set(TestList
    add sub sll slt sltu xor srl sra or and
    addi slli slti sltiu xori srli srai ori andi
    lb lh lw lbu lhu sb sh sw
    beq bne blt bge bltu bgeu jal jalr
    auipc lui
)

foreach(test_case IN LISTS TestList)
    add_test(NAME Test_${test_case}
             COMMAND RiscvTestsTester ${CMAKE_SOURCE_DIR}/riscv-tests/isa/rv32ui-p-${test_case})
endforeach()
