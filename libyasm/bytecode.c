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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "file.h"

#include "errwarn.h"
#include "intnum.h"
#include "expr.h"

#include "bytecode.h"
#include "objfmt.h"

#include "arch.h"

#include "bc-int.h"
#include "expr-int.h"


struct dataval {
    /*@reldef@*/ STAILQ_ENTRY(dataval) link;

    enum { DV_EMPTY, DV_EXPR, DV_STRING } type;

    union {
	/*@only@*/ expr *expn;
	/*@only@*/ char *str_val;
    } data;
};

typedef struct bytecode_data {
    bytecode bc;    /* base structure */

    /* non-converted data (linked list) */
    datavalhead datahead;

    /* final (converted) size of each element (in bytes) */
    unsigned char size;
} bytecode_data;

typedef struct bytecode_reserve {
    bytecode bc;    /* base structure */

    /*@only@*/ expr *numitems;	/* number of items to reserve */
    unsigned char itemsize;	/* size of each item (in bytes) */
} bytecode_reserve;

typedef struct bytecode_incbin {
    bytecode bc;    /* base structure */

    /*@only@*/ char *filename;		/* file to include data from */

    /* starting offset to read from (NULL=0) */
    /*@only@*/ /*@null@*/ expr *start;

    /* maximum number of bytes to read (NULL=no limit) */
    /*@only@*/ /*@null@*/ expr *maxlen;
} bytecode_incbin;

typedef struct bytecode_align {
    bytecode bc;    /* base structure */

    unsigned long boundary;	/* alignment boundary */
} bytecode_align;

typedef struct bytecode_objfmt_data {
    bytecode bc;    /* base structure */

    unsigned int type;		/* objfmt-specific type */
    /*@dependent@*/ objfmt *of;	/* objfmt that created the data */
    /*@only@*/ void *data;	/* objfmt-specific data */
} bytecode_objfmt_data;

/* Static structures for when NULL is passed to conversion functions. */
/*  for Convert*ToBytes() */
unsigned char bytes_static[16];

/*@dependent@*/ static arch *cur_arch;
/*@dependent@*/ static errwarn *cur_we;


void
bc_initialize(arch *a, errwarn *we)
{
    cur_arch = a;
    cur_we = we;
}

immval *
imm_new_int(unsigned long int_val, unsigned long lindex)
{
    return imm_new_expr(expr_new_ident(ExprInt(intnum_new_uint(int_val)),
				       lindex));
}

immval *
imm_new_expr(expr *expr_ptr)
{
    immval *im = xmalloc(sizeof(immval));

    im->val = expr_ptr;
    im->len = 0;
    im->sign = 0;

    return im;
}

const expr *
ea_get_disp(const effaddr *ptr)
{
    return ptr->disp;
}

void
ea_set_len(effaddr *ptr, unsigned char len)
{
    if (!ptr)
	return;

    /* Currently don't warn if length truncated, as this is called only from
     * an explicit override, where we expect the user knows what they're doing.
     */

    ptr->len = len;
}

void
ea_set_nosplit(effaddr *ptr, unsigned char nosplit)
{
    if (!ptr)
	return;

    ptr->nosplit = nosplit;
}

/*@-nullstate@*/
void
ea_delete(effaddr *ea)
{
    if (cur_arch->ea_data_delete)
	cur_arch->ea_data_delete(ea);
    expr_delete(ea->disp);
    xfree(ea);
}
/*@=nullstate@*/

/*@-nullstate@*/
void
ea_print(FILE *f, int indent_level, const effaddr *ea)
{
    fprintf(f, "%*sDisp=", indent_level, "");
    expr_print(f, ea->disp);
    fprintf(f, "\n%*sLen=%u\n", indent_level, "", (unsigned int)ea->len);
    fprintf(f, "%*sNoSplit=%u\n", indent_level, "", (unsigned int)ea->nosplit);
    if (cur_arch->ea_data_print)
	cur_arch->ea_data_print(f, indent_level, ea);
}
/*@=nullstate@*/

void
bc_set_multiple(bytecode *bc, expr *e)
{
    if (bc->multiple)
	bc->multiple = expr_new_tree(bc->multiple, EXPR_MUL, e, e->line);
    else
	bc->multiple = e;
}

bytecode *
bc_new_common(bytecode_type type, size_t size, unsigned long lindex)
{
    bytecode *bc = xmalloc(size);

    bc->type = type;

    bc->multiple = (expr *)NULL;
    bc->len = 0;

    bc->line = lindex;

    bc->offset = 0;

    bc->opt_flags = 0;

    return bc;
}

bytecode *
bc_new_data(datavalhead *datahead, unsigned char size, unsigned long lindex)
{
    bytecode_data *data;

    data = (bytecode_data *)bc_new_common(BC_DATA, sizeof(bytecode_data),
					  lindex);

    data->datahead = *datahead;
    data->size = size;

    return (bytecode *)data;
}

bytecode *
bc_new_reserve(expr *numitems, unsigned char itemsize, unsigned long lindex)
{
    bytecode_reserve *reserve;

    reserve = (bytecode_reserve *)bc_new_common(BC_RESERVE,
						sizeof(bytecode_reserve),
						lindex);

    /*@-mustfree@*/
    reserve->numitems = numitems;
    /*@=mustfree@*/
    reserve->itemsize = itemsize;

    return (bytecode *)reserve;
}

bytecode *
bc_new_incbin(char *filename, expr *start, expr *maxlen, unsigned long lindex)
{
    bytecode_incbin *incbin;

    incbin = (bytecode_incbin *)bc_new_common(BC_INCBIN,
					      sizeof(bytecode_incbin),
					      lindex);

    /*@-mustfree@*/
    incbin->filename = filename;
    incbin->start = start;
    incbin->maxlen = maxlen;
    /*@=mustfree@*/

    return (bytecode *)incbin;
}

bytecode *
bc_new_align(unsigned long boundary, unsigned long lindex)
{
    bytecode_align *align;

    align = (bytecode_align *)bc_new_common(BC_ALIGN, sizeof(bytecode_align),
					    lindex);

    align->boundary = boundary;

    return (bytecode *)align;
}

bytecode *
bc_new_objfmt_data(unsigned int type, unsigned long len, objfmt *of,
		   void *data, unsigned long lindex)
{
    bytecode_objfmt_data *objfmt_data;

    objfmt_data =
	(bytecode_objfmt_data *)bc_new_common(BC_ALIGN,
					      sizeof(bytecode_objfmt_data),
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

    return (bytecode *)objfmt_data;
}

void
bc_delete(bytecode *bc)
{
    bytecode_data *data;
    bytecode_reserve *reserve;
    bytecode_incbin *incbin;
    bytecode_objfmt_data *objfmt_data;

    if (!bc)
	return;

    /*@-branchstate@*/
    switch (bc->type) {
	case BC_EMPTY:
	    break;
	case BC_DATA:
	    data = (bytecode_data *)bc;
	    dvs_delete(&data->datahead);
	    break;
	case BC_RESERVE:
	    reserve = (bytecode_reserve *)bc;
	    expr_delete(reserve->numitems);
	    break;
	case BC_INCBIN:
	    incbin = (bytecode_incbin *)bc;
	    xfree(incbin->filename);
	    expr_delete(incbin->start);
	    expr_delete(incbin->maxlen);
	    break;
	case BC_ALIGN:
	    break;
	case BC_OBJFMT_DATA:
	    objfmt_data = (bytecode_objfmt_data *)bc;
	    if (objfmt_data->of->bc_objfmt_data_delete)
		objfmt_data->of->bc_objfmt_data_delete(objfmt_data->type,
						       objfmt_data->data);
	    else
		cur_we->internal_error(
		    N_("objfmt can't handle its own objfmt data bytecode"));
	    break;
	default:
	    if (bc->type < cur_arch->bc.type_max)
		cur_arch->bc.bc_delete(bc);
	    else
		cur_we->internal_error(N_("Unknown bytecode type"));
	    break;
    }
    /*@=branchstate@*/

    expr_delete(bc->multiple);
    xfree(bc);
}

void
bc_print(FILE *f, int indent_level, const bytecode *bc)
{
    const bytecode_data *data;
    const bytecode_reserve *reserve;
    const bytecode_incbin *incbin;
    const bytecode_align *align;
    const bytecode_objfmt_data *objfmt_data;

    switch (bc->type) {
	case BC_EMPTY:
	    fprintf(f, "%*s_Empty_\n", indent_level, "");
	    break;
	case BC_DATA:
	    data = (const bytecode_data *)bc;
	    fprintf(f, "%*s_Data_\n", indent_level, "");
	    fprintf(f, "%*sFinal Element Size=%u\n", indent_level+1, "",
		    (unsigned int)data->size);
	    fprintf(f, "%*sElements:\n", indent_level+1, "");
	    dvs_print(f, indent_level+2, &data->datahead);
	    break;
	case BC_RESERVE:
	    reserve = (const bytecode_reserve *)bc;
	    fprintf(f, "%*s_Reserve_\n", indent_level, "");
	    fprintf(f, "%*sNum Items=", indent_level, "");
	    expr_print(f, reserve->numitems);
	    fprintf(f, "\n%*sItem Size=%u\n", indent_level, "",
		    (unsigned int)reserve->itemsize);
	    break;
	case BC_INCBIN:
	    incbin = (const bytecode_incbin *)bc;
	    fprintf(f, "%*s_IncBin_\n", indent_level, "");
	    fprintf(f, "%*sFilename=`%s'\n", indent_level, "",
		    incbin->filename);
	    fprintf(f, "%*sStart=", indent_level, "");
	    if (!incbin->start)
		fprintf(f, "nil (0)");
	    else
		expr_print(f, incbin->start);
	    fprintf(f, "%*sMax Len=", indent_level, "");
	    if (!incbin->maxlen)
		fprintf(f, "nil (unlimited)");
	    else
		expr_print(f, incbin->maxlen);
	    fprintf(f, "\n");
	    break;
	case BC_ALIGN:
	    align = (const bytecode_align *)bc;
	    fprintf(f, "%*s_Align_\n", indent_level, "");
	    fprintf(f, "%*sBoundary=%lu\n", indent_level, "", align->boundary);
	    break;
	case BC_OBJFMT_DATA:
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
	    if (bc->type < cur_arch->bc.type_max)
		cur_arch->bc.bc_print(f, indent_level, bc);
	    else
		fprintf(f, "%*s_Unknown_\n", indent_level, "");
	    break;
    }
    fprintf(f, "%*sMultiple=", indent_level, "");
    if (!bc->multiple)
	fprintf(f, "nil (1)");
    else
	expr_print(f, bc->multiple);
    fprintf(f, "\n%*sLength=%lu\n", indent_level, "", bc->len);
    fprintf(f, "%*sLine Index=%lu\n", indent_level, "", bc->line);
    fprintf(f, "%*sOffset=%lx\n", indent_level, "", bc->offset);
}

/*@null@*/ intnum *
common_calc_bc_dist(section *sect, /*@null@*/ bytecode *precbc1,
		    /*@null@*/ bytecode *precbc2)
{
    unsigned int dist;
    intnum *intn;

    if (precbc2) {
	dist = precbc2->offset + precbc2->len;
	if (precbc1) {
	    if (dist < precbc1->offset + precbc1->len) {
		intn = intnum_new_uint(precbc1->offset + precbc1->len - dist);
		intnum_calc(intn, EXPR_NEG, NULL);
		return intn;
	    }
	    dist -= precbc1->offset + precbc1->len;
	}
	return intnum_new_uint(dist);
    } else {
	if (precbc1) {
	    intn = intnum_new_uint(precbc1->offset + precbc1->len);
	    intnum_calc(intn, EXPR_NEG, NULL);
	    return intn;
	} else {
	    return intnum_new_uint(0);
	}
    }
}

static bc_resolve_flags
bc_resolve_data(bytecode_data *bc_data, unsigned long *len)
{
    dataval *dv;
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

    return BC_RESOLVE_MIN_LEN;
}

static bc_resolve_flags
bc_resolve_reserve(bytecode_reserve *reserve, unsigned long *len, int save,
		   unsigned long line, const section *sect,
		   calc_bc_dist_func calc_bc_dist)
{
    bc_resolve_flags retval = BC_RESOLVE_MIN_LEN;
    /*@null@*/ expr *temp;
    expr **tempp;
    /*@dependent@*/ /*@null@*/ const intnum *num;

    if (save) {
	temp = NULL;
	tempp = &reserve->numitems;
    } else {
	temp = expr_copy(reserve->numitems);
	assert(temp != NULL);
	tempp = &temp;
    }
    num = expr_get_intnum(tempp, calc_bc_dist);
    if (!num) {
	/* For reserve, just say non-constant quantity instead of allowing
	 * the circular reference error to filter through.
	 */
	if (temp && expr_contains(temp, EXPR_FLOAT))
	    cur_we->error(line,
		N_("expression must not contain floating point value"));
	else
	    cur_we->error(line,
		N_("attempt to reserve non-constant quantity of space"));
	retval = BC_RESOLVE_ERROR | BC_RESOLVE_UNKNOWN_LEN;
    } else
	*len += intnum_get_uint(num)*reserve->itemsize;
    expr_delete(temp);
    return retval;
}

static bc_resolve_flags
bc_resolve_incbin(bytecode_incbin *incbin, unsigned long *len, int save,
		  unsigned long line, const section *sect,
		  calc_bc_dist_func calc_bc_dist)
{
    FILE *f;
    /*@null@*/ expr *temp;
    expr **tempp;
    /*@dependent@*/ /*@null@*/ const intnum *num;
    unsigned long start = 0, maxlen = 0xFFFFFFFFUL, flen;

    /* Try to convert start to integer value */
    if (incbin->start) {
	if (save) {
	    temp = NULL;
	    tempp = &incbin->start;
	} else {
	    temp = expr_copy(incbin->start);
	    assert(temp != NULL);
	    tempp = &temp;
	}
	num = expr_get_intnum(tempp, calc_bc_dist);
	if (num)
	    start = intnum_get_uint(num);
	expr_delete(temp);
	if (!num)
	    return BC_RESOLVE_UNKNOWN_LEN;
    }

    /* Try to convert maxlen to integer value */
    if (incbin->maxlen) {
	if (save) {
	    temp = NULL;
	    tempp = &incbin->maxlen;
	} else {
	    temp = expr_copy(incbin->maxlen);
	    assert(temp != NULL);
	    tempp = &temp;
	}
	num = expr_get_intnum(tempp, calc_bc_dist);
	if (num)
	    maxlen = intnum_get_uint(num);
	expr_delete(temp);
	if (!num)
	    return BC_RESOLVE_UNKNOWN_LEN;
    }

    /* FIXME: Search include path for filename.  Save full path back into
     * filename if save is true.
     */

    /* Open file and determine its length */
    f = fopen(incbin->filename, "rb");
    if (!f) {
	cur_we->error(line, N_("`incbin': unable to open file `%s'"),
		      incbin->filename);
	return BC_RESOLVE_ERROR | BC_RESOLVE_UNKNOWN_LEN;
    }
    if (fseek(f, 0L, SEEK_END) < 0) {
	cur_we->error(line, N_("`incbin': unable to seek on file `%s'"),
		      incbin->filename);
	return BC_RESOLVE_ERROR | BC_RESOLVE_UNKNOWN_LEN;
    }
    flen = (unsigned long)ftell(f);
    fclose(f);

    /* Compute length of incbin from start, maxlen, and len */
    if (start > flen) {
	cur_we->warning(WARN_GENERAL, line,
			N_("`incbin': start past end of file `%s'"),
			incbin->filename);
	start = flen;
    }
    flen -= start;
    if (incbin->maxlen)
	if (maxlen < flen)
	    flen = maxlen;
    *len += flen;
    return BC_RESOLVE_MIN_LEN;
}

bc_resolve_flags
bc_resolve(bytecode *bc, int save, const section *sect,
	   calc_bc_dist_func calc_bc_dist)
{
    bc_resolve_flags retval = BC_RESOLVE_MIN_LEN;
    /*@null@*/ expr *temp;
    expr **tempp;
    /*@dependent@*/ /*@null@*/ const intnum *num;

    bc->len = 0;	/* start at 0 */

    switch (bc->type) {
	case BC_EMPTY:
	    cur_we->internal_error(N_("got empty bytecode in bc_calc_len"));
	case BC_DATA:
	    retval = bc_resolve_data((bytecode_data *)bc, &bc->len);
	    break;
	case BC_RESERVE:
	    retval = bc_resolve_reserve((bytecode_reserve *)bc, &bc->len, save,
					bc->line, sect, calc_bc_dist);
	    break;
	case BC_INCBIN:
	    retval = bc_resolve_incbin((bytecode_incbin *)bc, &bc->len, save,
				       bc->line, sect, calc_bc_dist);
	    break;
	case BC_ALIGN:
	    /* TODO */
	    cur_we->internal_error(
		N_("TODO: align bytecode not implemented!"));
	    /*break;*/
	case BC_OBJFMT_DATA:
	    cur_we->internal_error(N_("resolving objfmt data bytecode?"));
	    /*break;*/
	default:
	    if (bc->type < cur_arch->bc.type_max)
		retval = cur_arch->bc.bc_resolve(bc, save, sect,
						 calc_bc_dist);
	    else
		cur_we->internal_error(N_("Unknown bytecode type"));
    }

    /* Multiply len by number of multiples */
    if (bc->multiple) {
	if (save) {
	    temp = NULL;
	    tempp = &bc->multiple;
	} else {
	    temp = expr_copy(bc->multiple);
	    assert(temp != NULL);
	    tempp = &temp;
	}
	num = expr_get_intnum(tempp, calc_bc_dist);
	if (!num) {
	    retval = BC_RESOLVE_UNKNOWN_LEN;
	    if (temp && expr_contains(temp, EXPR_FLOAT)) {
		cur_we->error(bc->line,
		    N_("expression must not contain floating point value"));
		retval |= BC_RESOLVE_ERROR;
	    }
	} else
	    bc->len *= intnum_get_uint(num);
	expr_delete(temp);
    }

    /* If we got an error somewhere along the line, clear out any calc len */
    if (retval & BC_RESOLVE_UNKNOWN_LEN)
	bc->len = 0;

    return retval;
}

static int
bc_tobytes_data(bytecode_data *bc_data, unsigned char **bufp,
		const section *sect, const bytecode *bc, void *d,
		output_expr_func output_expr)
    /*@sets **bufp@*/
{
    dataval *dv;
    size_t slen;
    size_t i;
    unsigned char *bufp_orig = *bufp;

    STAILQ_FOREACH(dv, &bc_data->datahead, link) {
	switch (dv->type) {
	    case DV_EMPTY:
		break;
	    case DV_EXPR:
		if (output_expr(&dv->data.expn, bufp, bc_data->size,
				*bufp-bufp_orig, sect, bc, 0, d))
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
			WRITE_8(*bufp, 0);
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
    /*@dependent@*/ /*@null@*/ const intnum *num;
    unsigned long start = 0;

    /* Convert start to integer value */
    if (incbin->start) {
	num = expr_get_intnum(&incbin->start, NULL);
	if (!num)
	    cur_we->internal_error(
		N_("could not determine start in bc_tobytes_incbin"));
	start = intnum_get_uint(num);
    }

    /* Open file */
    f = fopen(incbin->filename, "rb");
    if (!f) {
	cur_we->error(line, N_("`incbin': unable to open file `%s'"),
		      incbin->filename);
	return 1;
    }

    /* Seek to start of data */
    if (fseek(f, (long)start, SEEK_SET) < 0) {
	cur_we->error(line, N_("`incbin': unable to seek on file `%s'"),
		      incbin->filename);
	fclose(f);
	return 1;
    }

    /* Read len bytes */
    if (fread(*bufp, (size_t)len, 1, f) < (size_t)len) {
	cur_we->error(line,
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
bc_tobytes(bytecode *bc, unsigned char *buf, unsigned long *bufsize,
	   /*@out@*/ unsigned long *multiple, /*@out@*/ int *gap,
	   const section *sect, void *d, output_expr_func output_expr,
	   /*@null@*/ output_bc_objfmt_data_func output_bc_objfmt_data)
    /*@sets *buf@*/
{
    /*@only@*/ /*@null@*/ unsigned char *mybuf = NULL;
    unsigned char *origbuf, *destbuf;
    /*@dependent@*/ /*@null@*/ const intnum *num;
    bytecode_objfmt_data *objfmt_data;
    unsigned long datasize;
    int error = 0;

    if (bc->multiple) {
	num = expr_get_intnum(&bc->multiple, NULL);
	if (!num)
	    cur_we->internal_error(
		N_("could not determine multiple in bc_tobytes"));
	*multiple = intnum_get_uint(num);
	if (*multiple == 0) {
	    *bufsize = 0;
	    return NULL;
	}
    } else
	*multiple = 1;

    datasize = bc->len / (*multiple);
    *bufsize = datasize;

    if (bc->type == BC_RESERVE) {
	*gap = 1;
	return NULL;	/* we didn't allocate a buffer */
    }

    *gap = 0;

    if (*bufsize < datasize) {
	mybuf = xmalloc(sizeof(bc->len));
	origbuf = mybuf;
	destbuf = mybuf;
    } else {
	origbuf = buf;
	destbuf = buf;
    }

    switch (bc->type) {
	case BC_EMPTY:
	    cur_we->internal_error(N_("got empty bytecode in bc_tobytes"));
	case BC_DATA:
	    error = bc_tobytes_data((bytecode_data *)bc, &destbuf, sect, bc, d,
				    output_expr);
	    break;
	case BC_INCBIN:
	    error = bc_tobytes_incbin((bytecode_incbin *)bc, &destbuf, bc->len,
				      bc->line);
	    break;
	case BC_ALIGN:
	    /* TODO */
	    cur_we->internal_error(
		N_("TODO: align bytecode not implemented!"));
	    /*break;*/
	case BC_OBJFMT_DATA:
	    objfmt_data = (bytecode_objfmt_data *)bc;
	    if (output_bc_objfmt_data)
		error = output_bc_objfmt_data(objfmt_data->type,
					      objfmt_data->data, &destbuf);
	    else
		cur_we->internal_error(
		    N_("Have objfmt data bytecode but no way to output it"));
	    break;
	default:
	    if (bc->type < cur_arch->bc.type_max)
		error = cur_arch->bc.bc_tobytes(bc, &destbuf, sect, d,
						output_expr);
	    else
		cur_we->internal_error(N_("Unknown bytecode type"));
    }

    if (!error && ((unsigned long)(destbuf - origbuf) != datasize))
	cur_we->internal_error(
	    N_("written length does not match optimized length"));
    return mybuf;
}

bytecode *
bcs_last(bytecodehead *headp)
{
    return STAILQ_LAST(headp, bytecode, link);
}

void
bcs_delete(bytecodehead *headp)
{
    bytecode *cur, *next;

    cur = STAILQ_FIRST(headp);
    while (cur) {
	next = STAILQ_NEXT(cur, link);
	bc_delete(cur);
	cur = next;
    }
    STAILQ_INIT(headp);
}

bytecode *
bcs_append(bytecodehead *headp, bytecode *bc)
{
    if (bc) {
	if (bc->type != BC_EMPTY) {
	    STAILQ_INSERT_TAIL(headp, bc, link);
	    return bc;
	} else {
	    xfree(bc);
	}
    }
    return (bytecode *)NULL;
}

void
bcs_print(FILE *f, int indent_level, const bytecodehead *headp)
{
    bytecode *cur;

    STAILQ_FOREACH(cur, headp, link) {
	fprintf(f, "%*sNext Bytecode:\n", indent_level, "");
	bc_print(f, indent_level+1, cur);
    }
}

int
bcs_traverse(bytecodehead *headp, void *d,
	     int (*func) (bytecode *bc, /*@null@*/ void *d))
{
    bytecode *cur;

    STAILQ_FOREACH(cur, headp, link) {
	int retval = func(cur, d);
	if (retval != 0)
	    return retval;
    }
    return 0;
}

dataval *
dv_new_expr(expr *expn)
{
    dataval *retval = xmalloc(sizeof(dataval));

    retval->type = DV_EXPR;
    retval->data.expn = expn;

    return retval;
}

dataval *
dv_new_string(char *str_val)
{
    dataval *retval = xmalloc(sizeof(dataval));

    retval->type = DV_STRING;
    retval->data.str_val = str_val;

    return retval;
}

void
dvs_delete(datavalhead *headp)
{
    dataval *cur, *next;

    cur = STAILQ_FIRST(headp);
    while (cur) {
	next = STAILQ_NEXT(cur, link);
	switch (cur->type) {
	    case DV_EXPR:
		expr_delete(cur->data.expn);
		break;
	    case DV_STRING:
		xfree(cur->data.str_val);
		break;
	    default:
		break;
	}
	xfree(cur);
	cur = next;
    }
    STAILQ_INIT(headp);
}

dataval *
dvs_append(datavalhead *headp, dataval *dv)
{
    if (dv) {
	STAILQ_INSERT_TAIL(headp, dv, link);
	return dv;
    }
    return (dataval *)NULL;
}

void
dvs_print(FILE *f, int indent_level, const datavalhead *head)
{
    dataval *cur;

    STAILQ_FOREACH(cur, head, link) {
	switch (cur->type) {
	    case DV_EMPTY:
		fprintf(f, "%*sEmpty\n", indent_level, "");
		break;
	    case DV_EXPR:
		fprintf(f, "%*sExpr=", indent_level, "");
		expr_print(f, cur->data.expn);
		fprintf(f, "\n");
		break;
	    case DV_STRING:
		fprintf(f, "%*sString=%s\n", indent_level, "",
			cur->data.str_val);
		break;
	}
    }
}
