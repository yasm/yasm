/*
 * Extended Dynamic Object format dumper
 *
 *  Copyright (C) 2004-2007  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>

#include "xdf.h"

void
print_file_header(const FILE_HEADER *filehead)
{
    printf("File Header:\n");
    printf("\tMagic=0x%08X\n", filehead->f_magic);
    printf("\tNumber of Sections=%d\n", filehead->f_nsect);
    printf("\tNumber of Symbols=%d\n", filehead->f_nsyms);
    printf("\tTotal Headers Size=%d\n", filehead->f_size);
}

void
read_syment(SYMBOL_ENTRY *syment, FILE *f)
{
    fread(&syment->e_sect_idx, sizeof(syment->e_sect_idx), 1, f);
    fread(&syment->e_sect_off, sizeof(syment->e_sect_off), 1, f);
    fread(&syment->e_name_off, sizeof(syment->e_name_off), 1, f);
    fread(&syment->e_flags, sizeof(syment->e_flags), 1, f);
}

const char *
get_syment_name(const SYMBOL_ENTRY *syment, FILE *f)
{
    static char symname[256];
    long oldpos = ftell(f);
    fseek(f, syment->e_name_off, SEEK_SET);
    fread(symname, 256, 1, f);
    symname[255] = '\0';
    fseek(f, oldpos, SEEK_SET);
    return symname;
}

const char *
get_sym_name(u32 idx, FILE *f, size_t symtab_off)
{
    SYMBOL_ENTRY syment;
    long oldpos = ftell(f);
    fseek(f, symtab_off+idx*SYMBOL_ENTRY_SIZE, SEEK_SET);
    read_syment(&syment, f);
    fseek(f, oldpos, SEEK_SET);
    return get_syment_name(&syment, f);
}

const char *
get_sect_name(u32 idx, FILE *f, size_t symtab_off, size_t secttab_off)
{
    SECTION_HEADER secthead;
    long oldpos = ftell(f);
    fseek(f, secttab_off+idx*SECTION_HEADER_SIZE, SEEK_SET);
    fread(&secthead.s_name_idx, sizeof(secthead.s_name_idx), 1, f);
    fseek(f, oldpos, SEEK_SET);
    return get_sym_name(secthead.s_name_idx, f, symtab_off);
}

void
print_symbol(const SYMBOL_ENTRY *syment, FILE *f, size_t symtab_off,
             size_t secttab_off)
{
    int first = 1;
    printf("\tOffset=0x%08X Flags=", syment->e_sect_off);
    if (syment->e_flags & XDF_SYM_EXTERN)
        printf("%sEXTERN", first-->0?"":"|");
    if (syment->e_flags & XDF_SYM_GLOBAL)
        printf("%sGLOBAL", first-->0?"":"|");
    if (syment->e_flags & XDF_SYM_EQU)
        printf("%sEQU", first-->0?"":"|");
    if (first>0)
        printf("None");
    printf(" Name=`%s' Section=", get_syment_name(syment, f));
    if (syment->e_sect_idx >= 0x80000000)
        printf("%d\n", syment->e_sect_idx);
    else
        printf("`%s' (%d)\n",
               get_sect_name(syment->e_sect_idx, f, symtab_off, secttab_off),
               syment->e_sect_idx);
}

void
print_reloc(const RELOCATION_ENTRY *relocent, FILE *f, size_t symtab_off)
{
    const char *type = "UNK";
    switch (relocent->r_type) {
        case XDF_RELOC_REL:
            type = "REL";
            break;
        case XDF_RELOC_WRT:
            type = "WRT";
            break;
        case XDF_RELOC_RIP:
            type = "RIP";
            break;
        case XDF_RELOC_SEG:
            type = "SEG";
            break;
    }
    printf("\t Offset=0x%08X Type=%s Size=%d Shift=%d Target=`%s' (%d)", 
           relocent->r_off, type, relocent->r_size, relocent->r_shift,
           get_sym_name(relocent->r_targ_idx, f, symtab_off),
           relocent->r_targ_idx);
    if (relocent->r_type == XDF_RELOC_WRT)
        printf(" Base=`%s' (%d)",
           get_sym_name(relocent->r_base_idx, f, symtab_off),
           relocent->r_base_idx);
    printf("\n");
}

void
print_section(const SECTION_HEADER *secthead, FILE *f, size_t symtab_off)
{
    int first = 1;
    u32 i;
    printf("Section `%s' (section name symtab %d):\n",
           get_sym_name(secthead->s_name_idx, f, symtab_off),
           secthead->s_name_idx);
    printf("\tPhysical Address=0x%016llX\n", secthead->s_addr);
    printf("\tVirtual Address=0x%016llX\n", secthead->s_vaddr);
    printf("\tAlign=%d\n", secthead->s_align);
    printf("\tFlags=");
    if (secthead->s_flags & XDF_SECT_ABSOLUTE)
        printf("%sABSOLUTE", first-->0?"":"|");
    if (secthead->s_flags & XDF_SECT_FLAT)
        printf("%sFLAT", first-->0?"":"|");
    if (secthead->s_flags & XDF_SECT_BSS)
        printf("%sBSS", first-->0?"":"|");
    if (secthead->s_flags & XDF_SECT_USE_16)
        printf("%sUSE16", first-->0?"":"|");
    if (secthead->s_flags & XDF_SECT_USE_32)
        printf("%sUSE32", first-->0?"":"|");
    if (secthead->s_flags & XDF_SECT_USE_64)
        printf("%sUSE64", first-->0?"":"|");
    if (first>0)
        printf("None");
    printf("\n\tData Offset=0x%08X\n", secthead->s_data_off);
    printf("\tData Size=%d\n", secthead->s_data_size);
    if (!(secthead->s_flags & XDF_SECT_BSS) && secthead->s_data_size > 0) {
        printf("\tSection Data:");
        long oldpos = ftell(f);
        fseek(f, secthead->s_data_off, SEEK_SET);
        for (i=0; i<secthead->s_data_size; i++) {
            if (i % 16 == 0)
                printf("\n\t\t%08X:", i);
            if (i % 2 == 0)
                printf(" ");
            printf("%02X", fgetc(f));
        }
        printf("\n");
        fseek(f, oldpos, SEEK_SET);
    }
    printf("\tReloc Table Offset=0x%08X\n", secthead->s_reltab_off);
    printf("\tNum Relocs=%d\n", secthead->s_num_reloc);
    if (secthead->s_num_reloc > 0) {
        printf("\tRelocations:\n");
        long oldpos = ftell(f);
        fseek(f, secthead->s_reltab_off, SEEK_SET);
        for (i=0; i<secthead->s_num_reloc; i++) {
            RELOCATION_ENTRY relocent;
            fread(&relocent, sizeof(relocent), 1, f);
            print_reloc(&relocent, f, symtab_off);
        }
        fseek(f, oldpos, SEEK_SET);
    }
}

int
main(int argc, char **argv)
{
    FILE *f;
    FILE_HEADER filehead;
    size_t symtab_off;
    u32 i;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <xdf>\n", argv[0]);
        return EXIT_FAILURE;
    }

    f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "Could not open `%s'\n", argv[1]);
        return EXIT_FAILURE;
    }

    fread(&filehead.f_magic, sizeof(filehead.f_magic), 1, f);
    fread(&filehead.f_nsect, sizeof(filehead.f_nsect), 1, f);
    fread(&filehead.f_nsyms, sizeof(filehead.f_nsyms), 1, f);
    fread(&filehead.f_size, sizeof(filehead.f_size), 1, f);

    if (filehead.f_magic != XDF_MAGIC) {
        fprintf(stderr, "Magic number mismatch (expected %08X, got %08X\n",
                XDF_MAGIC, filehead.f_magic);
        return EXIT_FAILURE;
    }

    print_file_header(&filehead);
    symtab_off = FILE_HEADER_SIZE+filehead.f_nsect*SECTION_HEADER_SIZE;
    for (i=0; i<filehead.f_nsect; i++) {
        SECTION_HEADER secthead;
        fread(&secthead.s_name_idx, sizeof(secthead.s_name_idx), 1, f);
        fread(&secthead.s_addr, sizeof(secthead.s_addr), 1, f);
        fread(&secthead.s_vaddr, sizeof(secthead.s_vaddr), 1, f);
        fread(&secthead.s_align, sizeof(secthead.s_align), 1, f);
        fread(&secthead.s_flags, sizeof(secthead.s_flags), 1, f);
        fread(&secthead.s_data_off, sizeof(secthead.s_data_off), 1, f);
        fread(&secthead.s_data_size, sizeof(secthead.s_data_size), 1, f);
        fread(&secthead.s_reltab_off, sizeof(secthead.s_reltab_off), 1, f);
        fread(&secthead.s_num_reloc, sizeof(secthead.s_num_reloc), 1, f);
        print_section(&secthead, f, symtab_off);
    }

    printf("Symbol Table:\n");
    for (i=0; i<filehead.f_nsyms; i++) {
        SYMBOL_ENTRY syment;
        read_syment(&syment, f);
        print_symbol(&syment, f, symtab_off, sizeof(FILE_HEADER));
    }

    return EXIT_SUCCESS;
}

