add_executable(RiscvTestsTester riscv_tests_tester.c)
target_link_libraries(RiscvTestsTester iss)

#######################################
# There are 37 instructions in total. #
#######################################
set(OPCODE_LIST OP OPIMM LOAD STORE BRANCH JAL JALR LUI AUIPC) # 9 opcodes
set(OP_INST add sub sll slt sltu xor srl sra or and) # 10 insts.
set(OPIMM_INST addi slli slti sltiu xori srli srai ori andi) # 9 insts.
set(LOAD_INST lb lh lw lbu lhu) # 5 insts.
set(STORE_INST sb sh sw) # 3 insts.
set(BRANCH_INST beq bne blt bge bltu bgeu) # 6 insts.
set(JAL_INST jal)
set(JALR_INST jalr)
set(LUI_INST lui)
set(AUIPC_INST auipc)

foreach(opcode IN LISTS OPCODE_LIST)
    foreach(inst IN LISTS ${opcode}_INST)
        add_test(NAME ${opcode}_${inst}
                 COMMAND RiscvTestsTester ${CMAKE_SOURCE_DIR}/riscv-tests/isa/rv32ui-p-${inst})
    endforeach()
endforeach()
