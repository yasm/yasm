/*
 * Flat-format binary object format
 *
 *  Copyright (C) 2002  Peter Johnson
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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "file.h"

#include "globals.h"
#include "errwarn.h"
#include "intnum.h"
#include "floatnum.h"
#include "expr.h"
#include "symrec.h"

#include "bytecode.h"
#include "section.h"

#include "expr-int.h"
#include "bc-int.h"

#include "objfmt.h"


#define REGULAR_OUTBUF_SIZE	1024

static /*@null@*/ intnum *
    bin_objfmt_resolve_label(symrec *sym, section *sect,
			     /*@null@*/ bytecode *precbc,
			     /*@null@*/ bytecode *bc,
			     /*@unused@*/ unsigned long startval);

static void
bin_objfmt_initialize(/*@unused@*/ const char *in_filename,
		      /*@unused@*/ const char *obj_filename)
{
}

/* Aligns sect to either its specified alignment (in its objfmt-specific data)
 * or def_align if no alignment was specified.  Uses prevsect and base to both
 * determine the new starting address (returned) and the total length of
 * prevsect after sect has been aligned.
 */
static unsigned long
bin_objfmt_align_section(section *sect, section *prevsect, unsigned long base,
			 unsigned long def_align,
			 /*@out@*/ unsigned long *prevsectlen,
			 /*@out@*/ unsigned long *padamt)
{
    /*@dependent@*/ /*@null@*/ bytecode *last;
    unsigned long start;
    /*@dependent@*/ /*@null@*/ unsigned long *alignptr;
    unsigned long align;

    /* Figure out the size of .text by looking at the last bytecode's offset
     * plus its length.  Add the start and size together to get the new start.
     */
    last = bcs_last(section_get_bytecodes(prevsect));
    if (last)
	*prevsectlen = last->offset + last->len;
    else
	*prevsectlen = 0;
    start = base + *prevsectlen;

    /* Round new start up to alignment of .data section, and adjust textlen to
     * indicate padded size.  Because aignment is always a power of two, we
     * can use some bit trickery to do this easily.
     */
    alignptr = section_get_of_data(sect);
    if (alignptr)
	align = *alignptr;
    else
	align = def_align;	/* No alignment: use default */

    if (start & ~(align-1))
	start = (start & ~(align-1)) + align;

    *padamt = start - (base + *prevsectlen);

    return start;
}

static /*@null@*/ intnum *
bin_objfmt_resolve_label2(symrec *sym, section *sect,
			  /*@null@*/ bytecode *precbc, /*@null@*/ bytecode *bc,
			  /*@null@*/ const section *cursect,
			  unsigned long cursectstart, int withstart)
{
    /*@null@*/ expr *startexpr;
    /*@dependent@*/ /*@null@*/ const intnum *start;
    unsigned long startval = 0;

    /* Figure out the starting offset of the entire section */
    if (withstart || (cursect && sect != cursect) ||
	section_is_absolute(sect)) {
	startexpr = expr_copy(section_get_start(sect));
	assert(startexpr != NULL);
	expr_expand_labelequ(startexpr, sect, 0, bin_objfmt_resolve_label,
			     NULL);
	start = expr_get_intnum(&startexpr);
	if (!start) {
	    expr_delete(startexpr);
	    return NULL;
	}
	startval = intnum_get_uint(start);
	expr_delete(startexpr);

	/* Compensate for current section start */
	startval -= cursectstart;
    }

    if (precbc)
	return intnum_new_uint(startval + precbc->offset + precbc->len);
    else
	return intnum_new_uint(startval + bc->offset);
}

static intnum *
bin_objfmt_resolve_label(symrec *sym, section *sect,
			 /*@null@*/ bytecode *precbc, /*@null@*/ bytecode *bc,
			 /*@unused@*/ unsigned long startval)
{
    return bin_objfmt_resolve_label2(sym, sect, precbc, bc, NULL, 0, 1);
}

typedef struct bin_objfmt_expr_data {
    /*@observer@*/ const section *sect;
    unsigned long start;
    int withstart;
} bin_objfmt_expr_data;

static int
bin_objfmt_expr_traverse_callback(ExprItem *ei, void *d)
{
    bin_objfmt_expr_data *data = (bin_objfmt_expr_data *)d;
    const expr *equ_expr;

    if (ei->type == EXPR_SYM) {
	equ_expr = symrec_get_equ(ei->data.sym);
	if (equ_expr) {
	    ei->type = EXPR_EXPR;
	    ei->data.expn = expr_copy(equ_expr);
	    expr_traverse_leaves_in(ei->data.expn, data,
				    bin_objfmt_expr_traverse_callback);
	} else {
	    /*@dependent@*/ section *sect;
	    /*@dependent@*/ /*@null@*/ bytecode *precbc;
	    /*@null@*/ bytecode *bc;
	    intnum *intn;

	    if (symrec_get_label(ei->data.sym, &sect, &precbc)) {
		if (!precbc)
		    bc = bcs_first(section_get_bytecodes(sect));
		else
		    bc = bcs_next(precbc);

		intn = bin_objfmt_resolve_label2(ei->data.sym, sect, precbc,
						 bc, data->sect, data->start,
						 data->withstart);
		if (intn) {
		    ei->type = EXPR_INT;
		    ei->data.intn = intn;
		}
	    }
	}
    }
    return 0;
}

typedef struct bin_objfmt_output_info {
    /*@dependent@*/ FILE *f;
    /*@only@*/ unsigned char *buf;
    /*@observer@*/ const section *sect;
    unsigned long start;
} bin_objfmt_output_info;

static int
bin_objfmt_output_expr(expr **ep, unsigned char **bufp, unsigned long valsize,
		       /*@observer@*/ const section *sect,
		       /*@observer@*/ const bytecode *bc, int rel,
		       /*@null@*/ void *d)
{
    /*@null@*/ bin_objfmt_output_info *info = (bin_objfmt_output_info *)d;
    /*@observer@*/ bin_objfmt_expr_data data;
    /*@dependent@*/ /*@null@*/ const intnum *num;
    /*@dependent@*/ /*@null@*/ const floatnum *flt;

    assert(info != NULL);

    /* For binary output, this is trivial: any expression that doesn't simplify
     * to an integer is an error (references something external).
     * Other object formats need to generate their relocation list from here!
     * Note: we can't just use expr_expand_labelequ() because it doesn't
     *  resolve between different sections (on purpose).. but for bin we
     *  WANT that.
     */
    data.sect = sect;
    if (rel) {
	data.start = info->start;
	data.withstart = 0;
    } else {
	data.start = 0;
	data.withstart = 1;
    }
    expr_traverse_leaves_in(*ep, &data, bin_objfmt_expr_traverse_callback);

    /* Handle floating point expressions */
    flt = expr_get_floatnum(ep);
    if (flt) {
	int fltret;

	if (!floatnum_check_size(flt, (size_t)valsize)) {
	    ErrorAt((*ep)->line, _("invalid floating point constant size"));
	    return 1;
	}

	fltret = floatnum_get_sized(flt, *bufp, (size_t)valsize);
	if (fltret < 0) {
	    ErrorAt((*ep)->line, _("underflow in floating point expression"));
	    return 1;
	}
	if (fltret > 0) {
	    ErrorAt((*ep)->line, _("overflow in floating point expression"));
	    return 1;
	}
	*bufp += valsize;
	return 0;
    }

    /* Handle integer expressions */
    num = expr_get_intnum(ep);
    if (num) {
	if (rel) {
	    unsigned long val;
	    /* FIXME: Check against BITS setting on x86 */
	    if (valsize != 1 && valsize != 2 && valsize != 4)
		InternalError(_("tried to do PC-relative offset from invalid sized value"));
	    val = intnum_get_uint(num);
	    val = (unsigned long)((long)(val - (bc->offset + bc->len)));
	    switch (valsize) {
		case 1:
		    WRITE_BYTE(*bufp, val);
		    break;
		case 2:
		    WRITE_SHORT(*bufp, val);
		    break;
		case 4:
		    WRITE_LONG(*bufp, val);
		    break;
	    }
	} else {
	    /* Write value out. */
	    intnum_get_sized(num, *bufp, (size_t)valsize);
	    *bufp += valsize;
	}
	return 0;
    }

    /* Check for complex float expressions */
    if (expr_contains(*ep, EXPR_FLOAT)) {
	ErrorAt((*ep)->line, _("floating point expression too complex"));
	return 1;
    }

    /* Couldn't output, assume it contains an external reference. */
    ErrorAt((*ep)->line,
	    _("binary object format does not support external references"));
    return 1;
}

static int
bin_objfmt_output_bytecode(bytecode *bc, /*@null@*/ void *d)
{
    /*@null@*/ bin_objfmt_output_info *info = (bin_objfmt_output_info *)d;
    /*@null@*/ /*@only@*/ unsigned char *bigbuf;
    unsigned long size = REGULAR_OUTBUF_SIZE;
    unsigned long multiple;
    unsigned long i;
    int gap;

    assert(info != NULL);

    bigbuf = bc_tobytes(bc, info->buf, &size, &multiple, &gap, info->sect,
			info, bin_objfmt_output_expr, NULL);

    /* Don't bother doing anything else if size ended up being 0. */
    if (size == 0) {
	if (bigbuf)
	    xfree(bigbuf);
	return 0;
    }

    /* Warn that gaps are converted to 0 and write out the 0's. */
    if (gap) {
	unsigned long left;
	WarningAt(bc->line,
		  _("uninitialized space declared in code/data section: zeroing"));
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

static void
bin_objfmt_output(FILE *f, sectionhead *sections)
{
    /*@observer@*/ /*@null@*/ section *text, *data, *bss, *prevsect;
    /*@null@*/ expr *startexpr;
    /*@dependent@*/ /*@null@*/ const intnum *startnum;
    unsigned long start = 0, textstart = 0, datastart = 0;
    unsigned long textlen = 0, textpad = 0, datalen = 0, datapad = 0;
    unsigned long *prevsectlenptr, *prevsectpadptr;
    unsigned long i;
    bin_objfmt_output_info info;

    info.f = f;
    info.buf = xmalloc(REGULAR_OUTBUF_SIZE);

    text = sections_find_general(sections, ".text");
    data = sections_find_general(sections, ".data");
    bss = sections_find_general(sections, ".bss");

    if (!text)
	InternalError(_("No `.text' section in bin objfmt output"));

    /* First determine the actual starting offsets for .data and .bss.
     * As the order in the file is .text -> .data -> .bss (not present),
     * use the last bytecode in .text (and the .text section start) to
     * determine the starting offset in .data, and likewise for .bss.
     * Also compensate properly for alignment.
     */

    /* Find out the start of .text */
    startexpr = expr_copy(section_get_start(text));
    assert(startexpr != NULL);
    startnum = expr_get_intnum(&startexpr);
    if (!startnum)
	InternalError(_("Complex expr for start in bin objfmt output"));
    start = intnum_get_uint(startnum);
    expr_delete(startexpr);
    textstart = start;

    /* Align .data and .bss (if present) by adjusting their starts. */
    prevsect = text;
    prevsectlenptr = &textlen;
    prevsectpadptr = &textpad;
    if (data) {
	start = bin_objfmt_align_section(data, prevsect, start, 4,
					 prevsectlenptr, prevsectpadptr);
	section_set_start(data, start);
	datastart = start;
	prevsect = data;
	prevsectlenptr = &datalen;
	prevsectpadptr = &datapad;
    }
    if (bss) {
	start = bin_objfmt_align_section(bss, prevsect, start, 4,
					 prevsectlenptr, prevsectpadptr);
	section_set_start(bss, start);
    }

    /* Output .text first. */
    info.sect = text;
    info.start = textstart;
    bcs_traverse(section_get_bytecodes(text), &info,
		 bin_objfmt_output_bytecode);

    /* If .data is present, output it */
    if (data) {
	/* Add padding to align .data.  Just use a for loop, as this will
	 * seldom be very many bytes.
	 */
	for (i=0; i<textpad; i++)
	    fputc(0, f);

	/* Output .data bytecodes */
	info.sect = data;
	info.start = datastart;
	bcs_traverse(section_get_bytecodes(data), &info,
		     bin_objfmt_output_bytecode);
    }

    /* If .bss is present, check it for non-reserve bytecodes */


    xfree(info.buf);
}

static void
bin_objfmt_cleanup(void)
{
}

static /*@observer@*/ /*@null@*/ section *
bin_objfmt_sections_switch(sectionhead *headp, valparamhead *valparams,
			   /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    valparam *vp;
    section *retval;
    int isnew;
    unsigned long start;
    char *sectname;
    /*@null@*/ void *data = NULL;
    int resonly = 0;

    if ((vp = vps_first(valparams)) && !vp->param && vp->val != NULL) {
	/* If it's the first section output (.text) start at 0, otherwise
	 * make sure the start is > 128.
	 */
	sectname = vp->val;
	if (strcmp(sectname, ".text") == 0)
	    start = 0;
	else if (strcmp(sectname, ".data") == 0)
	    start = 200;
	else if (strcmp(sectname, ".bss") == 0) {
	    start = 200;
	    resonly = 1;
	} else {
	    /* other section names not recognized. */
	    Error(_("segment name `%s' not recognized"), sectname);
	    return NULL;
	}

	/* Check for ALIGN qualifier */
	while ((vp = vps_next(vp))) {
	    if (strcasecmp(vp->val, "align") == 0 && vp->param) {
		/*@dependent@*/ /*@null@*/ const intnum *align;
		unsigned long alignval;
		unsigned long bitcnt;

		if (strcmp(sectname, ".text") == 0) {
		    Error(_("cannot specify an alignment to the `%s' section"),
			  sectname);
		    return NULL;
		}
		
		align = expr_get_intnum(&vp->param);
		if (!align) {
		    Error(_("argument to `%s' is not a power of two"),
			  vp->val);
		    return NULL;
		}
		alignval = intnum_get_uint(align);

		/* Check to see if alignval is a power of two.
		 * This can be checked by seeing if only one bit is set.
		 */
		BitCount(bitcnt, alignval);
		if (bitcnt > 1) {
		    Error(_("argument to `%s' is not a power of two"),
			  vp->val);
		    return NULL;
		}

		/* Point data to (a copy of) alignval. */
		data = xmalloc(sizeof(unsigned long));
		*((unsigned long *)data) = alignval;
	    }
	}

	retval = sections_switch_general(headp, sectname, start, data, resonly,
					 &isnew);
	if (isnew)
	    symrec_define_label(sectname, retval, (bytecode *)NULL, 1);
	return retval;
    } else
	return NULL;
}

static /*@null@*/ void *
bin_objfmt_extern_data_new(/*@unused@*/ const char *name, /*@unused@*/
			   /*@null@*/ valparamhead *objext_valparams)
{
    return NULL;
}

static /*@null@*/ void *
bin_objfmt_global_data_new(/*@unused@*/ const char *name, /*@unused@*/
			   /*@null@*/ valparamhead *objext_valparams)
{
    return NULL;
}

static /*@null@*/ void *
bin_objfmt_common_data_new(/*@unused@*/ const char *name,
			   /*@only@*/ expr *size, /*@unused@*/ /*@null@*/
			   valparamhead *objext_valparams)
{
    expr_delete(size);
    Error(_("binary object format does not support common variables"));
    return NULL;
}

static int
bin_objfmt_directive(const char *name, valparamhead *valparams,
		     /*@unused@*/ /*@null@*/ valparamhead *objext_valparams,
		     sectionhead *headp)
{
    section *sect;
    valparam *vp;

    if (strcasecmp(name, "org") == 0) {
	/*@dependent@*/ /*@null@*/ const intnum *start = NULL;

	/* ORG takes just a simple integer as param */
	vp = vps_first(valparams);
	if (vp->val) {
	    Error(_("argument to ORG should be numeric"));
	    return 0;
	} else if (vp->param)
	    start = expr_get_intnum(&vp->param);

	if (!start) {
	    Error(_("argument to ORG should be numeric"));
	    return 0;
	}

	/* ORG changes the start of the .text section */
	sect = sections_find_general(headp, ".text");
	if (!sect)
	    InternalError(_("bin objfmt: .text section does not exist before ORG is called?"));
	section_set_start(sect, intnum_get_uint(start));

	return 0;	    /* directive recognized */
    } else
	return 1;	    /* directive unrecognized */
}

/* Define objfmt structure -- see objfmt.h for details */
objfmt yasm_bin_LTX_objfmt = {
    "Flat format binary",
    "bin",
    NULL,
    ".text",
    16,
    bin_objfmt_initialize,
    bin_objfmt_output,
    bin_objfmt_cleanup,
    bin_objfmt_sections_switch,
    NULL /*bin_objfmt_section_data_delete*/,
    NULL /*bin_objfmt_section_data_print*/,
    bin_objfmt_extern_data_new,
    bin_objfmt_global_data_new,
    bin_objfmt_common_data_new,
    NULL /*bin_objfmt_declare_data_copy*/,
    NULL /*bin_objfmt_declare_data_delete*/,
    NULL /*bin_objfmt_declare_data_print*/,
    bin_objfmt_directive,
    NULL /*bin_objfmt_bc_objfmt_data_delete*/,
    NULL /*bin_objfmt_bc_objfmt_data_print*/
};
