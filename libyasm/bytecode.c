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
/*@unused@*/ RCSID("$IdPath: yasm/libyasm/bytecode.c,v 1.93 2003/03/31 05:36:29 peter Exp $");

#include "file.h"

#include "errwarn.h"
#include "intnum.h"
#include "expr.h"

#include "bytecode.h"
#include "objfmt.h"

#include "arch.h"

#include "bc-int.h"
#include "expr-int.h"


struct yasm_dataval {
    /*@reldef@*/ STAILQ_ENTRY(yasm_dataval) link;

    enum { DV_EMPTY, DV_EXPR, DV_STRING } type;

    union {
	/*@only@*/ yasm_expr *expn;
	/*@only@*/ char *str_val;
    } data;
};

typedef struct bytecode_data {
    yasm_bytecode bc;	/* base structure */

    /* non-converted data (linked list) */
    yasm_datavalhead datahead;

    /* final (converted) size of each element (in bytes) */
    unsigned char size;
} bytecode_data;

typedef struct bytecode_reserve {
    yasm_bytecode bc;   /* base structure */

    /*@only@*/ yasm_expr *numitems; /* number of items to reserve */
    unsigned char itemsize;	    /* size of each item (in bytes) */
} bytecode_reserve;

typedef struct bytecode_incbin {
    yasm_bytecode bc;   /* base structure */

    /*@only@*/ char *filename;		/* file to include data from */

    /* starting offset to read from (NULL=0) */
    /*@only@*/ /*@null@*/ yasm_expr *start;

    /* maximum number of bytes to read (NULL=no limit) */
    /*@only@*/ /*@null@*/ yasm_expr *maxlen;
} bytecode_incbin;

typedef struct bytecode_align {
    yasm_bytecode bc;   /* base structure */

    unsigned long boundary;	/* alignment boundary */
} bytecode_align;

typedef struct bytecode_objfmt_data {
    yasm_bytecode bc;   /* base structure */

    unsigned int type;			/* objfmt-specific type */
    /*@dependent@*/ yasm_objfmt *of;	/* objfmt that created the data */
    /*@only@*/ void *data;		/* objfmt-specific data */
} bytecode_objfmt_data;

/* Static structures for when NULL is passed to conversion functions. */
/*  for Convert*ToBytes() */
unsigned char bytes_static[16];

/*@dependent@*/ static yasm_arch *cur_arch;


void
yasm_bc_initialize(yasm_arch *a)
{
    cur_arch = a;
}

yasm_immval *
yasm_imm_new_int(unsigned long int_val, unsigned long lindex)
{
    return yasm_imm_new_expr(
	yasm_expr_new_ident(yasm_expr_int(yasm_intnum_new_uint(int_val)),
			    lindex));
}

yasm_immval *
yasm_imm_new_expr(yasm_expr *expr_ptr)
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

/*@-nullstate@*/
void
yasm_ea_delete(yasm_effaddr *ea)
{
    if (cur_arch->ea_data_delete)
	cur_arch->ea_data_delete(ea);
    yasm_expr_delete(ea->disp);
    yasm_xfree(ea);
}
/*@=nullstate@*/

/*@-nullstate@*/
void
yasm_ea_print(FILE *f, int indent_level, const yasm_effaddr *ea)
{
    fprintf(f, "%*sDisp=", indent_level, "");
    yasm_expr_print(f, ea->disp);
    fprintf(f, "\n%*sLen=%u\n", indent_level, "", (unsigned int)ea->len);
    fprintf(f, "%*sNoSplit=%u\n", indent_level, "", (unsigned int)ea->nosplit);
    if (cur_arch->ea_data_print)
	cur_arch->ea_data_print(f, indent_level, ea);
}
/*@=nullstate@*/

void
yasm_bc_set_multiple(yasm_bytecode *bc, yasm_expr *e)
{
    if (bc->multiple)
	bc->multiple = yasm_expr_new_tree(bc->multiple, YASM_EXPR_MUL, e,
					  e->line);
    else
	bc->multiple = e;
}

yasm_bytecode *
yasm_bc_new_common(yasm_bytecode_type type, size_t size, unsigned long lindex)
{
    yasm_bytecode *bc = yasm_xmalloc(size);

    bc->type = type;

    bc->multiple = (yasm_expr *)NULL;
    bc->len = 0;

    bc->line = lindex;

    bc->offset = 0;

    bc->opt_flags = 0;

    return bc;
}

yasm_bytecode *
yasm_bc_new_data(yasm_datavalhead *datahead, unsigned int size,
		 unsigned long lindex)
{
    bytecode_data *data;

    data = (bytecode_data *)
	yasm_bc_new_common(YASM_BC__DATA, sizeof(bytecode_data), lindex);

    data->datahead = *datahead;
    data->size = (unsigned char)size;

    return (yasm_bytecode *)data;
}

yasm_bytecode *
yasm_bc_new_reserve(yasm_expr *numitems, unsigned int itemsize,
		    unsigned long lindex)
{
    bytecode_reserve *reserve;

    reserve = (bytecode_reserve *)
	yasm_bc_new_common(YASM_BC__RESERVE, sizeof(bytecode_reserve), lindex);

    /*@-mustfree@*/
    reserve->numitems = numitems;
    /*@=mustfree@*/
    reserve->itemsize = (unsigned char)itemsize;

    return (yasm_bytecode *)reserve;
}

yasm_bytecode *
yasm_bc_new_incbin(char *filename, yasm_expr *start, yasm_expr *maxlen,
		   unsigned long lindex)
{
    bytecode_incbin *incbin;

    incbin = (bytecode_incbin *)
	yasm_bc_new_common(YASM_BC__INCBIN, sizeof(bytecode_incbin), lindex);

    /*@-mustfree@*/
    incbin->filename = filename;
    incbin->start = start;
    incbin->maxlen = maxlen;
    /*@=mustfree@*/

    return (yasm_bytecode *)incbin;
}

yasm_bytecode *
yasm_bc_new_align(unsigned long boundary, unsigned long lindex)
{
    bytecode_align *align;

    align = (bytecode_align *)
	yasm_bc_new_common(YASM_BC__ALIGN, sizeof(bytecode_align), lindex);

    align->boundary = boundary;

    return (yasm_bytecode *)align;
}

yasm_bytecode *
yasm_bc_new_objfmt_data(unsigned int type, unsigned long len, yasm_objfmt *of,
			void *data, unsigned long lindex)
{
    bytecode_objfmt_data *objfmt_data;

    objfmt_data = (bytecode_objfmt_data *)
	yasm_bc_new_common(YASM_BC__OBJFMT_DATA, sizeof(bytecode_objfmt_data),
			   lindex);

    objfmt_data->type = type;
    objfmt_data->of = of;
    /*@-mustfree@*/
    objfmt_data->data = data;
    /*@=mustfree@*/

    /* Yes, this breaks the paradigm just a little.  But this data is very
     * unlike other bytecode data--it's internally generated after the
     * other bytecodes have been resolved, and the length is ALWAYS known.
     */
    objfmt_data->bc.len = len;

    return (yasm_bytecode *)objfmt_data;
}

void
yasm_bc_delete(yasm_bytecode *bc)
{
    bytecode_data *data;
    bytecode_reserve *reserve;
    bytecode_incbin *incbin;
    bytecode_objfmt_data *objfmt_data;

    if (!bc)
	return;

    /*@-branchstate@*/
    switch (bc->type) {
	case YASM_BC__EMPTY:
	    break;
	case YASM_BC__DATA:
	    data = (bytecode_data *)bc;
	    yasm_dvs_delete(&data->datahead);
	    break;
	case YASM_BC__RESERVE:
	    reserve = (bytecode_reserve *)bc;
	    yasm_expr_delete(reserve->numitems);
	    break;
	case YASM_BC__INCBIN:
	    incbin = (bytecode_incbin *)bc;
	    yasm_xfree(incbin->filename);
	    yasm_expr_delete(incbin->start);
	    yasm_expr_delete(incbin->maxlen);
	    break;
	case YASM_BC__ALIGN:
	    break;
	case YASM_BC__OBJFMT_DATA:
	    objfmt_data = (bytecode_objfmt_data *)bc;
	    if (objfmt_data->of->bc_objfmt_data_delete)
		objfmt_data->of->bc_objfmt_data_delete(objfmt_data->type,
						       objfmt_data->data);
	    else
		yasm_internal_error(
		    N_("objfmt can't handle its own objfmt data bytecode"));
	    break;
	default:
	    if ((unsigned int)bc->type < (unsigned int)cur_arch->bc_type_max)
		cur_arch->bc_delete(bc);
	    else
		yasm_internal_error(N_("Unknown bytecode type"));
	    break;
    }
    /*@=branchstate@*/

    yasm_expr_delete(bc->multiple);
    yasm_xfree(bc);
}

void
yasm_bc_print(FILE *f, int indent_level, const yasm_bytecode *bc)
{
    const bytecode_data *data;
    const bytecode_reserve *reserve;
    const bytecode_incbin *incbin;
    const bytecode_align *align;
    const bytecode_objfmt_data *objfmt_data;

    switch (bc->type) {
	case YASM_BC__EMPTY:
	    fprintf(f, "%*s_Empty_\n", indent_level, "");
	    break;
	case YASM_BC__DATA:
	    data = (const bytecode_data *)bc;
	    fprintf(f, "%*s_Data_\n", indent_level, "");
	    fprintf(f, "%*sFinal Element Size=%u\n", indent_level+1, "",
		    (unsigned int)data->size);
	    fprintf(f, "%*sElements:\n", indent_level+1, "");
	    yasm_dvs_print(f, indent_level+2, &data->datahead);
	    break;
	case YASM_BC__RESERVE:
	    reserve = (const bytecode_reserve *)bc;
	    fprintf(f, "%*s_Reserve_\n", indent_level, "");
	    fprintf(f, "%*sNum Items=", indent_level, "");
	    yasm_expr_print(f, reserve->numitems);
	    fprintf(f, "\n%*sItem Size=%u\n", indent_level, "",
		    (unsigned int)reserve->itemsize);
	    break;
	case YASM_BC__INCBIN:
	    incbin = (const bytecode_incbin *)bc;
	    fprintf(f, "%*s_IncBin_\n", indent_level, "");
	    fprintf(f, "%*sFilename=`%s'\n", indent_level, "",
		    incbin->filename);
	    fprintf(f, "%*sStart=", indent_level, "");
	    if (!incbin->start)
		fprintf(f, "nil (0)");
	    else
		yasm_expr_print(f, incbin->start);
	    fprintf(f, "%*sMax Len=", indent_level, "");
	    if (!incbin->maxlen)
		fprintf(f, "nil (unlimited)");
	    else
		yasm_expr_print(f, incbin->maxlen);
	    fprintf(f, "\n");
	    break;
	case YASM_BC__ALIGN:
	    align = (const bytecode_align *)bc;
	    fprintf(f, "%*s_Align_\n", indent_level, "");
	    fprintf(f, "%*sBoundary=%lu\n", indent_level, "", align->boundary);
	    break;
	case YASM_BC__OBJFMT_DATA:
	    objfmt_data = (const bytecode_objfmt_data *)bc;
	    fprintf(f, "%*s_ObjFmt_Data_\n", indent_level, "");
	    if (objfmt_data->of->bc_objfmt_data_print)
		objfmt_data->of->bc_objfmt_data_print(f, indent_level,
						      objfmt_data->type,
						      objfmt_data->data);
	    else
		fprintf(f, "%*sUNKNOWN\n", indent_level, "");
	    break;
	default:
	    if ((unsigned int)bc->type < (unsigned int)cur_arch->bc_type_max)
		cur_arch->bc_print(f, indent_level, bc);
	    else
		fprintf(f, "%*s_Unknown_\n", indent_level, "");
	    break;
    }
    fprintf(f, "%*sMultiple=", indent_level, "");
    if (!bc->multiple)
	fprintf(f, "nil (1)");
    else
	yasm_expr_print(f, bc->multiple);
    fprintf(f, "\n%*sLength=%lu\n", indent_level, "", bc->len);
    fprintf(f, "%*sLine Index=%lu\n", indent_level, "", bc->line);
    fprintf(f, "%*sOffset=%lx\n", indent_level, "", bc->offset);
}

/*@null@*/ yasm_intnum *
yasm_common_calc_bc_dist(yasm_section *sect, /*@null@*/ yasm_bytecode *precbc1,
			 /*@null@*/ yasm_bytecode *precbc2)
{
    unsigned int dist;
    yasm_intnum *intn;

    if (precbc2) {
	dist = precbc2->offset + precbc2->len;
	if (precbc1) {
	    if (dist < precbc1->offset + precbc1->len) {
		intn = yasm_intnum_new_uint(precbc1->offset + precbc1->len
					    - dist);
		yasm_intnum_calc(intn, YASM_EXPR_NEG, NULL);
		return intn;
	    }
	    dist -= precbc1->offset + precbc1->len;
	}
	return yasm_intnum_new_uint(dist);
    } else {
	if (precbc1) {
	    intn = yasm_intnum_new_uint(precbc1->offset + precbc1->len);
	    yasm_intnum_calc(intn, YASM_EXPR_NEG, NULL);
	    return intn;
	} else {
	    return yasm_intnum_new_uint(0);
	}
    }
}

static yasm_bc_resolve_flags
bc_resolve_data(bytecode_data *bc_data, unsigned long *len)
{
    yasm_dataval *dv;
    size_t slen;

    /* Count up element sizes, rounding up string length. */
    STAILQ_FOREACH(dv, &bc_data->datahead, link) {
	switch (dv->type) {
	    case DV_EMPTY:
		break;
	    case DV_EXPR:
		*len += bc_data->size;
		break;
	    case DV_STRING:
		slen = strlen(dv->data.str_val);
		/* find count, rounding up to nearest multiple of size */
		slen = (slen + bc_data->size - 1) / bc_data->size;
		*len += slen*bc_data->size;
		break;
	}
    }

    return YASM_BC_RESOLVE_MIN_LEN;
}

static yasm_bc_resolve_flags
bc_resolve_reserve(bytecode_reserve *reserve, unsigned long *len,
		   int save, unsigned long line, const yasm_section *sect,
		   yasm_calc_bc_dist_func calc_bc_dist)
{
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
	    yasm__error(line,
		N_("expression must not contain floating point value"));
	else
	    yasm__error(line,
		N_("attempt to reserve non-constant quantity of space"));
	retval = YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
    } else
	*len += yasm_intnum_get_uint(num)*reserve->itemsize;
    yasm_expr_delete(temp);
    return retval;
}

static yasm_bc_resolve_flags
bc_resolve_incbin(bytecode_incbin *incbin, unsigned long *len, int save,
		  unsigned long line, const yasm_section *sect,
		  yasm_calc_bc_dist_func calc_bc_dist)
{
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
	yasm_expr_delete(temp);
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
	yasm_expr_delete(temp);
	if (!num)
	    return YASM_BC_RESOLVE_UNKNOWN_LEN;
    }

    /* FIXME: Search include path for filename.  Save full path back into
     * filename if save is true.
     */

    /* Open file and determine its length */
    f = fopen(incbin->filename, "rb");
    if (!f) {
	yasm__error(line, N_("`incbin': unable to open file `%s'"),
		    incbin->filename);
	return YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
    }
    if (fseek(f, 0L, SEEK_END) < 0) {
	yasm__error(line, N_("`incbin': unable to seek on file `%s'"),
		    incbin->filename);
	return YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
    }
    flen = (unsigned long)ftell(f);
    fclose(f);

    /* Compute length of incbin from start, maxlen, and len */
    if (start > flen) {
	yasm__warning(YASM_WARN_GENERAL, line,
		      N_("`incbin': start past end of file `%s'"),
		      incbin->filename);
	start = flen;
    }
    flen -= start;
    if (incbin->maxlen)
	if (maxlen < flen)
	    flen = maxlen;
    *len += flen;
    return YASM_BC_RESOLVE_MIN_LEN;
}

yasm_bc_resolve_flags
yasm_bc_resolve(yasm_bytecode *bc, int save, const yasm_section *sect,
		yasm_calc_bc_dist_func calc_bc_dist)
{
    yasm_bc_resolve_flags retval = YASM_BC_RESOLVE_MIN_LEN;
    /*@null@*/ yasm_expr *temp;
    yasm_expr **tempp;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;

    bc->len = 0;	/* start at 0 */

    switch (bc->type) {
	case YASM_BC__EMPTY:
	    yasm_internal_error(N_("got empty bytecode in bc_calc_len"));
	    /*break;*/
	case YASM_BC__DATA:
	    retval = bc_resolve_data((bytecode_data *)bc, &bc->len);
	    break;
	case YASM_BC__RESERVE:
	    retval = bc_resolve_reserve((bytecode_reserve *)bc, &bc->len,
					save, bc->line, sect, calc_bc_dist);
	    break;
	case YASM_BC__INCBIN:
	    retval = bc_resolve_incbin((bytecode_incbin *)bc, &bc->len,
				       save, bc->line, sect, calc_bc_dist);
	    break;
	case YASM_BC__ALIGN:
	    /* TODO */
	    yasm_internal_error(N_("TODO: align bytecode not implemented!"));
	    /*break;*/
	case YASM_BC__OBJFMT_DATA:
	    yasm_internal_error(N_("resolving objfmt data bytecode?"));
	    /*break;*/
	default:
	    if ((unsigned int)bc->type < (unsigned int)cur_arch->bc_type_max)
		retval = cur_arch->bc_resolve(bc, save, sect, calc_bc_dist);
	    else
		yasm_internal_error(N_("Unknown bytecode type"));
    }

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
	yasm_expr_delete(temp);
    }

    /* If we got an error somewhere along the line, clear out any calc len */
    if (retval & YASM_BC_RESOLVE_UNKNOWN_LEN)
	bc->len = 0;

    return retval;
}

static int
bc_tobytes_data(bytecode_data *bc_data, unsigned char **bufp,
		const yasm_section *sect, yasm_bytecode *bc, void *d,
		yasm_output_expr_func output_expr)
    /*@sets **bufp@*/
{
    yasm_dataval *dv;
    size_t slen;
    size_t i;
    unsigned char *bufp_orig = *bufp;

    STAILQ_FOREACH(dv, &bc_data->datahead, link) {
	switch (dv->type) {
	    case DV_EMPTY:
		break;
	    case DV_EXPR:
		if (output_expr(&dv->data.expn, bufp, bc_data->size,
				(unsigned long)(*bufp-bufp_orig), sect, bc, 0,
				d))
		    return 1;
		break;
	    case DV_STRING:
		slen = strlen(dv->data.str_val);
		strncpy((char *)*bufp, dv->data.str_val, slen);
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
    }

    return 0;
}

static int
bc_tobytes_incbin(bytecode_incbin *incbin, unsigned char **bufp,
		  unsigned long len, unsigned long line)
    /*@sets **bufp@*/
{
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
	yasm__error(line, N_("`incbin': unable to open file `%s'"),
		    incbin->filename);
	return 1;
    }

    /* Seek to start of data */
    if (fseek(f, (long)start, SEEK_SET) < 0) {
	yasm__error(line, N_("`incbin': unable to seek on file `%s'"),
		    incbin->filename);
	fclose(f);
	return 1;
    }

    /* Read len bytes */
    if (fread(*bufp, (size_t)len, 1, f) < (size_t)len) {
	yasm__error(line,
		    N_("`incbin': unable to read %lu bytes from file `%s'"),
		    len, incbin->filename);
	fclose(f);
	return 1;
    }

    *bufp += len;
    fclose(f);
    return 0;
}

/*@null@*/ /*@only@*/ unsigned char *
yasm_bc_tobytes(yasm_bytecode *bc, unsigned char *buf, unsigned long *bufsize,
		/*@out@*/ unsigned long *multiple, /*@out@*/ int *gap,
		const yasm_section *sect, void *d,
		yasm_output_expr_func output_expr,
		/*@null@*/ yasm_output_bc_objfmt_data_func
		    output_bc_objfmt_data)
    /*@sets *buf@*/
{
    /*@only@*/ /*@null@*/ unsigned char *mybuf = NULL;
    unsigned char *origbuf, *destbuf;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;
    bytecode_objfmt_data *objfmt_data;
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

    if (bc->type == YASM_BC__RESERVE) {
	*gap = 1;
	return NULL;	/* we didn't allocate a buffer */
    }

    *gap = 0;

    if (*bufsize < datasize) {
	mybuf = yasm_xmalloc(sizeof(bc->len));
	origbuf = mybuf;
	destbuf = mybuf;
    } else {
	origbuf = buf;
	destbuf = buf;
    }

    switch (bc->type) {
	case YASM_BC__EMPTY:
	    yasm_internal_error(N_("got empty bytecode in bc_tobytes"));
	    /*break;*/
	case YASM_BC__DATA:
	    error = bc_tobytes_data((bytecode_data *)bc, &destbuf, sect, bc, d,
				    output_expr);
	    break;
	case YASM_BC__INCBIN:
	    error = bc_tobytes_incbin((bytecode_incbin *)bc, &destbuf, bc->len,
				      bc->line);
	    break;
	case YASM_BC__ALIGN:
	    /* TODO */
	    yasm_internal_error(N_("TODO: align bytecode not implemented!"));
	    /*break;*/
	case YASM_BC__OBJFMT_DATA:
	    objfmt_data = (bytecode_objfmt_data *)bc;
	    if (output_bc_objfmt_data)
		error = output_bc_objfmt_data(objfmt_data->type,
					      objfmt_data->data, &destbuf);
	    else
		yasm_internal_error(
		    N_("Have objfmt data bytecode but no way to output it"));
	    break;
	default:
	    if ((unsigned int)bc->type < (unsigned int)cur_arch->bc_type_max)
		error = cur_arch->bc_tobytes(bc, &destbuf, sect, d,
					     output_expr);
	    else
		yasm_internal_error(N_("Unknown bytecode type"));
    }

    if (!error && ((unsigned long)(destbuf - origbuf) != datasize))
	yasm_internal_error(
	    N_("written length does not match optimized length"));
    return mybuf;
}

yasm_bytecode *
yasm_bcs_last(yasm_bytecodehead *headp)
{
    return STAILQ_LAST(headp, yasm_bytecode, link);
}

void
yasm_bcs_delete(yasm_bytecodehead *headp)
{
    yasm_bytecode *cur, *next;

    cur = STAILQ_FIRST(headp);
    while (cur) {
	next = STAILQ_NEXT(cur, link);
	yasm_bc_delete(cur);
	cur = next;
    }
    STAILQ_INIT(headp);
    yasm_xfree(headp);
}

yasm_bytecode *
yasm_bcs_append(yasm_bytecodehead *headp, yasm_bytecode *bc)
{
    if (bc) {
	if (bc->type != YASM_BC__EMPTY) {
	    STAILQ_INSERT_TAIL(headp, bc, link);
	    return bc;
	} else {
	    yasm_xfree(bc);
	}
    }
    return (yasm_bytecode *)NULL;
}

void
yasm_bcs_print(FILE *f, int indent_level, const yasm_bytecodehead *headp)
{
    yasm_bytecode *cur;

    STAILQ_FOREACH(cur, headp, link) {
	fprintf(f, "%*sNext Bytecode:\n", indent_level, "");
	yasm_bc_print(f, indent_level+1, cur);
    }
}

int
yasm_bcs_traverse(yasm_bytecodehead *headp, void *d,
		  int (*func) (yasm_bytecode *bc, /*@null@*/ void *d))
{
    yasm_bytecode *cur;

    STAILQ_FOREACH(cur, headp, link) {
	int retval = func(cur, d);
	if (retval != 0)
	    return retval;
    }
    return 0;
}

yasm_dataval *
yasm_dv_new_expr(yasm_expr *expn)
{
    yasm_dataval *retval = yasm_xmalloc(sizeof(yasm_dataval));

    retval->type = DV_EXPR;
    retval->data.expn = expn;

    return retval;
}

yasm_dataval *
yasm_dv_new_string(char *str_val)
{
    yasm_dataval *retval = yasm_xmalloc(sizeof(yasm_dataval));

    retval->type = DV_STRING;
    retval->data.str_val = str_val;

    return retval;
}

void
yasm_dvs_delete(yasm_datavalhead *headp)
{
    yasm_dataval *cur, *next;

    cur = STAILQ_FIRST(headp);
    while (cur) {
	next = STAILQ_NEXT(cur, link);
	switch (cur->type) {
	    case DV_EXPR:
		yasm_expr_delete(cur->data.expn);
		break;
	    case DV_STRING:
		yasm_xfree(cur->data.str_val);
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
yasm_dvs_print(FILE *f, int indent_level, const yasm_datavalhead *head)
{
    yasm_dataval *cur;

    STAILQ_FOREACH(cur, head, link) {
	switch (cur->type) {
	    case DV_EMPTY:
		fprintf(f, "%*sEmpty\n", indent_level, "");
		break;
	    case DV_EXPR:
		fprintf(f, "%*sExpr=", indent_level, "");
		yasm_expr_print(f, cur->data.expn);
		fprintf(f, "\n");
		break;
	    case DV_STRING:
		fprintf(f, "%*sString=%s\n", indent_level, "",
			cur->data.str_val);
		break;
	}
    }
}

yasm_bytecodehead *
yasm_bcs_new(void)
{
    yasm_bytecodehead *headp;
    headp = yasm_xmalloc(sizeof(yasm_bytecodehead));
    STAILQ_INIT(headp);
    return headp;
}

/* Non-macro yasm_bcs_first() for non-YASM_INTERNAL users. */
#undef yasm_bcs_first
yasm_bytecode *
yasm_bcs_first(yasm_bytecodehead *headp)
{
    return STAILQ_FIRST(headp);
}

/* Non-macro yasm_dvs_initialize() for non-YASM_INTERNAL users. */
#undef yasm_dvs_initialize
void
yasm_dvs_initialize(yasm_datavalhead *headp)
{
    STAILQ_INIT(headp);
}
