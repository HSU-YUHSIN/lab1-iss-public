# Lab 1 Assignment Report

## Student Information

Please type your name and student ID at here:

- Student Name: 許宇欣
- Student ID: E24131398

## Questions (20 pts)

There are **five** questions, and it is **4 pts** for each question.

### Question 1 

**What is the benefit for a processor of using the MMIO scheme to control I/O devices?**

#### You Answer

MMIO lets processor control I/O devices using the same load/store instructions and hardware used for normal memory, avoid special instructions.

### Question 2

**What components in a RISC-V processor are considered part of the architectural visible state, and why are they important for program execution?**

#### Your Answer

The architecture visible state of a RISC-V processor includes the PC, general purpose and floating point register, control/status registers, memory, MMIO, privilege mode.
They must be saved across context switches, and what the ISA guarantees to software.

### Question 3

**Why does RISC-V define conditional set instructions? Can we use branch instructions only to achieve the same functionality? *If yes, what is the benefit of having the additional conditional set instructions?***

#### Your Answer

They enable branchless conditional logic.
We can use branch instructions only to achieve the same functionality, but having the additional conditional set instructions can avoid control flow changes, reduce code size, improve efficiency and si,plify compiler output.

### Question 4

**What is the different between `ROM` and `MainMem`? Answer this question by tracing the source code.**

#### Your Answer

ROM is "Read-Only Memory." 

For instance :
MemoryMap_generic_store(&self->mem_map, addr, 4, buf);
(1) if addr in ROM: calls ROM_ops.store  -> returns error/assert/no-op
(2) if addr in RAM: calls MainMem_ops.store -> writes 4 bytes

### Question 5

**Consider the instruction: jalr xN, xN, imm, where N is a positive number from 1 to 31.**
**What potential problem might arise when implementing the JALR instruction in an ISS, particularly for this special case where the destination register is the same as the source register?**

#### Your Answer

If ISS execute like:
___________________________________
|  uint32_t target = regs[rs1] + imm;
|  regs[rd] = pc + 4;
|  pc = target & ~1;
_____________________________________
->the target vlaue will be computed from the modified register

### Question 6 (2 pts for bonus)

**Why do the encoding formats for JAL and BRANCH instructions omit the least-significant bit (LSB, `imm[0]`) of immediates?**

#### Your Answer

Instruction addresses in RISC-V are always aligned, therefore, the LSB of imm is always 0, so ISA omits it to save one bit and reconstructs it by left-shifting the imm when computing target addr.

## Reflection Report (0 pts)

In this section, please write a short reflection about the lab:

1. What did you learn from completing this assignment?
2. What challenges did you encounter, and how did you solve them?
3. What feedback would you like to give to the TAs regarding this lab?

### You Report

1. I learned how to make a simple ISS in C for decoing and executing RISC-V instructions. Also, I learned how the computer read from the binary code and execute then deocode and execute them.
2. There are so many instructions so I need to write many cases, any missing could leads to some bug. I solve it by test multiple times and see what the test result is then fix them.
3. The explainatons and notes TA gave in class really helps alots, so I can save some time to watch anime.
