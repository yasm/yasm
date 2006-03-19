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

#define XDF_MAGIC	0x87654322

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
    yasm_intnum *vaddr;	    /* starting virtual address */
    long scnum;		    /* section number (0=first section) */
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

typedef struct yasm_objfmt_xdf {
    yasm_objfmt_base objfmt;		    /* base structure */

    long parse_scnum;		    /* sect numbering in parser */

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

    unsigned long indx;		    /* current symbol index */
    int all_syms;		    /* outputting all symbols? */
    unsigned long strtab_offset;    /* current string table offset */
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


static yasm_objfmt *
xdf_objfmt_create(yasm_object *object, yasm_arch *a)
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

    objfmt_xdf->objfmt.module = &yasm_xdf_LTX_objfmt;

    return (yasm_objfmt *)objfmt_xdf;
}

static int
xdf_objfmt_output_value(yasm_value *value, unsigned char *buf, size_t destsize,
			size_t valsize, int shift, unsigned long offset,
			yasm_bytecode *bc, int warn, /*@null@*/ void *d)
{
    /*@null@*/ xdf_objfmt_output_info *info = (xdf_objfmt_output_info *)d;
    yasm_objfmt_xdf *objfmt_xdf;
    /*@dependent@*/ /*@null@*/ yasm_intnum *intn;
    unsigned long intn_minus;
    int retval;

    assert(info != NULL);
    objfmt_xdf = info->objfmt_xdf;

    if (value->abs)
	value->abs = yasm_expr_simplify(value->abs, yasm_common_calc_bc_dist);

    /* Try to output constant and PC-relative section-local first.
     * Note this does NOT output any value with a SEG, WRT, external,
     * cross-section, or non-PC-relative reference (those are handled below).
     */
    switch (yasm_value_output_basic(value, buf, destsize, valsize, shift, bc,
				    warn, info->objfmt_xdf->arch,
				    yasm_common_calc_bc_dist)) {
	case -1:
	    return 1;
	case 0:
	    break;
	default:
	    return 0;
    }

    intn_minus = 0;
    if (value->rel) {
	xdf_reloc *reloc;

	reloc = yasm_xmalloc(sizeof(xdf_reloc));
	reloc->reloc.addr = yasm_intnum_create_uint(bc->offset + offset);
	reloc->reloc.sym = value->rel;
	reloc->base = NULL;
	reloc->size = valsize/8;
	reloc->shift = value->rshift;

	if (value->seg_of)
	    reloc->type = XDF_RELOC_SEG;
	else if (value->wrt) {
	    reloc->base = value->wrt;
	    reloc->type = XDF_RELOC_WRT;
	} else if (value->curpos_rel) {
	    reloc->type = XDF_RELOC_RIP;
	    /* Adjust to start of section, so subtract out the bytecode
	     * offset.
	     */
	    intn_minus = bc->offset;
	} else
	    reloc->type = XDF_RELOC_REL;
	info->xsd->nreloc++;
	yasm_section_add_reloc(info->sect, (yasm_reloc *)reloc, yasm_xfree);
    }

    if (intn_minus > 0) {
	intn = yasm_intnum_create_uint(intn_minus);
	yasm_intnum_calc(intn, YASM_EXPR_NEG, NULL, bc->line);
    } else
	intn = yasm_intnum_create_uint(0);

    if (value->abs) {
	yasm_intnum *intn2 = yasm_expr_get_intnum(&value->abs, NULL);
	if (!intn2) {
	    yasm__error(bc->line, N_("xdf: relocation too complex"));
	    yasm_intnum_destroy(intn);
	    return 1;
	}
	yasm_intnum_calc(intn, YASM_EXPR_ADD, intn2, bc->line);
    }

    retval = yasm_arch_intnum_tobytes(objfmt_xdf->arch, intn, buf, destsize,
				      valsize, shift, bc, warn, bc->line);
    yasm_intnum_destroy(intn);
    return retval;
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
			     xdf_objfmt_output_value, NULL);

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
	yasm__warning(YASM_WARN_UNINIT_CONTENTS, bc->line,
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
    if (xsd->vaddr) {
	yasm_intnum_get_sized(xsd->vaddr, localbuf, 8, 64, 0, 0, 0, 0);
	localbuf += 8;				/* virtual address */
    } else if (xsd->addr) {
	yasm_intnum_get_sized(xsd->addr, localbuf, 8, 64, 0, 0, 0, 0);
	localbuf += 8;				/* VA=PA */
    } else {
	YASM_WRITE_32_L(localbuf, 0);
	YASM_WRITE_32_L(localbuf, 0);
    }
    YASM_WRITE_16_L(localbuf, yasm_section_get_align(sect)); /* alignment */
    YASM_WRITE_16_L(localbuf, xsd->flags);	/* flags */
    YASM_WRITE_32_L(localbuf, xsd->scnptr);	/* file ptr to data */
    YASM_WRITE_32_L(localbuf, xsd->size);	/* section size */
    YASM_WRITE_32_L(localbuf, xsd->relptr);	/* file ptr to relocs */
    YASM_WRITE_32_L(localbuf, xsd->nreloc); /* num of relocation entries */
    fwrite(info->buf, 40, 1, info->f);

    return 0;
}

static int
xdf_objfmt_count_sym(yasm_symrec *sym, /*@null@*/ void *d)
{
    /*@null@*/ xdf_objfmt_output_info *info = (xdf_objfmt_output_info *)d;
    assert(info != NULL);
    if (info->all_syms || yasm_symrec_get_visibility(sym) != YASM_SYM_LOCAL) {
	/* Save index in symrec data */
	xdf_symrec_data *sym_data = yasm_xmalloc(sizeof(xdf_symrec_data));
	sym_data->index = info->indx;
	yasm_symrec_add_data(sym, &xdf_symrec_data_cb, sym_data);

	info->indx++;
    }
    return 0;
}

static int
xdf_objfmt_output_sym(yasm_symrec *sym, /*@null@*/ void *d)
{
    /*@null@*/ xdf_objfmt_output_info *info = (xdf_objfmt_output_info *)d;
    yasm_sym_vis vis = yasm_symrec_get_visibility(sym);

    assert(info != NULL);

    if (info->all_syms || vis != YASM_SYM_LOCAL) {
	const char *name = yasm_symrec_get_name(sym);
	const yasm_expr *equ_val;
	const yasm_intnum *intn;
	size_t len = strlen(name);
	unsigned long value = 0;
	long scnum = -3;	/* -3 = debugging symbol */
	/*@dependent@*/ /*@null@*/ yasm_section *sect;
	/*@dependent@*/ /*@null@*/ yasm_bytecode *precbc;
	unsigned long flags = 0;
	unsigned char *localbuf;

	if (vis & YASM_SYM_GLOBAL)
	    flags = XDF_SYM_GLOBAL;

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
	} else if ((equ_val = yasm_symrec_get_equ(sym))) {
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

	localbuf = info->buf;
	YASM_WRITE_32_L(localbuf, scnum);	/* section number */
	YASM_WRITE_32_L(localbuf, value);	/* value */
	YASM_WRITE_32_L(localbuf, info->strtab_offset);
	info->strtab_offset += len+1;
	YASM_WRITE_32_L(localbuf, flags);	/* flags */
	fwrite(info->buf, 16, 1, info->f);
    }
    return 0;
}

static int
xdf_objfmt_output_str(yasm_symrec *sym, /*@null@*/ void *d)
{
    /*@null@*/ xdf_objfmt_output_info *info = (xdf_objfmt_output_info *)d;
    yasm_sym_vis vis = yasm_symrec_get_visibility(sym);

    assert(info != NULL);

    if (info->all_syms || vis != YASM_SYM_LOCAL) {
	const char *name = yasm_symrec_get_name(sym);
	size_t len = strlen(name);
	fwrite(name, len+1, 1, info->f);
    }
    return 0;
}

static void
xdf_objfmt_output(yasm_objfmt *objfmt, FILE *f, int all_syms,
		  /*@unused@*/ yasm_dbgfmt *df)
{
    yasm_objfmt_xdf *objfmt_xdf = (yasm_objfmt_xdf *)objfmt;
    xdf_objfmt_output_info info;
    unsigned char *localbuf;
    unsigned long symtab_count = 0;

    info.objfmt_xdf = objfmt_xdf;
    info.f = f;
    info.buf = yasm_xmalloc(REGULAR_OUTBUF_SIZE);

    /* Allocate space for headers by seeking forward */
    if (fseek(f, (long)(16+40*(objfmt_xdf->parse_scnum)), SEEK_SET) < 0) {
	yasm__fatal(N_("could not seek on output file"));
	/*@notreached@*/
	return;
    }

    /* Get number of symbols */
    info.indx = 0;
    info.all_syms = 1;	/* force all syms into symbol table */
    yasm_symtab_traverse(objfmt_xdf->symtab, &info, xdf_objfmt_count_sym);
    symtab_count = info.indx;

    /* Get file offset of start of string table */
    info.strtab_offset = 16+40*(objfmt_xdf->parse_scnum)+16*symtab_count;

    /* Output symbol table */
    yasm_symtab_traverse(objfmt_xdf->symtab, &info, xdf_objfmt_output_sym);

    /* Output string table */
    yasm_symtab_traverse(objfmt_xdf->symtab, &info, xdf_objfmt_output_str);

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
    YASM_WRITE_32_L(localbuf, info.strtab_offset-16);
    fwrite(info.buf, 16, 1, f);

    yasm_object_sections_traverse(objfmt_xdf->object, &info,
				  xdf_objfmt_output_secthead);

    yasm_xfree(info.buf);
}

static void
xdf_objfmt_destroy(yasm_objfmt *objfmt)
{
    yasm_xfree(objfmt);
}

static xdf_section_data *
xdf_objfmt_init_new_section(yasm_objfmt_xdf *objfmt_xdf, yasm_section *sect,
			    const char *sectname, unsigned long line)
{
    xdf_section_data *data;
    yasm_symrec *sym;

    data = yasm_xmalloc(sizeof(xdf_section_data));
    data->scnum = objfmt_xdf->parse_scnum++;
    data->flags = 0;
    data->addr = NULL;
    data->vaddr = NULL;
    data->scnptr = 0;
    data->size = 0;
    data->relptr = 0;
    data->nreloc = 0;
    STAILQ_INIT(&data->relocs);
    yasm_section_add_data(sect, &xdf_section_data_cb, data);

    sym = yasm_symtab_define_label(objfmt_xdf->symtab, sectname,
				   yasm_section_bcs_first(sect), 1, line);
    data->sym = sym;
    return data;
}

static yasm_section *
xdf_objfmt_add_default_section(yasm_objfmt *objfmt)
{
    yasm_objfmt_xdf *objfmt_xdf = (yasm_objfmt_xdf *)objfmt;
    yasm_section *retval;
    xdf_section_data *xsd;
    int isnew;

    retval = yasm_object_get_general(objfmt_xdf->object, ".text", 0, 0, 1, 0,
				     &isnew, 0);
    if (isnew) {
	xsd = xdf_objfmt_init_new_section(objfmt_xdf, retval, ".text", 0);
	yasm_section_set_default(retval, 1);
    }
    return retval;
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
    /*@dependent@*/ /*@null@*/ const yasm_intnum *vaddr = NULL;
    unsigned long align = 0;
    unsigned long flags = 0;
    int flags_override = 0;
    char *sectname;
    int resonly = 0;
    xdf_section_data *xsd;

    if (!vp || vp->param || !vp->val)
	return NULL;

    sectname = vp->val;

    while ((vp = yasm_vps_next(vp))) {
	if (!vp->val) {
	    yasm__warning(YASM_WARN_GENERAL, line,
			  N_("Unrecognized numeric qualifier"));
	    continue;
	}

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
	} else if (yasm__strcasecmp(vp->val, "virtual") == 0 && vp->param) {
	    vaddr = yasm_expr_get_intnum(&vp->param, NULL);
	    if (!vaddr) {
		yasm__error(line, N_("argument to `%s' is not an integer"),
			    vp->val);
		return NULL;
	    }
	} else if (yasm__strcasecmp(vp->val, "align") == 0 && vp->param) {
	    /*@dependent@*/ /*@null@*/ const yasm_intnum *align_expr;

	    align_expr = yasm_expr_get_intnum(&vp->param, NULL);
	    if (!align_expr) {
		yasm__error(line, N_("argument to `%s' is not a power of two"),
			    vp->val);
		return NULL;
	    }
	    align = yasm_intnum_get_uint(align_expr);

            /* Alignments must be a power of two. */
            if ((align & (align - 1)) != 0) {
		yasm__error(line, N_("argument to `%s' is not a power of two"),
			    vp->val);
		return NULL;
	    }

	    /* Check to see if alignment is supported size */
	    if (align > 4096) {
		yasm__error(line,
			    N_("XDF does not support alignments > 4096"));
		return NULL;
	    }
	} else
	    yasm__warning(YASM_WARN_GENERAL, line,
			  N_("Unrecognized qualifier `%s'"), vp->val);
    }

    retval = yasm_object_get_general(objfmt_xdf->object, sectname, 0, align, 1,
				     resonly, &isnew, line);

    if (isnew)
	xsd = xdf_objfmt_init_new_section(objfmt_xdf, retval, sectname, line);
    else
	xsd = yasm_section_get_data(retval, &xdf_section_data_cb);

    if (isnew || yasm_section_is_default(retval)) {
	yasm_section_set_default(retval, 0);
	xsd->flags = flags;
	if (absaddr) {
	    if (xsd->addr)
		yasm_intnum_destroy(xsd->addr);
	    xsd->addr = yasm_intnum_copy(absaddr);
	}
	if (vaddr) {
	    if (xsd->vaddr)
		yasm_intnum_destroy(xsd->vaddr);
	    xsd->vaddr = yasm_intnum_copy(vaddr);
	}
	yasm_section_set_align(retval, align, line);
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
    if (xsd->vaddr)
	yasm_intnum_destroy(xsd->vaddr);
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
    fprintf(f, "%*svaddr=", indent_level, "");
    yasm_intnum_print(xsd->vaddr, f);
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

    return yasm_symtab_declare(objfmt_xdf->symtab, name, YASM_SYM_EXTERN,
			       line);
}

static yasm_symrec *
xdf_objfmt_global_declare(yasm_objfmt *objfmt, const char *name, /*@unused@*/
			   /*@null@*/ yasm_valparamhead *objext_valparams,
			   unsigned long line)
{
    yasm_objfmt_xdf *objfmt_xdf = (yasm_objfmt_xdf *)objfmt;

    return yasm_symtab_declare(objfmt_xdf->symtab, name, YASM_SYM_GLOBAL,
			       line);
}

static yasm_symrec *
xdf_objfmt_common_declare(yasm_objfmt *objfmt, const char *name,
			   /*@only@*/ yasm_expr *size, /*@unused@*/ /*@null@*/
			   yasm_valparamhead *objext_valparams,
			   unsigned long line)
{
    yasm_objfmt_xdf *objfmt_xdf = (yasm_objfmt_xdf *)objfmt;

    yasm_expr_destroy(size);
    yasm__error(line,
	N_("XDF object format does not support common variables"));

    return yasm_symtab_declare(objfmt_xdf->symtab, name, YASM_SYM_COMMON,
			       line);
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
    32,
    xdf_objfmt_dbgfmt_keywords,
    "null",
    xdf_objfmt_create,
    xdf_objfmt_output,
    xdf_objfmt_destroy,
    xdf_objfmt_add_default_section,
    xdf_objfmt_section_switch,
    xdf_objfmt_extern_declare,
    xdf_objfmt_global_declare,
    xdf_objfmt_common_declare,
    xdf_objfmt_directive
};
