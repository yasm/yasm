/*
 * Extended Dynamic Object format
 *
 *  Copyright (C) 2004  Peter Johnson
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

#define XDF_MAGIC	0x87654321

#define XDF_SYM_EXTERN	1
#define XDF_SYM_GLOBAL	2
#define XDF_SYM_EQU	4

typedef STAILQ_HEAD(xdf_reloc_head, xdf_reloc) xdf_reloc_head;

typedef struct xdf_reloc {
    yasm_reloc reloc;
    /*@null@*/ yasm_symrec *base;   /* base symbol (for WRT) */
    enum {
	XDF_RELOC_REL = 1,	    /* relative to segment */
	XDF_RELOC_WRT = 2,	    /* relative to symbol */
	XDF_RELOC_RIP = 4,	    /* RIP-relative */
	XDF_RELOC_SEG = 8	    /* segment containing symbol */
    } type;			    /* type of relocation */
    enum {
	XDF_RELOC_8  = 1,         
	XDF_RELOC_16 = 2,      
	XDF_RELOC_32 = 4,      
	XDF_RELOC_64 = 8
    } size;			    /* size of relocation */
    unsigned int shift;		    /* relocation shift (0,4,8,16,24,32) */
} xdf_reloc;

typedef struct xdf_section_data {
    /*@dependent@*/ yasm_symrec *sym;	/* symbol created for this section */
    yasm_intnum *addr;	    /* starting memory address */
    long scnum;		    /* section number (0=first section) */
    unsigned int align;	    /* section alignment (0-4096) */
    enum {
	XDF_SECT_ABSOLUTE = 0x01,
	XDF_SECT_FLAT = 0x02,
	XDF_SECT_BSS = 0x04,
	XDF_SECT_EQU = 0x08,
	XDF_SECT_USE_16 = 0x10,
	XDF_SECT_USE_32 = 0x20,
	XDF_SECT_USE_64 = 0x40
    } flags;		    /* section flags */
    unsigned long scnptr;   /* file ptr to raw data */
    unsigned long size;	    /* size of raw data (section data) in bytes */
    unsigned long relptr;   /* file ptr to relocation */
    unsigned long nreloc;   /* number of relocation entries >64k -> error */
    /*@owned@*/ xdf_reloc_head relocs;
} xdf_section_data;

typedef struct xdf_symrec_data {
    unsigned long index;		/* assigned XDF symbol table index */
} xdf_symrec_data;

typedef struct xdf_symtab_entry {
    STAILQ_ENTRY(xdf_symtab_entry) link;
    /*@dependent@*/ yasm_symrec *sym;
} xdf_symtab_entry;
typedef STAILQ_HEAD(xdf_symtab_head, xdf_symtab_entry) xdf_symtab_head;

typedef struct yasm_objfmt_xdf {
    yasm_objfmt_base objfmt;		    /* base structure */

    long parse_scnum;		    /* sect numbering in parser */
    xdf_symtab_head xdf_symtab;	    /* symbol table of indexed syms */

    yasm_object *object;
    yasm_symtab *symtab;
    /*@dependent@*/ yasm_arch *arch;
} yasm_objfmt_xdf;

typedef struct xdf_objfmt_output_info {
    yasm_objfmt_xdf *objfmt_xdf;
    /*@dependent@*/ FILE *f;
    /*@only@*/ unsigned char *buf;
    yasm_section *sect;
    /*@dependent@*/ xdf_section_data *xsd;
} xdf_objfmt_output_info;

static void xdf_section_data_destroy(/*@only@*/ void *d);
static void xdf_section_data_print(void *data, FILE *f, int indent_level);

static const yasm_assoc_data_callback xdf_section_data_cb = {
    xdf_section_data_destroy,
    xdf_section_data_print
};

static void xdf_symrec_data_destroy(/*@only@*/ void *d);
static void xdf_symrec_data_print(void *data, FILE *f, int indent_level);

static const yasm_assoc_data_callback xdf_symrec_data_cb = {
    xdf_symrec_data_destroy,
    xdf_symrec_data_print
};

yasm_objfmt_module yasm_xdf_LTX_objfmt;


static /*@dependent@*/ xdf_symtab_entry *
xdf_objfmt_symtab_append(yasm_objfmt_xdf *objfmt_xdf, yasm_symrec *sym)
{
    /*@null@*/ /*@dependent@*/ xdf_symrec_data *sym_data_prev;
    xdf_symrec_data *sym_data;
    xdf_symtab_entry *entry;
    unsigned long indx;

    if (STAILQ_EMPTY(&objfmt_xdf->xdf_symtab))
	indx = 0;
    else {
	entry = STAILQ_LAST(&objfmt_xdf->xdf_symtab, xdf_symtab_entry, link);
	sym_data_prev = yasm_symrec_get_data(entry->sym, &xdf_symrec_data_cb);
	assert(sym_data_prev != NULL);
	indx = sym_data_prev->index + 1;
    }

    sym_data = yasm_xmalloc(sizeof(xdf_symrec_data));
    sym_data->index = indx;
    yasm_symrec_add_data(sym, &xdf_symrec_data_cb, sym_data);

    entry = yasm_xmalloc(sizeof(xdf_symtab_entry));
    entry->sym = sym;
    STAILQ_INSERT_TAIL(&objfmt_xdf->xdf_symtab, entry, link);

    return entry;
}

static int
xdf_objfmt_append_local_sym(yasm_symrec *sym, /*@unused@*/ /*@null@*/ void *d)
{
    /*@null@*/ yasm_objfmt_xdf *objfmt_xdf = (yasm_objfmt_xdf *)d;
    assert(objfmt_xdf != NULL);
    if (!yasm_symrec_get_data(sym, &xdf_symrec_data_cb))
	xdf_objfmt_symtab_append(objfmt_xdf, sym);
    return 1;
}

static yasm_objfmt *
xdf_objfmt_create(const char *in_filename, yasm_object *object, yasm_arch *a)
{
    yasm_objfmt_xdf *objfmt_xdf = yasm_xmalloc(sizeof(yasm_objfmt_xdf));

    objfmt_xdf->object = object;
    objfmt_xdf->symtab = yasm_object_get_symtab(object);
    objfmt_xdf->arch = a;

    /* Only support x86 arch */
    if (yasm__strcasecmp(yasm_arch_keyword(a), "x86") != 0) {
	yasm_xfree(objfmt_xdf);
	return NULL;
    }

    /* Support x86 and amd64 machines of x86 arch */
    if (yasm__strcasecmp(yasm_arch_get_machine(a), "x86") &&
	yasm__strcasecmp(yasm_arch_get_machine(a), "amd64")) {
	yasm_xfree(objfmt_xdf);
	return NULL;
    }

    objfmt_xdf->parse_scnum = 0;    /* section numbering starts at 0 */
    STAILQ_INIT(&objfmt_xdf->xdf_symtab);

    objfmt_xdf->objfmt.module = &yasm_xdf_LTX_objfmt;

    return (yasm_objfmt *)objfmt_xdf;
}

static int
xdf_objfmt_output_expr(yasm_expr **ep, unsigned char *buf, size_t destsize,
			size_t valsize, int shift, unsigned long offset,
			yasm_bytecode *bc, int rel, int warn,
			/*@null@*/ void *d)
{
    /*@null@*/ xdf_objfmt_output_info *info = (xdf_objfmt_output_info *)d;
    yasm_objfmt_xdf *objfmt_xdf;
    /*@dependent@*/ /*@null@*/ yasm_intnum *intn;
    /*@dependent@*/ /*@null@*/ const yasm_floatnum *flt;
    /*@dependent@*/ /*@null@*/ yasm_symrec *sym;
    yasm_expr *shr_expr;
    yasm_expr *wrt_expr;
    unsigned int shr = 0;
    unsigned int seg = 0;

    assert(info != NULL);
    objfmt_xdf = info->objfmt_xdf;

    *ep = yasm_expr_simplify(*ep, yasm_common_calc_bc_dist);

    /* Handle floating point expressions */
    flt = yasm_expr_get_floatnum(ep);
    if (flt) {
	if (shift < 0)
	    yasm_internal_error(N_("attempting to negative shift a float"));
	return yasm_arch_floatnum_tobytes(objfmt_xdf->arch, flt, buf,
					  destsize, valsize,
					  (unsigned int)shift, warn, bc->line);
    }

    /* Check for a right shift value */
    shr_expr = yasm_expr_extract_shr(ep);
    if (shr_expr) {
	/*@dependent@*/ /*@null@*/ const yasm_intnum *shr_intn;
	shr_intn = yasm_expr_get_intnum(&shr_expr, NULL);
	if (!shr_intn) {
	    yasm__error(bc->line, N_("shift expression too complex"));
	    return 1;
	}
	shr = yasm_intnum_get_uint(shr_intn);
    }

    /* Check for a segment relocation */
    if (yasm_expr_extract_seg(ep))
	seg = 1;

    /* Check for a WRT relocation */
    wrt_expr = yasm_expr_extract_wrt(ep);

    /* Handle integer expressions, with relocation if necessary */
    sym = yasm_expr_extract_symrec(ep, 0, yasm_common_calc_bc_dist);
    if (sym) {
	xdf_reloc *reloc;

	reloc = yasm_xmalloc(sizeof(xdf_reloc));
	reloc->reloc.addr = yasm_intnum_create_uint(bc->offset + offset);
	reloc->reloc.sym = sym;
	reloc->base = NULL;
	reloc->size = valsize/8;
	reloc->shift = shr;

	if (seg)
	    reloc->type = XDF_RELOC_SEG;
	else if (wrt_expr) {
	    reloc->base = yasm_expr_extract_symrec(&wrt_expr, 0,
						   yasm_common_calc_bc_dist);
	    if (!reloc->base) {
		yasm__error(bc->line, N_("WRT expression too complex"));
		return 1;
	    }
	    reloc->type = XDF_RELOC_WRT;
	} else if (rel) {
	    reloc->type = XDF_RELOC_RIP;
	    /* Need to reference to start of section, so add $$ in. */
	    *ep = yasm_expr_create(YASM_EXPR_ADD, yasm_expr_expr(*ep),
		yasm_expr_sym(yasm_symtab_define_label2("$$",
		    yasm_section_bcs_first(info->sect), 0, (*ep)->line)),
		(*ep)->line);
	    *ep = yasm_expr_simplify(*ep, yasm_common_calc_bc_dist);
	} else
	    reloc->type = XDF_RELOC_REL;
	info->xsd->nreloc++;
	yasm_section_add_reloc(info->sect, (yasm_reloc *)reloc, yasm_xfree);
    }
    intn = yasm_expr_get_intnum(ep, NULL);
    if (intn) {
	if (rel) {
	    int retval = yasm_arch_intnum_fixup_rel(objfmt_xdf->arch, intn,
						    valsize, bc, bc->line);
	    if (retval)
		return retval;
	}
	return yasm_arch_intnum_tobytes(objfmt_xdf->arch, intn, buf, destsize,
					valsize, shift, bc, warn, bc->line);
    }

    /* Check for complex float expressions */
    if (yasm_expr__contains(*ep, YASM_EXPR_FLOAT)) {
	yasm__error(bc->line, N_("floating point expression too complex"));
	return 1;
    }

    yasm__error(bc->line, N_("xdf: relocation too complex"));
    return 1;
}

static int
xdf_objfmt_output_bytecode(yasm_bytecode *bc, /*@null@*/ void *d)
{
    /*@null@*/ xdf_objfmt_output_info *info = (xdf_objfmt_output_info *)d;
    /*@null@*/ /*@only@*/ unsigned char *bigbuf;
    unsigned long size = REGULAR_OUTBUF_SIZE;
    unsigned long multiple;
    unsigned long i;
    int gap;

    assert(info != NULL);

    bigbuf = yasm_bc_tobytes(bc, info->buf, &size, &multiple, &gap, info,
			     xdf_objfmt_output_expr, NULL);

    /* Don't bother doing anything else if size ended up being 0. */
    if (size == 0) {
	if (bigbuf)
	    yasm_xfree(bigbuf);
	return 0;
    }

    info->xsd->size += multiple*size;

    /* Warn that gaps are converted to 0 and write out the 0's. */
    if (gap) {
	unsigned long left;
	yasm__warning(YASM_WARN_GENERAL, bc->line,
	    N_("uninitialized space: zeroing"));
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
	yasm_xfree(bigbuf);

    return 0;
}

static int
xdf_objfmt_output_section(yasm_section *sect, /*@null@*/ void *d)
{
    /*@null@*/ xdf_objfmt_output_info *info = (xdf_objfmt_output_info *)d;
    /*@dependent@*/ /*@null@*/ xdf_section_data *xsd;
    long pos;
    xdf_reloc *reloc;
    yasm_bytecode *last;

    /* FIXME: Don't output absolute sections into the section table */
    if (yasm_section_is_absolute(sect))
	return 0;

    assert(info != NULL);
    xsd = yasm_section_get_data(sect, &xdf_section_data_cb);
    assert(xsd != NULL);

    last = yasm_section_bcs_last(sect);
    if (xsd->flags & XDF_SECT_BSS) {
	/* Don't output BSS sections.
	 * TODO: Check for non-reserve bytecodes?
	 */
	pos = 0;    /* position = 0 because it's not in the file */
	xsd->size = last->offset + last->len;
    } else {
	pos = ftell(info->f);
	if (pos == -1) {
	    yasm__fatal(N_("could not get file position on output file"));
	    /*@notreached@*/
	    return 1;
	}

	info->sect = sect;
	info->xsd = xsd;
	yasm_section_bcs_traverse(sect, info, xdf_objfmt_output_bytecode);

	/* Sanity check final section size */
	if (xsd->size != (last->offset + last->len))
	    yasm_internal_error(
		N_("xdf: section computed size did not match actual size"));
    }

    /* Empty?  Go on to next section */
    if (xsd->size == 0)
	return 0;

    xsd->scnptr = (unsigned long)pos;

    /* No relocations to output?  Go on to next section */
    if (xsd->nreloc == 0)
	return 0;

    pos = ftell(info->f);
    if (pos == -1) {
	yasm__fatal(N_("could not get file position on output file"));
	/*@notreached@*/
	return 1;
    }
    xsd->relptr = (unsigned long)pos;

    reloc = (xdf_reloc *)yasm_section_relocs_first(sect);
    while (reloc) {
	unsigned char *localbuf = info->buf;
	/*@null@*/ xdf_symrec_data *xsymd;

	xsymd = yasm_symrec_get_data(reloc->reloc.sym, &xdf_symrec_data_cb);
	if (!xsymd)
	    yasm_internal_error(
		N_("xdf: no symbol data for relocated symbol"));

	yasm_intnum_get_sized(reloc->reloc.addr, localbuf, 4, 32, 0, 0, 0, 0);
	localbuf += 4;				/* address of relocation */
	YASM_WRITE_32_L(localbuf, xsymd->index);    /* relocated symbol */
	if (reloc->base) {
	    xsymd = yasm_symrec_get_data(reloc->base, &xdf_symrec_data_cb);
	    if (!xsymd)
		yasm_internal_error(
		    N_("xdf: no symbol data for relocated base symbol"));
	    YASM_WRITE_32_L(localbuf, xsymd->index); /* base symbol */
	} else {
	    if (reloc->type == XDF_RELOC_WRT)
		yasm_internal_error(
		    N_("xdf: no base symbol for WRT relocation"));
	    YASM_WRITE_32_L(localbuf, 0);	    /* no base symbol */
	}
	YASM_WRITE_8(localbuf, reloc->type);	    /* type of relocation */
	YASM_WRITE_8(localbuf, reloc->size);	    /* size of relocation */
	YASM_WRITE_8(localbuf, reloc->shift);	    /* relocation shift */
	YASM_WRITE_8(localbuf, 0);		    /* flags */
	fwrite(info->buf, 16, 1, info->f);

	reloc = (xdf_reloc *)yasm_section_reloc_next((yasm_reloc *)reloc);
    }

    return 0;
}

static int
xdf_objfmt_output_secthead(yasm_section *sect, /*@null@*/ void *d)
{
    /*@null@*/ xdf_objfmt_output_info *info = (xdf_objfmt_output_info *)d;
    yasm_objfmt_xdf *objfmt_xdf;
    /*@dependent@*/ /*@null@*/ xdf_section_data *xsd;
    /*@null@*/ xdf_symrec_data *xsymd;
    unsigned char *localbuf;

    /* Don't output absolute sections into the section table */
    if (yasm_section_is_absolute(sect))
	return 0;

    assert(info != NULL);
    objfmt_xdf = info->objfmt_xdf;
    xsd = yasm_section_get_data(sect, &xdf_section_data_cb);
    assert(xsd != NULL);

    localbuf = info->buf;
    xsymd = yasm_symrec_get_data(xsd->sym, &xdf_symrec_data_cb);
    assert(xsymd != NULL);

    YASM_WRITE_32_L(localbuf, xsymd->index);	/* section name symbol */
    if (xsd->addr) {
	yasm_intnum_get_sized(xsd->addr, localbuf, 8, 64, 0, 0, 0, 0);
	localbuf += 8;				/* physical address */
    } else {
	YASM_WRITE_32_L(localbuf, 0);
	YASM_WRITE_32_L(localbuf, 0);
    }
    YASM_WRITE_16_L(localbuf, xsd->align);	/* alignment */
    YASM_WRITE_16_L(localbuf, xsd->flags);	/* flags */
    YASM_WRITE_32_L(localbuf, xsd->scnptr);	/* file ptr to data */
    YASM_WRITE_32_L(localbuf, xsd->size);	/* section size */
    YASM_WRITE_32_L(localbuf, xsd->relptr);	/* file ptr to relocs */
    YASM_WRITE_32_L(localbuf, xsd->nreloc); /* num of relocation entries */
    fwrite(info->buf, 32, 1, info->f);

    return 0;
}

static void
xdf_objfmt_output(yasm_objfmt *objfmt, FILE *f, const char *obj_filename,
		   int all_syms, /*@unused@*/ yasm_dbgfmt *df)
{
    yasm_objfmt_xdf *objfmt_xdf = (yasm_objfmt_xdf *)objfmt;
    xdf_objfmt_output_info info;
    unsigned char *localbuf;
    unsigned long symtab_count = 0;
    unsigned long strtab_offset;
    xdf_symtab_entry *entry;

    info.objfmt_xdf = objfmt_xdf;
    info.f = f;
    info.buf = yasm_xmalloc(REGULAR_OUTBUF_SIZE);

    /* Allocate space for headers by seeking forward */
    if (fseek(f, (long)(16+32*(objfmt_xdf->parse_scnum)), SEEK_SET) < 0) {
	yasm__fatal(N_("could not seek on output file"));
	/*@notreached@*/
	return;
    }

    /* Symbol table */
    all_syms = 1;	/* force all syms into symbol table */
    if (all_syms) {
	/* Need to put all local syms into XDF symbol table */
	yasm_symtab_traverse(objfmt_xdf->symtab, objfmt_xdf,
			     xdf_objfmt_append_local_sym);
    }

    /* Get number of symbols */
    if (STAILQ_EMPTY(&objfmt_xdf->xdf_symtab))
	symtab_count = 0;
    else {
	/*@null@*/ /*@dependent@*/ xdf_symrec_data *sym_data_prev;
	entry = STAILQ_LAST(&objfmt_xdf->xdf_symtab, xdf_symtab_entry, link);
	sym_data_prev = yasm_symrec_get_data(entry->sym, &xdf_symrec_data_cb);
	assert(sym_data_prev != NULL);
	symtab_count = sym_data_prev->index + 1;
    }

    /* Get file offset of start of string table */
    strtab_offset = 16+32*(objfmt_xdf->parse_scnum)+16*symtab_count;

    STAILQ_FOREACH(entry, &objfmt_xdf->xdf_symtab, link) {
	const char *name = yasm_symrec_get_name(entry->sym);
	const yasm_expr *equ_val;
	const yasm_intnum *intn;
	size_t len = strlen(name);
	/*@dependent@*/ /*@null@*/ xdf_symrec_data *xsymd;
	unsigned long value = 0;
	long scnum = -3;	/* -3 = debugging symbol */
	/*@dependent@*/ /*@null@*/ yasm_section *sect;
	/*@dependent@*/ /*@null@*/ yasm_bytecode *precbc;
	yasm_sym_vis vis = yasm_symrec_get_visibility(entry->sym);
	unsigned long flags = 0;

	/* Get symrec's of_data (needed for storage class) */
	xsymd = yasm_symrec_get_data(entry->sym, &xdf_symrec_data_cb);
	if (!xsymd)
	    yasm_internal_error(N_("xdf: expected sym data to be present"));

	if (vis & YASM_SYM_GLOBAL)
	    flags = XDF_SYM_GLOBAL;

	/* Look at symrec for value/scnum/etc. */
	if (yasm_symrec_get_label(entry->sym, &precbc)) {
	    if (precbc)
		sect = yasm_bc_get_section(precbc);
	    else
		sect = NULL;
	    /* it's a label: get value and offset.
	     * If there is not a section, leave as debugging symbol.
	     */
	    if (sect) {
		/*@dependent@*/ /*@null@*/ xdf_section_data *csectd;
		csectd = yasm_section_get_data(sect, &xdf_section_data_cb);
		if (csectd) {
		    scnum = csectd->scnum;
		} else if (yasm_section_is_absolute(sect)) {
		    yasm_expr *abs_start;

		    abs_start = yasm_expr_copy(yasm_section_get_start(sect));
		    intn = yasm_expr_get_intnum(&abs_start,
						yasm_common_calc_bc_dist);
		    if (!intn)
			yasm__error(abs_start->line,
			    N_("absolute section start not an integer expression"));
		    else
			value = yasm_intnum_get_uint(intn);
		    yasm_expr_destroy(abs_start);

		    flags |= XDF_SYM_EQU;
		    scnum = -2;	/* -2 = absolute symbol */
		} else
		    yasm_internal_error(N_("didn't understand section"));
		if (precbc)
		    value += precbc->offset + precbc->len;
	    }
	} else if ((equ_val = yasm_symrec_get_equ(entry->sym))) {
	    yasm_expr *equ_val_copy = yasm_expr_copy(equ_val);
	    intn = yasm_expr_get_intnum(&equ_val_copy,
					yasm_common_calc_bc_dist);
	    if (!intn) {
		if (vis & YASM_SYM_GLOBAL)
		    yasm__error(equ_val->line,
			N_("global EQU value not an integer expression"));
	    } else
		value = yasm_intnum_get_uint(intn);
	    yasm_expr_destroy(equ_val_copy);

	    flags |= XDF_SYM_EQU;
	    scnum = -2;     /* -2 = absolute symbol */
	} else {
	    if (vis & YASM_SYM_EXTERN) {
		flags = XDF_SYM_EXTERN;
		scnum = -1;
	    }
	}

	localbuf = info.buf;
	YASM_WRITE_32_L(localbuf, scnum);	/* section number */
	YASM_WRITE_32_L(localbuf, value);	/* value */
	YASM_WRITE_32_L(localbuf, strtab_offset); /* string table offset */
	strtab_offset += len+1;
	YASM_WRITE_32_L(localbuf, flags);	/* flags */
	fwrite(info.buf, 16, 1, f);
    }

    /* String table */
    STAILQ_FOREACH(entry, &objfmt_xdf->xdf_symtab, link) {
	const char *name = yasm_symrec_get_name(entry->sym);
	size_t len = strlen(name);
	fwrite(name, len+1, 1, f);
    }

    /* Section data/relocs */
    if (yasm_object_sections_traverse(objfmt_xdf->object, &info,
				      xdf_objfmt_output_section))
	return;

    /* Write headers */
    if (fseek(f, 0, SEEK_SET) < 0) {
	yasm__fatal(N_("could not seek on output file"));
	/*@notreached@*/
	return;
    }

    localbuf = info.buf;
    YASM_WRITE_32_L(localbuf, XDF_MAGIC);	/* magic number */
    YASM_WRITE_32_L(localbuf, objfmt_xdf->parse_scnum); /* number of sects */
    YASM_WRITE_32_L(localbuf, symtab_count);		/* number of symtabs */
    /* size of sect headers + symbol table + strings */
    YASM_WRITE_32_L(localbuf, strtab_offset-16);
    fwrite(info.buf, 16, 1, f);

    yasm_object_sections_traverse(objfmt_xdf->object, &info,
				  xdf_objfmt_output_secthead);

    yasm_xfree(info.buf);
}

static void
xdf_objfmt_destroy(yasm_objfmt *objfmt)
{
    yasm_objfmt_xdf *objfmt_xdf = (yasm_objfmt_xdf *)objfmt;
    xdf_symtab_entry *entry1, *entry2;

    /* Delete local symbol table */
    entry1 = STAILQ_FIRST(&objfmt_xdf->xdf_symtab);
    while (entry1 != NULL) {
	entry2 = STAILQ_NEXT(entry1, link);
	yasm_xfree(entry1);
	entry1 = entry2;
    }
    yasm_xfree(objfmt);
}

static /*@observer@*/ /*@null@*/ yasm_section *
xdf_objfmt_section_switch(yasm_objfmt *objfmt, yasm_valparamhead *valparams,
			    /*@unused@*/ /*@null@*/
			    yasm_valparamhead *objext_valparams,
			    unsigned long line)
{
    yasm_objfmt_xdf *objfmt_xdf = (yasm_objfmt_xdf *)objfmt;
    yasm_valparam *vp = yasm_vps_first(valparams);
    yasm_section *retval;
    int isnew;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *absaddr = NULL;
    unsigned int addralign = 0;
    unsigned long flags = 0;
    int flags_override = 0;
    char *sectname;
    int resonly = 0;

    if (!vp || vp->param || !vp->val)
	return NULL;

    sectname = vp->val;

    while ((vp = yasm_vps_next(vp))) {
	flags_override = 1;
	if (yasm__strcasecmp(vp->val, "use16") == 0) {
	    flags &= ~(XDF_SECT_USE_32|XDF_SECT_USE_64);
	    flags |= XDF_SECT_USE_16;
	    yasm_arch_set_var(objfmt_xdf->arch, "mode_bits", 16);
	} else if (yasm__strcasecmp(vp->val, "use32") == 0) {
	    flags &= ~(XDF_SECT_USE_16|XDF_SECT_USE_64);
	    flags |= XDF_SECT_USE_32;
	    yasm_arch_set_var(objfmt_xdf->arch, "mode_bits", 32);
	} else if (yasm__strcasecmp(vp->val, "use64") == 0) {
	    flags &= ~(XDF_SECT_USE_16|XDF_SECT_USE_32);
	    flags |= XDF_SECT_USE_64;
	    yasm_arch_set_var(objfmt_xdf->arch, "mode_bits", 64);
	} else if (yasm__strcasecmp(vp->val, "bss") == 0) {
	    flags |= XDF_SECT_BSS;
	} else if (yasm__strcasecmp(vp->val, "flat") == 0) {
	    flags |= XDF_SECT_FLAT;
	} else if (yasm__strcasecmp(vp->val, "absolute") == 0 && vp->param) {
	    flags |= XDF_SECT_ABSOLUTE;
	    absaddr = yasm_expr_get_intnum(&vp->param, NULL);
	    if (!absaddr) {
		yasm__error(line, N_("argument to `%s' is not an integer"),
			    vp->val);
		return NULL;
	    }
	} else if (yasm__strcasecmp(vp->val, "align") == 0 && vp->param) {
	    /*@dependent@*/ /*@null@*/ const yasm_intnum *align;
	    unsigned long bitcnt;

	    align = yasm_expr_get_intnum(&vp->param, NULL);
	    if (!align) {
		yasm__error(line, N_("argument to `%s' is not a power of two"),
			    vp->val);
		return NULL;
	    }
	    addralign = yasm_intnum_get_uint(align);

	    /* Check to see if alignment is a power of two.
	     * This can be checked by seeing if only one bit is set.
	     */
	    BitCount(bitcnt, addralign);
	    if (bitcnt > 1) {
		yasm__error(line, N_("argument to `%s' is not a power of two"),
			    vp->val);
		return NULL;
	    }

	    /* Check to see if alignment is supported size */
	    if (addralign > 4096) {
		yasm__error(line,
			    N_("XDF does not support alignments > 4096"));
		return NULL;
	    }
	} else
	    yasm__warning(YASM_WARN_GENERAL, line,
			  N_("Unrecognized qualifier `%s'"), vp->val);
    }

    retval = yasm_object_get_general(objfmt_xdf->object, sectname, 0, resonly,
				     &isnew, line);

    if (isnew) {
	xdf_section_data *data;
	yasm_symrec *sym;

	data = yasm_xmalloc(sizeof(xdf_section_data));
	data->scnum = objfmt_xdf->parse_scnum++;
	data->align = addralign;
	data->flags = flags;
	if (absaddr)
	    data->addr = yasm_intnum_copy(absaddr);
	else
	    data->addr = NULL;
	data->scnptr = 0;
	data->size = 0;
	data->relptr = 0;
	data->nreloc = 0;
	STAILQ_INIT(&data->relocs);
	yasm_section_add_data(retval, &xdf_section_data_cb, data);

	sym =
	    yasm_symtab_define_label(objfmt_xdf->symtab, sectname,
				     yasm_section_bcs_first(retval), 1, line);
	xdf_objfmt_symtab_append(objfmt_xdf, sym);
	data->sym = sym;
    } else if (flags_override)
	yasm__warning(YASM_WARN_GENERAL, line,
		      N_("section flags ignored on section redeclaration"));
    return retval;
}

static void
xdf_section_data_destroy(void *data)
{
    xdf_section_data *xsd = (xdf_section_data *)data;
    if (xsd->addr)
	yasm_intnum_destroy(xsd->addr);
    yasm_xfree(data);
}

static void
xdf_section_data_print(void *data, FILE *f, int indent_level)
{
    xdf_section_data *xsd = (xdf_section_data *)data;

    fprintf(f, "%*ssym=\n", indent_level, "");
    yasm_symrec_print(xsd->sym, f, indent_level+1);
    fprintf(f, "%*sscnum=%ld\n", indent_level, "", xsd->scnum);
    fprintf(f, "%*sflags=0x%x\n", indent_level, "", xsd->flags);
    fprintf(f, "%*saddr=", indent_level, "");
    yasm_intnum_print(xsd->addr, f);
    fprintf(f, "%*sscnptr=0x%lx\n", indent_level, "", xsd->scnptr);
    fprintf(f, "%*ssize=%ld\n", indent_level, "", xsd->size);
    fprintf(f, "%*srelptr=0x%lx\n", indent_level, "", xsd->relptr);
    fprintf(f, "%*snreloc=%ld\n", indent_level, "", xsd->nreloc);
    fprintf(f, "%*srelocs:\n", indent_level, "");
}

static yasm_symrec *
xdf_objfmt_extern_declare(yasm_objfmt *objfmt, const char *name, /*@unused@*/
			   /*@null@*/ yasm_valparamhead *objext_valparams,
			   unsigned long line)
{
    yasm_objfmt_xdf *objfmt_xdf = (yasm_objfmt_xdf *)objfmt;
    yasm_symrec *sym;

    sym = yasm_symtab_declare(objfmt_xdf->symtab, name, YASM_SYM_EXTERN,
			      line);
    xdf_objfmt_symtab_append(objfmt_xdf, sym);

    return sym;
}

static yasm_symrec *
xdf_objfmt_global_declare(yasm_objfmt *objfmt, const char *name, /*@unused@*/
			   /*@null@*/ yasm_valparamhead *objext_valparams,
			   unsigned long line)
{
    yasm_objfmt_xdf *objfmt_xdf = (yasm_objfmt_xdf *)objfmt;
    yasm_symrec *sym;

    sym = yasm_symtab_declare(objfmt_xdf->symtab, name, YASM_SYM_GLOBAL,
			      line);
    xdf_objfmt_symtab_append(objfmt_xdf, sym);

    return sym;
}

static yasm_symrec *
xdf_objfmt_common_declare(yasm_objfmt *objfmt, const char *name,
			   /*@only@*/ yasm_expr *size, /*@unused@*/ /*@null@*/
			   yasm_valparamhead *objext_valparams,
			   unsigned long line)
{
    yasm_objfmt_xdf *objfmt_xdf = (yasm_objfmt_xdf *)objfmt;
    yasm_symrec *sym;

    yasm_expr_destroy(size);
    yasm__error(line,
	N_("XDF object format does not support common variables"));

    sym = yasm_symtab_declare(objfmt_xdf->symtab, name, YASM_SYM_COMMON, line);
    return sym;
}

static void
xdf_symrec_data_destroy(void *data)
{
    yasm_xfree(data);
}

static void
xdf_symrec_data_print(void *data, FILE *f, int indent_level)
{
    xdf_symrec_data *xsd = (xdf_symrec_data *)data;

    fprintf(f, "%*ssymtab index=%lu\n", indent_level, "", xsd->index);
}

static int
xdf_objfmt_directive(/*@unused@*/ yasm_objfmt *objfmt,
		      /*@unused@*/ const char *name,
		      /*@unused@*/ yasm_valparamhead *valparams,
		      /*@unused@*/ /*@null@*/
		      yasm_valparamhead *objext_valparams,
		      /*@unused@*/ unsigned long line)
{
    return 1;	/* no objfmt directives */
}


/* Define valid debug formats to use with this object format */
static const char *xdf_objfmt_dbgfmt_keywords[] = {
    "null",
    NULL
};

/* Define objfmt structure -- see objfmt.h for details */
yasm_objfmt_module yasm_xdf_LTX_objfmt = {
    "Extended Dynamic Object",
    "xdf",
    "xdf",
    ".text",
    32,
    xdf_objfmt_dbgfmt_keywords,
    "null",
    xdf_objfmt_create,
    xdf_objfmt_output,
    xdf_objfmt_destroy,
    xdf_objfmt_section_switch,
    xdf_objfmt_extern_declare,
    xdf_objfmt_global_declare,
    xdf_objfmt_common_declare,
    xdf_objfmt_directive
};
