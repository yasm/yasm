/* $IdPath$
 * Expression handling
 *
 *  Copyright (C) 2001  Michael Urman, Peter Johnson
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util.h"

#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif

#include "globals.h"
#include "errwarn.h"
#include "floatnum.h"
#include "expr.h"
#include "symrec.h"

RCSID("$IdPath$");

/* allocate a new expression node, with children as defined.
 * If it's a unary operator, put the element on the right */
expr *
expr_new(ExprType ltype,
	 ExprItem left,
	 ExprOp   op,
	 ExprType rtype,
	 ExprItem right)
{
    expr *ptr;
    ptr = xmalloc(sizeof(expr));

    ptr->ltype = ltype;
    ptr->op = op;
    ptr->rtype = rtype;
    switch (ltype) {
	case EXPR_SYM:
	case EXPR_EXPR:
	case EXPR_INT:
	case EXPR_FLOAT:
	    memcpy(&ptr->left, &left, sizeof(ExprItem));
	    break;
	case EXPR_NONE:
	    break;
    }
    switch (rtype) {
	case EXPR_SYM:
	case EXPR_EXPR:
	case EXPR_INT:
	case EXPR_FLOAT:
	    memcpy(&ptr->right, &right, sizeof(ExprItem));
	    break;
	case EXPR_NONE:
	    Fatal(FATAL_UNKNOWN);	/* TODO: better error? */
	    break;
    }
    return ptr;
}

/* helpers */
ExprItem
ExprSym(struct symrec_s *s)
{
    ExprItem e;

    e.sym = s;
    return e;
}

ExprItem
ExprExpr(expr *x)
{
    ExprItem e;
    e.exp = x;

    return e;
}

ExprItem
ExprInt(unsigned long i)
{
    ExprItem e;

    e.int_val = i;
    return e;
}

ExprItem
ExprFloat(floatnum *f)
{
    ExprItem e;

    e.flt = f;
    return e;
}

ExprItem
ExprNone(void)
{
    ExprItem e;

    e.int_val = 0;
    return e;
}

/* get rid of unnecessary branches if possible.  report. */
int
expr_simplify(expr *e)
{
    int simplified = 0;
    unsigned long int_val;

    /* try to simplify the left side */
    if (e->ltype == EXPR_EXPR) {
	/* if the left subexpr isn't an IDENT, recurse simplification */
	if (e->left.exp->op != EXPR_IDENT)
	    simplified |= expr_simplify(e->left.exp);

	/* if the left subexpr is just an IDENT (or string thereof),
	 * pull it up into the current node */
	while (e->ltype == EXPR_EXPR && e->left.exp->op == EXPR_IDENT) {
	    ExprItem tmp;
	    e->ltype = e->left.exp->rtype;
	    memcpy(&tmp, &(e->left.exp->right), sizeof(ExprItem));
	    free(e->left.exp);
	    memcpy(&(e->left.int_val), &tmp, sizeof(ExprItem));
	    simplified = 1;
	}
    } else if (e->ltype == EXPR_SYM) {
	/* try to get value of symbol */
	if (symrec_get_int_value(e->left.sym, &int_val, 0)) {
	    e->ltype = EXPR_INT;
	    /* don't try to free the symrec here. */
	    e->left.int_val = int_val;
	    simplified = 1;
	}
    }

    /* ditto on the right */
    if (e->rtype == EXPR_EXPR) {
	if (e->right.exp->op != EXPR_IDENT)
	    simplified |= expr_simplify(e->right.exp);

	while (e->rtype == EXPR_EXPR && e->right.exp->op == EXPR_IDENT) {
	    ExprItem tmp;
	    e->rtype = e->right.exp->rtype;
	    memcpy(&tmp, &(e->right.exp->right), sizeof(ExprItem));
	    free(e->right.exp);
	    memcpy(&(e->right.int_val), &tmp, sizeof(ExprItem));
	    simplified = 1;
	}
    } else if (e->rtype == EXPR_SYM) {
	if (symrec_get_int_value(e->right.sym, &int_val, 0)) {
	    e->rtype = EXPR_INT;
	    /* don't try to free the symrec here. */
	    e->right.int_val = int_val;
	    simplified = 1;
	}
    }

    if ((e->ltype == EXPR_INT || e->ltype == EXPR_NONE)
	&& e->rtype == EXPR_INT && e->op != EXPR_IDENT) {
	switch (e->op) {
	    case EXPR_ADD:
		e->right.int_val = e->left.int_val + e->right.int_val;
		break;
	    case EXPR_SUB:
		e->right.int_val = e->left.int_val - e->right.int_val;
		break;
	    case EXPR_MUL:
		e->right.int_val = e->left.int_val * e->right.int_val;
		break;
	    case EXPR_DIV:
		e->right.int_val = e->left.int_val / e->right.int_val;
		break;
	    case EXPR_MOD:
		e->right.int_val = e->left.int_val % e->right.int_val;
		break;
	    case EXPR_NEG:
		e->right.int_val = -(e->right.int_val);
		break;
	    case EXPR_NOT:
		e->right.int_val = ~(e->right.int_val);
		break;
	    case EXPR_OR:
		e->right.int_val = e->left.int_val | e->right.int_val;
		break;
	    case EXPR_AND:
		e->right.int_val = e->left.int_val & e->right.int_val;
		break;
	    case EXPR_XOR:
		e->right.int_val = e->left.int_val ^ e->right.int_val;
		break;
	    case EXPR_SHL:
		e->right.int_val = e->right.int_val << e->left.int_val;
		break;
	    case EXPR_SHR:
		e->right.int_val = e->right.int_val << e->left.int_val;
		break;
	    case EXPR_LOR:
		e->right.int_val = e->left.int_val || e->right.int_val;
		break;
	    case EXPR_LAND:
		e->right.int_val = e->left.int_val && e->right.int_val;
		break;
	    case EXPR_LNOT:
		e->right.int_val = !e->right.int_val;
		break;
	    case EXPR_EQ:
		e->right.int_val = e->right.int_val == e->left.int_val;
		break;
	    case EXPR_LT:
		e->right.int_val = e->right.int_val < e->left.int_val;
		break;
	    case EXPR_GT:
		e->right.int_val = e->right.int_val > e->left.int_val;
		break;
	    case EXPR_LE:
		e->right.int_val = e->right.int_val <= e->left.int_val;
		break;
	    case EXPR_GE:
		e->right.int_val = e->right.int_val >= e->left.int_val;
		break;
	    case EXPR_NE:
		e->right.int_val = e->right.int_val != e->left.int_val;
		break;
	    case EXPR_IDENT:
		break;
	}
	e->op = EXPR_IDENT;
	simplified = 1;
    }

    /* catch simple identities like 0+x, 1*x, etc., for x not a num */
    else if (e->ltype == EXPR_INT && ((e->left.int_val == 1 && e->op == EXPR_MUL)
				      || (e->left.int_val == 0 &&
					  e->op == EXPR_ADD)
				      || (e->left.int_val == -1 &&
					  e->op == EXPR_AND)
				      || (e->left.int_val == 0 &&
					  e->op == EXPR_OR))) {
	e->op = EXPR_IDENT;
	simplified = 1;
    }
    /* and the corresponding x+|-0, x*&/1 */
    else if (e->rtype == EXPR_INT && ((e->right.int_val == 1 && e->op == EXPR_MUL)
				      || (e->right.int_val == 1 &&
					  e->op == EXPR_DIV)
				      || (e->right.int_val == 0 &&
					  e->op == EXPR_ADD)
				      || (e->right.int_val == 0 &&
					  e->op == EXPR_SUB)
				      || (e->right.int_val == -1 &&
					  e->op == EXPR_AND)
				      || (e->right.int_val == 0 &&
					  e->op == EXPR_OR)
				      || (e->right.int_val == 0 &&
					  e->op == EXPR_SHL)
				      || (e->right.int_val == 0 &&
					  e->op == EXPR_SHR))) {
	e->op = EXPR_IDENT;
	e->rtype = e->ltype;
	memcpy(&e->right, &e->left, sizeof(ExprItem));
	simplified = 1;
    }

    return simplified;
}

int
expr_get_value(expr *e, unsigned long *retval)
{
    while (!(e->op == EXPR_IDENT && e->rtype == EXPR_INT)
	   && expr_simplify(e)) ;

    if (e->op == EXPR_IDENT && e->rtype == EXPR_INT) {
	*retval = e->right.int_val;
	return 1;
    } else
	return 0;
}

void
expr_print(expr *e)
{
    if (e->op != EXPR_IDENT) {
	switch (e->ltype) {
	    case EXPR_SYM:
		printf("%s", e->left.sym->name);
		break;
	    case EXPR_EXPR:
		printf("(");
		expr_print(e->left.exp);
		printf(")");
		break;
	    case EXPR_INT:
		printf("%lu", e->left.int_val);
		break;
	    case EXPR_FLOAT:
		floatnum_print(e->left.flt);
		break;
	    case EXPR_NONE:
		break;
	}
    }
    switch (e->op) {
	case EXPR_ADD:
	    printf("+");
	    break;
	case EXPR_SUB:
	    printf("-");
	    break;
	case EXPR_MUL:
	    printf("*");
	    break;
	case EXPR_DIV:
	    printf("/");
	    break;
	case EXPR_MOD:
	    printf("%%");
	    break;
	case EXPR_NEG:
	    printf("-");
	    break;
	case EXPR_NOT:
	    printf("~");
	    break;
	case EXPR_OR:
	    printf("|");
	    break;
	case EXPR_AND:
	    printf("&");
	    break;
	case EXPR_XOR:
	    printf("^");
	    break;
	case EXPR_SHL:
	    printf("<<");
	    break;
	case EXPR_SHR:
	    printf(">>");
	    break;
	case EXPR_LOR:
	    printf("||");
	    break;
	case EXPR_LAND:
	    printf("&&");
	    break;
	case EXPR_LNOT:
	    printf("!");
	    break;
	case EXPR_LT:
	    printf("<");
	    break;
	case EXPR_GT:
	    printf(">");
	    break;
	case EXPR_LE:
	    printf("<=");
	    break;
	case EXPR_GE:
	    printf(">=");
	    break;
	case EXPR_NE:
	    printf("!=");
	    break;
	case EXPR_EQ:
	    printf("==");
	    break;
	case EXPR_IDENT:
	    break;
    }
    switch (e->rtype) {
	case EXPR_SYM:
	    printf("%s", e->right.sym->name);
	    break;
	case EXPR_EXPR:
	    printf("(");
	    expr_print(e->right.exp);
	    printf(")");
	    break;
	case EXPR_INT:
	    printf("%lu", e->right.int_val);
	    break;
	case EXPR_FLOAT:
	    floatnum_print(e->right.flt);
	    break;
	case EXPR_NONE:
	    break;
    }
}
