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

#include "globals.h"
#include "errwarn.h"
#include "intnum.h"
#include "expr.h"

#include "bytecode.h"

#include "arch.h"

#include "bc-int.h"


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
    immval *im = xmalloc(sizeof(immval));

    im->val = expr_new_ident(ExprInt(intnum_new_int(int_val)));

    if ((int_val & 0xFF) == int_val)
	im->len = 1;
    else if ((int_val & 0xFFFF) == int_val)
	im->len = 2;
    else
	im->len = 4;

    im->isneg = 0;
    im->f_len = 0;
    im->f_sign = 0;

    return im;
}

immval *
imm_new_expr(expr *expr_ptr)
{
    immval *im = xmalloc(sizeof(immval));

    im->val = expr_ptr;
    im->len = 0;
    im->isneg = 0;
    im->f_len = 0;
    im->f_sign = 0;

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

int
bc_calc_len(bytecode *bc, intnum *(*resolve_label) (symrec *sym))
{
    switch (bc->type) {
	case BC_EMPTY:
	    InternalError(_("got empty bytecode in bc_calc_len"));
	case BC_DATA:
	    break;
	case BC_RESERVE:
	    break;
	case BC_INCBIN:
	    break;
	default:
	    if (bc->type < cur_arch->bc.type_max)
		return cur_arch->bc.bc_calc_len(bc, resolve_label);
	    else
		InternalError(_("Unknown bytecode type"));
    }
    return 0;
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

    STAILQ_FOREACH(cur, headp, link)
	if (func(cur, d) == 0)
	    return 0;
    return 1;
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
