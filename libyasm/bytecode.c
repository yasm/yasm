/*
 * Bytecode utility functions
 *
 *  Copyright (C) 2001  Peter Johnson
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
#include "expr.h"

#include "bytecode.h"

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
    /* non-converted data (linked list) */
    datavalhead datahead;

    /* final (converted) size of each element (in bytes) */
    unsigned char size;
} bytecode_data;

typedef struct bytecode_reserve {
    /*@only@*/ expr *numitems;	/* number of items to reserve */
    unsigned char itemsize;	/* size of each item (in bytes) */
} bytecode_reserve;

typedef struct bytecode_incbin {
    /*@only@*/ char *filename;		/* file to include data from */

    /* starting offset to read from (NULL=0) */
    /*@only@*/ /*@null@*/ expr *start;

    /* maximum number of bytes to read (NULL=no limit) */
    /*@only@*/ /*@null@*/ expr *maxlen;
} bytecode_incbin;

/* Static structures for when NULL is passed to conversion functions. */
/*  for Convert*ToBytes() */
unsigned char bytes_static[16];

immval *
imm_new_int(unsigned long int_val)
{
    return imm_new_expr(expr_new_ident(ExprInt(intnum_new_int(int_val))));
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

void
bc_set_multiple(bytecode *bc, expr *e)
{
    if (bc->multiple)
	bc->multiple = expr_new_tree(bc->multiple, EXPR_MUL, e);
    else
	bc->multiple = e;
}

bytecode *
bc_new_common(bytecode_type type, size_t datasize)
{
    bytecode *bc = xmalloc(sizeof(bytecode)+datasize);

    bc->type = type;

    bc->multiple = (expr *)NULL;
    bc->len = 0;

    bc->line = line_index;

    bc->offset = 0;

    bc->opt_flags = 0;

    return bc;
}

bytecode *
bc_new_data(datavalhead *datahead, unsigned char size)
{
    bytecode *bc = bc_new_common(BC_DATA, sizeof(bytecode_data));
    bytecode_data *data = bc_get_data(bc);

    data->datahead = *datahead;
    data->size = size;

    return bc;
}

bytecode *
bc_new_reserve(expr *numitems, unsigned char itemsize)
{
    bytecode *bc = bc_new_common(BC_RESERVE, sizeof(bytecode_reserve));
    bytecode_reserve *reserve = bc_get_data(bc);

    /*@-mustfree@*/
    reserve->numitems = numitems;
    /*@=mustfree@*/
    reserve->itemsize = itemsize;

    return bc;
}

bytecode *
bc_new_incbin(char *filename, expr *start, expr *maxlen)
{
    bytecode *bc = bc_new_common(BC_INCBIN, sizeof(bytecode_incbin));
    bytecode_incbin *incbin = bc_get_data(bc);

    /*@-mustfree@*/
    incbin->filename = filename;
    incbin->start = start;
    incbin->maxlen = maxlen;
    /*@=mustfree@*/

    return bc;
}

void
bc_delete(bytecode *bc)
{
    bytecode_data *data;
    bytecode_reserve *reserve;
    bytecode_incbin *incbin;

    if (!bc)
	return;

    /*@-branchstate@*/
    switch (bc->type) {
	case BC_EMPTY:
	    break;
	case BC_DATA:
	    data = bc_get_data(bc);
	    dvs_delete(&data->datahead);
	    break;
	case BC_RESERVE:
	    reserve = bc_get_data(bc);
	    expr_delete(reserve->numitems);
	    break;
	case BC_INCBIN:
	    incbin = bc_get_data(bc);
	    xfree(incbin->filename);
	    expr_delete(incbin->start);
	    expr_delete(incbin->maxlen);
	    break;
	default:
	    if (bc->type < cur_arch->bc.type_max)
		cur_arch->bc.bc_delete(bc);
	    else
		InternalError(_("Unknown bytecode type"));
	    break;
    }
    /*@=branchstate@*/

    expr_delete(bc->multiple);
    xfree(bc);
}

void
bc_print(FILE *f, const bytecode *bc)
{
    const bytecode_data *data;
    const bytecode_reserve *reserve;
    const bytecode_incbin *incbin;
    const char *filename;
    unsigned long line;

    switch (bc->type) {
	case BC_EMPTY:
	    fprintf(f, "%*s_Empty_\n", indent_level, "");
	    break;
	case BC_DATA:
	    data = bc_get_const_data(bc);
	    fprintf(f, "%*s_Data_\n", indent_level, "");
	    indent_level++;
	    fprintf(f, "%*sFinal Element Size=%u\n", indent_level, "",
		    (unsigned int)data->size);
	    fprintf(f, "%*sElements:\n", indent_level, "");
	    indent_level++;
	    dvs_print(f, &data->datahead);
	    indent_level-=2;
	    break;
	case BC_RESERVE:
	    reserve = bc_get_const_data(bc);
	    fprintf(f, "%*s_Reserve_\n", indent_level, "");
	    fprintf(f, "%*sNum Items=", indent_level, "");
	    expr_print(f, reserve->numitems);
	    fprintf(f, "\n%*sItem Size=%u\n", indent_level, "",
		    (unsigned int)reserve->itemsize);
	    break;
	case BC_INCBIN:
	    incbin = bc_get_const_data(bc);
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
	default:
	    if (bc->type < cur_arch->bc.type_max)
		cur_arch->bc.bc_print(f, bc);
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
    line_lookup(bc->line, &filename, &line);
    fprintf(f, "%*sFilename=\"%s\" Line Number=%lu\n", indent_level, "",
	    filename, line);
    fprintf(f, "%*sOffset=%lx\n", indent_level, "", bc->offset);
}

static int
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

    return 1;
}

static int
bc_resolve_reserve(bytecode_reserve *reserve, unsigned long *len, int save,
		   unsigned long line, const section *sect,
		   resolve_label_func resolve_label)
{
    int retval = 1;
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
    expr_expand_labelequ(*tempp, sect, 1, resolve_label);
    num = expr_get_intnum(tempp);
    if (!num) {
	if (expr_contains(temp, EXPR_FLOAT))
	    ErrorAt(line,
		    _("expression must not contain floating point value"));
	retval = -1;
    } else
	*len += intnum_get_uint(num)*reserve->itemsize;
    expr_delete(temp);
    return retval;
}

static int
bc_resolve_incbin(bytecode_incbin *incbin, unsigned long *len, int save,
		  unsigned long line, const section *sect,
		  resolve_label_func resolve_label)
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
	expr_expand_labelequ(*tempp, sect, 1, resolve_label);
	num = expr_get_intnum(tempp);
	if (num)
	    start = intnum_get_uint(num);
	expr_delete(temp);
	if (!num)
	    return -1;
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
	expr_expand_labelequ(*tempp, sect, 1, resolve_label);
	num = expr_get_intnum(tempp);
	if (num)
	    maxlen = intnum_get_uint(num);
	expr_delete(temp);
	if (!num)
	    return -1;
    }

    /* FIXME: Search include path for filename.  Save full path back into
     * filename if save is true.
     */

    /* Open file and determine its length */
    f = fopen(incbin->filename, "rb");
    if (!f) {
	ErrorAt(line, _("`incbin': unable to open file `%s'"),
		incbin->filename);
	return -1;
    }
    if (fseek(f, 0L, SEEK_END) < 0) {
	ErrorAt(line, _("`incbin': unable to seek on file `%s'"),
		incbin->filename);
	return -1;
    }
    flen = (unsigned long)ftell(f);
    fclose(f);

    /* Compute length of incbin from start, maxlen, and len */
    if (start > flen) {
	WarningAt(line, _("`incbin': start past end of file `%s'"),
		  incbin->filename);
	start = flen;
    }
    flen -= start;
    if (incbin->maxlen)
	if (maxlen < flen)
	    flen = maxlen;
    *len += flen;
    return 1;
}

int
bc_resolve(bytecode *bc, int save, const section *sect,
	   resolve_label_func resolve_label)
{
    int retval = 1;
    bytecode_data *bc_data;
    bytecode_reserve *reserve;
    bytecode_incbin *incbin;
    /*@null@*/ expr *temp;
    expr **tempp;
    /*@dependent@*/ /*@null@*/ const intnum *num;

    bc->len = 0;	/* start at 0 */

    switch (bc->type) {
	case BC_EMPTY:
	    InternalError(_("got empty bytecode in bc_calc_len"));
	case BC_DATA:
	    bc_data = bc_get_data(bc);
	    retval = bc_resolve_data(bc_data, &bc->len);
	    break;
	case BC_RESERVE:
	    reserve = bc_get_data(bc);
	    retval = bc_resolve_reserve(reserve, &bc->len, save, bc->line,
					sect, resolve_label);
	    break;
	case BC_INCBIN:
	    incbin = bc_get_data(bc);
	    retval = bc_resolve_incbin(incbin, &bc->len, save, bc->line, sect,
				       resolve_label);
	    break;
	default:
	    if (bc->type < cur_arch->bc.type_max)
		retval = cur_arch->bc.bc_resolve(bc, save, sect,
						 resolve_label);
	    else
		InternalError(_("Unknown bytecode type"));
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
	expr_expand_labelequ(*tempp, sect, 1, resolve_label);
	num = expr_get_intnum(tempp);
	if (!num) {
	    if (expr_contains(temp, EXPR_FLOAT))
		ErrorAt(bc->line,
			_("expression must not contain floating point value"));
	    retval = -1;
	} else
	    bc->len *= intnum_get_uint(num);
	expr_delete(temp);
    }

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

    STAILQ_FOREACH(dv, &bc_data->datahead, link) {
	switch (dv->type) {
	    case DV_EMPTY:
		break;
	    case DV_EXPR:
		if (output_expr(&dv->data.expn, bufp, bc_data->size, sect, bc,
				0, d))
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
			WRITE_BYTE(*bufp, 0);
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
	num = expr_get_intnum(&incbin->start);
	if (!num)
	    InternalError(_("could not determine start in bc_tobytes_incbin"));
	start = intnum_get_uint(num);
    }

    /* Open file */
    f = fopen(incbin->filename, "rb");
    if (!f) {
	ErrorAt(line, _("`incbin': unable to open file `%s'"),
		incbin->filename);
	return 1;
    }

    /* Seek to start of data */
    if (fseek(f, (long)start, SEEK_SET) < 0) {
	ErrorAt(line, _("`incbin': unable to seek on file `%s'"),
		incbin->filename);
	fclose(f);
	return 1;
    }

    /* Read len bytes */
    if (fread(*bufp, (size_t)len, 1, f) < (size_t)len) {
	ErrorAt(line, _("`incbin': unable to read %lu bytes from file `%s'"),
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
	   const section *sect, void *d, output_expr_func output_expr)
    /*@sets *buf@*/
{
    /*@only@*/ /*@null@*/ unsigned char *mybuf = NULL;
    unsigned char *origbuf, *destbuf;
    /*@dependent@*/ /*@null@*/ const intnum *num;
    bytecode_data *bc_data;
    bytecode_incbin *incbin;
    unsigned long datasize;
    int error = 0;

    if (bc->multiple) {
	num = expr_get_intnum(&bc->multiple);
	if (!num)
	    InternalError(_("could not determine multiple in bc_tobytes"));
	*multiple = intnum_get_uint(num);
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
	    InternalError(_("got empty bytecode in bc_tobytes"));
	case BC_DATA:
	    bc_data = bc_get_data(bc);
	    error = bc_tobytes_data(bc_data, &destbuf, sect, bc, d,
				    output_expr);
	    break;
	case BC_INCBIN:
	    incbin = bc_get_data(bc);
	    error = bc_tobytes_incbin(incbin, &destbuf, bc->len, bc->line);
	    break;
	default:
	    if (bc->type < cur_arch->bc.type_max)
		error = cur_arch->bc.bc_tobytes(bc, &destbuf, sect, d,
						output_expr);
	    else
		InternalError(_("Unknown bytecode type"));
    }

    if (!error && ((unsigned long)(destbuf - origbuf) != datasize))
	InternalError(_("written length does not match optimized length"));
    return mybuf;
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
bcs_print(FILE *f, const bytecodehead *headp)
{
    bytecode *cur;

    STAILQ_FOREACH(cur, headp, link) {
	fprintf(f, "%*sNext Bytecode:\n", indent_level, "");
	indent_level++;
	bc_print(f, cur);
	indent_level--;
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
dvs_print(FILE *f, const datavalhead *head)
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
