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
RCSID("$IdPath$");

#include "globals.h"
#include "errwarn.h"
#include "intnum.h"
#include "expr.h"

#include "bytecode.h"

#include "arch.h"

#include "bc-int.h"


struct dataval {
    STAILQ_ENTRY(dataval) link;

    enum { DV_EMPTY, DV_EXPR, DV_STRING } type;

    union {
	expr *expn;
	char *str_val;
    } data;
};

typedef struct bytecode_data {
    /* non-converted data (linked list) */
    datavalhead datahead;

    /* final (converted) size of each element (in bytes) */
    unsigned char size;
} bytecode_data;

typedef struct bytecode_reserve {
    expr *numitems;		/* number of items to reserve */
    unsigned char itemsize;	/* size of each item (in bytes) */
} bytecode_reserve;

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

    return im;
}

immval *
imm_new_expr(expr *expr_ptr)
{
    immval *im = xmalloc(sizeof(immval));

    im->val = expr_ptr;
    im->len = 0;
    im->isneg = 0;

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

    bc->filename = in_filename;
    bc->lineno = line_number;

    bc->offset = 0;

    return bc;
}

bytecode *
bc_new_data(datavalhead *datahead, unsigned long size)
{
    bytecode *bc = bc_new_common(BC_DATA, sizeof(bytecode_data));
    bytecode_data *data = bc_get_data(bc);

    data->datahead = *datahead;
    data->size = size;

    return bc;
}

bytecode *
bc_new_reserve(expr *numitems, unsigned long itemsize)
{
    bytecode *bc = bc_new_common(BC_RESERVE, sizeof(bytecode_reserve));
    bytecode_reserve *reserve = bc_get_data(bc);

    reserve->numitems = numitems;
    reserve->itemsize = itemsize;

    return bc;
}

void
bc_delete(bytecode *bc)
{
    bytecode_data *data;
    bytecode_reserve *reserve;

    if (!bc)
	return;

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
	default:
	    if (bc->type < cur_arch->bc.type_max)
		cur_arch->bc.bc_delete(bc);
	    else
		InternalError(_("Unknown bytecode type"));
	    break;
    }

    expr_delete(bc->multiple);
    xfree(bc);
}

int
bc_get_offset(section *sect, bytecode *bc, unsigned long *ret_val)
{
    return 0;	/* TODO */
}

void
bc_print(const bytecode *bc)
{
    const bytecode_data *data;
    const bytecode_reserve *reserve;

    switch (bc->type) {
	case BC_EMPTY:
	    printf("_Empty_\n");
	    break;
	case BC_DATA:
	    data = bc_get_const_data(bc);
	    printf("_Data_\n");
	    printf("Final Element Size=%u\n",
		   (unsigned int)data->size);
	    printf("Elements:\n");
	    dvs_print(&data->datahead);
	    break;
	case BC_RESERVE:
	    reserve = bc_get_const_data(bc);
	    printf("_Reserve_\n");
	    printf("Num Items=");
	    expr_print(reserve->numitems);
	    printf("\nItem Size=%u\n",
		   (unsigned int)reserve->itemsize);
	    break;
	default:
	    if (bc->type < cur_arch->bc.type_max)
		cur_arch->bc.bc_print(bc);
	    else
		printf("_Unknown_\n");
	    break;
    }
    printf("Multiple=");
    if (!bc->multiple)
	printf("nil (1)");
    else
	expr_print(bc->multiple);
    printf("\n");
    printf("Length=%lu\n", bc->len);
    printf("Filename=\"%s\" Line Number=%u\n",
	   bc->filename ? bc->filename : "<UNKNOWN>", bc->lineno);
    printf("Offset=%lx\n", bc->offset);
}

void
bc_parser_finalize(bytecode *bc)
{
    switch (bc->type) {
	case BC_EMPTY:
	    /* FIXME: delete it (probably in bytecodes_ level, not here */
	    InternalError(_("got empty bytecode in parser_finalize"));
	    break;
	default:
	    if (bc->type < cur_arch->bc.type_max)
		cur_arch->bc.bc_parser_finalize(bc);
	    else
		InternalError(_("Unknown bytecode type"));
	    break;
    }
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
bcs_print(const bytecodehead *headp)
{
    bytecode *cur;

    STAILQ_FOREACH(cur, headp, link) {
	printf("---Next Bytecode---\n");
	bc_print(cur);
    }
}

void
bcs_parser_finalize(bytecodehead *headp)
{
    bytecode *cur;

    STAILQ_FOREACH(cur, headp, link)
	bc_parser_finalize(cur);
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
	if (cur->type == DV_EXPR)
	    expr_delete(cur->data.expn);
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
dvs_print(const datavalhead *head)
{
    dataval *cur;

    STAILQ_FOREACH(cur, head, link) {
	switch (cur->type) {
	    case DV_EMPTY:
		printf(" Empty\n");
		break;
	    case DV_EXPR:
		printf(" Expr=");
		expr_print(cur->data.expn);
		printf("\n");
		break;
	    case DV_STRING:
		printf(" String=%s\n", cur->data.str_val);
		break;
	}
    }
}
