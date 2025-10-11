#include "load_elf.h"

#include "arch.h"
#include "common.h"
#include "main_mem.h"

#include <elf.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

void load_elf(const char *file_name, byte_t *buffer, unsigned long buffer_size, reg_t *entry_pc) {
    /* try to open ELF file */
    FILE *f = fopen(file_name, "rb");
    Assert(f != NULL, "Fail to open file: %s", file_name);

    /* read ELF header */
    Elf32_Ehdr elf_header;
    if (fread(&elf_header, sizeof(Elf32_Ehdr), 1, f) != 1) {
        Panic("Failed to load ELF header from the file: %s\n", file_name);
        goto end;
    }

    /* check ELF magic number */
    if (memcmp(elf_header.e_ident, ELFMAG, SELFMAG) != 0) {
        Panic("NOt a valid ELF file");
        goto end;
    }

    /* check ELF Class (32 or 64-bits) */
    if (elf_header.e_ident[EI_CLASS] != ELFCLASS32) {
        Panic("Only 32-bits ELF files are supported");
        goto end;
    }

    /* check the ISA of ELF */
    if (elf_header.e_machine != EM_RISCV) {
        Panic("Only RISC-V ARchitecture ELF files are supported");
        goto end;
    }

    /* get the entry-point of the ELF file */
    reg_t entry = elf_header.e_entry;
    *entry_pc   = entry;
    LOG("Initialize Program Counter: 0x%08x\n", entry);

    /* try to read Program Header */
    for (int i = 0; i < elf_header.e_phnum; i++) {
        /* try to load program header of each sections */
        if (fseek(f, elf_header.e_phoff + i * sizeof(Elf32_Phdr), SEEK_SET) != 0) {
            Panic("fail to load program header");
            goto end;
        }
        Elf32_Phdr prog_header;
        if (fread(&prog_header, sizeof(Elf32_Phdr), 1, f) != 1) {
            Panic("Fail to read the file: %s", file_name);
            goto end;
        }

        /* try to load each "loadable" sections into buffer */
        if (prog_header.p_type == PT_LOAD && prog_header.p_filesz > 0) {
            if (fseek(f, prog_header.p_offset, SEEK_SET) != 0) {
                fprintf(stderr, "Fail to seek the file\n");
                goto end;
            }
            LOG("Load a lodable segment with p_paddr 0x%08x, p_memsz 0x%08x "
                "and p_filesz: 0x%08x\n",
                prog_header.p_paddr, prog_header.p_memsz, prog_header.p_filesz);
            if (fread(&buffer[prog_header.p_paddr - MAIN_MEM_MMAP_BASE],
                      prog_header.p_filesz, 1, f) != 1) {
                fprintf(stderr, "Failed to load section in ELF file\n");
                goto end;
            }
        }
    }

end:
    fclose(f);
}

