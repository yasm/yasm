/*
 * Stabs debugging format
 *
 *  Copyright (C) 2003  Michael Urman
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
#include <util.h>
/*@unused@*/ RCSID("$IdPath$");

#define YASM_LIB_INTERNAL
#define YASM_BC_INTERNAL
#include <libyasm.h>

typedef enum {
    N_UNDF = 0x00,	/* Undefined */
    N_GSYM = 0x20,	/* Global symbol */
    N_FNAME = 0x22,	/* Function name (BSD Fortran) */
    N_FUN = 0x24,	/* Function name or Text segment variable */
    N_STSYM = 0x26,	/* Data segment file-scope variable */
    N_LCSYM = 0x28,	/* BSS segment file-scope variable */
    N_MAIN = 0x2a,	/* Name of main routine */
    N_ROSYM = 0x2c,	/* Variable in .rodata section */
    N_PC = 0x30,	/* Global symbol (Pascal) */
    N_SYMS = 0x32,	/* Number of symbols (Ultrix V4.0) */
    N_NOMAP = 0x34,	/* No DST map */
    N_OBJ = 0x38,	/* Object file (Solaris2) */
    N_OPT = 0x3c,	/* Debugger options (Solaris2) */
    N_RSYM = 0x40,	/* Register variable */
    N_M2C = 0x42,	/* Modula-2 compilation unit */
    N_SLINE = 0x44,	/* Line numbers in .text segment */
    N_DSLINE = 0x46,	/* Line numbers in .data segment */
    N_BSLINE = 0x48,	/* Line numbers in .bss segment */
    N_BROWS = 0x48,	/* Source code .cb file's path */
    N_DEFD = 0x4a,	/* GNU Modula-2 definition module dependency */
    N_FLINE = 0x4c,	/* Function start/body/end line numbers (Solaris2) */
    N_EHDECL = 0x50,	/* GNU C++ exception variable */
    N_MOD2 = 0x50,	/* Modula2 info for imc (Ultrix V4.0) */
    N_CATCH = 0x54,	/* GNU C++ catch clause */
    N_SSYM = 0x60,	/* Structure or union element */
    N_ENDM = 0x62,	/* Last stab for module (Solaris2) */
    N_SO = 0x64,	/* Path and name of source files */
    N_LSYM = 0x80,	/* Stack variable */
    N_BINCL = 0x84,	/* Beginning of include file */
    N_SOL = 0x84,	/* Name of include file */
    N_PSYM = 0xa0,	/* Parameter variable */
    N_EINCL = 0xa2,	/* End of include file */
    N_ENTRY = 0xa4,	/* Alternate entry point */
    N_LBRAC = 0xc0,	/* Beginning of lexical block */
    N_EXCL = 0xc2,	/* Placeholder for a deleted include file */
    N_SCOPE = 0xc4,	/* Modula 2 scope info (Sun) */
    N_RBRAC = 0xe0,	/* End of lexical block */
    N_BCOMM = 0xe2,	/* Begin named common block */
    N_ECOMM = 0xe4,	/* End named common block */
    N_ECOML = 0xe8,	/* Member of common block */
    N_WITH = 0xea,	/* Pascal with statement: type,,0,0,offset (Solaris2) */
    N_NBTEXT = 0xf0,	/* Gould non-base registers */
    N_NBDATA = 0xf2,	/* Gould non-base registers */
    N_NBBSS = 0xf4,	/* Gould non-base registers */
    N_NBSTS = 0xf6,	/* Gould non-base registers */
    N_NBLCS = 0xf8	/* Gould non-base registers */
} stabs_stab_type;

#define STABS_DEBUG_DATA 1
#define STABS_DEBUG_STR 2

typedef struct {
    unsigned long lastline;	/* track line and file of bytecodes */
    unsigned long curline;
    const char *lastfile;
    const char *curfile;

    unsigned int stablen;	/* size of a stab for current machine */
    unsigned long stabcount;	/* count stored stabs; doesn't include first */

    yasm_section *stab;		/* sections to which stabs, stabstrs appended */
    yasm_section *stabstr;
    yasm_symrec *firstsym;	/* track leading sym of section/function */
    yasm_bytecode *firstbc;	/* and its bytecode */
} stabs_info;

typedef struct {
    /*@null@*/ yasm_bytecode *bcstr;	/* bytecode in stabstr for string */
    stabs_stab_type type;		/* stab type: N_* */
    unsigned char other;		/* unused, but stored here anyway */
    unsigned short desc;		/* description element of a stab */
    /*@null@*/ yasm_symrec *symvalue;	/* value element needing relocation */
    /*@null@*/yasm_bytecode *bcvalue;	/* relocated stab's bytecode */
    unsigned long value;		/* fallthrough value if above NULL */
} stabs_stab;

/* helper struct for finding first sym (and bytecode) of a section */
typedef struct {
    yasm_symrec *sym;
    yasm_bytecode *precbc;
    yasm_section *sect;
} stabs_symsect;

yasm_dbgfmt yasm_stabs_LTX_dbgfmt;

static yasm_objfmt *cur_objfmt = NULL;
static const char *filename = NULL;
static yasm_linemgr *linemgr = NULL;
static yasm_arch *cur_arch = NULL;
static const char *cur_machine = NULL;
static size_t stabs_relocsize_bits = 0;
static size_t stabs_relocsize_bytes = 0;

static int
stabs_dbgfmt_initialize(const char *in_filename, const char *obj_filename,
			yasm_linemgr *lm, yasm_objfmt *of, yasm_arch *a,
			const char *machine)
{
    cur_objfmt = of;
    filename = in_filename;
    linemgr = lm;
    cur_arch = a;
    cur_machine = machine;
    return 0;
}

static void
stabs_dbgfmt_cleanup(void)
{
}

/* Create and add a new strtab-style string bytecode to a section, updating
 * offset on insertion; no optimization necessary */
/* Copies the string, so you must still free yours as normal */
static yasm_bytecode *
stabs_dbgfmt_append_bcstr(yasm_section *sect, const char *str)
{
    yasm_bytecode *bc = yasm_bc_new_dbgfmt_data(
			    STABS_DEBUG_STR, strlen(str)+1,
			    &yasm_stabs_LTX_dbgfmt,
			    (void *)yasm__xstrdup(str), 0);
    yasm_bytecodehead *bcs = yasm_section_get_bytecodes(sect);
    yasm_bytecode *precbc = yasm_bcs_last(bcs);
    bc->offset = precbc ? precbc->offset + precbc->len : 0;
    yasm_bcs_append(bcs, bc);

    return bc;
}

/* Create and add a new stab bytecode to a section, updating offset on
 * insertion; no optimization necessary. */
/* Requires a string bytecode, or NULL, for its string entry */
static stabs_stab *
stabs_dbgfmt_append_stab(stabs_info *info, yasm_section *sect,
			 /*@null@*/ yasm_bytecode *bcstr, stabs_stab_type type,
			 unsigned long desc, /*@null@*/ yasm_symrec *symvalue,
			 /*@null@*/ yasm_bytecode *bcvalue, unsigned long value)
{
    yasm_bytecode *bc, *precbc;
    yasm_bytecodehead *bcs;
    stabs_stab *stab = yasm_xmalloc(sizeof(stabs_stab));
    stab->other = 0;
    stab->bcstr = bcstr;
    stab->type = type;
    stab->desc = (unsigned short)desc;
    stab->symvalue = symvalue;
    stab->bcvalue = bcvalue;
    stab->value = value;

    bc = yasm_bc_new_dbgfmt_data(STABS_DEBUG_DATA, info->stablen,
				 &yasm_stabs_LTX_dbgfmt, (void *)stab,
				 bcvalue ? bcvalue->line : 0);
    bcs = yasm_section_get_bytecodes(sect);
    precbc = yasm_bcs_last(bcs);
    bc->offset = precbc ? precbc->offset + precbc->len : 0;
    yasm_bcs_append(bcs, bc);
    info->stabcount++;
    return stab;
}

/* Update current first sym and bytecode if it's in the right section */
static int
stabs_dbgfmt_first_sym_traversal(yasm_symrec *sym, void *d)
{
    stabs_symsect *symsect = (stabs_symsect *)d;
    yasm_section *sect;
    yasm_bytecode *precbc;

    if (!yasm_symrec_get_label(sym, &sect, &precbc))
	return 1;
    if (precbc == NULL)
	precbc = yasm_bcs_first(yasm_section_get_bytecodes(sect));
    if ((sect == symsect->sect)
	&& ((symsect->sym == NULL)
	    || precbc->offset < symsect->precbc->offset))
    {
	symsect->sym = sym;
	symsect->precbc = precbc;
    }
    return 1;
}

/* Find the first sym and its preceding bytecode in a given section */
static void
stabs_dbgfmt_first_sym_by_sect(stabs_info *info, yasm_section *sect)
{
    stabs_symsect symsect = { NULL, NULL, NULL };
    if (sect == NULL) {
	info->firstsym = NULL;
	info->firstbc = NULL;
    }

    symsect.sect = sect;
    yasm_symrec_traverse((void *)&symsect, stabs_dbgfmt_first_sym_traversal);
    info->firstsym = symsect.sym;
    info->firstbc = symsect.precbc;
}

static int
stabs_dbgfmt_generate_bcs(yasm_bytecode *bc, void *d)
{
    stabs_info *info = (stabs_info *)d;
    linemgr->lookup(bc->line, &info->curfile, &info->curline);

    if (info->lastfile != info->curfile) {
	info->lastline = 0; /* new file, so line changes */
	/*stabs_dbgfmt_append_stab(info, info->stab,
	    stabs_dbgfmt_append_bcstr(info->stabstr, info->curfile),
	    N_SOL, 0, NULL, bc, 0);*/
    }
    if (info->curline != info->lastline) {
	info->lastline = bc->line;
	stabs_dbgfmt_append_stab(info, info->stab, NULL, N_SLINE,
				 info->curline, NULL, NULL,
				 bc->offset - info->firstbc->offset);
    }

    info->lastline = info->curline;
    info->lastfile = info->curfile;

    return 0;
}

static int
stabs_dbgfmt_generate_sections(yasm_section *sect, /*@null@*/ void *d)
{
    stabs_info *info = (stabs_info *)d;
    const char *sectname=yasm_section_get_name(sect);

    stabs_dbgfmt_first_sym_by_sect(info, sect);
    if (yasm__strcasecmp(sectname, ".text")==0) {
	char *str;
	const char *symname=yasm_symrec_get_name(info->firstsym);
	size_t len = strlen(symname)+4;
	str = yasm_xmalloc(len);
	snprintf(str, len, "%s:F1", symname);
	stabs_dbgfmt_append_stab(info, info->stab,
				 stabs_dbgfmt_append_bcstr(info->stabstr, str),
				 N_FUN, 0, info->firstsym, info->firstbc, 0);
	yasm_xfree(str);
    }
    yasm_bcs_traverse(yasm_section_get_bytecodes(sect), d,
		      stabs_dbgfmt_generate_bcs);

    return 1;
}

static void
stabs_dbgfmt_generate(yasm_sectionhead *sections)
{
    stabs_info info;
    int new;
    yasm_bytecode *dbgbc;
    yasm_bytecodehead *bcs;
    stabs_stab *stab;
    yasm_bytecode *filebc, *nullbc, *laststr;
    yasm_section *stext;

    /* Stablen is determined by arch/machine */
    if (yasm__strcasecmp(cur_arch->keyword, "x86") == 0) {
	if (yasm__strcasecmp(cur_machine, "x86") == 0) {
	    info.stablen = 12;
	}
	else if (yasm__strcasecmp(cur_machine, "amd64") == 0) {
	    info.stablen = 16;
	}
	else
	    return;
    }
    else /* unknown machine; generate nothing */
	return;

    stabs_relocsize_bytes = info.stablen - 8;
    stabs_relocsize_bits = stabs_relocsize_bytes * 8;

    info.lastline = 0;
    info.stabcount = 0;
    info.stab = yasm_sections_switch_general(sections, ".stab", 0, 0, &new, 0);
    if (!new) {
	yasm_bytecode *last = yasm_bcs_last(
	    yasm_section_get_bytecodes(info.stab));
	if (last == NULL)
	    yasm__error(
		yasm_bcs_first(yasm_section_get_bytecodes(info.stab))->line,
		N_("stabs debugging conflicts with user-defined section .stab"));
	else
	    yasm__warning(YASM_WARN_GENERAL, 0,
		N_("stabs debugging overrides empty section .stab"));
    }

    info.stabstr = yasm_sections_switch_general(sections, ".stabstr", 0, 0,
						&new, 0);
    if (!new) {
	yasm_bytecode *last = yasm_bcs_last(
	    yasm_section_get_bytecodes(info.stabstr));
	if (last == NULL)
	    yasm__error(
		yasm_bcs_first(yasm_section_get_bytecodes(info.stabstr))->line,
		N_("stabs debugging conflicts with user-defined section .stabstr"));
	else
	    yasm__warning(YASM_WARN_GENERAL, 0,
		N_("stabs debugging overrides empty section .stabstr"));
    }



    /* initial pseudo-stab */
    stab = yasm_xmalloc(sizeof(stabs_stab));
    dbgbc = yasm_bc_new_dbgfmt_data(STABS_DEBUG_DATA, info.stablen,
				    &yasm_stabs_LTX_dbgfmt, (void *)stab, 0);
    bcs = yasm_section_get_bytecodes(info.stab);
    yasm_bcs_append(bcs, dbgbc);

    /* initial strtab bytecodes */
    nullbc = stabs_dbgfmt_append_bcstr(info.stabstr, "");
    filebc = stabs_dbgfmt_append_bcstr(info.stabstr, filename);

    stext = yasm_sections_find_general(sections, ".text");
    info.firstsym = yasm_symrec_use(".text", 0);
    info.firstbc = yasm_bcs_first(yasm_section_get_bytecodes(stext));
    /* N_SO file stab */
    stabs_dbgfmt_append_stab(&info, info.stab, filebc, N_SO, 0,
			     info.firstsym, info.firstbc, 0);

    yasm_sections_traverse(sections, (void *)&info,
			   stabs_dbgfmt_generate_sections);

    /* fill initial pseudo-stab's fields */
    laststr = yasm_bcs_last(yasm_section_get_bytecodes(info.stabstr));
    if (laststr == NULL)
	yasm_internal_error(".stabstr has no entries");

    stab->bcvalue = NULL;
    stab->symvalue = NULL;
    stab->value = laststr->offset + laststr->len;
    stab->bcstr = filebc;
    stab->type = N_UNDF;
    stab->other = 0;
    stab->desc = info.stabcount;
}

static int
stabs_dbgfmt_bc_data_output(yasm_bytecode *bc, unsigned int type,
			    const void *data, unsigned char **buf,
			    const yasm_section *sect,
			    yasm_output_reloc_func output_reloc, void *objfmt_d)
{
    unsigned char *bufp = *buf;
    if (type == STABS_DEBUG_DATA) {
	const stabs_stab *stab = data;

	YASM_WRITE_32_L(bufp, stab->bcstr ? stab->bcstr->offset : 0);
	YASM_WRITE_8(bufp, stab->type);
	YASM_WRITE_8(bufp, stab->other);
	YASM_WRITE_16_L(bufp, stab->desc);

	if (stab->symvalue != NULL) {
	    printf("DBG: ");
	    bc->offset += 8;
	    output_reloc(stab->symvalue, bc, bufp, stabs_relocsize_bytes,
			 stabs_relocsize_bits, 0, 0, sect, objfmt_d);
	    bc->offset -= 8;
	    bufp += stabs_relocsize_bytes;
	}
	else if (stab->bcvalue != NULL) {
	    YASM_WRITE_32_L(bufp, stab->bcvalue->offset);
	}
	else {
	    YASM_WRITE_32_L(bufp, stab->value);
	}
    }
    else if (type == STABS_DEBUG_STR) {
	const char *str = data;
	strcpy((char *)bufp, str);
	bufp += strlen(str)+1;
    }

    *buf = bufp;
    return 0;
}

static void
stabs_dbgfmt_bc_data_delete(unsigned int type, void *data)
{
    /* both stabs and strs are allocated at the top level pointer */
    yasm_xfree(data);
}

static void
stabs_dbgfmt_bc_data_print(FILE *f, int indent_level, unsigned int type,
		    const void *data)
{
    if (type == STABS_DEBUG_DATA) {
	const stabs_stab *stab = data;
	const char *str = "";
	fprintf(f, "%*s.stabs \"%s\", 0x%x, 0x%x, 0x%x, 0x%lx\n",
		indent_level, "", str, stab->type, stab->other, stab->desc,
		stab->bcvalue ? stab->bcvalue->offset : stab->value);
    }
    else if (type == STABS_DEBUG_STR)
	fprintf(f, "%*s\"%s\"\n", indent_level, "", (const char *)data);
}

/* Define dbgfmt structure -- see dbgfmt.h for details */
yasm_dbgfmt yasm_stabs_LTX_dbgfmt = {
    YASM_DBGFMT_VERSION,
    "Stabs debugging format",
    "stabs",
    stabs_dbgfmt_initialize,
    stabs_dbgfmt_cleanup,
    NULL /*stabs_dbgfmt_directive*/,
    stabs_dbgfmt_generate,
    stabs_dbgfmt_bc_data_output,
    stabs_dbgfmt_bc_data_delete,
    stabs_dbgfmt_bc_data_print
};
