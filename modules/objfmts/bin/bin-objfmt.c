/*
 * Flat-format binary object format
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
/*@unused@*/ RCSID("$IdPath$");

#define YASM_LIB_INTERNAL
#define YASM_BC_INTERNAL
#define YASM_EXPR_INTERNAL
#include <libyasm.h>


#define REGULAR_OUTBUF_SIZE	1024

static void bin_section_data_destroy(/*@only@*/ void *d);
static void bin_section_data_print(void *data, FILE *f, int indent_level);

static const yasm_assoc_data_callback bin_section_data_callback = {
    bin_section_data_destroy,
    bin_section_data_print
};
static /*@dependent@*/ yasm_arch *cur_arch;


static int
bin_objfmt_initialize(/*@unused@*/ const char *in_filename,
		      /*@unused@*/ const char *obj_filename,
		      /*@unused@*/ yasm_object *object,
		      /*@unused@*/ yasm_dbgfmt *df, yasm_arch *a)
{
    cur_arch = a;
    return 0;
}

/* Aligns sect to either its specified alignment (in its objfmt-specific data)
 * or def_align if no alignment was specified.  Uses prevsect and base to both
 * determine the new starting address (returned) and the total length of
 * prevsect after sect has been aligned.
 */
static unsigned long
bin_objfmt_align_section(yasm_section *sect, yasm_section *prevsect,
			 unsigned long base, unsigned long def_align,
			 /*@out@*/ unsigned long *prevsectlen,
			 /*@out@*/ unsigned long *padamt)
{
    /*@dependent@*/ /*@null@*/ yasm_bytecode *last;
    unsigned long start;
    /*@dependent@*/ /*@null@*/ unsigned long *alignptr;
    unsigned long align;

    /* Figure out the size of .text by looking at the last bytecode's offset
     * plus its length.  Add the start and size together to get the new start.
     */
    last = yasm_section_bcs_last(prevsect);
    if (last)
	*prevsectlen = last->offset + last->len;
    else
	*prevsectlen = 0;
    start = base + *prevsectlen;

    /* Round new start up to alignment of .data section, and adjust textlen to
     * indicate padded size.  Because aignment is always a power of two, we
     * can use some bit trickery to do this easily.
     */
    alignptr = yasm_section_get_data(sect, &bin_section_data_callback);
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
    /*@observer@*/ const yasm_section *sect;
    unsigned long start;
} bin_objfmt_output_info;

static /*@only@*/ yasm_expr *
bin_objfmt_expr_xform(/*@returned@*/ /*@only@*/ yasm_expr *e,
		      /*@unused@*/ /*@null@*/ void *d)
{
    int i;
    /*@dependent@*/ yasm_section *sect;
    /*@dependent@*/ /*@null@*/ yasm_bytecode *precbc;
    /*@null@*/ yasm_intnum *dist;

    for (i=0; i<e->numterms; i++) {
	/* Transform symrecs that reference sections into
	 * start expr + intnum(dist).
	 */
	if (e->terms[i].type == YASM_EXPR_SYM &&
	    yasm_symrec_get_label(e->terms[i].data.sym, &precbc) &&
	    (sect = yasm_bc_get_section(precbc)) &&
	    (dist = yasm_common_calc_bc_dist(yasm_section_bcs_first(sect),
					     precbc))) {
	    const yasm_expr *start = yasm_section_get_start(sect);
	    e->terms[i].type = YASM_EXPR_EXPR;
	    e->terms[i].data.expn =
		yasm_expr_create(YASM_EXPR_ADD,
				 yasm_expr_expr(yasm_expr_copy(start)),
				 yasm_expr_int(dist), e->line);
	}
    }

    return e;
}

static int
bin_objfmt_output_expr(yasm_expr **ep, unsigned char *buf, size_t destsize,
		       size_t valsize, int shift,
		       /*@unused@*/ unsigned long offset, yasm_bytecode *bc,
		       int rel, int warn, /*@unused@*/ /*@null@*/ void *d)
{
    /*@dependent@*/ /*@null@*/ const yasm_intnum *intn;
    /*@dependent@*/ /*@null@*/ const yasm_floatnum *flt;

    /* For binary output, this is trivial: any expression that doesn't simplify
     * to an integer is an error (references something external).
     * Other object formats need to generate their relocation list from here!
     */

    *ep = yasm_expr__level_tree(*ep, 1, 1, NULL, bin_objfmt_expr_xform, NULL,
				NULL);

    /* Handle floating point expressions */
    flt = yasm_expr_get_floatnum(ep);
    if (flt) {
	if (shift < 0)
	    yasm_internal_error(N_("attempting to negative shift a float"));
	return cur_arch->module->floatnum_tobytes(cur_arch, flt, buf, destsize,
						  valsize, (unsigned int)shift,
						  warn, bc->line);
    }

    /* Handle integer expressions */
    intn = yasm_expr_get_intnum(ep, NULL);
    if (intn)
	return cur_arch->module->intnum_tobytes(cur_arch, intn, buf, destsize,
						valsize, shift, bc, rel, warn,
						bc->line);

    /* Check for complex float expressions */
    if (yasm_expr__contains(*ep, YASM_EXPR_FLOAT)) {
	yasm__error(bc->line, N_("floating point expression too complex"));
	return 1;
    }

    /* Couldn't output, assume it contains an external reference. */
    yasm__error(bc->line,
	N_("binary object format does not support external references"));
    return 1;
}

static int
bin_objfmt_output_bytecode(yasm_bytecode *bc, /*@null@*/ void *d)
{
    /*@null@*/ bin_objfmt_output_info *info = (bin_objfmt_output_info *)d;
    /*@null@*/ /*@only@*/ unsigned char *bigbuf;
    unsigned long size = REGULAR_OUTBUF_SIZE;
    unsigned long multiple;
    unsigned long i;
    int gap;

    assert(info != NULL);

    bigbuf = yasm_bc_tobytes(bc, info->buf, &size, &multiple, &gap, info,
			     bin_objfmt_output_expr, NULL);

    /* Don't bother doing anything else if size ended up being 0. */
    if (size == 0) {
	if (bigbuf)
	    yasm_xfree(bigbuf);
	return 0;
    }

    /* Warn that gaps are converted to 0 and write out the 0's. */
    if (gap) {
	unsigned long left;
	yasm__warning(YASM_WARN_GENERAL, bc->line,
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
	yasm_xfree(bigbuf);

    return 0;
}

static void
bin_objfmt_output(FILE *f, yasm_object *object, /*@unused@*/ int all_syms)
{
    /*@observer@*/ /*@null@*/ yasm_section *text, *data, *bss, *prevsect;
    /*@null@*/ yasm_expr *startexpr;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *startnum;
    unsigned long start = 0, textstart = 0, datastart = 0;
    unsigned long textlen = 0, textpad = 0, datalen = 0, datapad = 0;
    unsigned long *prevsectlenptr, *prevsectpadptr;
    unsigned long i;
    bin_objfmt_output_info info;

    info.f = f;
    info.buf = yasm_xmalloc(REGULAR_OUTBUF_SIZE);

    text = yasm_object_find_general(object, ".text");
    data = yasm_object_find_general(object, ".data");
    bss = yasm_object_find_general(object, ".bss");

    if (!text)
	yasm_internal_error(N_("No `.text' section in bin objfmt output"));

    /* First determine the actual starting offsets for .data and .bss.
     * As the order in the file is .text -> .data -> .bss (not present),
     * use the last bytecode in .text (and the .text section start) to
     * determine the starting offset in .data, and likewise for .bss.
     * Also compensate properly for alignment.
     */

    /* Find out the start of .text */
    startexpr = yasm_expr_copy(yasm_section_get_start(text));
    assert(startexpr != NULL);
    startnum = yasm_expr_get_intnum(&startexpr, NULL);
    if (!startnum) {
	yasm__error(startexpr->line, N_("ORG expression too complex"));
	return;
    }
    start = yasm_intnum_get_uint(startnum);
    yasm_expr_destroy(startexpr);
    textstart = start;

    /* Align .data and .bss (if present) by adjusting their starts. */
    prevsect = text;
    prevsectlenptr = &textlen;
    prevsectpadptr = &textpad;
    if (data) {
	start = bin_objfmt_align_section(data, prevsect, start, 4,
					 prevsectlenptr, prevsectpadptr);
	yasm_section_set_start(data, yasm_expr_create_ident(
	    yasm_expr_int(yasm_intnum_create_uint(start)), 0), 0);
	datastart = start;
	prevsect = data;
	prevsectlenptr = &datalen;
	prevsectpadptr = &datapad;
    }
    if (bss) {
	start = bin_objfmt_align_section(bss, prevsect, start, 4,
					 prevsectlenptr, prevsectpadptr);
	yasm_section_set_start(bss, yasm_expr_create_ident(
	    yasm_expr_int(yasm_intnum_create_uint(start)), 0), 0);
    }

    /* Output .text first. */
    info.sect = text;
    info.start = textstart;
    yasm_section_bcs_traverse(text, &info, bin_objfmt_output_bytecode);

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
	yasm_section_bcs_traverse(data, &info, bin_objfmt_output_bytecode);
    }

    /* If .bss is present, check it for non-reserve bytecodes */


    yasm_xfree(info.buf);
}

static void
bin_objfmt_cleanup(void)
{
}

static /*@observer@*/ /*@null@*/ yasm_section *
bin_objfmt_section_switch(yasm_object *object, yasm_valparamhead *valparams,
			  /*@unused@*/ /*@null@*/
			  yasm_valparamhead *objext_valparams,
			  unsigned long line)
{
    yasm_valparam *vp;
    yasm_section *retval;
    int isnew;
    unsigned long start;
    char *sectname;
    int resonly = 0;
    unsigned long alignval = 0;
    int have_alignval = 0;

    if ((vp = yasm_vps_first(valparams)) && !vp->param && vp->val != NULL) {
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
	    yasm__error(line, N_("segment name `%s' not recognized"),
			sectname);
	    return NULL;
	}

	/* Check for ALIGN qualifier */
	while ((vp = yasm_vps_next(vp))) {
	    if (yasm__strcasecmp(vp->val, "align") == 0 && vp->param) {
		/*@dependent@*/ /*@null@*/ const yasm_intnum *align;
		unsigned long bitcnt;

		if (strcmp(sectname, ".text") == 0) {
		    yasm__error(line,
			N_("cannot specify an alignment to the `%s' section"),
			sectname);
		    return NULL;
		}
		
		align = yasm_expr_get_intnum(&vp->param, NULL);
		if (!align) {
		    yasm__error(line,
				N_("argument to `%s' is not a power of two"),
				vp->val);
		    return NULL;
		}
		alignval = yasm_intnum_get_uint(align);

		/* Check to see if alignval is a power of two.
		 * This can be checked by seeing if only one bit is set.
		 */
		BitCount(bitcnt, alignval);
		if (bitcnt > 1) {
		    yasm__error(line,
				N_("argument to `%s' is not a power of two"),
				vp->val);
		    return NULL;
		}

		have_alignval = 1;
	    }
	}

	retval = yasm_object_get_general(object, sectname,
	    yasm_expr_create_ident(
		yasm_expr_int(yasm_intnum_create_uint(start)), line), resonly,
	    &isnew, line);

	if (isnew) {
	    if (have_alignval) {
		unsigned long *data = yasm_xmalloc(sizeof(unsigned long));
		*data = alignval;
		yasm_section_add_data(retval, &bin_section_data_callback,
				      data);
	    }

	    yasm_symtab_define_label(yasm_object_get_symtab(object), sectname,
				     yasm_section_bcs_first(retval), 1, line);
	} else if (have_alignval)
	    yasm__warning(YASM_WARN_GENERAL, line,
		N_("alignment value ignored on section redeclaration"));

	return retval;
    } else
	return NULL;
}

static void
bin_section_data_destroy(/*@only@*/ void *d)
{
    yasm_xfree(d);
}

static void
bin_objfmt_common_declare(/*@unused@*/ yasm_symrec *sym,
			  /*@only@*/ yasm_expr *size, /*@unused@*/ /*@null@*/
			  yasm_valparamhead *objext_valparams,
			  unsigned long line)
{
    yasm_expr_destroy(size);
    yasm__error(line,
	N_("binary object format does not support common variables"));
}

static int
bin_objfmt_directive(const char *name, yasm_valparamhead *valparams,
		     /*@unused@*/ /*@null@*/
		     yasm_valparamhead *objext_valparams,
		     yasm_object *object, unsigned long line)
{
    yasm_section *sect;
    yasm_valparam *vp;

    if (yasm__strcasecmp(name, "org") == 0) {
	/*@null@*/ yasm_expr *start = NULL;

	/* ORG takes just a simple integer as param */
	vp = yasm_vps_first(valparams);
	if (vp->val)
	    start = yasm_expr_create_ident(yasm_expr_sym(yasm_symtab_use(
		yasm_object_get_symtab(object), vp->val, line)), line);
	else if (vp->param) {
	    start = vp->param;
	    vp->param = NULL;	/* Don't let valparams delete it */
	}

	if (!start) {
	    yasm__error(line, N_("argument to ORG must be expression"));
	    return 0;
	}

	/* ORG changes the start of the .text section */
	sect = yasm_object_find_general(object, ".text");
	if (!sect)
	    yasm_internal_error(
		N_("bin objfmt: .text section does not exist before ORG is called?"));
	yasm_section_set_start(sect, start, line);

	return 0;	    /* directive recognized */
    } else
	return 1;	    /* directive unrecognized */
}

static void
bin_section_data_print(void *data, FILE *f, int indent_level)
{
    fprintf(f, "%*salign=%ld\n", indent_level, "", *((unsigned long *)data));
}


/* Define valid debug formats to use with this object format */
static const char *bin_objfmt_dbgfmt_keywords[] = {
    "null",
    NULL
};

/* Define objfmt structure -- see objfmt.h for details */
yasm_objfmt yasm_bin_LTX_objfmt = {
    YASM_OBJFMT_VERSION,
    "Flat format binary",
    "bin",
    NULL,
    ".text",
    16,
    bin_objfmt_dbgfmt_keywords,
    "null",
    bin_objfmt_initialize,
    bin_objfmt_output,
    bin_objfmt_cleanup,
    bin_objfmt_section_switch,
    NULL /*bin_objfmt_extern_declare*/,
    NULL /*bin_objfmt_global_declare*/,
    bin_objfmt_common_declare,
    bin_objfmt_directive
};
