/*
 * COFF (DJGPP) object format
 *
 *  Copyright (C) 2002  Peter Johnson
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
/*@unused@*/ RCSID("$Id$");

#define YASM_LIB_INTERNAL
#define YASM_BC_INTERNAL
#define YASM_EXPR_INTERNAL
#include <libyasm.h>


#define REGULAR_OUTBUF_SIZE	1024

/* Defining this to 0 sets all section VMA's to 0 rather than as the same as
 * the LMA.  According to the DJGPP COFF Spec, this should be set to 1
 * (VMA=LMA), and indeed DJGPP's GCC output shows VMA=LMA.  However, NASM
 * outputs VMA=0 (as if this was 0), and GNU objdump output looks a lot nicer
 * with VMA=0.  Who's right?  This is #defined as changing this setting affects
 * several places in the code.
 */
#define COFF_SET_VMA	(!objfmt_coff->win32)

#define COFF_MACHINE_I386	0x014C
#define COFF_MACHINE_AMD64	0x8664

#define COFF_F_LNNO	0x0004	    /* line number info NOT present */
#define COFF_F_LSYMS	0x0008	    /* local symbols NOT present */
#define COFF_F_AR32WR	0x0100	    /* 32-bit little endian file */

typedef struct coff_reloc {
    yasm_reloc reloc;
    enum {
	COFF_RELOC_ABSOLUTE = 0,	    /* absolute, no reloc needed */

	/* I386 relocations */
	COFF_RELOC_I386_ADDR16 = 0x1,	    /* 16-bit absolute reference */
	COFF_RELOC_I386_REL16 = 0x2,	    /* 16-bit PC-relative reference */
	COFF_RELOC_I386_ADDR32 = 0x6,	    /* 32-bit absolute reference */
	COFF_RELOC_I386_ADDR32NB = 0x7,	    /* 32-bit absolute ref w/o base */
	COFF_RELOC_I386_SEG12 = 0x9,	    /* 16-bit absolute segment ref */
	COFF_RELOC_I386_SECTION = 0xA,	    /* section index */
	COFF_RELOC_I386_SECREL = 0xB,	    /* offset from start of segment */
	COFF_RELOC_I386_TOKEN = 0xC,	    /* CLR metadata token */
	COFF_RELOC_I386_SECREL7 = 0xD,	/* 7-bit offset from base of sect */
	COFF_RELOC_I386_REL32 = 0x14,	    /* 32-bit PC-relative reference */

	/* AMD64 relocations */
	COFF_RELOC_AMD64_ADDR64 = 0x1,	    /* 64-bit address (VA) */
	COFF_RELOC_AMD64_ADDR32 = 0x2,	    /* 32-bit address (VA) */
	COFF_RELOC_AMD64_ADDR32NB = 0x3,    /* 32-bit address w/o base (RVA) */
	COFF_RELOC_AMD64_REL32 = 0x4,	    /* 32-bit relative (0 byte dist) */
	COFF_RELOC_AMD64_REL32_1 = 0x5,	    /* 32-bit relative (1 byte dist) */
	COFF_RELOC_AMD64_REL32_2 = 0x6,	    /* 32-bit relative (2 byte dist) */
	COFF_RELOC_AMD64_REL32_3 = 0x7,	    /* 32-bit relative (3 byte dist) */
	COFF_RELOC_AMD64_REL32_4 = 0x8,	    /* 32-bit relative (4 byte dist) */
	COFF_RELOC_AMD64_REL32_5 = 0x9,	    /* 32-bit relative (5 byte dist) */
	COFF_RELOC_AMD64_SECTION = 0xA,	    /* section index */
	COFF_RELOC_AMD64_SECREL = 0xB,	/* 32-bit offset from base of sect */
	COFF_RELOC_AMD64_SECREL7 = 0xC,	/* 7-bit offset from base of sect */
	COFF_RELOC_AMD64_TOKEN = 0xD	    /* CLR metadata token */
    } type;			    /* type of relocation */
} coff_reloc;

#define COFF_STYP_TEXT		0x00000020UL
#define COFF_STYP_DATA		0x00000040UL
#define COFF_STYP_BSS		0x00000080UL
#define COFF_STYP_INFO		0x00000200UL
#define COFF_STYP_STD_MASK	0x000003FFUL
#define COFF_STYP_ALIGN_MASK	0x00F00000UL
#define COFF_STYP_ALIGN_SHIFT	20
#define COFF_STYP_NRELOC_OVFL	0x01000000UL
#define COFF_STYP_DISCARD	0x02000000UL
#define COFF_STYP_NOCACHE	0x04000000UL
#define COFF_STYP_NOPAGE	0x08000000UL
#define COFF_STYP_SHARED	0x10000000UL
#define COFF_STYP_EXECUTE	0x20000000UL
#define COFF_STYP_READ		0x40000000UL
#define COFF_STYP_WRITE		0x80000000UL
#define COFF_STYP_WIN32_MASK	0xFF000000UL

#define COFF_FLAG_NOBASE	(1UL<<0)    /* Use no-base (NB) relocs */

typedef struct coff_section_data {
    /*@dependent@*/ yasm_symrec *sym;	/* symbol created for this section */
    unsigned int scnum;	    /* section number (1=first section) */
    unsigned long flags;    /* section flags (see COFF_STYP_* above) */
    unsigned long addr;	    /* starting memory address (first section -> 0) */
    unsigned long scnptr;   /* file ptr to raw data */
    unsigned long size;	    /* size of raw data (section data) in bytes */
    unsigned long relptr;   /* file ptr to relocation */
    unsigned long nreloc;   /* number of relocation entries >64k -> error */
    unsigned long flags2;   /* internal flags (see COFF_FLAG_* above) */
    unsigned long strtab_name;	/* strtab offset of name if name > 8 chars */
    int isdebug;	    /* is a debug section? */
} coff_section_data;

typedef enum coff_symrec_sclass {
    COFF_SCL_EFCN = 0xff,	/* physical end of function	*/
    COFF_SCL_NULL = 0,
    COFF_SCL_AUTO = 1,		/* automatic variable		*/
    COFF_SCL_EXT = 2,		/* external symbol		*/
    COFF_SCL_STAT = 3,		/* static			*/
    COFF_SCL_REG = 4,		/* register variable		*/
    COFF_SCL_EXTDEF = 5,	/* external definition		*/
    COFF_SCL_LABEL = 6,		/* label			*/
    COFF_SCL_ULABEL = 7,	/* undefined label		*/
    COFF_SCL_MOS = 8,		/* member of structure		*/
    COFF_SCL_ARG = 9,		/* function argument		*/
    COFF_SCL_STRTAG = 10,	/* structure tag		*/
    COFF_SCL_MOU = 11,		/* member of union		*/
    COFF_SCL_UNTAG = 12,	/* union tag			*/
    COFF_SCL_TPDEF = 13,	/* type definition		*/
    COFF_SCL_USTATIC = 14,	/* undefined static		*/
    COFF_SCL_ENTAG = 15,	/* enumeration tag		*/
    COFF_SCL_MOE = 16,		/* member of enumeration	*/
    COFF_SCL_REGPARM = 17,	/* register parameter		*/
    COFF_SCL_FIELD = 18,	/* bit field			*/
    COFF_SCL_AUTOARG = 19,	/* auto argument		*/
    COFF_SCL_LASTENT = 20,	/* dummy entry (end of block)	*/
    COFF_SCL_BLOCK = 100,	/* ".bb" or ".eb"		*/
    COFF_SCL_FCN = 101,		/* ".bf" or ".ef"		*/
    COFF_SCL_EOS = 102,		/* end of structure		*/
    COFF_SCL_FILE = 103,	/* file name			*/
    COFF_SCL_LINE = 104,	/* line # reformatted as symbol table entry */
    COFF_SCL_ALIAS = 105,	/* duplicate tag		*/
    COFF_SCL_HIDDEN = 106	/* ext symbol in dmert public lib */
} coff_symrec_sclass;

typedef union coff_symtab_auxent {
    /* no data needed for section symbol auxent, all info avail from sym */
    /*@owned@*/ char *fname;	    /* filename aux entry */
} coff_symtab_auxent;

typedef enum coff_symtab_auxtype {
    COFF_SYMTAB_AUX_NONE = 0,
    COFF_SYMTAB_AUX_SECT,
    COFF_SYMTAB_AUX_FILE
} coff_symtab_auxtype;

typedef struct coff_symrec_data {
    unsigned long index;		/* assigned COFF symbol table index */
    coff_symrec_sclass sclass;		/* storage class */

    /*@owned@*/ /*@null@*/ yasm_expr *size; /* size if COMMON declaration */

    int numaux;			/* number of auxiliary entries */
    coff_symtab_auxtype auxtype;    /* type of aux entries */
    coff_symtab_auxent aux[1];	/* actually may be any size (including 0) */
} coff_symrec_data;

typedef struct yasm_objfmt_coff {
    yasm_objfmt_base objfmt;		    /* base structure*/

    unsigned int parse_scnum;		    /* sect numbering in parser */
    int win32;				    /* nonzero for win32/64 output */
    int win64;				    /* nonzero for win64 output */

    unsigned int machine;		    /* COFF machine to use */

    yasm_object *object;
    yasm_symtab *symtab;
    /*@dependent@*/ yasm_arch *arch;

    coff_symrec_data *filesym_data;	    /* Data for .file symbol */
} yasm_objfmt_coff;

typedef struct coff_objfmt_output_info {
    yasm_objfmt_coff *objfmt_coff;
    yasm_errwarns *errwarns;
    /*@dependent@*/ FILE *f;
    /*@only@*/ unsigned char *buf;
    yasm_section *sect;
    /*@dependent@*/ coff_section_data *csd;
    unsigned long addr;			/* start of next section */

    unsigned long indx;			/* current symbol index */
    int all_syms;			/* outputting all symbols? */
    unsigned long strtab_offset;	/* current string table offset */
} coff_objfmt_output_info;

static void coff_section_data_destroy(/*@only@*/ void *d);
static void coff_section_data_print(void *data, FILE *f, int indent_level);

static const yasm_assoc_data_callback coff_section_data_cb = {
    coff_section_data_destroy,
    coff_section_data_print
};

static void coff_symrec_data_destroy(/*@only@*/ void *d);
static void coff_symrec_data_print(void *data, FILE *f, int indent_level);

static const yasm_assoc_data_callback coff_symrec_data_cb = {
    coff_symrec_data_destroy,
    coff_symrec_data_print
};

yasm_objfmt_module yasm_coff_LTX_objfmt;
yasm_objfmt_module yasm_win32_LTX_objfmt;
yasm_objfmt_module yasm_win64_LTX_objfmt;


static /*@dependent@*/ coff_symrec_data *
coff_objfmt_sym_set_data(yasm_symrec *sym, coff_symrec_sclass sclass,
			 /*@only@*/ /*@null@*/ yasm_expr *size, int numaux,
			 coff_symtab_auxtype auxtype)
{
    coff_symrec_data *sym_data;

    sym_data = yasm_xmalloc(sizeof(coff_symrec_data) +
			    (numaux-1)*sizeof(coff_symtab_auxent));
    sym_data->index = 0;
    sym_data->sclass = sclass;
    sym_data->size = size;
    sym_data->numaux = numaux;
    sym_data->auxtype = auxtype;

    yasm_symrec_add_data(sym, &coff_symrec_data_cb, sym_data);

    return sym_data;
}

static yasm_objfmt_coff *
coff_common_create(yasm_object *object, yasm_arch *a)
{
    yasm_objfmt_coff *objfmt_coff = yasm_xmalloc(sizeof(yasm_objfmt_coff));
    yasm_symrec *filesym;

    objfmt_coff->object = object;
    objfmt_coff->symtab = yasm_object_get_symtab(object);
    objfmt_coff->arch = a;

    /* Only support x86 arch */
    if (yasm__strcasecmp(yasm_arch_keyword(a), "x86") != 0) {
	yasm_xfree(objfmt_coff);
	return NULL;
    }

    objfmt_coff->parse_scnum = 1;    /* section numbering starts at 1 */

    /* FIXME: misuse of NULL bytecode here; it works, but only barely. */
    filesym = yasm_symtab_define_special(objfmt_coff->symtab, ".file",
					 YASM_SYM_GLOBAL);
    objfmt_coff->filesym_data =
	coff_objfmt_sym_set_data(filesym, COFF_SCL_FILE, NULL, 1,
				 COFF_SYMTAB_AUX_FILE);
    /* Filename is set in coff_objfmt_output */
    objfmt_coff->filesym_data->aux[0].fname = NULL;

    return objfmt_coff;
}

static yasm_objfmt *
coff_objfmt_create(yasm_object *object, yasm_arch *a)
{
    yasm_objfmt_coff *objfmt_coff = coff_common_create(object, a);

    if (objfmt_coff) {
	/* Support x86 and amd64 machines of x86 arch */
	if (yasm__strcasecmp(yasm_arch_get_machine(a), "x86") == 0) {
	    objfmt_coff->machine = COFF_MACHINE_I386;
	} else if (yasm__strcasecmp(yasm_arch_get_machine(a), "amd64") == 0) {
	    objfmt_coff->machine = COFF_MACHINE_AMD64;
	} else {
	    yasm_xfree(objfmt_coff);
	    return NULL;
	}

	objfmt_coff->objfmt.module = &yasm_coff_LTX_objfmt;
	objfmt_coff->win32 = 0;
	objfmt_coff->win64 = 0;
    }
    return (yasm_objfmt *)objfmt_coff;
}

static yasm_objfmt *
win32_objfmt_create(yasm_object *object, yasm_arch *a)
{
    yasm_objfmt_coff *objfmt_coff = coff_common_create(object, a);

    if (objfmt_coff) {
	/* Support x86 and amd64 machines of x86 arch.
	 * (amd64 machine supported for backwards compatibility)
	 */
	if (yasm__strcasecmp(yasm_arch_get_machine(a), "x86") == 0) {
	    objfmt_coff->machine = COFF_MACHINE_I386;
	    objfmt_coff->objfmt.module = &yasm_win32_LTX_objfmt;
	} else if (yasm__strcasecmp(yasm_arch_get_machine(a), "amd64") == 0) {
	    objfmt_coff->machine = COFF_MACHINE_AMD64;
	    objfmt_coff->objfmt.module = &yasm_win64_LTX_objfmt;
	    objfmt_coff->win64 = 1;
	} else {
	    yasm_xfree(objfmt_coff);
	    return NULL;
	}

	objfmt_coff->win32 = 1;
    }
    return (yasm_objfmt *)objfmt_coff;
}

static yasm_objfmt *
win64_objfmt_create(yasm_object *object, yasm_arch *a)
{
    yasm_objfmt_coff *objfmt_coff = coff_common_create(object, a);

    if (objfmt_coff) {
	/* Support amd64 machine of x86 arch */
	if (yasm__strcasecmp(yasm_arch_get_machine(a), "amd64") == 0) {
	    objfmt_coff->machine = COFF_MACHINE_AMD64;
	} else {
	    yasm_xfree(objfmt_coff);
	    return NULL;
	}

	objfmt_coff->objfmt.module = &yasm_win64_LTX_objfmt;
	objfmt_coff->win32 = 1;
	objfmt_coff->win64 = 1;
    }
    return (yasm_objfmt *)objfmt_coff;
}

static coff_section_data *
coff_objfmt_init_new_section(yasm_objfmt_coff *objfmt_coff, yasm_section *sect,
			     const char *sectname, unsigned long line)
{
    coff_section_data *data;
    yasm_symrec *sym;

    data = yasm_xmalloc(sizeof(coff_section_data));
    data->scnum = objfmt_coff->parse_scnum++;
    data->flags = 0;
    data->addr = 0;
    data->scnptr = 0;
    data->size = 0;
    data->relptr = 0;
    data->nreloc = 0;
    data->flags2 = 0;
    data->strtab_name = 0;
    data->isdebug = 0;
    yasm_section_add_data(sect, &coff_section_data_cb, data);

    sym = yasm_symtab_define_label(objfmt_coff->symtab, sectname,
				   yasm_section_bcs_first(sect), 1, line);
    yasm_symrec_declare(sym, YASM_SYM_GLOBAL, line);
    coff_objfmt_sym_set_data(sym, COFF_SCL_STAT, NULL, 1,
			     COFF_SYMTAB_AUX_SECT);
    data->sym = sym;
    return data;
}

static int
coff_objfmt_init_remaining_section(yasm_section *sect, /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    /*@dependent@*/ /*@null@*/ coff_section_data *csd;

    /* Skip absolute sections */
    if (yasm_section_is_absolute(sect))
	return 0;

    assert(info != NULL);
    csd = yasm_section_get_data(sect, &coff_section_data_cb);
    if (!csd) {
	/* Initialize new one */
	const char *sectname = yasm_section_get_name(sect);
	csd = coff_objfmt_init_new_section(info->objfmt_coff, sect, sectname,
					   0);
	if (yasm__strncasecmp(sectname, ".debug", 6)==0) {
	    csd->flags = COFF_STYP_DATA;
	    if (info->objfmt_coff->win32)
		csd->flags |= COFF_STYP_DISCARD|COFF_STYP_READ;
	    csd->isdebug = 1;
	} else
	    csd->flags = COFF_STYP_TEXT;
    }

    return 0;
}

static int
coff_objfmt_set_section_addr(yasm_section *sect, /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    /*@dependent@*/ /*@null@*/ coff_section_data *csd;

    /* Don't output absolute sections */
    if (yasm_section_is_absolute(sect))
	return 0;

    assert(info != NULL);
    csd = yasm_section_get_data(sect, &coff_section_data_cb);
    assert(csd != NULL);

    csd->addr = info->addr;
    info->addr += yasm_bc_next_offset(yasm_section_bcs_last(sect));

    return 0;
}

static int
coff_objfmt_output_value(yasm_value *value, unsigned char *buf, size_t destsize,
			 unsigned long offset, yasm_bytecode *bc, int warn,
			 /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    yasm_objfmt_coff *objfmt_coff;
    /*@only@*/ /*@null@*/ yasm_intnum *dist = NULL;
    /*@dependent@*/ /*@null@*/ yasm_intnum *intn;
    unsigned long intn_val, intn_minus;
    int retval;
    unsigned int valsize = value->size;

    assert(info != NULL);
    objfmt_coff = info->objfmt_coff;

    if (value->abs)
	value->abs = yasm_expr_simplify(value->abs, 1);

    /* Try to output constant and PC-relative section-local first.
     * Note this does NOT output any value with a SEG, WRT, external,
     * cross-section, or non-PC-relative reference (those are handled below).
     */
    switch (yasm_value_output_basic(value, buf, destsize, bc, warn,
				    info->objfmt_coff->arch)) {
	case -1:
	    return 1;
	case 0:
	    break;
	default:
	    return 0;
    }

    /* Handle other expressions, with relocation if necessary */
    if (value->rshift > 0
	|| (value->seg_of && (value->wrt || value->curpos_rel))
	|| (value->section_rel && (value->wrt || value->curpos_rel))) {
	yasm_error_set(YASM_ERROR_TOO_COMPLEX,
		       N_("coff: relocation too complex"));
	return 1;
    }

    intn_val = 0;
    intn_minus = 0;
    if (value->rel) {
	yasm_sym_vis vis = yasm_symrec_get_visibility(value->rel);
	/*@dependent@*/ /*@null@*/ yasm_symrec *sym = value->rel;
	unsigned long addr;
	coff_reloc *reloc;

	/* Sometimes we want the relocation to be generated against one
	 * symbol but the value generated correspond to a different symbol.
	 * This is done through (sym being referenced) WRT (sym used for
	 * reloc).  Note both syms need to be in the same section!
	 */
	if (value->wrt) {
	    /*@dependent@*/ /*@null@*/ yasm_bytecode *rel_precbc, *wrt_precbc;
	    if (!yasm_symrec_get_label(sym, &rel_precbc)
		|| !yasm_symrec_get_label(value->wrt, &wrt_precbc)) {
		yasm_error_set(YASM_ERROR_TOO_COMPLEX,
			       N_("coff: wrt expression too complex"));
		return 1;
	    }
	    dist = yasm_calc_bc_dist(wrt_precbc, rel_precbc);
	    if (!dist) {
		yasm_error_set(YASM_ERROR_TOO_COMPLEX,
			       N_("coff: cannot wrt across sections"));
		return 1;
	    }
	    sym = value->wrt;
	}

	if (vis & YASM_SYM_COMMON) {
	    /* In standard COFF, COMMON symbols have their length added in */
	    if (!objfmt_coff->win32) {
		/*@dependent@*/ /*@null@*/ coff_symrec_data *csymd;
		/*@dependent@*/ /*@null@*/ yasm_intnum *common_size;

		csymd = yasm_symrec_get_data(sym, &coff_symrec_data_cb);
		assert(csymd != NULL);
		common_size = yasm_expr_get_intnum(&csymd->size, 1);
		if (!common_size) {
		    yasm_error_set(YASM_ERROR_TOO_COMPLEX,
				   N_("coff: common size too complex"));
		    return 1;
		}

		if (yasm_intnum_sign(common_size) < 0) {
		    yasm_error_set(YASM_ERROR_VALUE,
				   N_("coff: common size is negative"));
		    return 1;
		}

		intn_val += yasm_intnum_get_uint(common_size);
	    }
	} else if (!(vis & YASM_SYM_EXTERN) && !objfmt_coff->win64) {
	    /*@dependent@*/ /*@null@*/ yasm_bytecode *sym_precbc;

	    /* Local symbols need relocation to their section's start */
	    if (yasm_symrec_get_label(sym, &sym_precbc)) {
		yasm_section *sym_sect = yasm_bc_get_section(sym_precbc);
		/*@null@*/ coff_section_data *sym_csd;
		sym_csd = yasm_section_get_data(sym_sect,
						&coff_section_data_cb);
		assert(sym_csd != NULL);
		sym = sym_csd->sym;
		intn_val = yasm_bc_next_offset(sym_precbc);
		if (COFF_SET_VMA)
		    intn_val += sym_csd->addr;
	    }
	}

	if (value->curpos_rel) {
	    /* For standard COFF, need to adjust to start of section, e.g.
	     * subtract out the bytecode offset.
	     * For Win32 COFF, need to adjust based on value size and position.
	     * For Win64 COFF that's IP-relative, adjust to next bytecode;
	     * the difference between the offset+destsize and BC length is
	     * taken care of by special relocation types.
	     */
	    if (objfmt_coff->win64 && value->ip_rel)
		intn_val += bc->len*bc->mult_int;
	    else if (objfmt_coff->win32)
		intn_val += offset+destsize;
	    else
		intn_minus = bc->offset;
	}

	if (value->seg_of || value->section_rel) {
	    /* Segment or section-relative generation; zero value. */
	    intn_val = 0;
	    intn_minus = 0;
	}

	/* Generate reloc */
	reloc = yasm_xmalloc(sizeof(coff_reloc));
	addr = bc->offset + offset;
	if (COFF_SET_VMA)
	    addr += info->addr;
	reloc->reloc.addr = yasm_intnum_create_uint(addr);
	reloc->reloc.sym = sym;

	if (value->curpos_rel) {
	    if (objfmt_coff->machine == COFF_MACHINE_I386) {
		if (valsize == 32)
		    reloc->type = COFF_RELOC_I386_REL32;
		else {
		    yasm_error_set(YASM_ERROR_TYPE,
				   N_("coff: invalid relocation size"));
		    return 1;
		}
	    } else if (objfmt_coff->machine == COFF_MACHINE_AMD64) {
		if (valsize != 32) {
		    yasm_error_set(YASM_ERROR_TYPE,
				   N_("coff: invalid relocation size"));
		    return 1;
		}
		if (!value->ip_rel)
		    reloc->type = COFF_RELOC_AMD64_REL32;
		else switch (bc->len*bc->mult_int - (offset+destsize)) {
		    case 0:
			reloc->type = COFF_RELOC_AMD64_REL32;
			break;
		    case 1:
			reloc->type = COFF_RELOC_AMD64_REL32_1;
			break;
		    case 2:
			reloc->type = COFF_RELOC_AMD64_REL32_2;
			break;
		    case 3:
			reloc->type = COFF_RELOC_AMD64_REL32_3;
			break;
		    case 4:
			reloc->type = COFF_RELOC_AMD64_REL32_4;
			break;
		    case 5:
			reloc->type = COFF_RELOC_AMD64_REL32_5;
			break;
		    default:
			yasm_error_set(YASM_ERROR_TYPE,
				       N_("coff: invalid relocation size"));
			return 1;
		}
	    } else
		yasm_internal_error(N_("coff objfmt: unrecognized machine"));
	} else if (value->seg_of) {
	    if (objfmt_coff->machine == COFF_MACHINE_I386)
		reloc->type = COFF_RELOC_I386_SECTION;
	    else if (objfmt_coff->machine == COFF_MACHINE_AMD64)
		reloc->type = COFF_RELOC_AMD64_SECTION;
	    else
		yasm_internal_error(N_("coff objfmt: unrecognized machine"));
	} else if (value->section_rel) {
	    if (objfmt_coff->machine == COFF_MACHINE_I386)
		reloc->type = COFF_RELOC_I386_SECREL;
	    else if (objfmt_coff->machine == COFF_MACHINE_AMD64)
		reloc->type = COFF_RELOC_AMD64_SECREL;
	    else
		yasm_internal_error(N_("coff objfmt: unrecognized machine"));
	} else {
	    if (objfmt_coff->machine == COFF_MACHINE_I386) {
		if (info->csd->flags2 & COFF_FLAG_NOBASE)
		    reloc->type = COFF_RELOC_I386_ADDR32NB;
		else
		    reloc->type = COFF_RELOC_I386_ADDR32;
	    } else if (objfmt_coff->machine == COFF_MACHINE_AMD64) {
		if (valsize == 32) {
		    if (info->csd->flags2 & COFF_FLAG_NOBASE)
			reloc->type = COFF_RELOC_AMD64_ADDR32NB;
		    else
			reloc->type = COFF_RELOC_AMD64_ADDR32;
		} else if (valsize == 64)
		    reloc->type = COFF_RELOC_AMD64_ADDR64;
		else {
		    yasm_error_set(YASM_ERROR_TYPE,
				   N_("coff: invalid relocation size"));
		    return 1;
		}
	    } else
		yasm_internal_error(N_("coff objfmt: unrecognized machine"));
	}
	info->csd->nreloc++;
	yasm_section_add_reloc(info->sect, (yasm_reloc *)reloc, yasm_xfree);
    }

    /* Build up final integer output from intn_val, intn_minus, value->abs,
     * and dist.  We do all this at the end to avoid creating temporary
     * intnums above (except for dist).
     */
    if (intn_minus <= intn_val)
	intn = yasm_intnum_create_uint(intn_val-intn_minus);
    else {
	intn = yasm_intnum_create_uint(intn_minus-intn_val);
	yasm_intnum_calc(intn, YASM_EXPR_NEG, NULL);
    }

    if (value->abs) {
	yasm_intnum *intn2 = yasm_expr_get_intnum(&value->abs, 0);
	if (!intn2) {
	    yasm_error_set(YASM_ERROR_TOO_COMPLEX,
			   N_("coff: relocation too complex"));
	    yasm_intnum_destroy(intn);
	    if (dist)
		yasm_intnum_destroy(dist);
	    return 1;
	}
	yasm_intnum_calc(intn, YASM_EXPR_ADD, intn2);
    }

    if (dist) {
	yasm_intnum_calc(intn, YASM_EXPR_ADD, dist);
	yasm_intnum_destroy(dist);
    }

    retval = yasm_arch_intnum_tobytes(objfmt_coff->arch, intn, buf, destsize,
				      valsize, 0, bc, warn);
    yasm_intnum_destroy(intn);
    return retval;
}

static int
coff_objfmt_output_bytecode(yasm_bytecode *bc, /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    /*@null@*/ /*@only@*/ unsigned char *bigbuf;
    unsigned long size = REGULAR_OUTBUF_SIZE;
    int gap;

    assert(info != NULL);

    bigbuf = yasm_bc_tobytes(bc, info->buf, &size, &gap, info,
			     coff_objfmt_output_value, NULL);

    /* Don't bother doing anything else if size ended up being 0. */
    if (size == 0) {
	if (bigbuf)
	    yasm_xfree(bigbuf);
	return 0;
    }

    info->csd->size += size;

    /* Warn that gaps are converted to 0 and write out the 0's. */
    if (gap) {
	unsigned long left;
	yasm_warn_set(YASM_WARN_UNINIT_CONTENTS,
	    N_("uninitialized space declared in code/data section: zeroing"));
	/* Write out in chunks */
	memset(info->buf, 0, REGULAR_OUTBUF_SIZE);
	left = size;
	while (left > REGULAR_OUTBUF_SIZE) {
	    fwrite(info->buf, REGULAR_OUTBUF_SIZE, 1, info->f);
	    left -= REGULAR_OUTBUF_SIZE;
	}
	fwrite(info->buf, left, 1, info->f);
    } else {
	/* Output buf (or bigbuf if non-NULL) to file */
	fwrite(bigbuf ? bigbuf : info->buf, (size_t)size, 1, info->f);
    }

    /* If bigbuf was allocated, free it */
    if (bigbuf)
	yasm_xfree(bigbuf);

    return 0;
}

static int
coff_objfmt_output_section(yasm_section *sect, /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    /*@dependent@*/ /*@null@*/ coff_section_data *csd;
    long pos;
    coff_reloc *reloc;
    unsigned char *localbuf;

    /* Don't output absolute sections into the section table */
    if (yasm_section_is_absolute(sect))
	return 0;

    assert(info != NULL);
    csd = yasm_section_get_data(sect, &coff_section_data_cb);
    assert(csd != NULL);

    /* Add to strtab if in win32 format and name > 8 chars */
    if (info->objfmt_coff->win32) {
	size_t namelen = strlen(yasm_section_get_name(sect));
	if (namelen > 8) {
	    csd->strtab_name = info->strtab_offset;
	    info->strtab_offset += namelen + 1;
	}
    }

    if (!csd->isdebug)
	csd->addr = info->addr;

    if ((csd->flags & COFF_STYP_STD_MASK) == COFF_STYP_BSS) {
	/* Don't output BSS sections.
	 * TODO: Check for non-reserve bytecodes?
	 */
	pos = 0;    /* position = 0 because it's not in the file */
	csd->size = yasm_bc_next_offset(yasm_section_bcs_last(sect));
    } else {
	pos = ftell(info->f);
	if (pos == -1) {
	    yasm__fatal(N_("could not get file position on output file"));
	    /*@notreached@*/
	    return 1;
	}

	info->sect = sect;
	info->csd = csd;
	yasm_section_bcs_traverse(sect, info->errwarns, info,
				  coff_objfmt_output_bytecode);

	/* Sanity check final section size */
	if (csd->size != yasm_bc_next_offset(yasm_section_bcs_last(sect)))
	    yasm_internal_error(
		N_("coff: section computed size did not match actual size"));
    }

    /* Empty?  Go on to next section */
    if (csd->size == 0)
	return 0;

    if (!csd->isdebug)
	info->addr += csd->size;
    csd->scnptr = (unsigned long)pos;

    /* No relocations to output?  Go on to next section */
    if (csd->nreloc == 0)
	return 0;

    pos = ftell(info->f);
    if (pos == -1) {
	yasm__fatal(N_("could not get file position on output file"));
	/*@notreached@*/
	return 1;
    }
    csd->relptr = (unsigned long)pos;

    /* If >=64K relocs (for Win32/64), we set a flag in the section header
     * (NRELOC_OVFL) and the first relocation contains the number of relocs.
     */
    if (csd->nreloc >= 64*1024 && info->objfmt_coff->win32) {
	localbuf = info->buf;
	YASM_WRITE_32_L(localbuf, csd->nreloc+1);   /* address of relocation */
	YASM_WRITE_32_L(localbuf, 0);		/* relocated symbol */
	YASM_WRITE_16_L(localbuf, 0);		/* type of relocation */
	fwrite(info->buf, 10, 1, info->f);
    }

    reloc = (coff_reloc *)yasm_section_relocs_first(sect);
    while (reloc) {
	/*@null@*/ coff_symrec_data *csymd;
	localbuf = info->buf;

	csymd = yasm_symrec_get_data(reloc->reloc.sym, &coff_symrec_data_cb);
	if (!csymd)
	    yasm_internal_error(
		N_("coff: no symbol data for relocated symbol"));

	yasm_intnum_get_sized(reloc->reloc.addr, localbuf, 4, 32, 0, 0, 0);
	localbuf += 4;				/* address of relocation */
	YASM_WRITE_32_L(localbuf, csymd->index);    /* relocated symbol */
	YASM_WRITE_16_L(localbuf, reloc->type);	    /* type of relocation */
	fwrite(info->buf, 10, 1, info->f);

	reloc = (coff_reloc *)yasm_section_reloc_next((yasm_reloc *)reloc);
    }

    return 0;
}

static int
coff_objfmt_output_sectstr(yasm_section *sect, /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    const char *name;
    size_t len;

    /* Don't output absolute sections into the section table */
    if (yasm_section_is_absolute(sect))
	return 0;

    /* Add to strtab if in win32 format and name > 8 chars */
    if (!info->objfmt_coff->win32)
       return 0;
    
    name = yasm_section_get_name(sect);
    len = strlen(name);
    if (len > 8)
	fwrite(name, len+1, 1, info->f);
    return 0;
}

static int
coff_objfmt_output_secthead(yasm_section *sect, /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    yasm_objfmt_coff *objfmt_coff;
    /*@dependent@*/ /*@null@*/ coff_section_data *csd;
    unsigned char *localbuf;
    unsigned long align = yasm_section_get_align(sect);

    /* Don't output absolute sections into the section table */
    if (yasm_section_is_absolute(sect))
	return 0;

    assert(info != NULL);
    objfmt_coff = info->objfmt_coff;
    csd = yasm_section_get_data(sect, &coff_section_data_cb);
    assert(csd != NULL);

    /* Check to see if alignment is supported size */
    if (align > 8192)
	align = 8192;

    /* Convert alignment into flags setting */
    csd->flags &= ~COFF_STYP_ALIGN_MASK;
    while (align != 0) {
	csd->flags += 1<<COFF_STYP_ALIGN_SHIFT;
	align >>= 1;
    }

    /* section name */
    localbuf = info->buf;
    if (strlen(yasm_section_get_name(sect)) > 8) {
	char namenum[30];
	sprintf(namenum, "/%ld", csd->strtab_name);
	strncpy((char *)localbuf, namenum, 8);
    } else
	strncpy((char *)localbuf, yasm_section_get_name(sect), 8);
    localbuf += 8;
    if (csd->isdebug) {
	YASM_WRITE_32_L(localbuf, 0);		/* physical address */
	YASM_WRITE_32_L(localbuf, 0);		/* virtual address */
    } else {
	YASM_WRITE_32_L(localbuf, csd->addr);	/* physical address */
	if (COFF_SET_VMA)
	    YASM_WRITE_32_L(localbuf, csd->addr);/* virtual address */
	else
	    YASM_WRITE_32_L(localbuf, 0);	/* virtual address */
    }
    YASM_WRITE_32_L(localbuf, csd->size);	/* section size */
    YASM_WRITE_32_L(localbuf, csd->scnptr);	/* file ptr to data */
    YASM_WRITE_32_L(localbuf, csd->relptr);	/* file ptr to relocs */
    YASM_WRITE_32_L(localbuf, 0);		/* file ptr to line nums */
    if (csd->nreloc >= 64*1024) {
	/* Win32/64 has special handling for this case. */
	if (objfmt_coff->win32)
	    csd->flags |= COFF_STYP_NRELOC_OVFL;
	else {
	    yasm_warn_set(YASM_WARN_GENERAL,
			  N_("too many relocations in section `%s'"),
			  yasm_section_get_name(sect));
	    yasm_errwarn_propagate(info->errwarns, 0);
	}
	YASM_WRITE_16_L(localbuf, 0xFFFF);	/* max out */
    } else
	YASM_WRITE_16_L(localbuf, csd->nreloc); /* num of relocation entries */
    YASM_WRITE_16_L(localbuf, 0);		/* num of line number entries */
    YASM_WRITE_32_L(localbuf, csd->flags);	/* flags */
    fwrite(info->buf, 40, 1, info->f);

    return 0;
}

static int
coff_objfmt_count_sym(yasm_symrec *sym, /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    assert(info != NULL);
    if (info->all_syms || yasm_symrec_get_visibility(sym) != YASM_SYM_LOCAL) {
	/* Save index in symrec data */
	coff_symrec_data *sym_data =
	    yasm_symrec_get_data(sym, &coff_symrec_data_cb);
	if (!sym_data) {
	    sym_data = coff_objfmt_sym_set_data(sym, COFF_SCL_STAT, NULL, 0,
						COFF_SYMTAB_AUX_NONE);
	}
	sym_data->index = info->indx;

	info->indx += sym_data->numaux + 1;
    }
    return 0;
}

static int
coff_objfmt_output_sym(yasm_symrec *sym, /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    yasm_sym_vis vis = yasm_symrec_get_visibility(sym);

    assert(info != NULL);

    /* Don't output local syms unless outputting all syms */
    if (info->all_syms || vis != YASM_SYM_LOCAL) {
	const char *name = yasm_symrec_get_name(sym);
	const yasm_expr *equ_val;
	const yasm_intnum *intn;
	unsigned char *localbuf;
	size_t len = strlen(name);
	int aux;
	/*@dependent@*/ /*@null@*/ coff_symrec_data *csymd;
	unsigned long value = 0;
	unsigned int scnum = 0xfffe;	/* -2 = debugging symbol */
	/*@dependent@*/ /*@null@*/ yasm_section *sect;
	/*@dependent@*/ /*@null@*/ yasm_bytecode *precbc;
	unsigned long scnlen = 0;   /* for sect auxent */
	unsigned long nreloc = 0;   /* for sect auxent */
	yasm_objfmt_coff *objfmt_coff = info->objfmt_coff;

	/* Get symrec's of_data (needed for storage class) */
	csymd = yasm_symrec_get_data(sym, &coff_symrec_data_cb);
	if (!csymd)
	    yasm_internal_error(N_("coff: expected sym data to be present"));

	/* Look at symrec for value/scnum/etc. */
	if (yasm_symrec_get_label(sym, &precbc)) {
	    if (precbc)
		sect = yasm_bc_get_section(precbc);
	    else
		sect = NULL;
	    /* it's a label: get value and offset.
	     * If there is not a section, leave as debugging symbol.
	     */
	    if (sect) {
		/*@dependent@*/ /*@null@*/ coff_section_data *csectd;
		csectd = yasm_section_get_data(sect, &coff_section_data_cb);
		if (csectd) {
		    scnum = csectd->scnum;
		    scnlen = csectd->size;
		    nreloc = csectd->nreloc;
		    if (COFF_SET_VMA)
			value = csectd->addr;
		} else if (yasm_section_is_absolute(sect)) {
		    yasm_expr *abs_start;

		    abs_start = yasm_expr_copy(yasm_section_get_start(sect));
		    intn = yasm_expr_get_intnum(&abs_start, 1);
		    if (!intn) {
			yasm_error_set(YASM_ERROR_NOT_CONSTANT,
			    N_("absolute section start not an integer expression"));
			yasm_errwarn_propagate(info->errwarns,
					       abs_start->line);
		    } else
			value = yasm_intnum_get_uint(intn);
		    yasm_expr_destroy(abs_start);

		    scnum = 0xffff;	/* -1 = absolute symbol */
		} else
		    yasm_internal_error(N_("didn't understand section"));
		if (precbc)
		    value += yasm_bc_next_offset(precbc);
	    }
	} else if ((equ_val = yasm_symrec_get_equ(sym))) {
	    yasm_expr *equ_val_copy = yasm_expr_copy(equ_val);
	    intn = yasm_expr_get_intnum(&equ_val_copy, 1);
	    if (!intn) {
		if (vis & YASM_SYM_GLOBAL) {
		    yasm_error_set(YASM_ERROR_NOT_CONSTANT,
			N_("global EQU value not an integer expression"));
		    yasm_errwarn_propagate(info->errwarns, equ_val->line);
		}
	    } else
		value = yasm_intnum_get_uint(intn);
	    yasm_expr_destroy(equ_val_copy);

	    scnum = 0xffff;     /* -1 = absolute symbol */
	} else {
	    if (vis & YASM_SYM_COMMON) {
		intn = yasm_expr_get_intnum(&csymd->size, 1);
		if (!intn) {
		    yasm_error_set(YASM_ERROR_NOT_CONSTANT,
			N_("COMMON data size not an integer expression"));
		    yasm_errwarn_propagate(info->errwarns, csymd->size->line);
		} else
		    value = yasm_intnum_get_uint(intn);
		scnum = 0;
	    }
	    if (vis & YASM_SYM_EXTERN)
		scnum = 0;
	}

	localbuf = info->buf;
	if (len > 8) {
	    YASM_WRITE_32_L(localbuf, 0);	/* "zeros" field */
	    YASM_WRITE_32_L(localbuf, info->strtab_offset); /* strtab offset */
	    info->strtab_offset += len+1;
	} else {
	    /* <8 chars, so no string table entry needed */
	    strncpy((char *)localbuf, name, 8);
	    localbuf += 8;
	}
	YASM_WRITE_32_L(localbuf, value);	/* value */
	YASM_WRITE_16_L(localbuf, scnum);	/* section number */
	YASM_WRITE_16_L(localbuf, 0);	    /* type is always zero (for now) */
	YASM_WRITE_8(localbuf, csymd->sclass);	/* storage class */
	YASM_WRITE_8(localbuf, csymd->numaux);	/* number of aux entries */
	fwrite(info->buf, 18, 1, info->f);
	for (aux=0; aux<csymd->numaux; aux++) {
	    localbuf = info->buf;
	    memset(localbuf, 0, 18);
	    switch (csymd->auxtype) {
		case COFF_SYMTAB_AUX_NONE:
		    break;
		case COFF_SYMTAB_AUX_SECT:
		    YASM_WRITE_32_L(localbuf, scnlen);	/* section length */
		    YASM_WRITE_16_L(localbuf, nreloc);	/* number relocs */
		    YASM_WRITE_16_L(localbuf, 0);	/* number line nums */
		    break;
		case COFF_SYMTAB_AUX_FILE:
		    len = strlen(csymd->aux[0].fname);
		    if (len > 14) {
			YASM_WRITE_32_L(localbuf, 0);
			YASM_WRITE_32_L(localbuf, info->strtab_offset);
			info->strtab_offset += len+1;
		    } else
			strncpy((char *)localbuf, csymd->aux[0].fname, 14);
		    break;
		default:
		    yasm_internal_error(
			N_("coff: unrecognized aux symtab type"));
	    }
	    fwrite(info->buf, 18, 1, info->f);
	}
    }
    return 0;
}

static int
coff_objfmt_output_str(yasm_symrec *sym, /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    yasm_sym_vis vis = yasm_symrec_get_visibility(sym);

    assert(info != NULL);

    /* Don't output local syms unless outputting all syms */
    if (info->all_syms || vis != YASM_SYM_LOCAL) {
	const char *name = yasm_symrec_get_name(sym);
	/*@dependent@*/ /*@null@*/ coff_symrec_data *csymd;
	size_t len = strlen(name);
	int aux;

	csymd = yasm_symrec_get_data(sym, &coff_symrec_data_cb);
	if (!csymd)
	    yasm_internal_error(N_("coff: expected sym data to be present"));

	if (len > 8)
	    fwrite(name, len+1, 1, info->f);
	for (aux=0; aux<csymd->numaux; aux++) {
	    switch (csymd->auxtype) {
		case COFF_SYMTAB_AUX_FILE:
		    len = strlen(csymd->aux[0].fname);
		    if (len > 14)
			fwrite(csymd->aux[0].fname, len+1, 1, info->f);
		    break;
		default:
		    break;
	    }
	}
    }
    return 0;
}

static void
coff_objfmt_output(yasm_objfmt *objfmt, FILE *f, int all_syms, yasm_dbgfmt *df,
		   yasm_errwarns *errwarns)
{
    yasm_objfmt_coff *objfmt_coff = (yasm_objfmt_coff *)objfmt;
    coff_objfmt_output_info info;
    unsigned char *localbuf;
    long pos;
    unsigned long symtab_pos;
    unsigned long symtab_count;
    unsigned int flags;

    if (objfmt_coff->filesym_data->aux[0].fname)
	yasm_xfree(objfmt_coff->filesym_data->aux[0].fname);
    objfmt_coff->filesym_data->aux[0].fname =
	yasm__xstrdup(yasm_object_get_source_fn(objfmt_coff->object));

    /* Force all syms for win64 because they're needed for relocations.
     * FIXME: Not *all* syms need to be output, only the ones needed for
     * relocation.  Find a way to do that someday.
     */
    all_syms |= objfmt_coff->win64;

    info.strtab_offset = 4;
    info.objfmt_coff = objfmt_coff;
    info.errwarns = errwarns;
    info.f = f;
    info.buf = yasm_xmalloc(REGULAR_OUTBUF_SIZE);

    /* Initialize section data (and count in parse_scnum) any sections that
     * we've not initialized so far.
     */
    yasm_object_sections_traverse(objfmt_coff->object, &info,
				  coff_objfmt_init_remaining_section);

    /* Allocate space for headers by seeking forward */
    if (fseek(f, (long)(20+40*(objfmt_coff->parse_scnum-1)), SEEK_SET) < 0) {
	yasm__fatal(N_("could not seek on output file"));
	/*@notreached@*/
	return;
    }

    /* Finalize symbol table (assign index to each symbol) */
    info.indx = 0;
    info.all_syms = all_syms;
    yasm_symtab_traverse(objfmt_coff->symtab, &info, coff_objfmt_count_sym);
    symtab_count = info.indx;

    /* Section data/relocs */
    if (COFF_SET_VMA) {
	/* If we're setting the VMA, we need to do a first section pass to
	 * determine each section's addr value before actually outputting
	 * relocations, as a relocation's section address is added into the
	 * addends in the generated code.
	 */
	info.addr = 0;
	if (yasm_object_sections_traverse(objfmt_coff->object, &info,
					  coff_objfmt_set_section_addr))
	    return;
    }
    info.addr = 0;
    if (yasm_object_sections_traverse(objfmt_coff->object, &info,
				      coff_objfmt_output_section))
	return;

    /* Symbol table */
    pos = ftell(f);
    if (pos == -1) {
	yasm__fatal(N_("could not get file position on output file"));
	/*@notreached@*/
	return;
    }
    symtab_pos = (unsigned long)pos;
    yasm_symtab_traverse(objfmt_coff->symtab, &info, coff_objfmt_output_sym);

    /* String table */
    yasm_fwrite_32_l(info.strtab_offset, f); /* total length */
    yasm_object_sections_traverse(objfmt_coff->object, &info,
				  coff_objfmt_output_sectstr);
    yasm_symtab_traverse(objfmt_coff->symtab, &info, coff_objfmt_output_str);

    /* Write headers */
    if (fseek(f, 0, SEEK_SET) < 0) {
	yasm__fatal(N_("could not seek on output file"));
	/*@notreached@*/
	return;
    }

    localbuf = info.buf;
    YASM_WRITE_16_L(localbuf, objfmt_coff->machine);	/* magic number */
    YASM_WRITE_16_L(localbuf, objfmt_coff->parse_scnum-1);/* number of sects */
    YASM_WRITE_32_L(localbuf, 0);			/* time/date stamp */
    YASM_WRITE_32_L(localbuf, symtab_pos);		/* file ptr to symtab */
    YASM_WRITE_32_L(localbuf, symtab_count);		/* number of symtabs */
    YASM_WRITE_16_L(localbuf, 0);	/* size of optional header (none) */
    /* flags */
    flags = 0;
    if (strcmp(yasm_dbgfmt_keyword(df), "null")==0)
	flags = COFF_F_LNNO;
    if (!all_syms)
	flags |= COFF_F_LSYMS;
    if (objfmt_coff->machine != COFF_MACHINE_AMD64)
	flags |= COFF_F_AR32WR;
    YASM_WRITE_16_L(localbuf, flags);
    fwrite(info.buf, 20, 1, f);

    yasm_object_sections_traverse(objfmt_coff->object, &info,
				  coff_objfmt_output_secthead);

    yasm_xfree(info.buf);
}

static void
coff_objfmt_destroy(yasm_objfmt *objfmt)
{
    yasm_xfree(objfmt);
}

static yasm_section *
coff_objfmt_add_default_section(yasm_objfmt *objfmt)
{
    yasm_objfmt_coff *objfmt_coff = (yasm_objfmt_coff *)objfmt;
    yasm_section *retval;
    coff_section_data *csd;
    int isnew;

    retval = yasm_object_get_general(objfmt_coff->object, ".text", 0, 16, 1, 0,
				     &isnew, 0);
    if (isnew) {
	csd = coff_objfmt_init_new_section(objfmt_coff, retval, ".text", 0);
	csd->flags = COFF_STYP_TEXT;
	if (objfmt_coff->win32)
	    csd->flags |= COFF_STYP_EXECUTE | COFF_STYP_READ;
	yasm_section_set_default(retval, 1);
    }
    return retval;
}

static /*@observer@*/ /*@null@*/ yasm_section *
coff_objfmt_section_switch(yasm_objfmt *objfmt, yasm_valparamhead *valparams,
			    /*@unused@*/ /*@null@*/
			    yasm_valparamhead *objext_valparams,
			    unsigned long line)
{
    yasm_objfmt_coff *objfmt_coff = (yasm_objfmt_coff *)objfmt;
    yasm_valparam *vp = yasm_vps_first(valparams);
    yasm_section *retval;
    int isnew;
    int iscode = 0;
    unsigned long flags;
    unsigned long flags2 = 0;
    int flags_override = 0;
    char *sectname;
    int resonly = 0;
    unsigned long align = 0;
    coff_section_data *csd;

    static const struct {
	const char *name;
	unsigned long stdflags;	/* if 0, win32 only qualifier */
	unsigned long win32flags;
	unsigned long flags2;
	/* Mode: 0 => clear specified bits
	 *       1 => set specified bits
	 *       2 => clear all bits, then set specified bits
	 */
	int mode;
    } flagquals[] = {
	{ "code", COFF_STYP_TEXT, COFF_STYP_EXECUTE | COFF_STYP_READ, 0, 2 },
	{ "text", COFF_STYP_TEXT, COFF_STYP_EXECUTE | COFF_STYP_READ, 0, 2 },
	{ "data", COFF_STYP_DATA, COFF_STYP_READ | COFF_STYP_WRITE, 0, 2 },
	{ "bss", COFF_STYP_BSS, COFF_STYP_READ | COFF_STYP_WRITE, 0, 2 },
	{ "info", COFF_STYP_INFO, COFF_STYP_DISCARD | COFF_STYP_READ, 0, 2 },
	{ "discard", 0, COFF_STYP_DISCARD, 0, 1 },
	{ "nodiscard", 0, COFF_STYP_DISCARD, 0, 0 },
	{ "cache", 0, COFF_STYP_NOCACHE, 0, 0 },
	{ "nocache", 0, COFF_STYP_NOCACHE, 0, 1 },
	{ "page", 0, COFF_STYP_NOPAGE, 0, 0 },
	{ "nopage", 0, COFF_STYP_NOPAGE, 0, 1 },
	{ "share", 0, COFF_STYP_SHARED, 0, 1 },
	{ "noshare", 0, COFF_STYP_SHARED, 0, 0 },
	{ "execute", 0, COFF_STYP_EXECUTE, 0, 1 },
	{ "noexecute", 0, COFF_STYP_EXECUTE, 0, 0 },
	{ "read", 0, COFF_STYP_READ, 0, 1 },
	{ "noread", 0, COFF_STYP_READ, 0, 0 },
	{ "write", 0, COFF_STYP_WRITE, 0, 1 },
	{ "nowrite", 0, COFF_STYP_WRITE, 0, 0 },
	{ "base", 0, 0, COFF_FLAG_NOBASE, 0 },
	{ "nobase", 0, 0, COFF_FLAG_NOBASE, 1 },
    };

    if (!vp || vp->param || !vp->val)
	return NULL;

    sectname = vp->val;
    if (strlen(sectname) > 8 && !objfmt_coff->win32) {
	/* win32 format supports >8 character section names in object
	 * files via "/nnnn" (where nnnn is decimal offset into string table),
	 * so only warn for regular COFF.
	 */
	yasm_warn_set(YASM_WARN_GENERAL,
	    N_("COFF section names limited to 8 characters: truncating"));
	sectname[8] = '\0';
    }

    if (strcmp(sectname, ".data") == 0) {
	flags = COFF_STYP_DATA;
	if (objfmt_coff->win32) {
	    flags |= COFF_STYP_READ | COFF_STYP_WRITE;
	    if (objfmt_coff->machine == COFF_MACHINE_AMD64)
		align = 16;
	    else
		align = 4;
	}
    } else if (strcmp(sectname, ".bss") == 0) {
	flags = COFF_STYP_BSS;
	if (objfmt_coff->win32) {
	    flags |= COFF_STYP_READ | COFF_STYP_WRITE;
	    if (objfmt_coff->machine == COFF_MACHINE_AMD64)
		align = 16;
	    else
		align = 4;
	}
	resonly = 1;
    } else if (strcmp(sectname, ".text") == 0) {
	flags = COFF_STYP_TEXT;
	if (objfmt_coff->win32) {
	    flags |= COFF_STYP_EXECUTE | COFF_STYP_READ;
	    align = 16;
	}
	iscode = 1;
    } else if (strcmp(sectname, ".rdata") == 0
	       || strncmp(sectname, ".rodata", 7) == 0) {
	flags = COFF_STYP_DATA;
	if (objfmt_coff->win32) {
	    flags |= COFF_STYP_READ;
	    align = 8;
	} else
	    yasm_warn_set(YASM_WARN_GENERAL,
		N_("Standard COFF does not support read-only data sections"));
    } else if (strcmp(sectname, ".drectve") == 0) {
	flags = COFF_STYP_INFO;
	if (objfmt_coff->win32)
	    flags |= COFF_STYP_DISCARD | COFF_STYP_READ;
    } else if (objfmt_coff->win32 && strcmp(sectname, ".pdata") == 0) {
	flags = COFF_STYP_DATA | COFF_STYP_READ;
	align = 4;
	flags2 = COFF_FLAG_NOBASE;
    } else if (objfmt_coff->win32 && strcmp(sectname, ".xdata") == 0) {
	flags = COFF_STYP_DATA | COFF_STYP_READ;
	align = 8;
    } else if (strcmp(sectname, ".comment") == 0) {
	flags = COFF_STYP_INFO;
	if (objfmt_coff->win32)
	    flags |= COFF_STYP_DISCARD | COFF_STYP_READ;
    } else if (yasm__strncasecmp(sectname, ".debug", 6)==0) {
	flags = COFF_STYP_DATA;
	if (objfmt_coff->win32)
	    flags |= COFF_STYP_DISCARD|COFF_STYP_READ;
	align = 1;
    } else {
	/* Default to code */
	flags = COFF_STYP_TEXT;
	if (objfmt_coff->win32)
	    flags |= COFF_STYP_EXECUTE | COFF_STYP_READ;
	iscode = 1;
    }

    while ((vp = yasm_vps_next(vp))) {
	size_t i;
	int match, win32warn;

	win32warn = 0;

	if (!vp->val) {
	    yasm_warn_set(YASM_WARN_GENERAL,
			  N_("Unrecognized numeric qualifier"));
	    continue;
	}

	match = 0;
	for (i=0; i<NELEMS(flagquals) && !match; i++) {
	    if (yasm__strcasecmp(vp->val, flagquals[i].name) == 0) {
		if (!objfmt_coff->win32 && flagquals[i].stdflags == 0)
		    win32warn = 1;
		else switch (flagquals[i].mode) {
		    case 0:
			flags &= ~flagquals[i].stdflags;
			flags2 &= ~flagquals[i].flags2;
			if (objfmt_coff->win32)
			    flags &= ~flagquals[i].win32flags;
			if (flagquals[i].win32flags & COFF_STYP_EXECUTE)
			    iscode = 0;
			break;
		    case 1:
			flags |= flagquals[i].stdflags;
			flags2 |= flagquals[i].flags2;
			if (objfmt_coff->win32)
			    flags |= flagquals[i].win32flags;
			if (flagquals[i].win32flags & COFF_STYP_EXECUTE)
			    iscode = 1;
			break;
		    case 2:
			flags &= ~COFF_STYP_STD_MASK;
			flags |= flagquals[i].stdflags;
			flags2 = flagquals[i].flags2;
			if (objfmt_coff->win32) {
			    flags &= ~COFF_STYP_WIN32_MASK;
			    flags |= flagquals[i].win32flags;
			}
			if (flagquals[i].win32flags & COFF_STYP_EXECUTE)
			    iscode = 1;
			break;
		}
		flags_override = 1;
		match = 1;
	    }
	}

	if (match)
	    ;
	else if (yasm__strncasecmp(vp->val, "gas_", 4) == 0) {
	    /* GAS-style flags */
	    int alloc = 0, load = 0, readonly = 0, code = 0, data = 0;
	    int shared = 0;
	    iscode = 0;
	    for (i=4; i<strlen(vp->val); i++) {
		switch (vp->val[i]) {
		    case 'a':
			break;
		    case 'b':
			alloc = 1;
			load = 0;
			break;
		    case 'n':
			load = 0;
			break;
		    case 's':
			shared = 1;
			/*@fallthrough@*/
		    case 'd':
			data = 1;
			load = 1;
			readonly = 0;
		    case 'x':
			code = 1;
			load = 1;
			break;
		    case 'r':
			data = 1;
			load = 1;
			readonly = 1;
			break;
		    case 'w':
			readonly = 0;
			break;
		    default:
			yasm_warn_set(YASM_WARN_GENERAL,
				      N_("unrecognized section attribute: `%c'"),
				      vp->val[i]);
		}
	    }
	    if (code) {
		flags = COFF_STYP_TEXT;
		if (objfmt_coff->win32)
		    flags |= COFF_STYP_EXECUTE | COFF_STYP_READ;
		iscode = 1;
	    } else if (data) {
		flags = COFF_STYP_DATA;
		if (objfmt_coff->win32)
		    flags |= COFF_STYP_READ | COFF_STYP_WRITE;
	    } else if (readonly) {
		flags = COFF_STYP_DATA;
		if (objfmt_coff->win32)
		    flags |= COFF_STYP_READ;
	    } else if (load)
		flags = COFF_STYP_TEXT;
	    else if (alloc)
		flags = COFF_STYP_BSS;
	    if (shared && objfmt_coff->win32)
		flags |= COFF_STYP_SHARED;
	} else if (yasm__strcasecmp(vp->val, "align") == 0 && vp->param) {
	    if (objfmt_coff->win32) {
		/*@dependent@*/ /*@null@*/ const yasm_intnum *align_expr;
		align_expr = yasm_expr_get_intnum(&vp->param, 0);
		if (!align_expr) {
		    yasm_error_set(YASM_ERROR_VALUE,
				   N_("argument to `%s' is not an integer"),
				   vp->val);
		    return NULL;
		}
		align = yasm_intnum_get_uint(align_expr);

		/* Alignments must be a power of two. */
		if (!is_exp2(align)) {
		    yasm_error_set(YASM_ERROR_VALUE,
				   N_("argument to `%s' is not a power of two"),
				   vp->val);
		    return NULL;
		}

		/* Check to see if alignment is supported size */
		if (align > 8192) {
		    yasm_error_set(YASM_ERROR_VALUE,
			N_("Win32 does not support alignments > 8192"));
		    return NULL;
		}

	    } else
		win32warn = 1;
	} else
	    yasm_warn_set(YASM_WARN_GENERAL, N_("Unrecognized qualifier `%s'"),
			  vp->val);

	if (win32warn)
	    yasm_warn_set(YASM_WARN_GENERAL,
		N_("Standard COFF does not support qualifier `%s'"), vp->val);
    }

    retval = yasm_object_get_general(objfmt_coff->object, sectname, 0, align,
				     iscode, resonly, &isnew, line);

    if (isnew)
	csd = coff_objfmt_init_new_section(objfmt_coff, retval, sectname, line);
    else
	csd = yasm_section_get_data(retval, &coff_section_data_cb);

    if (isnew || yasm_section_is_default(retval)) {
	yasm_section_set_default(retval, 0);
	csd->flags = flags;
	csd->flags2 = flags2;
	yasm_section_set_align(retval, align, line);
    } else if (flags_override)
	yasm_warn_set(YASM_WARN_GENERAL,
		      N_("section flags ignored on section redeclaration"));
    return retval;
}

static void
coff_section_data_destroy(void *data)
{
    yasm_xfree(data);
}

static void
coff_section_data_print(void *data, FILE *f, int indent_level)
{
    coff_section_data *csd = (coff_section_data *)data;

    fprintf(f, "%*ssym=\n", indent_level, "");
    yasm_symrec_print(csd->sym, f, indent_level+1);
    fprintf(f, "%*sscnum=%d\n", indent_level, "", csd->scnum);
    fprintf(f, "%*sflags=", indent_level, "");
    switch (csd->flags & COFF_STYP_STD_MASK) {
	case COFF_STYP_TEXT:
	    fprintf(f, "TEXT");
	    break;
	case COFF_STYP_DATA:
	    fprintf(f, "DATA");
	    break;
	case COFF_STYP_BSS:
	    fprintf(f, "BSS");
	    break;
	default:
	    fprintf(f, "UNKNOWN");
	    break;
    }
    fprintf(f, "\n%*saddr=0x%lx\n", indent_level, "", csd->addr);
    fprintf(f, "%*sscnptr=0x%lx\n", indent_level, "", csd->scnptr);
    fprintf(f, "%*ssize=%ld\n", indent_level, "", csd->size);
    fprintf(f, "%*srelptr=0x%lx\n", indent_level, "", csd->relptr);
    fprintf(f, "%*snreloc=%ld\n", indent_level, "", csd->nreloc);
    fprintf(f, "%*srelocs:\n", indent_level, "");
}

static yasm_symrec *
coff_objfmt_extern_declare(yasm_objfmt *objfmt, const char *name, /*@unused@*/
			   /*@null@*/ yasm_valparamhead *objext_valparams,
			   unsigned long line)
{
    yasm_objfmt_coff *objfmt_coff = (yasm_objfmt_coff *)objfmt;
    yasm_symrec *sym;

    sym = yasm_symtab_declare(objfmt_coff->symtab, name, YASM_SYM_EXTERN,
			      line);
    coff_objfmt_sym_set_data(sym, COFF_SCL_EXT, NULL, 0,
			     COFF_SYMTAB_AUX_NONE);

    return sym;
}

static yasm_symrec *
coff_objfmt_global_declare(yasm_objfmt *objfmt, const char *name, /*@unused@*/
			   /*@null@*/ yasm_valparamhead *objext_valparams,
			   unsigned long line)
{
    yasm_objfmt_coff *objfmt_coff = (yasm_objfmt_coff *)objfmt;
    yasm_symrec *sym;

    sym = yasm_symtab_declare(objfmt_coff->symtab, name, YASM_SYM_GLOBAL,
			      line);
    coff_objfmt_sym_set_data(sym, COFF_SCL_EXT, NULL, 0,
			     COFF_SYMTAB_AUX_NONE);

    return sym;
}

static yasm_symrec *
coff_objfmt_common_declare(yasm_objfmt *objfmt, const char *name,
			   /*@only@*/ yasm_expr *size, /*@unused@*/ /*@null@*/
			   yasm_valparamhead *objext_valparams,
			   unsigned long line)
{
    yasm_objfmt_coff *objfmt_coff = (yasm_objfmt_coff *)objfmt;
    yasm_symrec *sym;

    sym = yasm_symtab_declare(objfmt_coff->symtab, name, YASM_SYM_COMMON,
			      line);
    coff_objfmt_sym_set_data(sym, COFF_SCL_EXT, size, 0,
			     COFF_SYMTAB_AUX_NONE);

    return sym;
}

static void
coff_symrec_data_destroy(void *data)
{
    coff_symrec_data *csymd = (coff_symrec_data *)data;
    if (csymd->size)
	yasm_expr_destroy(csymd->size);
    yasm_xfree(data);
}

static void
coff_symrec_data_print(void *data, FILE *f, int indent_level)
{
    coff_symrec_data *csd = (coff_symrec_data *)data;

    fprintf(f, "%*ssymtab index=%lu\n", indent_level, "", csd->index);
    fprintf(f, "%*ssclass=%d\n", indent_level, "", csd->sclass);
    fprintf(f, "%*ssize=", indent_level, "");
    if (csd->size)
	yasm_expr_print(csd->size, f);
    else
	fprintf(f, "nil");
    fprintf(f, "\n");
}

static int
coff_objfmt_directive(/*@unused@*/ yasm_objfmt *objfmt,
		      /*@unused@*/ const char *name,
		      /*@unused@*/ yasm_valparamhead *valparams,
		      /*@unused@*/ /*@null@*/
		      yasm_valparamhead *objext_valparams,
		      /*@unused@*/ unsigned long line)
{
    return 1;	/* no objfmt directives */
}

static int
win32_objfmt_directive(yasm_objfmt *objfmt, const char *name,
		       yasm_valparamhead *valparams,
		       /*@unused@*/ /*@null@*/
		       yasm_valparamhead *objext_valparams,
		       unsigned long line)
{
    yasm_objfmt_coff *objfmt_coff = (yasm_objfmt_coff *)objfmt;

    if (yasm__strcasecmp(name, "export") == 0) {
	yasm_valparam *vp = yasm_vps_first(valparams);
	int isnew;
	yasm_section *sect;
	yasm_datavalhead dvs;

	/* Reference exported symbol (to generate error if not declared) */
	if (vp->val)
	    yasm_symtab_use(objfmt_coff->symtab, vp->val, line);
	else {
	    yasm_error_set(YASM_ERROR_SYNTAX,
			   N_("argument to EXPORT must be symbol name"));
	    return 0;
	}

	/* Add to end of linker directives */
	sect = yasm_object_get_general(objfmt_coff->object, ".drectve", 0, 0,
				       0, 0, &isnew, line);

	/* Initialize directive section if needed */
	if (isnew) {
	    coff_section_data *csd;
	    csd = coff_objfmt_init_new_section(objfmt_coff, sect,
					       yasm_section_get_name(sect),
					       line);
	    csd->flags = COFF_STYP_INFO | COFF_STYP_DISCARD | COFF_STYP_READ;
	}

	/* Add text as data bytecode */
	yasm_dvs_initialize(&dvs);
	yasm_dvs_append(&dvs,
			yasm_dv_create_string(yasm__xstrdup("-export:"),
					      strlen("-export:")));
	yasm_dvs_append(&dvs, yasm_dv_create_string(yasm__xstrdup(vp->val),
						    strlen(vp->val)));
	yasm_dvs_append(&dvs, yasm_dv_create_string(yasm__xstrdup(" "), 1));
	yasm_section_bcs_append(sect, yasm_bc_create_data(&dvs, 1, 0, NULL,
							  line));

	return 0;
    } else
	return 1;
}


/* Define valid debug formats to use with this object format */
static const char *coff_objfmt_dbgfmt_keywords[] = {
    "null",
    "dwarf2",
    NULL
};

/* Define objfmt structure -- see objfmt.h for details */
yasm_objfmt_module yasm_coff_LTX_objfmt = {
    "COFF (DJGPP)",
    "coff",
    "o",
    32,
    coff_objfmt_dbgfmt_keywords,
    "null",
    coff_objfmt_create,
    coff_objfmt_output,
    coff_objfmt_destroy,
    coff_objfmt_add_default_section,
    coff_objfmt_section_switch,
    coff_objfmt_extern_declare,
    coff_objfmt_global_declare,
    coff_objfmt_common_declare,
    coff_objfmt_directive
};

/* Define valid debug formats to use with this object format */
static const char *winXX_objfmt_dbgfmt_keywords[] = {
    "null",
    "dwarf2",
    "cv8",
    NULL
};

/* Define objfmt structure -- see objfmt.h for details */
yasm_objfmt_module yasm_win32_LTX_objfmt = {
    "Win32",
    "win32",
    "obj",
    32,
    winXX_objfmt_dbgfmt_keywords,
    "null",
    win32_objfmt_create,
    coff_objfmt_output,
    coff_objfmt_destroy,
    coff_objfmt_add_default_section,
    coff_objfmt_section_switch,
    coff_objfmt_extern_declare,
    coff_objfmt_global_declare,
    coff_objfmt_common_declare,
    win32_objfmt_directive
};

/* Define objfmt structure -- see objfmt.h for details */
yasm_objfmt_module yasm_win64_LTX_objfmt = {
    "Win64",
    "win64",
    "obj",
    64,
    winXX_objfmt_dbgfmt_keywords,
    "null",
    win64_objfmt_create,
    coff_objfmt_output,
    coff_objfmt_destroy,
    coff_objfmt_add_default_section,
    coff_objfmt_section_switch,
    coff_objfmt_extern_declare,
    coff_objfmt_global_declare,
    coff_objfmt_common_declare,
    win32_objfmt_directive
};
yasm_objfmt_module yasm_x64_LTX_objfmt = {
    "Win64",
    "x64",
    "obj",
    64,
    winXX_objfmt_dbgfmt_keywords,
    "null",
    win64_objfmt_create,
    coff_objfmt_output,
    coff_objfmt_destroy,
    coff_objfmt_add_default_section,
    coff_objfmt_section_switch,
    coff_objfmt_extern_declare,
    coff_objfmt_global_declare,
    coff_objfmt_common_declare,
    win32_objfmt_directive
};
