/*
 * Bytecode utility functions
 *
 *  Copyright (C) 2001  Peter Johnson
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
#define YASM_LIB_INTERNAL
#include "util.h"
/*@unused@*/ RCSID("$Id$");

#include "coretype.h"
#include "file.h"

#include "errwarn.h"
#include "intnum.h"
#include "expr.h"

#include "bytecode.h"
#include "arch.h"
#include "objfmt.h"
#include "dbgfmt.h"

#include "bc-int.h"
#include "expr-int.h"


struct yasm_dataval {
    /*@reldef@*/ STAILQ_ENTRY(yasm_dataval) link;

    enum { DV_EMPTY, DV_EXPR, DV_STRING } type;

    union {
	/*@only@*/ yasm_expr *expn;
	struct {
	    /*@only@*/ char *contents;
	    size_t len;
	} str;
    } data;
};

/* Standard bytecode types */

typedef struct bytecode_data {
    /* non-converted data (linked list) */
    yasm_datavalhead datahead;

    /* final (converted) size of each element (in bytes) */
    unsigned int size;

    /* append a zero byte after each element? */
    int append_zero;
} bytecode_data;

typedef struct bytecode_leb128 {
    /* source data (linked list) */
    yasm_datavalhead datahead;

    /* signedness (0=unsigned, 1=signed) */
    int sign;

    /* total length (calculated at finalize time) */
    unsigned long len;
} bytecode_leb128;

typedef struct bytecode_reserve {
    /*@only@*/ yasm_expr *numitems; /* number of items to reserve */
    unsigned char itemsize;	    /* size of each item (in bytes) */
} bytecode_reserve;

typedef struct bytecode_incbin {
    /*@only@*/ char *filename;		/* file to include data from */

    /* starting offset to read from (NULL=0) */
    /*@only@*/ /*@null@*/ yasm_expr *start;

    /* maximum number of bytes to read (NULL=no limit) */
    /*@only@*/ /*@null@*/ yasm_expr *maxlen;
} bytecode_incbin;

typedef struct bytecode_align {
    /*@only@*/ yasm_expr *boundary;	/* alignment boundary */

    /* What to fill intervening locations with, NULL if using code_fill */
    /*@only@*/ /*@null@*/ yasm_expr *fill;

    /* Maximum number of bytes to skip, NULL if no maximum. */
    /*@only@*/ /*@null@*/ yasm_expr *maxskip;

    /* Code fill, NULL if using 0 fill */
    /*@null@*/ const unsigned char **code_fill;
} bytecode_align;

typedef struct bytecode_org {
    unsigned long start;	/* target starting offset within section */
} bytecode_org;

typedef struct bytecode_insn {
    /*@dependent@*/ yasm_arch *arch;
    unsigned long insn_data[4];

    int num_operands;
    /*@null@*/ yasm_insn_operands operands;

    /* array of 4-element prefix_data arrays */
    int num_prefixes;
    /*@null@*/ unsigned long **prefixes;

    /* array of segment prefixes */
    int num_segregs;
    /*@null@*/ unsigned long *segregs;
} bytecode_insn;

/* Standard bytecode callback function prototypes */

static void bc_data_destroy(void *contents);
static void bc_data_print(const void *contents, FILE *f, int indent_level);
static yasm_bc_resolve_flags bc_data_resolve
    (yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
static int bc_data_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
			   yasm_output_expr_func output_expr,
			   /*@null@*/ yasm_output_reloc_func output_reloc);

static void bc_leb128_destroy(void *contents);
static void bc_leb128_print(const void *contents, FILE *f, int indent_level);
static void bc_leb128_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc);
static yasm_bc_resolve_flags bc_leb128_resolve
    (yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
static int bc_leb128_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
			     yasm_output_expr_func output_expr,
			     /*@null@*/ yasm_output_reloc_func output_reloc);

static void bc_reserve_destroy(void *contents);
static void bc_reserve_print(const void *contents, FILE *f, int indent_level);
static yasm_bc_resolve_flags bc_reserve_resolve
    (yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
static int bc_reserve_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
			      yasm_output_expr_func output_expr,
			      /*@null@*/ yasm_output_reloc_func output_reloc);

static void bc_incbin_destroy(void *contents);
static void bc_incbin_print(const void *contents, FILE *f, int indent_level);
static yasm_bc_resolve_flags bc_incbin_resolve
    (yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
static int bc_incbin_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
			     yasm_output_expr_func output_expr,
			     /*@null@*/ yasm_output_reloc_func output_reloc);

static void bc_align_destroy(void *contents);
static void bc_align_print(const void *contents, FILE *f, int indent_level);
static void bc_align_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc);
static yasm_bc_resolve_flags bc_align_resolve
    (yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
static int bc_align_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
			    yasm_output_expr_func output_expr,
			    /*@null@*/ yasm_output_reloc_func output_reloc);

static void bc_org_destroy(void *contents);
static void bc_org_print(const void *contents, FILE *f, int indent_level);
static yasm_bc_resolve_flags bc_org_resolve
    (yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
static int bc_org_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
			  yasm_output_expr_func output_expr,
			  /*@null@*/ yasm_output_reloc_func output_reloc);

static void bc_insn_destroy(void *contents);
static void bc_insn_print(const void *contents, FILE *f, int indent_level);
static void bc_insn_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc);
static yasm_bc_resolve_flags bc_insn_resolve
    (yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
static int bc_insn_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
			   yasm_output_expr_func output_expr,
			   /*@null@*/ yasm_output_reloc_func output_reloc);

/* Standard bytecode callback structures */

static const yasm_bytecode_callback bc_data_callback = {
    bc_data_destroy,
    bc_data_print,
    yasm_bc_finalize_common,
    bc_data_resolve,
    bc_data_tobytes
};

static const yasm_bytecode_callback bc_leb128_callback = {
    bc_leb128_destroy,
    bc_leb128_print,
    bc_leb128_finalize,
    bc_leb128_resolve,
    bc_leb128_tobytes
};

static const yasm_bytecode_callback bc_reserve_callback = {
    bc_reserve_destroy,
    bc_reserve_print,
    yasm_bc_finalize_common,
    bc_reserve_resolve,
    bc_reserve_tobytes
};

static const yasm_bytecode_callback bc_incbin_callback = {
    bc_incbin_destroy,
    bc_incbin_print,
    yasm_bc_finalize_common,
    bc_incbin_resolve,
    bc_incbin_tobytes
};

static const yasm_bytecode_callback bc_align_callback = {
    bc_align_destroy,
    bc_align_print,
    bc_align_finalize,
    bc_align_resolve,
    bc_align_tobytes
};

static const yasm_bytecode_callback bc_org_callback = {
    bc_org_destroy,
    bc_org_print,
    yasm_bc_finalize_common,
    bc_org_resolve,
    bc_org_tobytes
};

static const yasm_bytecode_callback bc_insn_callback = {
    bc_insn_destroy,
    bc_insn_print,
    bc_insn_finalize,
    bc_insn_resolve,
    bc_insn_tobytes
};

/* Static structures for when NULL is passed to conversion functions. */
/*  for Convert*ToBytes() */
unsigned char bytes_static[16];


yasm_immval *
yasm_imm_create_int(unsigned long int_val, unsigned long line)
{
    return yasm_imm_create_expr(
	yasm_expr_create_ident(yasm_expr_int(yasm_intnum_create_uint(int_val)),
			       line));
}

yasm_immval *
yasm_imm_create_expr(yasm_expr *expr_ptr)
{
    yasm_immval *im = yasm_xmalloc(sizeof(yasm_immval));

    im->val = expr_ptr;
    im->len = 0;
    im->sign = 0;

    return im;
}

const yasm_expr *
yasm_ea_get_disp(const yasm_effaddr *ptr)
{
    return ptr->disp;
}

void
yasm_ea_set_len(yasm_effaddr *ptr, unsigned int len)
{
    if (!ptr)
	return;

    /* Currently don't warn if length truncated, as this is called only from
     * an explicit override, where we expect the user knows what they're doing.
     */

    ptr->len = (unsigned char)len;
}

void
yasm_ea_set_nosplit(yasm_effaddr *ptr, unsigned int nosplit)
{
    if (!ptr)
	return;

    ptr->nosplit = (unsigned char)nosplit;
}

void
yasm_ea_set_strong(yasm_effaddr *ptr, unsigned int strong)
{
    if (!ptr)
	return;

    ptr->strong = (unsigned char)strong;
}

void
yasm_ea_set_segreg(yasm_effaddr *ea, unsigned long segreg, unsigned long line)
{
    if (!ea)
	return;

    if (segreg != 0 && ea->segreg != 0)
	yasm__warning(YASM_WARN_GENERAL, line,
		      N_("multiple segment overrides, using leftmost"));

    ea->segreg = segreg;
}

/*@-nullstate@*/
void
yasm_ea_destroy(yasm_effaddr *ea)
{
    ea->callback->destroy(ea);
    yasm_expr_destroy(ea->disp);
    yasm_xfree(ea);
}
/*@=nullstate@*/

/*@-nullstate@*/
void
yasm_ea_print(const yasm_effaddr *ea, FILE *f, int indent_level)
{
    fprintf(f, "%*sDisp=", indent_level, "");
    yasm_expr_print(ea->disp, f);
    fprintf(f, "\n%*sLen=%u\n", indent_level, "", (unsigned int)ea->len);
    fprintf(f, "%*sNoSplit=%u\n", indent_level, "", (unsigned int)ea->nosplit);
    ea->callback->print(ea, f, indent_level);
}
/*@=nullstate@*/

void
yasm_bc_set_multiple(yasm_bytecode *bc, yasm_expr *e)
{
    if (bc->multiple)
	bc->multiple = yasm_expr_create_tree(bc->multiple, YASM_EXPR_MUL, e,
					     e->line);
    else
	bc->multiple = e;
}

void
yasm_bc_finalize_common(yasm_bytecode *bc, yasm_bytecode *prev_bc)
{
}

void
yasm_bc_transform(yasm_bytecode *bc, const yasm_bytecode_callback *callback,
		  void *contents)
{
    if (bc->callback)
	bc->callback->destroy(bc->contents);
    bc->callback = callback;
    bc->contents = contents;
}

yasm_bytecode *
yasm_bc_create_common(const yasm_bytecode_callback *callback, void *contents,
		      unsigned long line)
{
    yasm_bytecode *bc = yasm_xmalloc(sizeof(yasm_bytecode));

    bc->callback = callback;

    bc->section = NULL;

    bc->multiple = (yasm_expr *)NULL;
    bc->len = 0;

    bc->line = line;

    bc->offset = 0;

    bc->opt_flags = 0;

    bc->symrecs = NULL;

    bc->contents = contents;

    return bc;
}

static void
bc_data_destroy(void *contents)
{
    bytecode_data *bc_data = (bytecode_data *)contents;
    yasm_dvs_destroy(&bc_data->datahead);
    yasm_xfree(contents);
}

static void
bc_data_print(const void *contents, FILE *f, int indent_level)
{
    const bytecode_data *bc_data = (const bytecode_data *)contents;
    fprintf(f, "%*s_Data_\n", indent_level, "");
    fprintf(f, "%*sFinal Element Size=%u\n", indent_level+1, "", bc_data->size);
    fprintf(f, "%*sAppend Zero=%i\n", indent_level+1, "", bc_data->append_zero);
    fprintf(f, "%*sElements:\n", indent_level+1, "");
    yasm_dvs_print(&bc_data->datahead, f, indent_level+2);
}

static yasm_bc_resolve_flags
bc_data_resolve(yasm_bytecode *bc, int save,
		yasm_calc_bc_dist_func calc_bc_dist)
{
    bytecode_data *bc_data = (bytecode_data *)bc->contents;
    yasm_dataval *dv;
    size_t slen;

    /* Count up element sizes, rounding up string length. */
    STAILQ_FOREACH(dv, &bc_data->datahead, link) {
	switch (dv->type) {
	    case DV_EMPTY:
		break;
	    case DV_EXPR:
		bc->len += bc_data->size;
		break;
	    case DV_STRING:
		slen = dv->data.str.len;
		/* find count, rounding up to nearest multiple of size */
		slen = (slen + bc_data->size - 1) / bc_data->size;
		bc->len += slen*bc_data->size;
		break;
	}
	if (bc_data->append_zero)
	    bc->len++;
    }

    return YASM_BC_RESOLVE_MIN_LEN;
}

static int
bc_data_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		yasm_output_expr_func output_expr,
		/*@unused@*/ yasm_output_reloc_func output_reloc)
{
    bytecode_data *bc_data = (bytecode_data *)bc->contents;
    yasm_dataval *dv;
    size_t slen;
    size_t i;
    unsigned char *bufp_orig = *bufp;

    STAILQ_FOREACH(dv, &bc_data->datahead, link) {
	switch (dv->type) {
	    case DV_EMPTY:
		break;
	    case DV_EXPR:
		if (output_expr(&dv->data.expn, *bufp, bc_data->size,
				(size_t)(bc_data->size*8), 0,
				(unsigned long)(*bufp-bufp_orig), bc, 0, 1, d))
		    return 1;
		*bufp += bc_data->size;
		break;
	    case DV_STRING:
		slen = dv->data.str.len;
		memcpy(*bufp, dv->data.str.contents, slen);
		*bufp += slen;
		/* pad with 0's to nearest multiple of size */
		slen %= bc_data->size;
		if (slen > 0) {
		    slen = bc_data->size-slen;
		    for (i=0; i<slen; i++)
			YASM_WRITE_8(*bufp, 0);
		}
		break;
	}
	if (bc_data->append_zero)
	    YASM_WRITE_8(*bufp, 0);
    }

    return 0;
}

yasm_bytecode *
yasm_bc_create_data(yasm_datavalhead *datahead, unsigned int size,
		    int append_zero, unsigned long line)
{
    bytecode_data *data = yasm_xmalloc(sizeof(bytecode_data));

    data->datahead = *datahead;
    data->size = size;
    data->append_zero = append_zero;

    return yasm_bc_create_common(&bc_data_callback, data, line);
}

static void
bc_leb128_destroy(void *contents)
{
    bytecode_leb128 *bc_leb128 = (bytecode_leb128 *)contents;
    yasm_dvs_destroy(&bc_leb128->datahead);
    yasm_xfree(contents);
}

static void
bc_leb128_print(const void *contents, FILE *f, int indent_level)
{
    const bytecode_leb128 *bc_leb128 = (const bytecode_leb128 *)contents;
    fprintf(f, "%*s_Data_\n", indent_level, "");
    fprintf(f, "%*sSign=%u\n", indent_level+1, "",
	    (unsigned int)bc_leb128->sign);
    fprintf(f, "%*sElements:\n", indent_level+1, "");
    yasm_dvs_print(&bc_leb128->datahead, f, indent_level+2);
}

static void
bc_leb128_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc)
{
    bytecode_leb128 *bc_leb128 = (bytecode_leb128 *)bc->contents;
    yasm_dataval *dv;
    /*@dependent@*/ /*@null@*/ yasm_intnum *intn;

    /* Only constant expressions are allowed.
     * Because of this, go ahead and calculate length.
     */
    bc_leb128->len = 0;
    STAILQ_FOREACH(dv, &bc_leb128->datahead, link) {
	switch (dv->type) {
	    case DV_EMPTY:
		break;
	    case DV_EXPR:
		intn = yasm_expr_get_intnum(&dv->data.expn, NULL);
		if (!intn) {
		    yasm__error(bc->line,
				N_("LEB128 requires constant values"));
		    return;
		}
		/* Warn for negative values in unsigned environment.
		 * This could be an error instead: the likelihood this is
		 * desired is very low!
		 */
		if (yasm_intnum_sign(intn) == -1 && !bc_leb128->sign)
		    yasm__warning(YASM_WARN_GENERAL, bc->line,
				  N_("negative value in unsigned LEB128"));
		bc_leb128->len +=
		    yasm_intnum_size_leb128(intn, bc_leb128->sign);
		break;
	    case DV_STRING:
		yasm__error(bc->line,
			    N_("LEB128 does not allow string constants"));
		return;
	}
    }
}

static yasm_bc_resolve_flags
bc_leb128_resolve(yasm_bytecode *bc, int save,
		  yasm_calc_bc_dist_func calc_bc_dist)
{
    bytecode_leb128 *bc_leb128 = (bytecode_leb128 *)bc->contents;
    bc->len += bc_leb128->len;
    return YASM_BC_RESOLVE_MIN_LEN;
}

static int
bc_leb128_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		  yasm_output_expr_func output_expr,
		  /*@unused@*/ yasm_output_reloc_func output_reloc)
{
    bytecode_leb128 *bc_leb128 = (bytecode_leb128 *)bc->contents;
    yasm_dataval *dv;
    /*@dependent@*/ /*@null@*/ yasm_intnum *intn;

    STAILQ_FOREACH(dv, &bc_leb128->datahead, link) {
	switch (dv->type) {
	    case DV_EMPTY:
		break;
	    case DV_EXPR:
		intn = yasm_expr_get_intnum(&dv->data.expn, NULL);
		if (!intn)
		    yasm_internal_error(N_("non-constant in leb128_tobytes"));
		*bufp += yasm_intnum_get_leb128(intn, *bufp, bc_leb128->sign);
		break;
	    case DV_STRING:
		yasm_internal_error(N_("string in leb128_tobytes"));
	}
    }

    return 0;
}

yasm_bytecode *
yasm_bc_create_leb128(yasm_datavalhead *datahead, int sign, unsigned long line)
{
    bytecode_leb128 *leb128 = yasm_xmalloc(sizeof(bytecode_leb128));

    leb128->datahead = *datahead;
    leb128->sign = sign;

    return yasm_bc_create_common(&bc_leb128_callback, leb128, line);
}

static void
bc_reserve_destroy(void *contents)
{
    bytecode_reserve *reserve = (bytecode_reserve *)contents;
    yasm_expr_destroy(reserve->numitems);
    yasm_xfree(contents);
}

static void
bc_reserve_print(const void *contents, FILE *f, int indent_level)
{
    const bytecode_reserve *reserve = (const bytecode_reserve *)contents;
    fprintf(f, "%*s_Reserve_\n", indent_level, "");
    fprintf(f, "%*sNum Items=", indent_level, "");
    yasm_expr_print(reserve->numitems, f);
    fprintf(f, "\n%*sItem Size=%u\n", indent_level, "",
	    (unsigned int)reserve->itemsize);
}

static yasm_bc_resolve_flags
bc_reserve_resolve(yasm_bytecode *bc, int save,
		   yasm_calc_bc_dist_func calc_bc_dist)
{
    bytecode_reserve *reserve = (bytecode_reserve *)bc->contents;
    yasm_bc_resolve_flags retval = YASM_BC_RESOLVE_MIN_LEN;
    /*@null@*/ yasm_expr *temp;
    yasm_expr **tempp;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;

    if (save) {
	temp = NULL;
	tempp = &reserve->numitems;
    } else {
	temp = yasm_expr_copy(reserve->numitems);
	assert(temp != NULL);
	tempp = &temp;
    }
    num = yasm_expr_get_intnum(tempp, calc_bc_dist);
    if (!num) {
	/* For reserve, just say non-constant quantity instead of allowing
	 * the circular reference error to filter through.
	 */
	if (temp && yasm_expr__contains(temp, YASM_EXPR_FLOAT))
	    yasm__error(bc->line,
		N_("expression must not contain floating point value"));
	else
	    yasm__error(bc->line,
		N_("attempt to reserve non-constant quantity of space"));
	retval = YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
    } else
	bc->len += yasm_intnum_get_uint(num)*reserve->itemsize;
    yasm_expr_destroy(temp);
    return retval;
}

static int
bc_reserve_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		   yasm_output_expr_func output_expr,
		   /*@unused@*/ yasm_output_reloc_func output_reloc)
{
    yasm_internal_error(N_("bc_reserve_tobytes called"));
    /*@notreached@*/
    return 1;
}

yasm_bytecode *
yasm_bc_create_reserve(yasm_expr *numitems, unsigned int itemsize,
		       unsigned long line)
{
    bytecode_reserve *reserve = yasm_xmalloc(sizeof(bytecode_reserve));

    /*@-mustfree@*/
    reserve->numitems = numitems;
    /*@=mustfree@*/
    reserve->itemsize = (unsigned char)itemsize;

    return yasm_bc_create_common(&bc_reserve_callback, reserve, line);
}

static void
bc_incbin_destroy(void *contents)
{
    bytecode_incbin *incbin = (bytecode_incbin *)contents;
    yasm_xfree(incbin->filename);
    yasm_expr_destroy(incbin->start);
    yasm_expr_destroy(incbin->maxlen);
    yasm_xfree(contents);
}

static void
bc_incbin_print(const void *contents, FILE *f, int indent_level)
{
    const bytecode_incbin *incbin = (const bytecode_incbin *)contents;
    fprintf(f, "%*s_IncBin_\n", indent_level, "");
    fprintf(f, "%*sFilename=`%s'\n", indent_level, "",
	    incbin->filename);
    fprintf(f, "%*sStart=", indent_level, "");
    if (!incbin->start)
	fprintf(f, "nil (0)");
    else
	yasm_expr_print(incbin->start, f);
    fprintf(f, "%*sMax Len=", indent_level, "");
    if (!incbin->maxlen)
	fprintf(f, "nil (unlimited)");
    else
	yasm_expr_print(incbin->maxlen, f);
    fprintf(f, "\n");
}

static yasm_bc_resolve_flags
bc_incbin_resolve(yasm_bytecode *bc, int save,
		  yasm_calc_bc_dist_func calc_bc_dist)
{
    bytecode_incbin *incbin = (bytecode_incbin *)bc->contents;
    FILE *f;
    /*@null@*/ yasm_expr *temp;
    yasm_expr **tempp;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;
    unsigned long start = 0, maxlen = 0xFFFFFFFFUL, flen;

    /* Try to convert start to integer value */
    if (incbin->start) {
	if (save) {
	    temp = NULL;
	    tempp = &incbin->start;
	} else {
	    temp = yasm_expr_copy(incbin->start);
	    assert(temp != NULL);
	    tempp = &temp;
	}
	num = yasm_expr_get_intnum(tempp, calc_bc_dist);
	if (num)
	    start = yasm_intnum_get_uint(num);
	yasm_expr_destroy(temp);
	if (!num)
	    return YASM_BC_RESOLVE_UNKNOWN_LEN;
    }

    /* Try to convert maxlen to integer value */
    if (incbin->maxlen) {
	if (save) {
	    temp = NULL;
	    tempp = &incbin->maxlen;
	} else {
	    temp = yasm_expr_copy(incbin->maxlen);
	    assert(temp != NULL);
	    tempp = &temp;
	}
	num = yasm_expr_get_intnum(tempp, calc_bc_dist);
	if (num)
	    maxlen = yasm_intnum_get_uint(num);
	yasm_expr_destroy(temp);
	if (!num)
	    return YASM_BC_RESOLVE_UNKNOWN_LEN;
    }

    /* FIXME: Search include path for filename.  Save full path back into
     * filename if save is true.
     */

    /* Open file and determine its length */
    f = fopen(incbin->filename, "rb");
    if (!f) {
	yasm__error(bc->line, N_("`incbin': unable to open file `%s'"),
		    incbin->filename);
	return YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
    }
    if (fseek(f, 0L, SEEK_END) < 0) {
	yasm__error(bc->line, N_("`incbin': unable to seek on file `%s'"),
		    incbin->filename);
	return YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
    }
    flen = (unsigned long)ftell(f);
    fclose(f);

    /* Compute length of incbin from start, maxlen, and len */
    if (start > flen) {
	yasm__warning(YASM_WARN_GENERAL, bc->line,
		      N_("`incbin': start past end of file `%s'"),
		      incbin->filename);
	start = flen;
    }
    flen -= start;
    if (incbin->maxlen)
	if (maxlen < flen)
	    flen = maxlen;
    bc->len += flen;
    return YASM_BC_RESOLVE_MIN_LEN;
}

static int
bc_incbin_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		  yasm_output_expr_func output_expr,
		  /*@unused@*/ yasm_output_reloc_func output_reloc)
{
    bytecode_incbin *incbin = (bytecode_incbin *)bc->contents;
    FILE *f;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;
    unsigned long start = 0;

    /* Convert start to integer value */
    if (incbin->start) {
	num = yasm_expr_get_intnum(&incbin->start, NULL);
	if (!num)
	    yasm_internal_error(
		N_("could not determine start in bc_tobytes_incbin"));
	start = yasm_intnum_get_uint(num);
    }

    /* Open file */
    f = fopen(incbin->filename, "rb");
    if (!f) {
	yasm__error(bc->line, N_("`incbin': unable to open file `%s'"),
		    incbin->filename);
	return 1;
    }

    /* Seek to start of data */
    if (fseek(f, (long)start, SEEK_SET) < 0) {
	yasm__error(bc->line, N_("`incbin': unable to seek on file `%s'"),
		    incbin->filename);
	fclose(f);
	return 1;
    }

    /* Read len bytes */
    if (fread(*bufp, 1, (size_t)bc->len, f) < (size_t)bc->len) {
	yasm__error(bc->line,
		    N_("`incbin': unable to read %lu bytes from file `%s'"),
		    bc->len, incbin->filename);
	fclose(f);
	return 1;
    }

    *bufp += bc->len;
    fclose(f);
    return 0;
}

yasm_bytecode *
yasm_bc_create_incbin(char *filename, yasm_expr *start, yasm_expr *maxlen,
		      unsigned long line)
{
    bytecode_incbin *incbin = yasm_xmalloc(sizeof(bytecode_incbin));

    /*@-mustfree@*/
    incbin->filename = filename;
    incbin->start = start;
    incbin->maxlen = maxlen;
    /*@=mustfree@*/

    return yasm_bc_create_common(&bc_incbin_callback, incbin, line);
}

static void
bc_align_destroy(void *contents)
{
    bytecode_align *align = (bytecode_align *)contents;
    if (align->fill)
	yasm_expr_destroy(align->fill);
    yasm_xfree(contents);
}

static void
bc_align_print(const void *contents, FILE *f, int indent_level)
{
    const bytecode_align *align = (const bytecode_align *)contents;
    fprintf(f, "%*s_Align_\n", indent_level, "");
    fprintf(f, "%*sBoundary=", indent_level, "");
    yasm_expr_print(align->boundary, f);
    fprintf(f, "\n%*sFill=", indent_level, "");
    yasm_expr_print(align->fill, f);
    fprintf(f, "\n%*sMax Skip=", indent_level, "");
    yasm_expr_print(align->maxskip, f);
    fprintf(f, "\n");
}

static void
bc_align_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc)
{
    bytecode_align *align = (bytecode_align *)bc->contents;
    if (!yasm_expr_get_intnum(&align->boundary, NULL))
	yasm__error(bc->line, N_("align boundary must be a constant"));
    if (align->fill && !yasm_expr_get_intnum(&align->fill, NULL))
	yasm__error(bc->line, N_("align fill must be a constant"));
    if (align->maxskip && !yasm_expr_get_intnum(&align->maxskip, NULL))
	yasm__error(bc->line, N_("align maximum skip must be a constant"));
}

static yasm_bc_resolve_flags
bc_align_resolve(yasm_bytecode *bc, int save,
		 yasm_calc_bc_dist_func calc_bc_dist)
{
    bytecode_align *align = (bytecode_align *)bc->contents;
    unsigned long end;
    unsigned long boundary =
	yasm_intnum_get_uint(yasm_expr_get_intnum(&align->boundary, NULL));

    if (boundary == 0) {
	bc->len = 0;
	return YASM_BC_RESOLVE_MIN_LEN;
    }

    end = bc->offset;
    if (bc->offset & (boundary-1))
	end = (bc->offset & ~(boundary-1)) + boundary;

    bc->len = end - bc->offset;

    if (align->maxskip) {
	unsigned long maxskip =
	    yasm_intnum_get_uint(yasm_expr_get_intnum(&align->maxskip, NULL));
	if ((end - bc->offset) > maxskip)
	    bc->len = 0;
    }
    return YASM_BC_RESOLVE_MIN_LEN;
}

static int
bc_align_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		 yasm_output_expr_func output_expr,
		 /*@unused@*/ yasm_output_reloc_func output_reloc)
{
    bytecode_align *align = (bytecode_align *)bc->contents;
    unsigned long len;
    unsigned long boundary =
	yasm_intnum_get_uint(yasm_expr_get_intnum(&align->boundary, NULL));

    if (boundary == 0)
	return 0;
    else {
	unsigned long end = bc->offset;
	if (bc->offset & (boundary-1))
	    end = (bc->offset & ~(boundary-1)) + boundary;
	len = end - bc->offset;
	if (len == 0)
	    return 0;
	if (align->maxskip) {
	    unsigned long maxskip =
		yasm_intnum_get_uint(yasm_expr_get_intnum(&align->maxskip,
							  NULL));
	    if (len > maxskip)
		return 0;
	}
    }

    if (align->fill) {
	unsigned long v;
	v = yasm_intnum_get_uint(yasm_expr_get_intnum(&align->fill, NULL));
	memset(*bufp, (int)v, len);
	*bufp += len;
    } else if (align->code_fill) {
	unsigned long maxlen = 15;
	while (!align->code_fill[maxlen] && maxlen>0)
	    maxlen--;
	if (maxlen == 0) {
	    yasm__error(bc->line, N_("could not find any code alignment size"));
	    return 1;
	}

	/* Fill with maximum code fill as much as possible */
	while (len > maxlen) {
	    memcpy(*bufp, align->code_fill[maxlen], maxlen);
	    *bufp += maxlen;
	    len -= maxlen;
	}

	if (!align->code_fill[len]) {
	    yasm__error(bc->line, N_("invalid alignment size %d"), len);
	    return 1;
	}
	/* Handle rest of code fill */
	memcpy(*bufp, align->code_fill[len], len);
	*bufp += len;
    } else {
	/* Just fill with 0 */
	memset(*bufp, 0, len);
	*bufp += len;
    }
    return 0;
}

yasm_bytecode *
yasm_bc_create_align(yasm_expr *boundary, yasm_expr *fill,
		     yasm_expr *maxskip, const unsigned char **code_fill,
		     unsigned long line)
{
    bytecode_align *align = yasm_xmalloc(sizeof(bytecode_align));

    align->boundary = boundary;
    align->fill = fill;
    align->maxskip = maxskip;
    align->code_fill = code_fill;

    return yasm_bc_create_common(&bc_align_callback, align, line);
}

static void
bc_org_destroy(void *contents)
{
    yasm_xfree(contents);
}

static void
bc_org_print(const void *contents, FILE *f, int indent_level)
{
    const bytecode_org *org = (const bytecode_org *)contents;
    fprintf(f, "%*s_Org_\n", indent_level, "");
    fprintf(f, "%*sStart=%lu\n", indent_level, "", org->start);
}

static yasm_bc_resolve_flags
bc_org_resolve(yasm_bytecode *bc, int save,
	       yasm_calc_bc_dist_func calc_bc_dist)
{
    bytecode_org *org = (bytecode_org *)bc->contents;

    /* Check for overrun */
    if (bc->offset > org->start) {
	yasm__error(bc->line, N_("ORG overlap with already existing data"));
	return YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
    }

    /* Generate space to start offset */
    bc->len = org->start - bc->offset;
    return YASM_BC_RESOLVE_MIN_LEN;
}

static int
bc_org_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
	       yasm_output_expr_func output_expr,
	       /*@unused@*/ yasm_output_reloc_func output_reloc)
{
    bytecode_org *org = (bytecode_org *)bc->contents;
    unsigned long len, i;

    /* Sanity check for overrun */
    if (bc->offset > org->start) {
	yasm__error(bc->line, N_("ORG overlap with already existing data"));
	return 1;
    }
    len = org->start - bc->offset;
    for (i=0; i<len; i++)
	YASM_WRITE_8(*bufp, 0);
    return 0;
}

yasm_bytecode *
yasm_bc_create_org(unsigned long start, unsigned long line)
{
    bytecode_org *org = yasm_xmalloc(sizeof(bytecode_org));

    org->start = start;

    return yasm_bc_create_common(&bc_org_callback, org, line);
}

static void
bc_insn_destroy(void *contents)
{
    bytecode_insn *insn = (bytecode_insn *)contents;
    if (insn->num_operands > 0)
	yasm_ops_delete(&insn->operands, 0);
    if (insn->num_prefixes > 0) {
	int i;
	for (i=0; i<insn->num_prefixes; i++)
	    yasm_xfree(insn->prefixes[i]);
	yasm_xfree(insn->prefixes);
    }
    if (insn->num_segregs > 0)
	yasm_xfree(insn->segregs);
    yasm_xfree(contents);
}

static void
bc_insn_print(const void *contents, FILE *f, int indent_level)
{
}

static void
bc_insn_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc)
{
    bytecode_insn *insn = (bytecode_insn *)bc->contents;
    int i;
    yasm_insn_operand *op;

    /* Simplify the operands' expressions first. */
    for (i = 0, op = yasm_ops_first(&insn->operands);
	 op && i<insn->num_operands; op = yasm_operand_next(op), i++) {
	/* Check operand type */
	switch (op->type) {
	    case YASM_INSN__OPERAND_MEMORY:
		/* Don't get over-ambitious here; some archs' memory expr
		 * parser are sensitive to the presence of *1, etc, so don't
		 * simplify identities.
		 */
		if (op->data.ea)
		    op->data.ea->disp =
			yasm_expr__level_tree(op->data.ea->disp, 1, 0, NULL,
					      NULL, NULL, NULL);
		break;
	    case YASM_INSN__OPERAND_IMM:
		op->data.val = yasm_expr_simplify(op->data.val, NULL);
		break;
	    default:
		break;
	}
    }

    yasm_arch_finalize_insn(insn->arch, bc, prev_bc, insn->insn_data,
			    insn->num_operands, &insn->operands,
			    insn->num_prefixes, insn->prefixes,
			    insn->num_segregs, insn->segregs);
}

static yasm_bc_resolve_flags
bc_insn_resolve(yasm_bytecode *bc, int save,
		yasm_calc_bc_dist_func calc_bc_dist)
{
    yasm_internal_error(N_("bc_insn_resolve() is not implemented"));
    /*@notreached@*/
    return YASM_BC_RESOLVE_ERROR;
}

static int
bc_insn_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		yasm_output_expr_func output_expr,
		/*@unused@*/ yasm_output_reloc_func output_reloc)
{
    yasm_internal_error(N_("bc_insn_tobytes() is not implemented"));
    /*@notreached@*/
    return 1;
}

yasm_bytecode *
yasm_bc_create_insn(yasm_arch *arch, const unsigned long insn_data[4],
		    int num_operands, /*@null@*/ yasm_insn_operands *operands,
		    unsigned long line)
{
    bytecode_insn *insn = yasm_xmalloc(sizeof(bytecode_insn));

    insn->arch = arch;
    insn->insn_data[0] = insn_data[0];
    insn->insn_data[1] = insn_data[1];
    insn->insn_data[2] = insn_data[2];
    insn->insn_data[3] = insn_data[3];
    insn->num_operands = num_operands;
    if (operands)
	insn->operands = *operands;	/* structure copy */
    insn->num_prefixes = 0;
    insn->prefixes = NULL;
    insn->num_segregs = 0;
    insn->segregs = NULL;

    return yasm_bc_create_common(&bc_insn_callback, insn, line);
}

yasm_bytecode *
yasm_bc_create_empty_insn(yasm_arch *arch, unsigned long line)
{
    bytecode_insn *insn = yasm_xmalloc(sizeof(bytecode_insn));

    insn->arch = arch;
    insn->insn_data[0] = 0;
    insn->insn_data[1] = 0;
    insn->insn_data[2] = 0;
    insn->insn_data[3] = 0;
    insn->num_operands = 0;
    insn->num_prefixes = 0;
    insn->prefixes = NULL;
    insn->num_segregs = 0;
    insn->segregs = NULL;

    return yasm_bc_create_common(&bc_insn_callback, insn, line);
}

void
yasm_bc_insn_add_prefix(yasm_bytecode *bc, const unsigned long prefix_data[4])
{
    bytecode_insn *insn = (bytecode_insn *)bc->contents;

    assert(bc->callback == bc_insn_callback);

    insn->prefixes =
	yasm_xrealloc(insn->prefixes,
		      (insn->num_prefixes+1)*sizeof(unsigned long *));
    insn->prefixes[insn->num_prefixes] =
	yasm_xmalloc(4*sizeof(unsigned long));
    insn->prefixes[insn->num_prefixes][0] = prefix_data[0];
    insn->prefixes[insn->num_prefixes][1] = prefix_data[1];
    insn->prefixes[insn->num_prefixes][2] = prefix_data[2];
    insn->prefixes[insn->num_prefixes][3] = prefix_data[3];
    insn->num_prefixes++;
}

void
yasm_bc_insn_add_seg_prefix(yasm_bytecode *bc, unsigned long segreg)
{
    bytecode_insn *insn = (bytecode_insn *)bc->contents;

    assert(bc->callback == bc_insn_callback);

    insn->segregs =
	yasm_xrealloc(insn->segregs,
		      (insn->num_segregs+1)*sizeof(unsigned long));
    insn->segregs[insn->num_segregs] = segreg;
    insn->num_segregs++;
}

yasm_section *
yasm_bc_get_section(yasm_bytecode *bc)
{
    return bc->section;
}

void
yasm_bc__add_symrec(yasm_bytecode *bc, yasm_symrec *sym)
{
    if (!bc->symrecs) {
	bc->symrecs = yasm_xmalloc(2*sizeof(yasm_symrec *));
	bc->symrecs[0] = sym;
	bc->symrecs[1] = NULL;
    } else {
	/* Very inefficient implementation for large numbers of symbols.  But
	 * that would be very unusual, so use the simple algorithm instead.
	 */
	size_t count = 1;
	while (bc->symrecs[count])
	    count++;
	bc->symrecs = yasm_xrealloc(bc->symrecs,
				    (count+2)*sizeof(yasm_symrec *));
	bc->symrecs[count] = sym;
	bc->symrecs[count+1] = NULL;
    }
}

void
yasm_bc_destroy(yasm_bytecode *bc)
{
    if (!bc)
	return;

    if (bc->callback)
	bc->callback->destroy(bc->contents);
    yasm_expr_destroy(bc->multiple);
    yasm_xfree(bc->symrecs);
    yasm_xfree(bc);
}

void
yasm_bc_print(const yasm_bytecode *bc, FILE *f, int indent_level)
{
    if (!bc->callback)
	fprintf(f, "%*s_Empty_\n", indent_level, "");
    else
	bc->callback->print(bc->contents, f, indent_level);
    fprintf(f, "%*sMultiple=", indent_level, "");
    if (!bc->multiple)
	fprintf(f, "nil (1)");
    else
	yasm_expr_print(bc->multiple, f);
    fprintf(f, "\n%*sLength=%lu\n", indent_level, "", bc->len);
    fprintf(f, "%*sLine Index=%lu\n", indent_level, "", bc->line);
    fprintf(f, "%*sOffset=%lx\n", indent_level, "", bc->offset);
}

void
yasm_bc_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc)
{
    if (bc->callback)
	bc->callback->finalize(bc, prev_bc);
}

/*@null@*/ yasm_intnum *
yasm_common_calc_bc_dist(/*@null@*/ yasm_bytecode *precbc1,
			 /*@null@*/ yasm_bytecode *precbc2)
{
    unsigned int dist;
    yasm_intnum *intn;

    if (precbc2) {
	dist = precbc2->offset + precbc2->len;
	if (precbc1) {
	    if (dist < precbc1->offset + precbc1->len) {
		intn = yasm_intnum_create_uint(precbc1->offset + precbc1->len
					       - dist);
		yasm_intnum_calc(intn, YASM_EXPR_NEG, NULL, precbc1->line);
		return intn;
	    }
	    dist -= precbc1->offset + precbc1->len;
	}
	return yasm_intnum_create_uint(dist);
    } else {
	if (precbc1) {
	    intn = yasm_intnum_create_uint(precbc1->offset + precbc1->len);
	    yasm_intnum_calc(intn, YASM_EXPR_NEG, NULL, precbc1->line);
	    return intn;
	} else {
	    return yasm_intnum_create_uint(0);
	}
    }
}

yasm_bc_resolve_flags
yasm_bc_resolve(yasm_bytecode *bc, int save,
		yasm_calc_bc_dist_func calc_bc_dist)
{
    yasm_bc_resolve_flags retval = YASM_BC_RESOLVE_MIN_LEN;
    /*@null@*/ yasm_expr *temp;
    yasm_expr **tempp;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;

    bc->len = 0;	/* start at 0 */

    if (!bc->callback)
	yasm_internal_error(N_("got empty bytecode in bc_resolve"));
    else
	retval = bc->callback->resolve(bc, save, calc_bc_dist);

    /* Multiply len by number of multiples */
    if (bc->multiple) {
	if (save) {
	    temp = NULL;
	    tempp = &bc->multiple;
	} else {
	    temp = yasm_expr_copy(bc->multiple);
	    assert(temp != NULL);
	    tempp = &temp;
	}
	num = yasm_expr_get_intnum(tempp, calc_bc_dist);
	if (!num) {
	    retval = YASM_BC_RESOLVE_UNKNOWN_LEN;
	    if (temp && yasm_expr__contains(temp, YASM_EXPR_FLOAT)) {
		yasm__error(bc->line,
		    N_("expression must not contain floating point value"));
		retval |= YASM_BC_RESOLVE_ERROR;
	    }
	} else
	    bc->len *= yasm_intnum_get_uint(num);
	yasm_expr_destroy(temp);
    }

    /* If we got an error somewhere along the line, clear out any calc len */
    if (retval & YASM_BC_RESOLVE_UNKNOWN_LEN)
	bc->len = 0;

    return retval;
}

/*@null@*/ /*@only@*/ unsigned char *
yasm_bc_tobytes(yasm_bytecode *bc, unsigned char *buf, unsigned long *bufsize,
		/*@out@*/ unsigned long *multiple, /*@out@*/ int *gap,
		void *d, yasm_output_expr_func output_expr,
		/*@null@*/ yasm_output_reloc_func output_reloc)
    /*@sets *buf@*/
{
    /*@only@*/ /*@null@*/ unsigned char *mybuf = NULL;
    unsigned char *origbuf, *destbuf;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;
    unsigned long datasize;
    int error = 0;

    if (bc->multiple) {
	num = yasm_expr_get_intnum(&bc->multiple, NULL);
	if (!num)
	    yasm_internal_error(
		N_("could not determine multiple in bc_tobytes"));
	*multiple = yasm_intnum_get_uint(num);
	if (*multiple == 0) {
	    *bufsize = 0;
	    return NULL;
	}
    } else
	*multiple = 1;

    datasize = bc->len / (*multiple);
    *bufsize = datasize;

    /* special case for reserve bytecodes */
    if (bc->callback == &bc_reserve_callback) {
	*gap = 1;
	return NULL;	/* we didn't allocate a buffer */
    }

    *gap = 0;

    if (*bufsize < datasize) {
	mybuf = yasm_xmalloc(bc->len);
	origbuf = mybuf;
	destbuf = mybuf;
    } else {
	origbuf = buf;
	destbuf = buf;
    }

    if (!bc->callback)
	yasm_internal_error(N_("got empty bytecode in bc_tobytes"));
    else
	error = bc->callback->tobytes(bc, &destbuf, d, output_expr,
				      output_reloc);

    if (!error && ((unsigned long)(destbuf - origbuf) != datasize))
	yasm_internal_error(
	    N_("written length does not match optimized length"));
    return mybuf;
}

yasm_dataval *
yasm_dv_create_expr(yasm_expr *expn)
{
    yasm_dataval *retval = yasm_xmalloc(sizeof(yasm_dataval));

    retval->type = DV_EXPR;
    retval->data.expn = expn;

    return retval;
}

yasm_dataval *
yasm_dv_create_string(char *contents, size_t len)
{
    yasm_dataval *retval = yasm_xmalloc(sizeof(yasm_dataval));

    retval->type = DV_STRING;
    retval->data.str.contents = contents;
    retval->data.str.len = len;

    return retval;
}

void
yasm_dvs_destroy(yasm_datavalhead *headp)
{
    yasm_dataval *cur, *next;

    cur = STAILQ_FIRST(headp);
    while (cur) {
	next = STAILQ_NEXT(cur, link);
	switch (cur->type) {
	    case DV_EXPR:
		yasm_expr_destroy(cur->data.expn);
		break;
	    case DV_STRING:
		yasm_xfree(cur->data.str.contents);
		break;
	    default:
		break;
	}
	yasm_xfree(cur);
	cur = next;
    }
    STAILQ_INIT(headp);
}

yasm_dataval *
yasm_dvs_append(yasm_datavalhead *headp, yasm_dataval *dv)
{
    if (dv) {
	STAILQ_INSERT_TAIL(headp, dv, link);
	return dv;
    }
    return (yasm_dataval *)NULL;
}

void
yasm_dvs_print(const yasm_datavalhead *head, FILE *f, int indent_level)
{
    yasm_dataval *cur;

    STAILQ_FOREACH(cur, head, link) {
	switch (cur->type) {
	    case DV_EMPTY:
		fprintf(f, "%*sEmpty\n", indent_level, "");
		break;
	    case DV_EXPR:
		fprintf(f, "%*sExpr=", indent_level, "");
		yasm_expr_print(cur->data.expn, f);
		fprintf(f, "\n");
		break;
	    case DV_STRING:
		fprintf(f, "%*sLength=%lu\n", indent_level, "",
			(unsigned long)cur->data.str.len);
		fprintf(f, "%*sString=\"%s\"\n", indent_level, "",
			cur->data.str.contents);
		break;
	}
    }
}

/* Non-macro yasm_dvs_initialize() for non-YASM_LIB_INTERNAL users. */
#undef yasm_dvs_initialize
void
yasm_dvs_initialize(yasm_datavalhead *headp)
{
    STAILQ_INIT(headp);
}
