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

typedef struct yasm_dbgfmt_stabs {
    yasm_dbgfmt_base dbgfmt;	    /* base structure */

    yasm_object *object;
    yasm_symtab *symtab;
    const char *filename;
    yasm_linemap *linemap;
    yasm_arch *arch;
} yasm_dbgfmt_stabs;

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

    yasm_dbgfmt_stabs *dbgfmt_stabs;
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

/* Bytecode types */

typedef struct {
    yasm_bytecode bc;	/* base structure */
    /*@only@*/ char *str;
} stabs_bc_str;

typedef struct {
    yasm_bytecode bc;	/* base structure */
    stabs_stab *stab;
} stabs_bc_stab;

/* Bytecode callback function prototypes */

static void stabs_bc_str_destroy(yasm_bytecode *bc);
static void stabs_bc_str_print(const yasm_bytecode *bc, FILE *f, int
			       indent_level);
static yasm_bc_resolve_flags stabs_bc_str_resolve
    (yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
static int stabs_bc_str_tobytes
    (yasm_bytecode *bc, unsigned char **bufp, void *d,
     yasm_output_expr_func output_expr,
     /*@null@*/ yasm_output_reloc_func output_reloc);

static void stabs_bc_stab_destroy(yasm_bytecode *bc);
static void stabs_bc_stab_print(const yasm_bytecode *bc, FILE *f, int
				indent_level);
static yasm_bc_resolve_flags stabs_bc_stab_resolve
    (yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
static int stabs_bc_stab_tobytes
    (yasm_bytecode *bc, unsigned char **bufp, void *d,
     yasm_output_expr_func output_expr,
     /*@null@*/ yasm_output_reloc_func output_reloc);

/* Bytecode callback structures */

static const yasm_bytecode_callback stabs_bc_str_callback = {
    stabs_bc_str_destroy,
    stabs_bc_str_print,
    stabs_bc_str_resolve,
    stabs_bc_str_tobytes
};

static const yasm_bytecode_callback stabs_bc_stab_callback = {
    stabs_bc_stab_destroy,
    stabs_bc_stab_print,
    stabs_bc_stab_resolve,
    stabs_bc_stab_tobytes
};

yasm_dbgfmt_module yasm_stabs_LTX_dbgfmt;


static /*@null@*/ /*@only@*/ yasm_dbgfmt *
stabs_dbgfmt_create(const char *in_filename, const char *obj_filename,
		    yasm_object *object, yasm_objfmt *of, yasm_arch *a)
{
    yasm_dbgfmt_stabs *dbgfmt_stabs = yasm_xmalloc(sizeof(yasm_dbgfmt_stabs));
    dbgfmt_stabs->dbgfmt.module = &yasm_stabs_LTX_dbgfmt;
    dbgfmt_stabs->filename = in_filename;
    dbgfmt_stabs->object = object;
    dbgfmt_stabs->symtab = yasm_object_get_symtab(object);
    dbgfmt_stabs->linemap = yasm_object_get_linemap(object);
    dbgfmt_stabs->arch = a;
    return (yasm_dbgfmt *)dbgfmt_stabs;
}

static void
stabs_dbgfmt_destroy(/*@only@*/ yasm_dbgfmt *dbgfmt)
{
    yasm_xfree(dbgfmt);
}

/* Create and add a new strtab-style string bytecode to a section, updating
 * offset on insertion; no optimization necessary */
/* Copies the string, so you must still free yours as normal */
static yasm_bytecode *
stabs_dbgfmt_append_bcstr(yasm_section *sect, const char *str)
{
    yasm_bytecode *bc, *precbc;
    stabs_bc_str *bc_str;
   
    precbc = yasm_section_bcs_last(sect);
    bc = yasm_bc_create_common(&stabs_bc_str_callback, sizeof(stabs_bc_str),
			       0);
    bc_str = (stabs_bc_str *)bc;

    bc_str->str = yasm__xstrdup(str);

    bc->len = strlen(str)+1;
    bc->offset = precbc ? precbc->offset + precbc->len : 0;

    yasm_section_bcs_append(sect, bc);

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
    stabs_bc_stab *bc_stab;
    stabs_stab *stab = yasm_xmalloc(sizeof(stabs_stab));

    stab->other = 0;
    stab->bcstr = bcstr;
    stab->type = type;
    stab->desc = (unsigned short)desc;
    stab->symvalue = symvalue;
    stab->bcvalue = bcvalue;
    stab->value = value;

    precbc = yasm_section_bcs_last(sect);
    bc = yasm_bc_create_common(&stabs_bc_stab_callback, sizeof(stabs_bc_stab),
			       bcvalue ? bcvalue->line : 0);
    bc_stab = (stabs_bc_stab *)bc;

    bc_stab->stab = stab;

    bc->len = info->stablen;
    bc->offset = precbc ? precbc->offset + precbc->len : 0;

    yasm_section_bcs_append(sect, bc);

    info->stabcount++;
    return stab;
}

/* Update current first sym and bytecode if it's in the right section */
static int
stabs_dbgfmt_first_sym_traversal(yasm_symrec *sym, void *d)
{
    stabs_symsect *symsect = (stabs_symsect *)d;
    yasm_bytecode *precbc;

    if (!yasm_symrec_get_label(sym, &precbc))
	return 1;
    if ((precbc->section == symsect->sect)
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
stabs_dbgfmt_first_sym_by_sect(yasm_dbgfmt_stabs *dbgfmt_stabs,
			       stabs_info *info, yasm_section *sect)
{
    stabs_symsect symsect = { NULL, NULL, NULL };
    if (sect == NULL) {
	info->firstsym = NULL;
	info->firstbc = NULL;
    }

    symsect.sect = sect;
    yasm_symtab_traverse(dbgfmt_stabs->symtab, (void *)&symsect,
			 stabs_dbgfmt_first_sym_traversal);
    info->firstsym = symsect.sym;
    info->firstbc = symsect.precbc;
}

static int
stabs_dbgfmt_generate_bcs(yasm_bytecode *bc, void *d)
{
    stabs_info *info = (stabs_info *)d;
    yasm_linemap_lookup(info->dbgfmt_stabs->linemap, bc->line, &info->curfile,
			&info->curline);

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

    stabs_dbgfmt_first_sym_by_sect(info->dbgfmt_stabs, info, sect);
    if (yasm__strcasecmp(sectname, ".text")==0) {
	char *str;
	const char *symname=yasm_symrec_get_name(info->firstsym);
	str = yasm_xmalloc(strlen(symname)+4);
	strcpy(str, symname);
	strcat(str, ":F1");
	stabs_dbgfmt_append_stab(info, info->stab,
				 stabs_dbgfmt_append_bcstr(info->stabstr, str),
				 N_FUN, 0, info->firstsym, info->firstbc, 0);
	yasm_xfree(str);
    }
    yasm_section_bcs_traverse(sect, d, stabs_dbgfmt_generate_bcs);

    return 1;
}

static void
stabs_dbgfmt_generate(yasm_dbgfmt *dbgfmt)
{
    yasm_dbgfmt_stabs *dbgfmt_stabs = (yasm_dbgfmt_stabs *)dbgfmt;
    stabs_info info;
    int new;
    yasm_bytecode *dbgbc;
    stabs_bc_stab *dbgbc_stab;
    stabs_stab *stab;
    yasm_bytecode *filebc, *nullbc, *laststr;
    yasm_section *stext;

    /* Stablen is determined by arch/machine */
    if (yasm__strcasecmp(yasm_arch_keyword(dbgfmt_stabs->arch), "x86") == 0) {
        info.stablen = 12;
    }
    else /* unknown machine; generate nothing */
	return;

    info.dbgfmt_stabs = dbgfmt_stabs;
    info.lastline = 0;
    info.stabcount = 0;
    info.stab = yasm_object_get_general(dbgfmt_stabs->object, ".stab", 0, 0,
					&new, 0);
    if (!new) {
	yasm_bytecode *last = yasm_section_bcs_last(info.stab);
	if (last == NULL)
	    yasm__error(yasm_section_bcs_first(info.stab)->line,
		N_("stabs debugging conflicts with user-defined section .stab"));
	else
	    yasm__warning(YASM_WARN_GENERAL, 0,
		N_("stabs debugging overrides empty section .stab"));
    }

    info.stabstr = yasm_object_get_general(dbgfmt_stabs->object, ".stabstr", 0,
					   0, &new, 0);
    if (!new) {
	yasm_bytecode *last = yasm_section_bcs_last(info.stabstr);
	if (last == NULL)
	    yasm__error(yasm_section_bcs_first(info.stabstr)->line,
		N_("stabs debugging conflicts with user-defined section .stabstr"));
	else
	    yasm__warning(YASM_WARN_GENERAL, 0,
		N_("stabs debugging overrides empty section .stabstr"));
    }



    /* initial pseudo-stab */
    stab = yasm_xmalloc(sizeof(stabs_stab));
    dbgbc = yasm_bc_create_common(&stabs_bc_stab_callback,
				  sizeof(stabs_bc_stab), 0);
    dbgbc_stab = (stabs_bc_stab *)dbgbc;

    dbgbc->len = info.stablen;
    dbgbc_stab->stab = stab;

    yasm_section_bcs_append(info.stab, dbgbc);

    /* initial strtab bytecodes */
    nullbc = stabs_dbgfmt_append_bcstr(info.stabstr, "");
    filebc = stabs_dbgfmt_append_bcstr(info.stabstr, dbgfmt_stabs->filename);

    stext = yasm_object_find_general(dbgfmt_stabs->object, ".text");
    info.firstsym = yasm_symtab_use(dbgfmt_stabs->symtab, ".text", 0);
    info.firstbc = yasm_section_bcs_first(stext);
    /* N_SO file stab */
    stabs_dbgfmt_append_stab(&info, info.stab, filebc, N_SO, 0,
			     info.firstsym, info.firstbc, 0);

    yasm_object_sections_traverse(dbgfmt_stabs->object, (void *)&info,
				  stabs_dbgfmt_generate_sections);

    /* fill initial pseudo-stab's fields */
    laststr = yasm_section_bcs_last(info.stabstr);
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
stabs_bc_stab_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		      yasm_output_expr_func output_expr,
		      yasm_output_reloc_func output_reloc)
{
    /* This entire function, essentially the core of rendering stabs to a file,
     * needs to become endian aware.  Size appears not to be an issue, as known
     * 64-bit systems use truncated values in 32-bit fields. */

    stabs_bc_stab *bc_stab = (stabs_bc_stab *)bc;
    unsigned char *buf = *bufp;
    const stabs_stab *stab = bc_stab->stab;

    YASM_WRITE_32_L(buf, stab->bcstr ? stab->bcstr->offset : 0);
    YASM_WRITE_8(buf, stab->type);
    YASM_WRITE_8(buf, stab->other);
    YASM_WRITE_16_L(buf, stab->desc);

    if (stab->symvalue != NULL) {
	bc->offset += 8;
	output_reloc(stab->symvalue, bc, buf, 4, 32, 0, 0, d);
	bc->offset -= 8;
	buf += 4;
    }
    else if (stab->bcvalue != NULL) {
	YASM_WRITE_32_L(buf, stab->bcvalue->offset);
    }
    else {
	YASM_WRITE_32_L(buf, stab->value);
    }

    *bufp = buf;
    return 0;
}

static int
stabs_bc_str_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		     yasm_output_expr_func output_expr,
		     yasm_output_reloc_func output_reloc)
{
    stabs_bc_str *bc_str = (stabs_bc_str *)bc;
    unsigned char *buf = *bufp;
    const char *str = bc_str->str;

    strcpy((char *)buf, str);
    buf += strlen(str)+1;

    *bufp = buf;
    return 0;
}

static void
stabs_bc_stab_destroy(yasm_bytecode *bc)
{
    stabs_bc_stab *bc_stab = (stabs_bc_stab *)bc;
    yasm_xfree(bc_stab->stab);
}

static void
stabs_bc_str_destroy(yasm_bytecode *bc)
{
    stabs_bc_str *bc_str = (stabs_bc_str *)bc;
    yasm_xfree(bc_str->str);
}

static void
stabs_bc_stab_print(const yasm_bytecode *bc, FILE *f, int indent_level)
{
    const stabs_bc_stab *bc_stab = (const stabs_bc_stab *)bc;
    const stabs_stab *stab = bc_stab->stab;
    const char *str = "";
    fprintf(f, "%*s.stabs \"%s\", 0x%x, 0x%x, 0x%x, 0x%lx\n",
	    indent_level, "", str, stab->type, stab->other, stab->desc,
	    stab->bcvalue ? stab->bcvalue->offset : stab->value);
}

static void
stabs_bc_str_print(const yasm_bytecode *bc, FILE *f, int indent_level)
{
    const stabs_bc_str *bc_str = (const stabs_bc_str *)bc;
    fprintf(f, "%*s\"%s\"\n", indent_level, "", bc_str->str);
}

static yasm_bc_resolve_flags
stabs_bc_stab_resolve(yasm_bytecode *bc, int save,
		      yasm_calc_bc_dist_func calc_bc_dist)
{
    yasm_internal_error(N_("tried to resolve a stabs stab bytecode"));
    /*@notreached@*/
    return YASM_BC_RESOLVE_MIN_LEN;
}

static yasm_bc_resolve_flags
stabs_bc_str_resolve(yasm_bytecode *bc, int save,
		     yasm_calc_bc_dist_func calc_bc_dist)
{
    yasm_internal_error(N_("tried to resolve a stabs str bytecode"));
    /*@notreached@*/
    return YASM_BC_RESOLVE_MIN_LEN;
}

static int
stabs_dbgfmt_directive(yasm_dbgfmt *dbgfmt, const char *name,
		       yasm_valparamhead *valparams, unsigned long line)
{
    return 1;
}

/* Define dbgfmt structure -- see dbgfmt.h for details */
yasm_dbgfmt_module yasm_stabs_LTX_dbgfmt = {
    YASM_DBGFMT_VERSION,
    "Stabs debugging format",
    "stabs",
    stabs_dbgfmt_create,
    stabs_dbgfmt_destroy,
    stabs_dbgfmt_directive,
    stabs_dbgfmt_generate
};
