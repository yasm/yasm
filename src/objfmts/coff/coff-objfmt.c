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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "file.h"

#include "errwarn.h"
#include "intnum.h"
#include "floatnum.h"
#include "expr.h"
#include "symrec.h"

#include "bytecode.h"
#include "section.h"

#include "expr-int.h"
#include "bc-int.h"

#include "arch.h"
#include "objfmt.h"


#define REGULAR_OUTBUF_SIZE	1024

/* Defining this to 0 sets all section VMA's to 0 rather than as the same as
 * the LMA.  According to the DJGPP COFF Spec, this should be set to 1
 * (VMA=LMA), and indeed DJGPP's GCC output shows VMA=LMA.  However, NASM
 * outputs VMA=0 (as if this was 0), and GNU objdump output looks a lot nicer
 * with VMA=0.  Who's right?  This is #defined as changing this setting affects
 * several places in the code.
 */
#define COFF_SET_VMA	1

#define COFF_I386MAGIC	0x14C

#define COFF_F_LNNO	0x0004	    /* line number info NOT present */
#define COFF_F_LSYMS	0x0008	    /* local symbols NOT present */
#define COFF_F_AR32WR	0x0100	    /* 32-bit little endian file */

typedef STAILQ_HEAD(coff_reloc_head, coff_reloc) coff_reloc_head;

typedef struct coff_reloc {
    STAILQ_ENTRY(coff_reloc) link;  /* internal link, not in file */
    unsigned long addr;		    /* address of relocation */
    yasm_symrec *sym;		    /* relocated symbol */
    enum {
	COFF_RELOC_ADDR32 = 6,	    /* 32-bit absolute reference */
	COFF_RELOC_REL32 = 20	    /* 32-bit relative reference */
    } type;			    /* type of relocation */
} coff_reloc;

typedef enum coff_section_data_flags {
    COFF_STYP_TEXT = 0x0020,
    COFF_STYP_DATA = 0x0040,
    COFF_STYP_BSS = 0x0080
} coff_section_data_flags;

typedef struct coff_section_data {
    /*@dependent@*/ yasm_symrec *sym;	/* symbol created for this section */
    unsigned int scnum;	    /* section number (1=first section) */
    coff_section_data_flags flags;
    unsigned long addr;	    /* starting memory address (first section -> 0) */
    unsigned long scnptr;   /* file ptr to raw data */
    unsigned long size;	    /* size of raw data (section data) in bytes */
    unsigned long relptr;   /* file ptr to relocation */
    unsigned long nreloc;   /* number of relocation entries >64k -> error */
    /*@owned@*/ coff_reloc_head relocs;
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

typedef struct coff_symrec_data {
    unsigned long index;		/* assigned COFF symbol table index */
    coff_symrec_sclass sclass;		/* storage class */
    /*@owned@*/ /*@null@*/ yasm_expr *size; /* size if COMMON declaration */
} coff_symrec_data;

typedef union coff_symtab_auxent {
    /* no data needed for section symbol auxent, all info avail from sym */
    /*@owned@*/ char *fname;	    /* filename aux entry */
} coff_symtab_auxent;

typedef enum coff_symtab_auxtype {
    COFF_SYMTAB_AUX_NONE = 0,
    COFF_SYMTAB_AUX_SECT,
    COFF_SYMTAB_AUX_FILE
} coff_symtab_auxtype;

typedef struct coff_symtab_entry {
    STAILQ_ENTRY(coff_symtab_entry) link;
    /*@dependent@*/ yasm_symrec *sym;
    int numaux;			/* number of auxiliary entries */
    coff_symtab_auxtype auxtype;    /* type of aux entries */
    coff_symtab_auxent aux[1];	/* actually may be any size (including 0) */
} coff_symtab_entry;
typedef STAILQ_HEAD(coff_symtab_head, coff_symtab_entry) coff_symtab_head;

typedef struct coff_objfmt_output_info {
    /*@dependent@*/ FILE *f;
    /*@only@*/ unsigned char *buf;
    yasm_section *sect;
    /*@dependent@*/ coff_section_data *csd;
    unsigned long addr;	    /* start of next section */
} coff_objfmt_output_info;

static unsigned int coff_objfmt_parse_scnum;	/* sect numbering in parser */
static coff_symtab_head coff_symtab;	    /* symbol table of indexed syms */

yasm_objfmt yasm_coff_LTX_objfmt;
static /*@dependent@*/ yasm_arch *cur_arch;
static /*@dependent@*/ yasm_errwarn *cur_we;


static /*@dependent@*/ coff_symtab_entry *
coff_objfmt_symtab_append(yasm_symrec *sym, coff_symrec_sclass sclass,
			  /*@only@*/ /*@null@*/ yasm_expr *size, int numaux,
			  coff_symtab_auxtype auxtype)
{
    /*@null@*/ /*@dependent@*/ coff_symrec_data *sym_data_prev;
    coff_symrec_data *sym_data;
    coff_symtab_entry *entry;

    if (STAILQ_EMPTY(&coff_symtab))
	cur_we->internal_error(N_("empty COFF symbol table"));
    entry = STAILQ_LAST(&coff_symtab, coff_symtab_entry, link);
    sym_data_prev = yasm_symrec_get_of_data(entry->sym);
    assert(sym_data_prev != NULL);

    sym_data = xmalloc(sizeof(coff_symrec_data));
    sym_data->index = sym_data_prev->index + entry->numaux + 1;
    sym_data->sclass = sclass;
    sym_data->size = size;
    yasm_symrec_set_of_data(sym, &yasm_coff_LTX_objfmt, sym_data);

    entry = xmalloc(sizeof(coff_symtab_entry) +
		    (numaux-1)*sizeof(coff_symtab_auxent));
    entry->sym = sym;
    entry->numaux = numaux;
    entry->auxtype = auxtype;
    STAILQ_INSERT_TAIL(&coff_symtab, entry, link);

    return entry;
}

static void
coff_objfmt_initialize(const char *in_filename,
		       /*@unused@*/ const char *obj_filename,
		       /*@unused@*/ yasm_dbgfmt *df, yasm_arch *a,
		       yasm_errwarn *we)
{
    yasm_symrec *filesym;
    coff_symrec_data *data;
    coff_symtab_entry *entry;

    cur_arch = a;
    cur_we = we;

    coff_objfmt_parse_scnum = 1;    /* section numbering starts at 1 */
    STAILQ_INIT(&coff_symtab);

    data = xmalloc(sizeof(coff_symrec_data));
    data->index = 0;
    data->sclass = COFF_SCL_FILE;
    data->size = NULL;
    filesym = yasm_symrec_define_label(".file", NULL, NULL, 0, 0);
    yasm_symrec_set_of_data(filesym, &yasm_coff_LTX_objfmt, data);

    entry = xmalloc(sizeof(coff_symtab_entry));
    entry->sym = filesym;
    entry->numaux = 1;
    entry->auxtype = COFF_SYMTAB_AUX_FILE;
    entry->aux[0].fname = xstrdup(in_filename);
    STAILQ_INSERT_TAIL(&coff_symtab, entry, link);
}

static int
coff_objfmt_set_section_addr(yasm_section *sect, /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    /*@dependent@*/ /*@null@*/ coff_section_data *csd;
    /*@null@*/ yasm_bytecode *last;

    /* Don't output absolute sections */
    if (yasm_section_is_absolute(sect))
	return 0;

    assert(info != NULL);
    csd = yasm_section_get_of_data(sect);
    assert(csd != NULL);

    csd->addr = info->addr;
    last = yasm_bcs_last(yasm_section_get_bytecodes(sect));
    if (last)
	info->addr += last->offset + last->len;

    return 0;
}

static int
coff_objfmt_output_expr(yasm_expr **ep, unsigned char **bufp,
			unsigned long valsize, unsigned long offset,
			/*@observer@*/ const yasm_section *sect,
			/*@observer@*/ const yasm_bytecode *bc, int rel,
			/*@unused@*/ /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *intn;
    /*@dependent@*/ /*@null@*/ const yasm_floatnum *flt;
    /*@dependent@*/ /*@null@*/ yasm_symrec *sym;
    /*@dependent@*/ yasm_section *label_sect;
    /*@dependent@*/ /*@null@*/ yasm_bytecode *label_precbc;

    assert(info != NULL);

    *ep = yasm_expr_simplify(*ep, yasm_common_calc_bc_dist);

    /* Handle floating point expressions */
    flt = yasm_expr_get_floatnum(ep);
    if (flt)
	return cur_arch->floatnum_tobytes(flt, bufp, valsize, *ep);

    /* Handle integer expressions, with relocation if necessary */
    sym = yasm_expr_extract_symrec(ep, yasm_common_calc_bc_dist);
    if (sym) {
	coff_reloc *reloc;
	yasm_sym_vis vis;

	if (valsize != 4) {
	    cur_we->error((*ep)->line, N_("coff: invalid relocation size"));
	    return 1;
	}

	reloc = xmalloc(sizeof(coff_reloc));
	reloc->addr = bc->offset + offset;
	if (COFF_SET_VMA)
	    reloc->addr += info->addr;
	reloc->sym = sym;
	vis = yasm_symrec_get_visibility(sym);
	if (vis & YASM_SYM_COMMON) {
	    /*@dependent@*/ /*@null@*/ coff_symrec_data *csymd;

	    /* COMMON symbols have their length added in */
	    csymd = yasm_symrec_get_of_data(sym);
	    assert(csymd != NULL);
	    *ep = yasm_expr_new(YASM_EXPR_ADD, yasm_expr_expr(*ep),
				yasm_expr_expr(yasm_expr_copy(csymd->size)),
				csymd->size->line);
	    *ep = yasm_expr_simplify(*ep, yasm_common_calc_bc_dist);
	} else if (!(vis & YASM_SYM_EXTERN)) {
	    /* Local symbols need relocation to their section's start */
	    if (yasm_symrec_get_label(sym, &label_sect, &label_precbc)) {
		/*@null@*/ coff_section_data *label_csd;
		label_csd = yasm_section_get_of_data(label_sect);
		assert(label_csd != NULL);
		reloc->sym = label_csd->sym;
		if (COFF_SET_VMA)
		    *ep = yasm_expr_new(YASM_EXPR_ADD, yasm_expr_expr(*ep),
			yasm_expr_int(yasm_intnum_new_uint(label_csd->addr)),
			(*ep)->line);
	    }
	}

	if (rel) {
	    reloc->type = COFF_RELOC_REL32;
	    /* Need to reference to start of section, so add $$ in. */
	    *ep = yasm_expr_new(YASM_EXPR_ADD, yasm_expr_expr(*ep),
		yasm_expr_sym(yasm_symrec_define_label("$$", info->sect, NULL,
						       0, (*ep)->line)),
		(*ep)->line);
	    *ep = yasm_expr_simplify(*ep, yasm_common_calc_bc_dist);
	} else
	    reloc->type = COFF_RELOC_ADDR32;
	info->csd->nreloc++;
	STAILQ_INSERT_TAIL(&info->csd->relocs, reloc, link);
    }
    intn = yasm_expr_get_intnum(ep, NULL);
    if (intn)
	return cur_arch->intnum_tobytes(intn, bufp, valsize, *ep, bc, rel);

    /* Check for complex float expressions */
    if (yasm_expr__contains(*ep, YASM_EXPR_FLOAT)) {
	cur_we->error((*ep)->line,
		      N_("floating point expression too complex"));
	return 1;
    }

    cur_we->error((*ep)->line, N_("coff: relocation too complex"));
    return 1;
}

static int
coff_objfmt_output_bytecode(yasm_bytecode *bc, /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    /*@null@*/ /*@only@*/ unsigned char *bigbuf;
    unsigned long size = REGULAR_OUTBUF_SIZE;
    unsigned long multiple;
    unsigned long i;
    int gap;

    assert(info != NULL);

    bigbuf = yasm_bc_tobytes(bc, info->buf, &size, &multiple, &gap, info->sect,
			     info, coff_objfmt_output_expr, NULL);

    /* Don't bother doing anything else if size ended up being 0. */
    if (size == 0) {
	if (bigbuf)
	    xfree(bigbuf);
	return 0;
    }

    info->csd->size += size;

    /* Warn that gaps are converted to 0 and write out the 0's. */
    if (gap) {
	unsigned long left;
	cur_we->warning(YASM_WARN_GENERAL, bc->line,
	    N_("uninitialized space declared in code/data section: zeroing"));
	/* Write out in chunks */
	memset(info->buf, 0, REGULAR_OUTBUF_SIZE);
	left = multiple*size;
	while (left > REGULAR_OUTBUF_SIZE) {
	    fwrite(info->buf, REGULAR_OUTBUF_SIZE, 1, info->f);
	    left -= REGULAR_OUTBUF_SIZE;
	}
	fwrite(info->buf, left, 1, info->f);
    } else {
	/* Output multiple copies of buf (or bigbuf if non-NULL) to file */
	for (i=0; i<multiple; i++)
	    fwrite(bigbuf ? bigbuf : info->buf, (size_t)size, 1, info->f);
    }

    /* If bigbuf was allocated, free it */
    if (bigbuf)
	xfree(bigbuf);

    return 0;
}

static int
coff_objfmt_output_section(yasm_section *sect, /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    /*@dependent@*/ /*@null@*/ coff_section_data *csd;
    long pos;
    coff_reloc *reloc;

    /* Don't output absolute sections */
    if (yasm_section_is_absolute(sect))
	return 0;

    assert(info != NULL);
    csd = yasm_section_get_of_data(sect);
    assert(csd != NULL);

    csd->addr = info->addr;

    if (csd->flags == COFF_STYP_BSS) {
	/*@null@*/ yasm_bytecode *last =
	    yasm_bcs_last(yasm_section_get_bytecodes(sect));

	/* Don't output BSS sections.
	 * TODO: Check for non-reserve bytecodes?
	 */
	pos = 0;    /* position = 0 because it's not in the file */
	if (last)
	    csd->size = last->offset + last->len;
    } else {
	pos = ftell(info->f);
	if (pos == -1) {
	    cur_we->error(0, N_("could not get file position on output file"));
	    return 1;
	}

	info->sect = sect;
	info->csd = csd;
	yasm_bcs_traverse(yasm_section_get_bytecodes(sect), info,
			  coff_objfmt_output_bytecode);
    }

    /* Empty?  Go on to next section */
    if (csd->size == 0)
	return 0;

    info->addr += csd->size;
    csd->scnptr = (unsigned long)pos;

    /* No relocations to output?  Go on to next section */
    if (csd->nreloc == 0)
	return 0;

    pos = ftell(info->f);
    if (pos == -1) {
	cur_we->error(0, N_("could not get file position on output file"));
	return 1;
    }
    csd->relptr = (unsigned long)pos;

    STAILQ_FOREACH(reloc, &csd->relocs, link) {
	unsigned char *localbuf = info->buf;
	/*@null@*/ coff_symrec_data *csymd;

	csymd = yasm_symrec_get_of_data(reloc->sym);
	if (!csymd)
	    cur_we->internal_error(
		N_("coff: no symbol data for relocated symbol"));

	YASM_WRITE_32_L(localbuf, reloc->addr);	    /* address of relocation */
	YASM_WRITE_32_L(localbuf, csymd->index);    /* relocated symbol */
	YASM_WRITE_16_L(localbuf, reloc->type);	    /* type of relocation */
	fwrite(info->buf, 10, 1, info->f);
    }

    return 0;
}

static int
coff_objfmt_output_secthead(yasm_section *sect, /*@null@*/ void *d)
{
    /*@null@*/ coff_objfmt_output_info *info = (coff_objfmt_output_info *)d;
    /*@dependent@*/ /*@null@*/ coff_section_data *csd;
    unsigned char *localbuf;

    /* Don't output absolute sections */
    if (yasm_section_is_absolute(sect))
	return 0;

    assert(info != NULL);
    csd = yasm_section_get_of_data(sect);
    assert(csd != NULL);

    localbuf = info->buf;
    strncpy((char *)localbuf, yasm_section_get_name(sect), 8);/* section name */
    localbuf += 8;
    YASM_WRITE_32_L(localbuf, csd->addr);	/* physical address */
    if (COFF_SET_VMA)
	YASM_WRITE_32_L(localbuf, csd->addr);   /* virtual address */
    else
	YASM_WRITE_32_L(localbuf, 0);		/* virtual address */
    YASM_WRITE_32_L(localbuf, csd->size);	/* section size */
    YASM_WRITE_32_L(localbuf, csd->scnptr);	/* file ptr to data */
    YASM_WRITE_32_L(localbuf, csd->relptr);	/* file ptr to relocs */
    YASM_WRITE_32_L(localbuf, 0);		/* file ptr to line nums */
    if (csd->nreloc >= 64*1024) {
	cur_we->warning(YASM_WARN_GENERAL, 0,
			N_("too many relocations in section `%s'"),
			yasm_section_get_name(sect));
	YASM_WRITE_16_L(localbuf, 0xFFFF);	/* max out */
    } else
	YASM_WRITE_16_L(localbuf, csd->nreloc); /* num of relocation entries */
    YASM_WRITE_16_L(localbuf, 0);		/* num of line number entries */
    YASM_WRITE_32_L(localbuf, csd->flags);	/* flags */
    fwrite(info->buf, 40, 1, info->f);

    return 0;
}

static void
coff_objfmt_output(FILE *f, yasm_sectionhead *sections)
{
    coff_objfmt_output_info info;
    unsigned char *localbuf;
    long pos;
    unsigned long symtab_pos;
    unsigned long symtab_count = 0;
    unsigned long strtab_offset = 4;
    coff_symtab_entry *entry;

    info.f = f;
    info.buf = xmalloc(REGULAR_OUTBUF_SIZE);

    /* Allocate space for headers by seeking forward */
    if (fseek(f, 20+40*(coff_objfmt_parse_scnum-1), SEEK_SET) < 0) {
	cur_we->error(0, N_("could not seek on output file"));
	return;
    }

    /* Section data/relocs */
    if (COFF_SET_VMA) {
	/* If we're setting the VMA, we need to do a first section pass to
	 * determine each section's addr value before actually outputting
	 * relocations, as a relocation's section address is added into the
	 * addends in the generated code.
	 */
	info.addr = 0;
	if (yasm_sections_traverse(sections, &info,
				   coff_objfmt_set_section_addr))
	    return;
    }
    info.addr = 0;
    if (yasm_sections_traverse(sections, &info, coff_objfmt_output_section))
	return;

    /* Symbol table */
    pos = ftell(f);
    if (pos == -1) {
	cur_we->error(0, N_("could not get file position on output file"));
	return;
    }
    symtab_pos = (unsigned long)pos;
    STAILQ_FOREACH(entry, &coff_symtab, link) {
	const char *name = yasm_symrec_get_name(entry->sym);
	size_t len = strlen(name);
	int aux;
	/*@dependent@*/ /*@null@*/ coff_symrec_data *csymd;
	unsigned long value = 0;
	unsigned int scnum = 0xfffe;	/* -2 = debugging symbol */
	/*@dependent@*/ /*@null@*/ yasm_section *sect;
	/*@dependent@*/ /*@null@*/ yasm_bytecode *precbc;
	unsigned long scnlen = 0;   /* for sect auxent */
	unsigned long nreloc = 0;   /* for sect auxent */

	/* Get symrec's of_data (needed for storage class) */
	csymd = yasm_symrec_get_of_data(entry->sym);
	if (!csymd)
	    cur_we->internal_error(
		N_("coff: expected sym data to be present"));

	/* Look at symrec for value/scnum/etc. */
	if (yasm_symrec_get_label(entry->sym, &sect, &precbc)) {
	    /* it's a label: get value and offset.
	     * If there is not a section, leave as debugging symbol.
	     */
	    if (sect) {
		/*@dependent@*/ /*@null@*/ coff_section_data *csectd;
		csectd = yasm_section_get_of_data(sect);
		scnum = csectd->scnum;
		scnlen = csectd->size;
		nreloc = csectd->nreloc;
		if (precbc)
		    value = precbc->offset + precbc->len;
		if (COFF_SET_VMA)
		    value += csectd->addr;
	    }
	} else {
	    yasm_sym_vis vis = yasm_symrec_get_visibility(entry->sym);
	    if (vis & YASM_SYM_COMMON) {
		const yasm_intnum *intn;
		intn = yasm_expr_get_intnum(&csymd->size,
					    yasm_common_calc_bc_dist);
		if (!intn)
		    cur_we->error(csymd->size->line,
			N_("COMMON data size not an integer expression"));
		else
		    value = yasm_intnum_get_uint(intn);
		scnum = 0;
	    }
	    if (vis & YASM_SYM_EXTERN)
		scnum = 0;
	}

	localbuf = info.buf;
	if (len > 8) {
	    YASM_WRITE_32_L(localbuf, 0);	/* "zeros" field */
	    YASM_WRITE_32_L(localbuf, strtab_offset); /* string table offset */
	    strtab_offset += len+1;
	} else {
	    /* <8 chars, so no string table entry needed */
	    strncpy((char *)localbuf, name, 8);
	    localbuf += 8;
	}
	YASM_WRITE_32_L(localbuf, value);	/* value */
	YASM_WRITE_16_L(localbuf, scnum);	/* section number */
	YASM_WRITE_16_L(localbuf, 0);	    /* type is always zero (for now) */
	YASM_WRITE_8(localbuf, csymd->sclass);	/* storage class */
	YASM_WRITE_8(localbuf, entry->numaux);	/* number of aux entries */
	fwrite(info.buf, 18, 1, f);
	for (aux=0; aux<entry->numaux; aux++) {
	    localbuf = info.buf;
	    memset(localbuf, 0, 18);
	    switch (entry->auxtype) {
		case COFF_SYMTAB_AUX_NONE:
		    break;
		case COFF_SYMTAB_AUX_SECT:
		    YASM_WRITE_32_L(localbuf, scnlen);	/* section length */
		    YASM_WRITE_16_L(localbuf, nreloc);	/* number relocs */
		    YASM_WRITE_16_L(localbuf, 0);	/* number line nums */
		    break;
		case COFF_SYMTAB_AUX_FILE:
		    len = strlen(entry->aux[0].fname);
		    if (len > 14) {
			YASM_WRITE_32_L(localbuf, 0);
			YASM_WRITE_32_L(localbuf, strtab_offset);
			strtab_offset += len+1;
		    } else
			strncpy((char *)localbuf, entry->aux[0].fname, 14);
		    break;
		default:
		    cur_we->internal_error(
			N_("coff: unrecognized aux symtab type"));
	    }
	    fwrite(info.buf, 18, 1, f);
	    symtab_count++;
	}
	symtab_count++;
    }

    /* String table */
    yasm_fwrite_32_l(strtab_offset, f); /* first four bytes are total length */
    STAILQ_FOREACH(entry, &coff_symtab, link) {
	const char *name = yasm_symrec_get_name(entry->sym);
	size_t len = strlen(name);
	int aux;

	if (len > 8)
	    fwrite(name, len+1, 1, f);
	for (aux=0; aux<entry->numaux; aux++) {
	    switch (entry->auxtype) {
		case COFF_SYMTAB_AUX_FILE:
		    len = strlen(entry->aux[0].fname);
		    if (len > 14)
			fwrite(entry->aux[0].fname, len+1, 1, f);
		    break;
		default:
		    break;
	    }
	}
    }

    /* Write headers */
    if (fseek(f, 0, SEEK_SET) < 0) {
	cur_we->error(0, N_("could not seek on output file"));
	return;
    }

    localbuf = info.buf;
    YASM_WRITE_16_L(localbuf, COFF_I386MAGIC);		/* magic number */
    YASM_WRITE_16_L(localbuf, coff_objfmt_parse_scnum-1);/* number of sects */
    YASM_WRITE_32_L(localbuf, 0);			/* time/date stamp */
    YASM_WRITE_32_L(localbuf, symtab_pos);		/* file ptr to symtab */
    YASM_WRITE_32_L(localbuf, symtab_count);		/* number of symtabs */
    YASM_WRITE_16_L(localbuf, 0);	/* size of optional header (none) */
    YASM_WRITE_16_L(localbuf, COFF_F_AR32WR|COFF_F_LNNO|COFF_F_LSYMS); /* flags */
    fwrite(info.buf, 20, 1, f);

    yasm_sections_traverse(sections, &info, coff_objfmt_output_secthead);

    xfree(info.buf);
}

static void
coff_objfmt_cleanup(void)
{
    coff_symtab_entry *entry1, *entry2;

    /* Delete local symbol table */
    entry1 = STAILQ_FIRST(&coff_symtab);
    while (entry1 != NULL) {
	entry2 = STAILQ_NEXT(entry1, link);
	if (entry1->numaux == 1 && entry1->auxtype == COFF_SYMTAB_AUX_FILE)
	    xfree(entry1->aux[0].fname);
	xfree(entry1);
	entry1 = entry2;
    }
}

static /*@observer@*/ /*@null@*/ yasm_section *
coff_objfmt_sections_switch(yasm_sectionhead *headp,
			    yasm_valparamhead *valparams,
			    /*@unused@*/ /*@null@*/
			    yasm_valparamhead *objext_valparams,
			    unsigned long lindex)
{
    yasm_valparam *vp = yasm_vps_first(valparams);
    yasm_section *retval;
    int isnew;
    coff_section_data_flags flags;
    int flags_override = 0;
    char *sectname;
    int resonly = 0;

    if (!vp || vp->param || !vp->val)
	return NULL;

    sectname = vp->val;
    if (strlen(sectname) > 8) {
	cur_we->warning(YASM_WARN_GENERAL, lindex,
	    N_("COFF section names limited to 8 characters: truncating"));
	sectname[8] = '\0';
    }

    if (strcmp(sectname, ".data") == 0)
	flags = COFF_STYP_DATA;
    else if (strcmp(sectname, ".bss") == 0) {
	flags = COFF_STYP_BSS;
	resonly = 1;
    } else
	flags = COFF_STYP_TEXT;

    while ((vp = yasm_vps_next(vp))) {
	if (strcasecmp(vp->val, "code") == 0 ||
	    strcasecmp(vp->val, "text") == 0) {
	    flags = COFF_STYP_TEXT;
	    flags_override = 1;
	} else if (strcasecmp(vp->val, "data") == 0) {
	    flags = COFF_STYP_DATA;
	    flags_override = 1;
	} else if (strcasecmp(vp->val, "bss") == 0) {
	    flags = COFF_STYP_BSS;
	    flags_override = 1;
	    resonly = 1;
	}
    }

    retval = yasm_sections_switch_general(headp, sectname, 0, resonly, &isnew,
					  lindex, cur_we->internal_error_);

    if (isnew) {
	coff_section_data *data;
	yasm_symrec *sym;

	data = xmalloc(sizeof(coff_section_data));
	data->scnum = coff_objfmt_parse_scnum++;
	data->flags = flags;
	data->addr = 0;
	data->scnptr = 0;
	data->size = 0;
	data->relptr = 0;
	data->nreloc = 0;
	STAILQ_INIT(&data->relocs);
	yasm_section_set_of_data(retval, &yasm_coff_LTX_objfmt, data);

	sym = yasm_symrec_define_label(sectname, retval, (yasm_bytecode *)NULL,
				       1, lindex);
	coff_objfmt_symtab_append(sym, COFF_SCL_STAT, NULL, 1,
				  COFF_SYMTAB_AUX_SECT);
	data->sym = sym;
    } else if (flags_override)
	cur_we->warning(YASM_WARN_GENERAL, lindex,
			N_("section flags ignored on section redeclaration"));
    return retval;
}

static void
coff_objfmt_section_data_delete(/*@only@*/ void *data)
{
    coff_section_data *csd = (coff_section_data *)data;
    coff_reloc *r1, *r2;
    r1 = STAILQ_FIRST(&csd->relocs);
    while (r1 != NULL) {
	r2 = STAILQ_NEXT(r1, link);
	xfree(r1);
	r1 = r2;
    }
    xfree(data);
}

static void
coff_objfmt_section_data_print(FILE *f, int indent_level, void *data)
{
    coff_section_data *csd = (coff_section_data *)data;
    coff_reloc *reloc;
    unsigned long relocnum = 0;

    fprintf(f, "%*ssym=\n", indent_level, "");
    yasm_symrec_print(f, indent_level+1, csd->sym);
    fprintf(f, "%*sscnum=%d\n", indent_level, "", csd->scnum);
    fprintf(f, "%*sflags=", indent_level, "");
    switch (csd->flags) {
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
    STAILQ_FOREACH(reloc, &csd->relocs, link) {
	fprintf(f, "%*sReloc %lu:\n", indent_level+1, "", relocnum++);
	fprintf(f, "%*ssym=\n", indent_level+2, "");
	yasm_symrec_print(f, indent_level+3, reloc->sym);
	fprintf(f, "%*stype=", indent_level+2, "");
	switch (reloc->type) {
	    case COFF_RELOC_ADDR32:
		printf("Addr32\n");
		break;
	    case COFF_RELOC_REL32:
		printf("Rel32\n");
		break;
	}
    }
}

static void
coff_objfmt_extglob_declare(yasm_symrec *sym, /*@unused@*/
			    /*@null@*/ yasm_valparamhead *objext_valparams,
			    /*@unused@*/ unsigned long lindex)
{
    coff_objfmt_symtab_append(sym, COFF_SCL_EXT, NULL, 0,
			      COFF_SYMTAB_AUX_NONE);
}

static void
coff_objfmt_common_declare(yasm_symrec *sym, /*@only@*/ yasm_expr *size,
			   /*@unused@*/ /*@null@*/
			   yasm_valparamhead *objext_valparams,
			   /*@unused@*/ unsigned long lindex)
{
    coff_objfmt_symtab_append(sym, COFF_SCL_EXT, size, 0,
			      COFF_SYMTAB_AUX_NONE);
}

static void
coff_objfmt_symrec_data_delete(/*@only@*/ void *data)
{
    coff_symrec_data *csymd = (coff_symrec_data *)data;
    if (csymd->size)
	yasm_expr_delete(csymd->size);
    xfree(data);
}

static void
coff_objfmt_symrec_data_print(FILE *f, int indent_level, void *data)
{
    coff_symrec_data *csd = (coff_symrec_data *)data;

    fprintf(f, "%*ssymtab index=%lu\n", indent_level, "", csd->index);
    fprintf(f, "%*ssclass=%d\n", indent_level, "", csd->sclass);
    fprintf(f, "%*ssize=", indent_level, "");
    if (csd->size)
	yasm_expr_print(f, csd->size);
    else
	fprintf(f, "nil");
    fprintf(f, "\n");
}

static int
coff_objfmt_directive(/*@unused@*/ const char *name,
		      /*@unused@*/ yasm_valparamhead *valparams,
		      /*@unused@*/ /*@null@*/
		      yasm_valparamhead *objext_valparams,
		      /*@unused@*/ yasm_sectionhead *headp,
		      /*@unused@*/ unsigned long lindex)
{
    return 1;	/* no objfmt directives */
}


/* Define valid debug formats to use with this object format */
static const char *coff_objfmt_dbgfmt_keywords[] = {
    "null",
    NULL
};

/* Define objfmt structure -- see objfmt.h for details */
yasm_objfmt yasm_coff_LTX_objfmt = {
    "COFF (DJGPP)",
    "coff",
    "o",
    ".text",
    32,
    coff_objfmt_dbgfmt_keywords,
    "null",
    coff_objfmt_initialize,
    coff_objfmt_output,
    coff_objfmt_cleanup,
    coff_objfmt_sections_switch,
    coff_objfmt_section_data_delete,
    coff_objfmt_section_data_print,
    coff_objfmt_extglob_declare,
    coff_objfmt_extglob_declare,
    coff_objfmt_common_declare,
    coff_objfmt_symrec_data_delete,
    coff_objfmt_symrec_data_print,
    coff_objfmt_directive,
    NULL /*coff_objfmt_bc_objfmt_data_delete*/,
    NULL /*coff_objfmt_bc_objfmt_data_print*/
};
