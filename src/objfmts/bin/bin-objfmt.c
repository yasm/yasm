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

#include "arch.h"
#include "objfmt.h"


#define REGULAR_OUTBUF_SIZE	1024


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

    if (start & (align-1))
	start = (start & ~(align-1)) + align;

    *padamt = start - (base + *prevsectlen);

    return start;
}

typedef struct bin_objfmt_output_info {
    /*@dependent@*/ FILE *f;
    /*@only@*/ unsigned char *buf;
    /*@observer@*/ const section *sect;
    unsigned long start;
} bin_objfmt_output_info;

static /*@only@*/ expr *
bin_objfmt_expr_xform(/*@returned@*/ /*@only@*/ expr *e,
		      /*@unused@*/ /*@null@*/ void *d)
{
    int i;
    /*@dependent@*/ section *sect;
    /*@dependent@*/ /*@null@*/ bytecode *precbc;
    /*@null@*/ intnum *dist;

    for (i=0; i<e->numterms; i++) {
	/* Transform symrecs that reference sections into
	 * start expr + intnum(dist).
	 */
	if (e->terms[i].type == EXPR_SYM &&
	    symrec_get_label(e->terms[i].data.sym, &sect, &precbc) &&
	    (dist = common_calc_bc_dist(sect, NULL, precbc))) {
	    e->terms[i].type = EXPR_EXPR;
	    e->terms[i].data.expn =
		expr_new(EXPR_ADD,
			 ExprExpr(expr_copy(section_get_start(sect))),
			 ExprInt(dist));
	}
    }

    return e;
}

static int
bin_objfmt_output_expr(expr **ep, unsigned char **bufp, unsigned long valsize,
		       /*@unused@*/ unsigned long offset,
		       /*@observer@*/ const section *sect,
		       /*@observer@*/ const bytecode *bc, int rel,
		       /*@unused@*/ /*@null@*/ void *d)
{
    /*@dependent@*/ /*@null@*/ const intnum *intn;
    /*@dependent@*/ /*@null@*/ const floatnum *flt;

    assert(info != NULL);

    /* For binary output, this is trivial: any expression that doesn't simplify
     * to an integer is an error (references something external).
     * Other object formats need to generate their relocation list from here!
     */

    *ep = expr_xform_neg_tree(*ep);
    *ep = expr_level_tree(*ep, 1, 1, NULL, bin_objfmt_expr_xform, NULL, NULL);

    /* Handle floating point expressions */
    flt = expr_get_floatnum(ep);
    if (flt)
	return cur_arch->floatnum_tobytes(flt, bufp, valsize, *ep);

    /* Handle integer expressions */
    intn = expr_get_intnum(ep, NULL);
    if (intn)
	return cur_arch->intnum_tobytes(intn, bufp, valsize, *ep, bc, rel);

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
    startnum = expr_get_intnum(&startexpr, NULL);
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
    int resonly = 0;
    unsigned long alignval = 0;
    int have_alignval = 0;

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
		unsigned long bitcnt;

		if (strcmp(sectname, ".text") == 0) {
		    Error(_("cannot specify an alignment to the `%s' section"),
			  sectname);
		    return NULL;
		}
		
		align = expr_get_intnum(&vp->param, NULL);
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

		have_alignval = 1;
	    }
	}

	retval = sections_switch_general(headp, sectname, start, resonly,
					 &isnew);

	if (isnew) {
	    if (have_alignval) {
		unsigned long *data = xmalloc(sizeof(unsigned long));
		*data = alignval;
		section_set_of_data(retval, data);
	    }

	    symrec_define_label(sectname, retval, (bytecode *)NULL, 1);
	} else if (have_alignval)
	    Warning(_("alignment value ignored on section redeclaration"));

	return retval;
    } else
	return NULL;
}

static void
bin_objfmt_section_data_delete(/*@only@*/ void *d)
{
    xfree(d);
}

static void
bin_objfmt_common_declare(/*@unused@*/ symrec *sym, /*@only@*/ expr *size,
			  /*@unused@*/ /*@null@*/
			  valparamhead *objext_valparams)
{
    expr_delete(size);
    Error(_("binary object format does not support common variables"));
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
	    start = expr_get_intnum(&vp->param, NULL);

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

static void
bin_objfmt_section_data_print(FILE *f, void *data)
{
    fprintf(f, "%*salign=%ld\n", indent_level, "", *((unsigned long *)data));
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
    bin_objfmt_section_data_delete,
    bin_objfmt_section_data_print,
    NULL /*bin_objfmt_extern_declare*/,
    NULL /*bin_objfmt_global_declare*/,
    bin_objfmt_common_declare,
    NULL /*bin_objfmt_symrec_data_delete*/,
    NULL /*bin_objfmt_symrec_data_print*/,
    bin_objfmt_directive,
    NULL /*bin_objfmt_bc_objfmt_data_delete*/,
    NULL /*bin_objfmt_bc_objfmt_data_print*/
};
