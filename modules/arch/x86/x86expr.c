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

#include <libintl.h>
#define _(String)	gettext(String)
#ifdef gettext_noop
#define N_(String)	gettext_noop(String)
#else
#define N_(String)	(String)
#endif

#include "globals.h"
#include "errwarn.h"
#include "floatnum.h"
#include "expr.h"
#include "symrec.h"

RCSID("$IdPath$");

typedef enum {
    EXPR_NONE,			/* for left side of a NOT, NEG, etc. */
    EXPR_SYM,
    EXPR_EXPR,
    EXPR_INT,
    EXPR_FLOAT
} ExprType;

struct ExprItem {
    ExprType type;
    union {
	symrec *sym;
	expr *expn;
	unsigned long int_val;
	floatnum *flt;
    } data;
};

struct expr {
    ExprItem left, right;
    ExprOp op;
};

/* allocate a new expression node, with children as defined.
 * If it's a unary operator, put the element on the right */
expr *
expr_new(ExprItem *left, ExprOp op, ExprItem *right)
{
    expr *ptr;
    ptr = xmalloc(sizeof(expr));

    ptr->left.type = EXPR_NONE;
    ptr->op = op;
    ptr->right.type = EXPR_NONE;
    if (left) {
	memcpy(&ptr->left, left, sizeof(ExprItem));
	free(left);
    }
    if (right) {
	memcpy(&ptr->right, right, sizeof(ExprItem));
	free(right);
    } else {
	InternalError(__LINE__, __FILE__,
		      _("Right side of expression must exist"));
    }
    return ptr;
}

/* helpers */
ExprItem *
ExprSym(symrec *s)
{
    ExprItem *e = xmalloc(sizeof(ExprItem));
    e->type = EXPR_SYM;
    e->data.sym = s;
    return e;
}

ExprItem *
ExprExpr(expr *x)
{
    ExprItem *e = xmalloc(sizeof(ExprItem));
    e->type = EXPR_EXPR;
    e->data.expn = x;
    return e;
}

ExprItem *
ExprInt(unsigned long i)
{
    ExprItem *e = xmalloc(sizeof(ExprItem));
    e->type = EXPR_INT;
    e->data.int_val = i;
    return e;
}

ExprItem *
ExprFloat(floatnum *f)
{
    ExprItem *e = xmalloc(sizeof(ExprItem));
    e->type = EXPR_FLOAT;
    e->data.flt = f;
    return e;
}

/* get rid of unnecessary branches if possible.  report. */
int
expr_simplify(expr *e)
{
    int simplified = 0;
    unsigned long int_val;

    /* try to simplify the left side */
    if (e->left.type == EXPR_EXPR) {
	/* if the left subexpr isn't an IDENT, recurse simplification */
	if (e->left.data.expn->op != EXPR_IDENT)
	    simplified |= expr_simplify(e->left.data.expn);

	/* if the left subexpr is just an IDENT (or string thereof),
	 * pull it up into the current node */
	while (e->left.type == EXPR_EXPR && e->left.data.expn->op == EXPR_IDENT) {
	    ExprItem tmp;
	    e->left.type = e->left.data.expn->right.type;
	    memcpy(&tmp, &(e->left.data.expn->right), sizeof(ExprItem));
	    free(e->left.data.expn);
	    memcpy(&(e->left.data.int_val), &tmp, sizeof(ExprItem));
	    simplified = 1;
	}
    } else if (e->left.type == EXPR_SYM) {
	/* try to get value of symbol */
	if (symrec_get_int_value(e->left.data.sym, &int_val, 0)) {
	    e->left.type = EXPR_INT;
	    /* don't try to free the symrec here. */
	    e->left.data.int_val = int_val;
	    simplified = 1;
	}
    }

    /* ditto on the right */
    if (e->right.type == EXPR_EXPR) {
	if (e->right.data.expn->op != EXPR_IDENT)
	    simplified |= expr_simplify(e->right.data.expn);

	while (e->right.type == EXPR_EXPR && e->right.data.expn->op == EXPR_IDENT) {
	    ExprItem tmp;
	    e->right.type = e->right.data.expn->right.type;
	    memcpy(&tmp, &(e->right.data.expn->right), sizeof(ExprItem));
	    free(e->right.data.expn);
	    memcpy(&(e->right.data.int_val), &tmp, sizeof(ExprItem));
	    simplified = 1;
	}
    } else if (e->right.type == EXPR_SYM) {
	if (symrec_get_int_value(e->right.data.sym, &int_val, 0)) {
	    e->right.type = EXPR_INT;
	    /* don't try to free the symrec here. */
	    e->right.data.int_val = int_val;
	    simplified = 1;
	}
    }

    if ((e->left.type == EXPR_INT || e->left.type == EXPR_NONE)
	&& e->right.type == EXPR_INT && e->op != EXPR_IDENT) {
	switch (e->op) {
	    case EXPR_ADD:
		e->right.data.int_val = e->left.data.int_val + e->right.data.int_val;
		break;
	    case EXPR_SUB:
		e->right.data.int_val = e->left.data.int_val - e->right.data.int_val;
		break;
	    case EXPR_MUL:
		e->right.data.int_val = e->left.data.int_val * e->right.data.int_val;
		break;
	    case EXPR_DIV:
		e->right.data.int_val = e->left.data.int_val / e->right.data.int_val;
		break;
	    case EXPR_MOD:
		e->right.data.int_val = e->left.data.int_val % e->right.data.int_val;
		break;
	    case EXPR_NEG:
		e->right.data.int_val = -(e->right.data.int_val);
		break;
	    case EXPR_NOT:
		e->right.data.int_val = ~(e->right.data.int_val);
		break;
	    case EXPR_OR:
		e->right.data.int_val = e->left.data.int_val | e->right.data.int_val;
		break;
	    case EXPR_AND:
		e->right.data.int_val = e->left.data.int_val & e->right.data.int_val;
		break;
	    case EXPR_XOR:
		e->right.data.int_val = e->left.data.int_val ^ e->right.data.int_val;
		break;
	    case EXPR_SHL:
		e->right.data.int_val = e->right.data.int_val << e->left.data.int_val;
		break;
	    case EXPR_SHR:
		e->right.data.int_val = e->right.data.int_val << e->left.data.int_val;
		break;
	    case EXPR_LOR:
		e->right.data.int_val = e->left.data.int_val || e->right.data.int_val;
		break;
	    case EXPR_LAND:
		e->right.data.int_val = e->left.data.int_val && e->right.data.int_val;
		break;
	    case EXPR_LNOT:
		e->right.data.int_val = !e->right.data.int_val;
		break;
	    case EXPR_EQ:
		e->right.data.int_val = e->right.data.int_val == e->left.data.int_val;
		break;
	    case EXPR_LT:
		e->right.data.int_val = e->right.data.int_val < e->left.data.int_val;
		break;
	    case EXPR_GT:
		e->right.data.int_val = e->right.data.int_val > e->left.data.int_val;
		break;
	    case EXPR_LE:
		e->right.data.int_val = e->right.data.int_val <= e->left.data.int_val;
		break;
	    case EXPR_GE:
		e->right.data.int_val = e->right.data.int_val >= e->left.data.int_val;
		break;
	    case EXPR_NE:
		e->right.data.int_val = e->right.data.int_val != e->left.data.int_val;
		break;
	    case EXPR_IDENT:
		break;
	}
	e->op = EXPR_IDENT;
	simplified = 1;
    }

    /* catch simple identities like 0+x, 1*x, etc., for x not a num */
    else if (e->left.type == EXPR_INT && ((e->left.data.int_val == 1 && e->op == EXPR_MUL)
				      || (e->left.data.int_val == 0 &&
					  e->op == EXPR_ADD)
				      || (e->left.data.int_val == -1 &&
					  e->op == EXPR_AND)
				      || (e->left.data.int_val == 0 &&
					  e->op == EXPR_OR))) {
	e->op = EXPR_IDENT;
	simplified = 1;
    }
    /* and the corresponding x+|-0, x*&/1 */
    else if (e->right.type == EXPR_INT && ((e->right.data.int_val == 1 && e->op == EXPR_MUL)
				      || (e->right.data.int_val == 1 &&
					  e->op == EXPR_DIV)
				      || (e->right.data.int_val == 0 &&
					  e->op == EXPR_ADD)
				      || (e->right.data.int_val == 0 &&
					  e->op == EXPR_SUB)
				      || (e->right.data.int_val == -1 &&
					  e->op == EXPR_AND)
				      || (e->right.data.int_val == 0 &&
					  e->op == EXPR_OR)
				      || (e->right.data.int_val == 0 &&
					  e->op == EXPR_SHL)
				      || (e->right.data.int_val == 0 &&
					  e->op == EXPR_SHR))) {
	e->op = EXPR_IDENT;
	e->right.type = e->left.type;
	memcpy(&e->right, &e->left, sizeof(ExprItem));
	simplified = 1;
    }

    return simplified;
}

int
expr_get_value(expr *e, unsigned long *retval)
{
    while (!(e->op == EXPR_IDENT && e->right.type == EXPR_INT)
	   && expr_simplify(e)) ;

    if (e->op == EXPR_IDENT && e->right.type == EXPR_INT) {
	*retval = e->right.data.int_val;
	return 1;
    } else
	return 0;
}

void
expr_print(expr *e)
{
    if (e->op != EXPR_IDENT) {
	switch (e->left.type) {
	    case EXPR_SYM:
		printf("%s", symrec_get_name(e->left.data.sym));
		break;
	    case EXPR_EXPR:
		printf("(");
		expr_print(e->left.data.expn);
		printf(")");
		break;
	    case EXPR_INT:
		printf("%lu", e->left.data.int_val);
		break;
	    case EXPR_FLOAT:
		floatnum_print(e->left.data.flt);
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
    switch (e->right.type) {
	case EXPR_SYM:
	    printf("%s", symrec_get_name(e->right.data.sym));
	    break;
	case EXPR_EXPR:
	    printf("(");
	    expr_print(e->right.data.expn);
	    printf(")");
	    break;
	case EXPR_INT:
	    printf("%lu", e->right.data.int_val);
	    break;
	case EXPR_FLOAT:
	    floatnum_print(e->right.data.flt);
	    break;
	case EXPR_NONE:
	    break;
    }
}
