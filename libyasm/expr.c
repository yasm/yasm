/*
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
#include "util.h"
RCSID("$IdPath$");

#include "bitvect.h"

#include "globals.h"
#include "errwarn.h"
#include "intnum.h"
#include "floatnum.h"
#include "expr.h"
#include "symrec.h"

#include "expr-int.h"


static int expr_traverse_nodes_post(expr *e, void *d,
				    int (*func) (expr *e, void *d));

/* allocate a new expression node, with children as defined.
 * If it's a unary operator, put the element in left and set right=NULL. */
expr *
expr_new(ExprOp op, ExprItem *left, ExprItem *right)
{
    expr *ptr;
    ptr = xmalloc(sizeof(expr));

    ptr->op = op;
    ptr->numterms = 0;
    ptr->terms[0].type = EXPR_NONE;
    ptr->terms[1].type = EXPR_NONE;
    if (left) {
	ptr->terms[0] = *left;	/* structure copy */
	xfree(left);
	ptr->numterms++;

	/* Search downward until we find something *other* than an
	 * IDENT, then bring it up to the current level.
	 */
	while (ptr->terms[0].type == EXPR_EXPR &&
	       ptr->terms[0].data.expn->op == EXPR_IDENT) {
	    expr *sube = ptr->terms[0].data.expn;
	    ptr->terms[0] = sube->terms[0];	/* structure copy */
	    xfree(sube);
	}
    } else {
	InternalError(_("Right side of expression must exist"));
    }

    if (right) {
	ptr->terms[1] = *right;	/* structure copy */
	xfree(right);
	ptr->numterms++;

	/* Search downward until we find something *other* than an
	 * IDENT, then bring it up to the current level.
	 */
	while (ptr->terms[1].type == EXPR_EXPR &&
	       ptr->terms[1].data.expn->op == EXPR_IDENT) {
	    expr *sube = ptr->terms[1].data.expn;
	    ptr->terms[1] = sube->terms[0];	/* structure copy */
	    xfree(sube);
	}
    }

    ptr->filename = in_filename;
    ptr->line = line_number;

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
ExprInt(intnum *i)
{
    ExprItem *e = xmalloc(sizeof(ExprItem));
    e->type = EXPR_INT;
    e->data.intn = i;
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

ExprItem *
ExprReg(unsigned char reg, unsigned char size)
{
    ExprItem *e = xmalloc(sizeof(ExprItem));
    e->type = EXPR_REG;
    e->data.reg.num = reg;
    e->data.reg.size = size;
    return e;
}

/* Negate just a single ExprItem by building a -1*ei subexpression */
static void
expr_xform_neg_item(expr *e, ExprItem *ei)
{
    expr *sube = xmalloc(sizeof(expr));

    /* Build -1*ei subexpression */
    sube->op = EXPR_MUL;
    sube->filename = e->filename;
    sube->line = e->line;
    sube->numterms = 2;
    sube->terms[0].type = EXPR_INT;
    sube->terms[0].data.intn = intnum_new_int(-1);
    sube->terms[1] = *ei;	/* structure copy */

    /* Replace original ExprItem with subexp */
    ei->type = EXPR_EXPR;
    ei->data.expn = sube;
}

/* Negates e by multiplying by -1, with distribution over lower-precedence
 * operators (eg ADD) and special handling to simplify result w/ADD, NEG, and
 * others.
 *
 * Returns a possibly reallocated e.
 */
static expr *
expr_xform_neg_helper(expr *e)
{
    expr *ne;
    int i;

    switch (e->op) {
	case EXPR_ADD:
	    /* distribute (recursively if expr) over terms */
	    for (i=0; i<e->numterms; i++) {
		if (e->terms[i].type == EXPR_EXPR)
		    e->terms[i].data.expn =
			expr_xform_neg_helper(e->terms[i].data.expn);
		else
		    expr_xform_neg_item(e, &e->terms[i]);
	    }
	    break;
	case EXPR_SUB:
	    /* change op to ADD, and recursively negate left side (if expr) */
	    e->op = EXPR_ADD;
	    if (e->terms[0].type == EXPR_EXPR)
		e->terms[0].data.expn =
		    expr_xform_neg_helper(e->terms[0].data.expn);
	    else
		expr_xform_neg_item(e, &e->terms[0]);
	    break;
	case EXPR_NEG:
	    /* Negating a negated value?  Make it an IDENT. */
	    e->op = EXPR_IDENT;
	    break;
	case EXPR_IDENT:
	    /* Negating an ident?  Change it into a MUL w/ -1. */
	    e->op = EXPR_MUL;
	    e->numterms = 2;
	    e->terms[1].type = EXPR_INT;
	    e->terms[1].data.intn = intnum_new_int(-1);
	    break;
	default:
	    /* Everything else.  MUL will be combined when it's leveled.
	     * Make a new expr (to replace e) with -1*e.
	     */
	    ne = xmalloc(sizeof(expr));
	    ne->op = EXPR_MUL;
	    ne->filename = e->filename;
	    ne->line = e->line;
	    ne->numterms = 2;
	    ne->terms[0].type = EXPR_INT;
	    ne->terms[0].data.intn = intnum_new_int(-1);
	    ne->terms[1].type = EXPR_EXPR;
	    ne->terms[1].data.expn = e;
	    return ne;
    }
    return e;
}

/* Transforms negatives into expressions that are easier to combine:
 * -x -> -1*x
 * a-b -> a+(-1*b)
 *
 * Call post-order on an expression tree to transform the entire tree.
 *
 * Returns a possibly reallocated e.
 */
static expr *
expr_xform_neg(expr *e)
{
    switch (e->op) {
	case EXPR_NEG:
	    /* Turn -x into -1*x */
	    e->op = EXPR_IDENT;
	    return expr_xform_neg_helper(e);
	case EXPR_SUB:
	    /* Turn a-b into a+(-1*b) */

	    /* change op to ADD, and recursively negate right side (if expr) */
	    e->op = EXPR_ADD;
	    if (e->terms[1].type == EXPR_EXPR)
		e->terms[1].data.expn =
		    expr_xform_neg_helper(e->terms[1].data.expn);
	    else
		expr_xform_neg_item(e, &e->terms[1]);
	    break;
	default:
	    break;
    }

    return e;
}

/* Transform negatives throughout an entire expn tree */
expr *
expr_xform_neg_tree(expr *e)
{
    int i;

    if (!e)
	return 0;

    /* traverse terms */
    for (i=0; i<e->numterms; i++) {
	if (e->terms[i].type == EXPR_EXPR)
	    e->terms[i].data.expn = expr_xform_neg_tree(e->terms[i].data.expn);
    }

    /* do callback */
    return expr_xform_neg(e);
}

/* Look for simple identities that make the entire result constant:
 * 0*&x, -1|x, etc.
 */
static int
expr_is_constant(ExprOp op, intnum *intn)
{
    return ((intnum_is_zero(intn) && op == EXPR_MUL) ||
	    (intnum_is_zero(intn) && op == EXPR_AND) ||
	    (intnum_is_neg1(intn) && op == EXPR_OR));
}

/* Look for simple "left" identities like 0+x, 1*x, etc. */
static int
expr_can_delete_int_left(ExprOp op, intnum *intn)
{
    return ((intnum_is_pos1(intn) && op == EXPR_MUL) ||
	    (intnum_is_zero(intn) && op == EXPR_ADD) ||
	    (intnum_is_neg1(intn) && op == EXPR_AND) ||
	    (intnum_is_zero(intn) && op == EXPR_OR));
}

/* Look for simple "right" identities like x+|-0, x*&/1 */
static int
expr_can_delete_int_right(ExprOp op, intnum *intn)
{
    return ((intnum_is_pos1(intn) && op == EXPR_MUL) ||
	    (intnum_is_pos1(intn) && op == EXPR_DIV) ||
	    (intnum_is_zero(intn) && op == EXPR_ADD) ||
	    (intnum_is_zero(intn) && op == EXPR_SUB) ||
	    (intnum_is_neg1(intn) && op == EXPR_AND) ||
	    (intnum_is_zero(intn) && op == EXPR_OR) ||
	    (intnum_is_zero(intn) && op == EXPR_SHL) ||
	    (intnum_is_zero(intn) && op == EXPR_SHR));
}

/* Check for and simplify identities.  Returns new number of expr terms.
 * Sets e->op = EXPR_IDENT if numterms ends up being 1.
 * Uses numterms parameter instead of e->numterms for basis of "new" number
 * of terms.
 * Assumes int_term is *only* integer term in e.
 * NOTE: Really designed to only be used by expr_level_op().
 */
static int
expr_simplify_identity(expr *e, int numterms, int int_term)
{
    int i;

    /* Check for simple identities that delete the intnum.
     * Don't delete if the intnum is the only thing in the expn.
     */
    if ((int_term == 0 && numterms > 1 &&
	 expr_can_delete_int_left(e->op, e->terms[0].data.intn)) ||
	(int_term > 0 &&
	 expr_can_delete_int_right(e->op, e->terms[int_term].data.intn))) {
	/* Delete the intnum */
	intnum_delete(e->terms[int_term].data.intn);

	/* Slide everything to its right over by 1 */
	if (int_term != numterms-1) /* if it wasn't last.. */
	    memmove(&e->terms[int_term], &e->terms[int_term+1],
		    (numterms-1-int_term)*sizeof(ExprItem));

	/* Update numterms */
	numterms--;
    }

    /* Check for simple identites that delete everything BUT the intnum.
     * Don't bother if the intnum is the only thing in the expn.
     */
    if (numterms > 1 &&
	expr_is_constant(e->op, e->terms[int_term].data.intn)) {
	/* Loop through, deleting everything but the integer term */
	for (i=0; i<e->numterms; i++)
	    if (i != int_term)
		switch (e->terms[i].type) {
		    case EXPR_INT:
			intnum_delete(e->terms[i].data.intn);
			break;
		    case EXPR_FLOAT:
			floatnum_delete(e->terms[i].data.flt);
			break;
		    case EXPR_EXPR:
			expr_delete(e->terms[i].data.expn);
			break;
		    default:
			break;
		}

	/* Move integer term to the first term (if not already there) */
	if (int_term != 0)
	    e->terms[0] = e->terms[int_term];	/* structure copy */

	/* Set numterms to 1 */
	numterms = 1;
    }

    /* Change expression to IDENT if possible. */
    if (numterms == 1)
	e->op = EXPR_IDENT;

    /* Return the updated numterms */
    return numterms;
}

/* Levels the expression tree starting at e.  Eg:
 * a+(b+c) -> a+b+c
 * (a+b)+(c+d) -> a+b+c+d
 * Naturally, only levels operators that allow more than two operand terms.
 * NOTE: only does *one* level of leveling (no recursion).  Should be called
 *  post-order on a tree to combine deeper levels.
 * Also brings up any IDENT values into the current level (for ALL operators).
 * Folds (combines by evaluation) *integer* constant values if fold_const != 0.
 *
 * Returns a possibly reallocated e.
 */
static expr *
expr_level_op(expr *e, int fold_const, int simplify_ident)
{
    int i, j, o, fold_numterms, level_numterms, level_fold_numterms;
    int first_int_term = -1;

    /* Determine how many operands will need to be brought up (for leveling).
     * Go ahead and bring up any IDENT'ed values.
     */
    level_numterms = e->numterms;
    level_fold_numterms = 0;
    for (i=0; i<e->numterms; i++) {
	/* Search downward until we find something *other* than an
	 * IDENT, then bring it up to the current level.
	 */
	while (e->terms[i].type == EXPR_EXPR &&
	       e->terms[i].data.expn->op == EXPR_IDENT) {
	    expr *sube = e->terms[i].data.expn;
	    e->terms[i] = sube->terms[0];
	    xfree(sube);
	}

	if (e->terms[i].type == EXPR_EXPR &&
	    e->terms[i].data.expn->op == e->op) {
		/* It's an expression w/the same operator, add in its numterms.
		 * But don't forget to subtract one for the expr itself!
		 */
		level_numterms += e->terms[i].data.expn->numterms - 1;

		/* If we're folding constants, count up the number of constants
		 * that will be merged in.
		 */
		if (fold_const)
		    for (j=0; j<e->terms[i].data.expn->numterms; j++)
			if (e->terms[i].data.expn->terms[j].type == EXPR_INT)
			    level_fold_numterms++;
	}

	/* Find the first integer term (if one is present) if we're folding
	 * constants.
	 */
	if (fold_const && first_int_term == -1 && e->terms[i].type == EXPR_INT)
	    first_int_term = i;
    }

    /* Look for other integer terms if there's one and combine.
     * Also eliminate empty spaces when combining and adjust numterms
     * variables.
     */
    fold_numterms = e->numterms;
    if (first_int_term != -1) {
	for (i=first_int_term+1, o=first_int_term+1; i<e->numterms; i++) {
	    if (e->terms[i].type == EXPR_INT) {
		intnum_calc(e->terms[first_int_term].data.intn, e->op,
			    e->terms[i].data.intn);
		fold_numterms--;
		level_numterms--;
		/* make sure to delete folded intnum */
		intnum_delete(e->terms[i].data.intn);
	    } else if (o != i) {
		/* copy term if it changed places */
		e->terms[o++] = e->terms[i];
	    }
	}

	if (simplify_ident)
	    /* Simplify identities and make IDENT if possible. */
	    fold_numterms = expr_simplify_identity(e, fold_numterms,
						   first_int_term);
	else if (fold_numterms == 1)
	    e->op = EXPR_IDENT;
    }

    /* Only level operators that allow more than two operand terms.
     * Also don't bother leveling if it's not necessary to bring up any terms.
     */
    if ((e->op != EXPR_ADD && e->op != EXPR_MUL && e->op != EXPR_OR &&
	 e->op != EXPR_AND && e->op != EXPR_XOR) ||
	level_numterms <= fold_numterms) {
	/* Downsize e if necessary */
	if (fold_numterms < e->numterms && e->numterms > 2)
	    e = xrealloc(e, sizeof(expr)+((fold_numterms<2) ? 0 :
			 sizeof(ExprItem)*(fold_numterms-2)));
	/* Update numterms */
	e->numterms = fold_numterms;
	return e;
    }

    /* Adjust numterms for constant folding from terms being "pulled up".
     * Careful: if there's no integer term in e, then save space for it.
     */
    if (fold_const) {
	level_numterms -= level_fold_numterms;
	if (first_int_term == -1 && level_fold_numterms != 0)
	    level_numterms++;
    }

    /* Alloc more (or conceivably less, but not usually) space for e */
    e = xrealloc(e, sizeof(expr)+((level_numterms<2) ? 0 :
		 sizeof(ExprItem)*(level_numterms-2)));

    /* Copy up ExprItem's.  Iterate from right to left to keep the same
     * ordering as was present originally.
     * Combine integer terms as necessary.
     */
    for (i=e->numterms-1, o=level_numterms-1; i>=0; i--) {
	if (e->terms[i].type == EXPR_EXPR &&
	    e->terms[i].data.expn->op == e->op) {
	    /* bring up subexpression */
	    expr *sube = e->terms[i].data.expn;

	    /* copy terms right to left */
	    for (j=sube->numterms-1; j>=0; j--) {
		if (fold_const && sube->terms[j].type == EXPR_INT) {
		    /* Need to fold it in.. but if there's no int term already,
		     * just copy into a new one.
		     */
		    if (first_int_term == -1) {
			first_int_term = o--;
			e->terms[first_int_term] = sube->terms[j];  /* struc */
		    } else {
			intnum_calc(e->terms[first_int_term].data.intn, e->op,
				    sube->terms[j].data.intn);
			/* make sure to delete folded intnum */
			intnum_delete(sube->terms[j].data.intn);
		    }
		} else {
		    if (o == first_int_term)
			o--;
		    e->terms[o--] = sube->terms[j];	/* structure copy */
		}
	    }

	    /* delete subexpression, but *don't delete nodes* (as we've just
	     * copied them!)
	     */
	    xfree(sube);
	} else if (o != i) {
	    /* copy operand if it changed places */
	    if (o == first_int_term)
		o--;
	    e->terms[o] = e->terms[i];
	    /* If we moved the first_int_term, change first_int_num too */
	    if (i == first_int_term)
		first_int_term = o;
	    o--;
	}
    }

    /* Simplify identities, make IDENT if possible, and save to e->numterms. */
    if (simplify_ident && first_int_term != -1) {
	e->numterms = expr_simplify_identity(e, level_numterms,
					     first_int_term);
    } else {
	e->numterms = level_numterms;
	if (level_numterms == 1)
	    e->op = EXPR_IDENT;
    }

    return e;
}

/* Level an entire expn tree */
expr *
expr_level_tree(expr *e, int fold_const, int simplify_ident)
{
    int i;

    if (!e)
	return 0;

    /* traverse terms */
    for (i=0; i<e->numterms; i++) {
	if (e->terms[i].type == EXPR_EXPR)
	    e->terms[i].data.expn = expr_level_tree(e->terms[i].data.expn,
						    fold_const,
						    simplify_ident);
    }

    /* do callback */
    return expr_level_op(e, fold_const, simplify_ident);
}

/* Comparison function for expr_order_terms().
 * Assumes ExprType enum is in canonical order.
 */
static int
expr_order_terms_compare(const void *va, const void *vb)
{
    const ExprItem *a = va, *b = vb;
    return (a->type - b->type);
}

/* Reorder terms of e into canonical order.  Only reorders if reordering
 * doesn't change meaning of expression.  (eg, doesn't reorder SUB).
 * Canonical order: REG, INT, FLOAT, SYM, EXPR.
 * Multiple terms of a single type are kept in the same order as in
 * the original expression.
 * NOTE: Only performs reordering on *one* level (no recursion).
 */
void
expr_order_terms(expr *e)
{
    /* don't bother reordering if only one element */
    if (e->numterms == 1)
	return;

    /* only reorder some types of operations */
    switch (e->op) {
	case EXPR_ADD:
	case EXPR_MUL:
	case EXPR_OR:
	case EXPR_AND:
	case EXPR_XOR:
	    /* Use mergesort to sort.  It's fast on already sorted values and a
	     * stable sort (multiple terms of same type are kept in the same
	     * order).
	     */
	    mergesort(e->terms, e->numterms, sizeof(ExprItem),
		      expr_order_terms_compare);
	    break;
	default:
	    break;
    }
}

/* Copy entire expression EXCEPT for index "except" at *top level only*. */
expr *
expr_copy_except(const expr *e, int except)
{
    expr *n;
    int i;
    
    if (!e)
	return 0;

    n = xmalloc(sizeof(expr)+sizeof(ExprItem)*(e->numterms-2));

    n->op = e->op;
    n->filename = e->filename;
    n->line = e->line;
    n->numterms = e->numterms;
    for (i=0; i<e->numterms; i++) {
	ExprItem *dest = &n->terms[i];
	const ExprItem *src = &e->terms[i];

	if (i != except) {
	    dest->type = src->type;
	    switch (src->type) {
		case EXPR_SYM:
		    dest->data.sym = src->data.sym;
		    break;
		case EXPR_EXPR:
		    dest->data.expn = expr_copy_except(src->data.expn, -1);
		    break;
		case EXPR_INT:
		    dest->data.intn = intnum_copy(src->data.intn);
		    break;
		case EXPR_FLOAT:
		    dest->data.flt = floatnum_copy(src->data.flt);
		    break;
		case EXPR_REG:
		    dest->data.reg.num = src->data.reg.num;
		    dest->data.reg.size = src->data.reg.size;
		    break;
		default:
		    break;
	    }
	}
    }

    return n;
}

expr *
expr_copy(const expr *e)
{
    return expr_copy_except(e, -1);
}

static int
expr_delete_each(expr *e, void *d)
{
    int i;
    for (i=0; i<e->numterms; i++) {
	switch (e->terms[i].type) {
	    case EXPR_INT:
		intnum_delete(e->terms[i].data.intn);
		break;
	    case EXPR_FLOAT:
		floatnum_delete(e->terms[i].data.flt);
		break;
	    default:
		break;	/* none of the other types needs to be deleted */
	}
    }
    xfree(e);	/* free ourselves */
    return 0;	/* don't stop recursion */
}

void
expr_delete(expr *e)
{
    expr_traverse_nodes_post(e, NULL, expr_delete_each);
}

static int
expr_contains_callback(ExprItem *ei, void *d)
{
    ExprType *t = d;
    return (ei->type & *t);
}

int
expr_contains(expr *e, ExprType t)
{
    return expr_traverse_leaves_in(e, &t, expr_contains_callback);
}

static int
expr_expand_equ_callback(ExprItem *ei, void *d)
{
    const expr *equ_expr;
    if (ei->type == EXPR_SYM) {
	equ_expr = symrec_get_equ(ei->data.sym);
	if (equ_expr) {
	    ei->type = EXPR_EXPR;
	    ei->data.expn = expr_copy(equ_expr);
	}
    }
    return 0;
}

void
expr_expand_equ(expr *e)
{
    expr_traverse_leaves_in(e, NULL, expr_expand_equ_callback);
}

/* Traverse over expression tree, calling func for each operation AFTER the
 * branches (if expressions) have been traversed (eg, postorder
 * traversal).  The data pointer d is passed to each func call.
 *
 * Stops early (and returns 1) if func returns 1.  Otherwise returns 0.
 */
static int
expr_traverse_nodes_post(expr *e, void *d, int (*func) (expr *e, void *d))
{
    int i;

    if (!e)
	return 0;

    /* traverse terms */
    for (i=0; i<e->numterms; i++) {
	if (e->terms[i].type == EXPR_EXPR &&
	    expr_traverse_nodes_post(e->terms[i].data.expn, d, func))
	    return 1;
    }

    /* do callback */
    return func(e, d);
}

/* Traverse over expression tree in order, calling func for each leaf
 * (non-operation).  The data pointer d is passed to each func call.
 *
 * Stops early (and returns 1) if func returns 1.  Otherwise returns 0.
 */
int
expr_traverse_leaves_in(expr *e, void *d,
			int (*func) (ExprItem *ei, void *d))
{
    int i;

    if (!e)
	return 0;

    for (i=0; i<e->numterms; i++) {
	if (e->terms[i].type == EXPR_EXPR) {
	    if (expr_traverse_leaves_in(e->terms[i].data.expn, d, func))
		return 1;
	} else {
	    if (func(&e->terms[i], d))
		return 1;
	}
    }
    return 0;
}

/* Simplify expression by getting rid of unnecessary branches. */
expr *
expr_simplify(expr *e)
{
    e = expr_xform_neg_tree(e);
    e = expr_level_tree(e, 1, 1);
    return e;
}

const intnum *
expr_get_intnum(expr **ep)
{
    *ep = expr_simplify(*ep);

    if ((*ep)->op == EXPR_IDENT && (*ep)->terms[0].type == EXPR_INT)
	return (*ep)->terms[0].data.intn;
    else
	return (intnum *)NULL;
}

void
expr_print(expr *e)
{
    static const char *regs[] = {"ax","cx","dx","bx","sp","bp","si","di"};
    char opstr[3];
    int i;

    switch (e->op) {
	case EXPR_ADD:
	    strcpy(opstr, "+");
	    break;
	case EXPR_SUB:
	    strcpy(opstr, "-");
	    break;
	case EXPR_MUL:
	    strcpy(opstr, "*");
	    break;
	case EXPR_DIV:
	    strcpy(opstr, "/");
	    break;
	case EXPR_SIGNDIV:
	    strcpy(opstr, "//");
	    break;
	case EXPR_MOD:
	    strcpy(opstr, "%");
	    break;
	case EXPR_SIGNMOD:
	    strcpy(opstr, "%%");
	    break;
	case EXPR_NEG:
	    strcpy(opstr, "-");
	    break;
	case EXPR_NOT:
	    strcpy(opstr, "~");
	    break;
	case EXPR_OR:
	    strcpy(opstr, "|");
	    break;
	case EXPR_AND:
	    strcpy(opstr, "&");
	    break;
	case EXPR_XOR:
	    strcpy(opstr, "^");
	    break;
	case EXPR_SHL:
	    strcpy(opstr, "<<");
	    break;
	case EXPR_SHR:
	    strcpy(opstr, ">>");
	    break;
	case EXPR_LOR:
	    strcpy(opstr, "||");
	    break;
	case EXPR_LAND:
	    strcpy(opstr, "&&");
	    break;
	case EXPR_LNOT:
	    strcpy(opstr, "!");
	    break;
	case EXPR_LT:
	    strcpy(opstr, "<");
	    break;
	case EXPR_GT:
	    strcpy(opstr, ">");
	    break;
	case EXPR_LE:
	    strcpy(opstr, "<=");
	    break;
	case EXPR_GE:
	    strcpy(opstr, ">=");
	    break;
	case EXPR_NE:
	    strcpy(opstr, "!=");
	    break;
	case EXPR_EQ:
	    strcpy(opstr, "==");
	    break;
	case EXPR_IDENT:
	    opstr[0] = 0;
	    break;
    }
    for (i=0; i<e->numterms; i++) {
	switch (e->terms[i].type) {
	    case EXPR_SYM:
		printf("%s", symrec_get_name(e->terms[i].data.sym));
		break;
	    case EXPR_EXPR:
		printf("(");
		expr_print(e->terms[i].data.expn);
		printf(")");
		break;
	    case EXPR_INT:
		intnum_print(e->terms[i].data.intn);
		break;
	    case EXPR_FLOAT:
		floatnum_print(e->terms[i].data.flt);
		break;
	    case EXPR_REG:
		if (e->terms[i].data.reg.size == 32)
		    printf("e");
		printf("%s", regs[e->terms[i].data.reg.num&7]);
		break;
	    case EXPR_NONE:
		break;
	}
	if (i < e->numterms-1)
	    printf("%s", opstr);
    }
}
