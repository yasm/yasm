/* $Id: x86expr.c,v 1.4 2001/07/11 23:16:50 peter Exp $
 * Expression handling
 *
 *  Copyright (C) 2001  Michael Urman
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "expr.h"
#include "symrec.h"
#include "globals.h"
#include "errwarn.h"

/* allocate a new expression node, with children as defined.
 * If it's a unary operator, put the element on the right */
expr *
expr_new (ExprType ltype,
	  ExprItem left,
	  ExprOp   op,
	  ExprType rtype,
	  ExprItem right)
{
    expr *ptr;
    ptr = malloc (sizeof (expr));
    if (ptr == NULL) Fatal (FATAL_NOMEM);
    ptr->ltype = ltype;
    ptr->op = op;
    ptr->rtype = rtype;
    switch (ltype)
    {
	case EXPR_NUM:
	case EXPR_SYM:
	case EXPR_EXPR:
	    memcpy (&ptr->left, &left, sizeof (ExprItem));
	    break;
	case EXPR_NONE: break;
    }
    switch (rtype)
    {
	case EXPR_NUM:
	case EXPR_SYM:
	case EXPR_EXPR:
	    memcpy (&ptr->right, &right, sizeof (ExprItem));
	    break;
	case EXPR_NONE:
	    Fatal (FATAL_UNKNOWN); /* TODO: better error? */
	    break;
    }
    return ptr;
}

/* helpers */
ExprItem
ExprSym (struct symrec_s *s)
{
    ExprItem e;
    e.sym = s;
    return e;
}

ExprItem
ExprExpr (expr *x)
{
    ExprItem e;
    e.expr = x;
    return e;
}

ExprItem
ExprNum (unsigned long n)
{
    ExprItem e;
    e.num = n;
    return e;
}

ExprItem
ExprNone (void)
{
    ExprItem e;
    e.num = 0;
    return e;
}

/* get rid of unnecessary branches if possible.  report. */
int
expr_simplify (expr *e)
{
    int simplified = 0;

    /* try to simplify the left side */
    if (e->ltype == EXPR_EXPR)
    {
	/* if the left subexpr isn't an IDENT, recurse simplification */
	if (e->left.expr->op != EXPR_IDENT)
	    simplified |= expr_simplify (e->left.expr);

	/* if the left subexpr is just an IDENT (or string thereof),
	 * pull it up into the current node */
	while (e->ltype == EXPR_EXPR && e->left.expr->op == EXPR_IDENT)
	{
	    ExprItem tmp;
	    e->ltype = e->left.expr->rtype;
	    memcpy (&tmp, &(e->left.expr->right), sizeof (ExprItem));
	    free (e->left.expr);
	    memcpy (&(e->left.num), &tmp, sizeof (ExprItem));
	    simplified = 1;
	}
    }
    else if (e->ltype == EXPR_SYM)
    {
	/* if it's a symbol that has a defined value, turn it into a
	 * number */
	if (e->left.sym->status & SYM_VALUED)
	{
	    e->ltype = EXPR_NUM;
	    /* don't try to free the symrec here. */
	    e->left.num = e->left.sym->value;
	    simplified = 1;
	}
    }

    /* ditto on the right */
    if (e->rtype == EXPR_EXPR)
    {
	if (e->right.expr->op != EXPR_IDENT)
	    simplified |= expr_simplify (e->right.expr);

	while (e->rtype == EXPR_EXPR && e->right.expr->op == EXPR_IDENT)
	{
	    ExprItem tmp;
	    e->rtype = e->right.expr->rtype;
	    memcpy (&tmp, &(e->right.expr->right), sizeof (ExprItem));
	    free (e->right.expr);
	    memcpy (&(e->right.num), &tmp, sizeof (ExprItem));
	    simplified = 1;
	}
    }
    else if (e->rtype == EXPR_SYM)
    {
	if (e->right.sym->status & SYM_VALUED)
	{
	    e->rtype = EXPR_NUM;
	    /* don't try to free the symrec here. */
	    e->right.num = e->right.sym->value;
	    simplified = 1;
	}
    }

    if ((e->ltype == EXPR_NUM || e->ltype == EXPR_NONE)
	&& e->rtype == EXPR_NUM
	&& e->op != EXPR_IDENT)
    {
	switch (e->op)
	{
	    case EXPR_ADD: e->right.num = e->left.num + e->right.num; break;
	    case EXPR_SUB: e->right.num = e->left.num - e->right.num; break;
	    case EXPR_MUL: e->right.num = e->left.num * e->right.num; break;
	    case EXPR_DIV: e->right.num = e->left.num / e->right.num; break;
	    case EXPR_MOD: e->right.num = e->left.num % e->right.num; break;
	    case EXPR_NEG: e->right.num = -(e->right.num); break;
	    case EXPR_NOT: e->right.num = ~(e->right.num); break;
	    case EXPR_OR:  e->right.num = e->left.num | e->right.num; break;
	    case EXPR_AND: e->right.num = e->left.num & e->right.num; break;
	    case EXPR_XOR: e->right.num = e->left.num ^ e->right.num; break;
	    case EXPR_SHL: e->right.num = e->right.num << e->left.num; break;
	    case EXPR_SHR: e->right.num = e->right.num << e->left.num; break;
	    case EXPR_LOR: e->right.num = e->left.num || e->right.num; break;
	    case EXPR_LAND: e->right.num = e->left.num && e->right.num; break;
	    case EXPR_LNOT: e->right.num = !e->right.num; break;
	    case EXPR_EQ: e->right.num = e->right.num == e->left.num; break;
	    case EXPR_LT: e->right.num = e->right.num < e->left.num; break;
	    case EXPR_GT: e->right.num = e->right.num > e->left.num; break;
	    case EXPR_LE: e->right.num = e->right.num <= e->left.num; break;
	    case EXPR_GE: e->right.num = e->right.num >= e->left.num; break;
	    case EXPR_NE: e->right.num = e->right.num != e->left.num; break;
	    case EXPR_IDENT: break;
	}
	e->op = EXPR_IDENT;
	simplified = 1;
    }

    /* catch simple identities like 0+x, 1*x, etc., for x not a num */
    else if (e->ltype == EXPR_NUM
	     && ((e->left.num == 1 && e->op == EXPR_MUL)
		 ||(e->left.num == 0 && e->op == EXPR_ADD)
		 ||(e->left.num == -1 && e->op == EXPR_AND)
		 ||(e->left.num == 0 && e->op == EXPR_OR)))
    {
	e->op = EXPR_IDENT;
	simplified = 1;
    }
    /* and the corresponding x+|-0, x*&/1 */
    else if (e->rtype == EXPR_NUM
	     && ((e->right.num == 1 && e->op == EXPR_MUL)
		 ||(e->right.num == 1 && e->op == EXPR_DIV)
		 ||(e->right.num == 0 && e->op == EXPR_ADD)
		 ||(e->right.num == 0 && e->op == EXPR_SUB)
		 ||(e->right.num == -1 && e->op == EXPR_AND)
		 ||(e->right.num == 0 && e->op == EXPR_OR)
		 ||(e->right.num == 0 && e->op == EXPR_SHL)
		 ||(e->right.num == 0 && e->op == EXPR_SHR)))
    {
	e->op = EXPR_IDENT;
	e->rtype = e->ltype;
	memcpy (&e->right, &e->left, sizeof (ExprItem));
	simplified = 1;
    }

    return simplified;
}

int
expr_get_value (expr *e, unsigned long *retval)
{
    while (!(e->op == EXPR_IDENT && e->rtype == EXPR_NUM)
	   && expr_simplify (e));

    if (e->op == EXPR_IDENT && e->rtype == EXPR_NUM)
    {
	*retval = e->right.num;
	return 1;
    }
    else
	return 0;
}

void
expr_print (expr *e)
{
    if (e->op != EXPR_IDENT) {
	switch (e->ltype) {
	    case EXPR_NUM: printf ("%lu", e->left.num); break;
	    case EXPR_SYM: printf ("%s", e->left.sym->name); break;
	    case EXPR_EXPR: printf ("("); expr_print (e->left.expr); printf(")");
	    case EXPR_NONE: break;
	}
    }
    switch (e->op) {
	case EXPR_ADD: printf ("+"); break;
	case EXPR_SUB: printf ("-"); break;
	case EXPR_MUL: printf ("*"); break;
	case EXPR_DIV: printf ("/"); break;
	case EXPR_MOD: printf ("%%"); break;
	case EXPR_NEG: printf ("-"); break;
	case EXPR_NOT: printf ("~"); break;
	case EXPR_OR: printf ("|"); break;
	case EXPR_AND: printf ("&"); break;
	case EXPR_XOR: printf ("^"); break;
	case EXPR_SHL: printf ("<<"); break;
	case EXPR_SHR: printf (">>"); break;
	case EXPR_LOR: printf ("||"); break;
	case EXPR_LAND: printf ("&&"); break;
	case EXPR_LNOT: printf ("!"); break;
	case EXPR_LT: printf ("<"); break;
	case EXPR_GT: printf (">"); break;
	case EXPR_LE: printf ("<="); break;
	case EXPR_GE: printf (">="); break;
	case EXPR_NE: printf ("!="); break;
	case EXPR_EQ: printf ("=="); break;
	case EXPR_IDENT: break;
    }
    switch (e->rtype) {
	case EXPR_NUM: printf ("%lu", e->right.num); break;
	case EXPR_SYM: printf ("%s", e->right.sym->name); break;
	case EXPR_EXPR: printf ("("); expr_print (e->right.expr); printf(")");
	case EXPR_NONE: break;
    }
}
