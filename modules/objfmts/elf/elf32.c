/*
 * ELF-32 utility functions
 *
 *  Copyright (C) 2003  Peter Johnson
 *
 *  This file is part of YASM.
 *
 *  YASM is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  YASM is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#define YASM_LIB_INTERNAL
#include "libyasm.h"
/*@unused@*/ RCSID("$IdPath$");

#include "elf32.h"

/* Converts an ELF-32 header into little endian bytes */
void
elf32_write_elf_header(unsigned char **bufp, Elf32_Ehdr *ehdr)
{
    int i;

    for (i=0; i<EI_NIDENT; i++)
	YASM_WRITE_8(*bufp, ehdr->e_ident[i]);
    YASM_WRITE_16_L(*bufp, ehdr->e_type);
    YASM_WRITE_16_L(*bufp, ehdr->e_machine);
    YASM_WRITE_32_L(*bufp, ehdr->e_version);
    YASM_WRITE_32_L(*bufp, ehdr->e_entry);
    YASM_WRITE_32_L(*bufp, ehdr->e_phoff);
    YASM_WRITE_32_L(*bufp, ehdr->e_shoff);
    YASM_WRITE_32_L(*bufp, ehdr->e_flags);
    YASM_WRITE_16_L(*bufp, ehdr->e_ehsize);
    YASM_WRITE_16_L(*bufp, ehdr->e_phentsize);
    YASM_WRITE_16_L(*bufp, ehdr->e_phnum);
    YASM_WRITE_16_L(*bufp, ehdr->e_shentsize);
    YASM_WRITE_16_L(*bufp, ehdr->e_shnum);
    YASM_WRITE_16_L(*bufp, ehdr->e_shstrndx);
}

/* Converts an ELF-32 section header into little endian bytes */
void
elf32_write_sect_header(unsigned char **bufp, Elf32_Shdr *shdr)
{
    YASM_WRITE_32_L(*bufp, shdr->sh_name);
    YASM_WRITE_32_L(*bufp, shdr->sh_type);
    YASM_WRITE_32_L(*bufp, shdr->sh_flags);
    YASM_WRITE_32_L(*bufp, shdr->sh_addr);
    YASM_WRITE_32_L(*bufp, shdr->sh_offset);
    YASM_WRITE_32_L(*bufp, shdr->sh_size);
    YASM_WRITE_32_L(*bufp, shdr->sh_link);
    YASM_WRITE_32_L(*bufp, shdr->sh_info);
    YASM_WRITE_32_L(*bufp, shdr->sh_addralign);
    YASM_WRITE_32_L(*bufp, shdr->sh_entsize);
}

void
elf32_print_sect_header(FILE *f, int indent_level, Elf32_Shdr *shdr)
{
    static const char *sh_type_names[] = {
	"null", "progbits", "symtab", "strtab", "rela", "hash", "dynamic",
	"note", "nobits", "rel", "shlib", "dynsym"
    };

    fprintf(f, "%*ssh_name=%lu\n", indent_level, "", shdr->sh_name);
    fprintf(f, "%*ssh_type=", indent_level, "");
    if (shdr->sh_type < NELEMS(sh_type_names))
	fprintf(f, "%s", sh_type_names[shdr->sh_type]);
    else
	fprintf(f, "%lu", shdr->sh_type);
    fprintf(f, "\n%*ssh_flags=%lx\n", indent_level, "", shdr->sh_flags);
    fprintf(f, "%*ssh_addr=0x%lx\n", indent_level, "", shdr->sh_addr);
    fprintf(f, "%*ssh_offset=0x%lx\n", indent_level, "", shdr->sh_offset);
    fprintf(f, "%*ssh_size=0x%lx\n", indent_level, "", shdr->sh_size);
    fprintf(f, "%*ssh_link=%lu\n", indent_level, "", shdr->sh_link);
    fprintf(f, "%*ssh_info=%lu\n", indent_level, "", shdr->sh_info);
    fprintf(f, "%*ssh_addralign=%lu\n", indent_level, "", shdr->sh_addralign);
    fprintf(f, "%*ssh_entsize=%lu\n", indent_level, "", shdr->sh_entsize);
}

/* Converts an ELF-32 relocation (non-addend) entry into little endian bytes */
void
elf32_write_rel(unsigned char **bufp, Elf32_Rel *rel)
{
    YASM_WRITE_32_L(*bufp, rel->r_offset);
    YASM_WRITE_32_L(*bufp, rel->r_info);
}

/* Converts an ELF-32 symbol table entry into little endian bytes */
void
elf32_write_sym(unsigned char **bufp, Elf32_Sym *sym)
{
    YASM_WRITE_32_L(*bufp, sym->st_name);
    YASM_WRITE_32_L(*bufp, sym->st_value);
    YASM_WRITE_32_L(*bufp, sym->st_size);
    YASM_WRITE_8(*bufp, sym->st_info);
    YASM_WRITE_8(*bufp, sym->st_other);
    YASM_WRITE_16_L(*bufp, sym->st_shndx);
}
