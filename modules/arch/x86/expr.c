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

#include "bitvect.h"

#include "globals.h"
#include "errwarn.h"
#include "intnum.h"
#include "floatnum.h"
#include "expr.h"
#include "symrec.h"

#ifdef DMALLOC
# include <dmalloc.h>
#endif

RCSID("$IdPath$");

/* Types listed in canonical sorting order.  See expr_order_terms(). */
typedef enum {
    EXPR_NONE = 0,
    EXPR_REG = 1<<0,
    EXPR_INT = 1<<1,
    EXPR_FLOAT = 1<<2,
    EXPR_SYM = 1<<3,
    EXPR_EXPR = 1<<4
} ExprType;

struct ExprItem {
    ExprType type;
    union {
	symrec *sym;
	expr *expn;
	intnum *intn;
	floatnum *flt;
	struct reg {
	    unsigned char num;
	    unsigned char size;	/* in bits, eg AX=16, EAX=32 */
	} reg;
    } data;
};

/* Some operations may allow more than two operand terms:
 * ADD, MUL, OR, AND, XOR
 */
struct expr {
    ExprOp op;
    const char *filename;
    unsigned long line;
    int numterms;
    ExprItem terms[2];		/* structure may be extended to include more */
};

static int expr_traverse_nodes_post(expr *e, void *d,
				    int (*func) (expr *e, void *d));
static int expr_traverse_leaves_in(expr *e, void *d,
				   int (*func) (ExprItem *ei, void *d));

/* allocate a new expression node, with children as defined.
 * If it's a unary operator, put the element on the right */
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
	memcpy(&ptr->terms[0], left, sizeof(ExprItem));
	xfree(left);
	ptr->numterms++;
    } else {
	InternalError(_("Right side of expression must exist"));
    }

    if (right) {
	memcpy(&ptr->terms[1], right, sizeof(ExprItem));
	xfree(right);
	ptr->numterms++;
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

/* Level an entire expn tree */
static expr *
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
	    e->terms[o--] = e->terms[i];
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
static expr *
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
static void
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
static expr *
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

static int
expr_contains(expr *e, ExprType t)
{
    return expr_traverse_leaves_in(e, &t, expr_contains_callback);
}

/* Only works if ei->type == EXPR_REG (doesn't check).
 * Overwrites ei with intnum of 0 (to eliminate regs from the final expr).
 */
static int *
expr_checkea_get_reg32(ExprItem *ei, void *d)
{
    int *data = d;
    int *ret;

    /* don't allow 16-bit registers */
    if (ei->data.reg.size != 32)
	return 0;

    ret = &data[ei->data.reg.num & 7];	/* & 7 for sanity check */

    /* overwrite with 0 to eliminate register from displacement expr */
    ei->type = EXPR_INT;
    ei->data.intn = intnum_new_int(0);

    /* we're okay */
    return ret;
}

typedef struct checkea_reg16_data {
    int bx, si, di, bp;		/* total multiplier for each reg */
} checkea_reg16_data;

/* Only works if ei->type == EXPR_REG (doesn't check).
 * Overwrites ei with intnum of 0 (to eliminate regs from the final expr).
 */
static int *
expr_checkea_get_reg16(ExprItem *ei, void *d)
{
    checkea_reg16_data *data = d;
    /* in order: ax,cx,dx,bx,sp,bp,si,di */
    static int *reg16[8] = {0,0,0,0,0,0,0,0};
    int *ret;

    reg16[3] = &data->bx;
    reg16[5] = &data->bp;
    reg16[6] = &data->si;
    reg16[7] = &data->di;

    /* don't allow 32-bit registers */
    if (ei->data.reg.size != 16)
	return 0;

    ret = reg16[ei->data.reg.num & 7];	/* & 7 for sanity check */

    /* only allow BX, SI, DI, BP */
    if (!ret)
	return 0;

    /* overwrite with 0 to eliminate register from displacement expr */
    ei->type = EXPR_INT;
    ei->data.intn = intnum_new_int(0);

    /* we're okay */
    return ret;
}

/* Distribute over registers to help bring them to the topmost level of e.
 * Also check for illegal operations against registers.
 * Returns 0 if something was illegal, 1 if legal and nothing in e changed,
 * and 2 if legal and e needs to be simplified.
 *
 * Only half joking: Someday make this/checkea able to accept crazy things
 *  like: (bx+di)*(bx+di)-bx*bx-2*bx*di-di*di+di?  Probably not: NASM never
 *  accepted such things, and it's doubtful such an expn is valid anyway
 *  (even though the above one is).  But even macros would be hard-pressed
 *  to generate something like this.
 *
 * e must already have been simplified for this function to work properly
 * (as it doesn't think things like SUB are valid).
 *
 * IMPLEMENTATION NOTE: About the only thing this function really needs to
 * "distribute" is: (non-float-expn or intnum) * (sum expn of registers).
 *
 * TODO: Clean up this code, make it easier to understand.
 */
static int
expr_checkea_distcheck_reg(expr **ep)
{
    expr *e = *ep;
    int i;
    int havereg = -1, havereg_expr = -1;
    int retval = 1;	/* default to legal, no changes */

    for (i=0; i<e->numterms; i++) {
	switch (e->terms[i].type) {
	    case EXPR_REG:
		/* Check op to make sure it's valid to use w/register. */
		if (e->op != EXPR_ADD && e->op != EXPR_MUL &&
		    e->op != EXPR_IDENT)
		    return 0;
		/* Check for reg*reg */
		if (e->op == EXPR_MUL && havereg != -1)
		    return 0;
		havereg = i;
		break;
	    case EXPR_FLOAT:
		/* Floats not allowed. */
		return 0;
	    case EXPR_EXPR:
		if (expr_contains(e->terms[i].data.expn, EXPR_REG)) {
		    int ret2;

		    /* Check op to make sure it's valid to use w/register. */
		    if (e->op != EXPR_ADD && e->op != EXPR_MUL)
			return 0;
		    /* Check for reg*reg */
		    if (e->op == EXPR_MUL && havereg != -1)
			return 0;
		    havereg = i;
		    havereg_expr = i;
		    /* Recurse to check lower levels */
		    ret2 = expr_checkea_distcheck_reg(&e->terms[i].data.expn);
		    if (ret2 == 0)
			return 0;
		    if (ret2 == 2)
			retval = 2;
		} else if (expr_contains(e->terms[i].data.expn, EXPR_FLOAT))
		    return 0;	/* Disallow floats */
		break;
	    default:
		break;
	}
    }

    /* just exit if no registers were used */
    if (havereg == -1)
	return retval;

    /* Distribute */
    if (e->op == EXPR_MUL && havereg_expr != -1) {
	expr *ne;

	retval = 2;	/* we're going to change it */

	/* The reg expn *must* be EXPR_ADD at this point.  Sanity check. */
	if (e->terms[havereg_expr].type != EXPR_EXPR ||
	    e->terms[havereg_expr].data.expn->op != EXPR_ADD)
	    InternalError(_("Register expression not ADD or EXPN"));

	/* Iterate over each term in reg expn */
	for (i=0; i<e->terms[havereg_expr].data.expn->numterms; i++) {
	    /* Copy everything EXCEPT havereg_expr term into new expression */
	    ne = expr_copy_except(e, havereg_expr);
	    /* Copy reg expr term into uncopied (empty) term in new expn */
	    ne->terms[havereg_expr] =
		e->terms[havereg_expr].data.expn->terms[i]; /* struct copy */
	    /* Overwrite old reg expr term with new expn */
	    e->terms[havereg_expr].data.expn->terms[i].type = EXPR_EXPR;
	    e->terms[havereg_expr].data.expn->terms[i].data.expn = ne;
	}

	/* Replace e with expanded reg expn */
	ne = e->terms[havereg_expr].data.expn;
	e->terms[havereg_expr].type = EXPR_NONE;    /* don't delete it! */
	expr_delete(e);				    /* but everything else */
	e = ne;
	*ep = ne;
    }

    return retval;
}

/* Simplify and determine if expression is superficially valid:
 * Valid expr should be [(int-equiv expn)]+[reg*(int-equiv expn)+...]
 * where the [...] parts are optional.
 *
 * Don't simplify out constant identities if we're looking for an indexreg: we
 * may need the multiplier for determining what the indexreg is!
 *
 * Returns 0 if invalid register usage, 1 if unable to determine all values,
 * and 2 if all values successfully determined and saved in data.
 */
static int
expr_checkea_getregusage(expr **ep, int *indexreg, void *data,
			 int *(*get_reg)(ExprItem *ei, void *d))
{
    int i;
    int *reg;
    expr *e;

    *ep = expr_xform_neg_tree(*ep);
    *ep = expr_level_tree(*ep, 1, indexreg == 0);
    e = *ep;
    switch (expr_checkea_distcheck_reg(ep)) {
	case 0:
	    return 0;
	case 2:
	    /* Need to simplify again */
	    *ep = expr_xform_neg_tree(*ep);
	    *ep = expr_level_tree(*ep, 1, indexreg == 0);
	    e = *ep;
	    break;
	default:
	    break;
    }

    switch (e->op) {
	case EXPR_ADD:
	    /* Prescan for non-int multipliers.
	     * This is because if any of the terms is a more complex
	     * expr (eg, undetermined value), we don't want to try to
	     * figure out *any* of the expression, because each register
	     * lookup overwrites the register with a 0 value!  And storing
	     * the state of this routine from one excution to the next
	     * would be a major chore.
	     */
	    for (i=0; i<e->numterms; i++)
		if (e->terms[i].type == EXPR_EXPR) {
		    if (e->terms[i].data.expn->numterms > 2)
			return 1;
		    expr_order_terms(e->terms[i].data.expn);
		    if (e->terms[i].data.expn->terms[1].type != EXPR_INT)
			return 1;
		}

	    /* FALLTHROUGH */
	case EXPR_IDENT:
	    /* Check each term for register (and possible multiplier). */
	    for (i=0; i<e->numterms; i++) {
		if (e->terms[i].type == EXPR_REG) {
		    reg = get_reg(&e->terms[i], data);
		    if (!reg)
			return 0;
		    (*reg)++;
		    if (indexreg)
			*indexreg = reg-(int *)data;
		} else if (e->terms[i].type == EXPR_EXPR) {
		    /* Already ordered from ADD above, just grab the value.
		     * Sanity check for EXPR_INT.
		     */
		    if (e->terms[i].data.expn->terms[0].type != EXPR_REG)
			InternalError(_("Register not found in reg expn"));
		    if (e->terms[i].data.expn->terms[1].type != EXPR_INT)
			InternalError(_("Non-integer value in reg expn"));
		    reg = get_reg(&e->terms[i].data.expn->terms[0], data);
		    if (!reg)
			return 0;
		    (*reg) +=
			intnum_get_int(e->terms[i].data.expn->terms[1].data.intn);
		    if (indexreg)
			*indexreg = reg-(int *)data;
		}
	    }
	    break;
	case EXPR_MUL:
	    /* Here, too, check for non-int multipliers. */
	    if (e->numterms > 2)
		return 1;
	    expr_order_terms(e);
	    if (e->terms[1].type != EXPR_INT)
		return 1;
	    reg = get_reg(&e->terms[0], data);
	    if (!reg)
		return 0;
	    (*reg) += intnum_get_int(e->terms[1].data.intn);
	    if (indexreg)
		*indexreg = reg-(int *)data;
	    break;
	default:
	    /* Should never get here! */
	    break;
    }

    /* Simplify expr, which is now really just the displacement. This
     * should get rid of the 0's we put in for registers in the callback.
     */
    *ep = expr_simplify(*ep);
    /* e = *ep; */

    return 2;
}

/* Calculate the displacement length, if possible.
 * Takes several extra inputs so it can be used by both 32-bit and 16-bit
 * expressions:
 *  wordsize=2 for 16-bit, =4 for 32-bit.
 *  noreg=1 if the *ModRM byte* has no registers used.
 *  isbpreg=1 if BP/EBP is the *only* register used within the *ModRM byte*.
 */
static int
checkea_calc_displen(expr **ep, int wordsize, int noreg, int isbpreg,
		     unsigned char *displen, unsigned char *modrm,
		     unsigned char *v_modrm)
{
    expr *e = *ep;
    const intnum *intn;
    long dispval;

    *v_modrm = 0;	/* default to not yet valid */

    switch (*displen) {
	case 0:
	    /* the displacement length hasn't been forced, try to
	     * determine what it is.
	     */
	    if (noreg) {
		/* no register in ModRM expression, so it must be disp16/32,
		 * and as the Mod bits are set to 0 by the caller, we're done
		 * with the ModRM byte.
		 */
		*displen = wordsize;
		*v_modrm = 1;
	    } else if (isbpreg) {
		/* for BP/EBP, there *must* be a displacement value, but we
		 * may not know the size (8 or 16/32) for sure right now.
		 * We can't leave displen at 0, because that just means
		 * unknown displacement, including none.
		 */
		*displen = 0xff;
	    }

	    intn = expr_get_intnum(ep);
	    if (!intn)
		break;		/* expr still has unknown values */

	    /* make sure the displacement will fit in 16/32 bits if unsigned,
	     * and 8 bits if signed.
	     */
	    if (!intnum_check_size(intn, wordsize, 0) &&
		!intnum_check_size(intn, 1, 1)) {
		ErrorAt(e->filename, e->line, _("invalid effective address"));
		return 0;
	    }

	    /* don't try to find out what size displacement we have if
	     * displen is known.
	     */
	    if (*displen != 0 && *displen != 0xff)
		break;

	    /* Don't worry about overflows here (it's already guaranteed
	     * to be 16/32 or 8 bits).
	     */
	    dispval = intnum_get_int(intn);

	    /* Figure out what size displacement we will have. */
	    if (*displen != 0xff && dispval == 0) {
		/* if we know that the displacement is 0 right now,
		 * go ahead and delete the expr (making it so no
		 * displacement value is included in the output).
		 * The Mod bits of ModRM are set to 0 above, and
		 * we're done with the ModRM byte!
		 *
		 * Don't do this if we came from isbpreg check above, so
		 * check *displen.
		 */
		expr_delete(e);
		*ep = (expr *)NULL;
	    } else if (dispval >= -128 && dispval <= 127) {
		/* It fits into a signed byte */
		*displen = 1;
		*modrm |= 0100;
	    } else {
		/* It's a 16/32-bit displacement */
		*displen = wordsize;
		*modrm |= 0200;
	    }
	    *v_modrm = 1;	/* We're done with ModRM */

	    break;

	/* If not 0, the displacement length was forced; set the Mod bits
	 * appropriately and we're done with the ModRM byte.  We assume
	 * that the user knows what they're doing if they do an explicit
	 * override, so we don't check for overflow (we'll just truncate
	 * when we output).
	 */
	case 1:
	    *modrm |= 0100;
	    *v_modrm = 1;
	    break;
	case 2:
	case 4:
	    if (wordsize != *displen) {
		ErrorAt(e->filename, e->line,
			_("invalid effective address (displacement size)"));
		return 0;
	    }
	    *modrm |= 0200;
	    *v_modrm = 1;
	    break;
	default:
	    /* we shouldn't ever get any other size! */
	    InternalError(_("strange EA displacement size"));
    }

    return 1;
}

static int
expr_checkea_getregsize_callback(ExprItem *ei, void *d)
{
    unsigned char *addrsize = (unsigned char *)d;

    if (ei->type == EXPR_REG) {
	*addrsize = ei->data.reg.size;
	return 1;
    } else
	return 0;
}

int
expr_checkea(expr **ep, unsigned char *addrsize, unsigned char bits,
	     unsigned char nosplit, unsigned char *displen,
	     unsigned char *modrm, unsigned char *v_modrm,
	     unsigned char *n_modrm, unsigned char *sib, unsigned char *v_sib,
	     unsigned char *n_sib)
{
    expr *e = *ep;

    if (*addrsize == 0) {
	/* we need to figure out the address size from what we know about:
	 * - the displacement length
	 * - what registers are used in the expression
	 * - the bits setting
	 */
	switch (*displen) {
	    case 4:
		/* must be 32-bit */
		*addrsize = 32;
		break;
	    case 2:
		/* must be 16-bit */
		*addrsize = 16;
		break;
	    default:
		/* check for use of 16 or 32-bit registers; if none are used
		 * default to bits setting.
		 */
		if (!expr_traverse_leaves_in(e, addrsize,
					     expr_checkea_getregsize_callback))
		    *addrsize = bits;
		/* TODO: Add optional warning here if switched address size
		 * from bits setting just by register use.. eg [ax] in
		 * 32-bit mode would generate a warning.
		 */
	}
    }

    if (*addrsize == 32 && ((*n_modrm && !*v_modrm) || (*n_sib && !*v_sib))) {
	int i;
	typedef enum {
	    REG32_NONE = -1,
	    REG32_EAX = 0,
	    REG32_ECX = 1,
	    REG32_EDX = 2,
	    REG32_EBX = 3,
	    REG32_ESP = 4,
	    REG32_EBP = 5,
	    REG32_ESI = 6,
	    REG32_EDI = 7
	} reg32type;
	int reg32mult[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	int basereg = REG32_NONE;	/* "base" register (for SIB) */
	int indexreg = REG32_NONE;	/* "index" register (for SIB) */
	
	switch (expr_checkea_getregusage(ep, &indexreg, reg32mult,
					 expr_checkea_get_reg32)) {
	    case 0:
		e = *ep;
		ErrorAt(e->filename, e->line, _("invalid effective address"));
		return 0;
	    case 1:
		return 1;
	    default:
		e = *ep;
		break;
	}

	/* If indexreg mult is 0, discard it.
	 * This is possible because of the way indexreg is found in
	 * expr_checkea_getregusage().
	 */
	if (indexreg != REG32_NONE && reg32mult[indexreg] == 0)
	    indexreg = REG32_NONE;

	/* Find a basereg (*1, but not indexreg), if there is one.
	 * Also, if an indexreg hasn't been assigned, try to find one.
	 * Meanwhile, check to make sure there's no negative register mults.
	 */
	for (i=0; i<8; i++) {
	    if (reg32mult[i] < 0) {
		ErrorAt(e->filename, e->line, _("invalid effective address"));
		return 0;
	    }
	    if (i != indexreg && reg32mult[i] == 1)
		basereg = i;
	    else if (indexreg == REG32_NONE && reg32mult[i] > 0)
		indexreg = i;
	}

	/* Handle certain special cases of indexreg mults when basereg is
	 * empty.
	 */
	if (indexreg != REG32_NONE && basereg == REG32_NONE)
	    switch (reg32mult[indexreg]) {
		case 1:
		    /* Only optimize this way if nosplit wasn't specified */
		    if (!nosplit) {
			basereg = indexreg;
			indexreg = -1;
		    }
		    break;
		case 2:
		    /* Only split if nosplit wasn't specified */
		    if (!nosplit) {
			basereg = indexreg;
			reg32mult[indexreg] = 1;
		    }
		    break;
		case 3:
		case 5:
		case 9:
		    basereg = indexreg;
		    reg32mult[indexreg]--;
		    break;
	    }

	/* Make sure there's no other registers than the basereg and indexreg
	 * we just found.
	 */
	for (i=0; i<8; i++)
	    if (i != basereg && i != indexreg && reg32mult[i] != 0) {
		ErrorAt(e->filename, e->line, _("invalid effective address"));
		return 0;
	    }

	/* Check the index multiplier value for validity if present. */
	if (indexreg != REG32_NONE && reg32mult[indexreg] != 1 &&
	    reg32mult[indexreg] != 2 && reg32mult[indexreg] != 4 &&
	    reg32mult[indexreg] != 8) {
	    ErrorAt(e->filename, e->line, _("invalid effective address"));
	    return 0;
	}

	/* ESP is not a legal indexreg. */
	if (indexreg == REG32_ESP) {
	    /* If mult>1 or basereg is ESP also, there's no way to make it
	     * legal.
	     */
	    if (reg32mult[REG32_ESP] > 1 || basereg == REG32_ESP) {
		ErrorAt(e->filename, e->line, _("invalid effective address"));
		return 0;
	    }
	    /* If mult==1 and basereg is not ESP, swap indexreg w/basereg. */
	    indexreg = basereg;
	    basereg = REG32_ESP;
	}

	/* At this point, we know the base and index registers and that the
	 * memory expression is (essentially) valid.  Now build the ModRM and
	 * (optional) SIB bytes.
	 */

	/* First determine R/M (Mod is later determined from disp size) */
	*n_modrm = 1;	/* we always need ModRM */
	if (basereg == REG32_NONE) {
	    /* disp32[index] */
	    *modrm |= 5;
	    /* we must have a SIB */
	    *n_sib = 1;
	} else if (indexreg == REG32_NONE) {
	    /* basereg only */
	    *modrm |= basereg;
	    /* we don't need an SIB *unless* basereg is ESP */
	    if (basereg == REG32_ESP)
		*n_sib = 1;
	    else {
		*sib = 0;
		*v_sib = 0;
		*n_sib = 0;
	    }
	} else {
	    /* both base AND index */
	    *modrm |= 4;
	    *n_sib = 1;
	}

	/* Determine SIB if needed */
	if (*n_sib == 1) {
	    *sib = 0;	/* start with 0 */

	    /* Special case: no basereg (only happens in disp32[index] case) */
	    if (basereg == REG32_NONE)
		*sib |= 5;
	    else
		*sib |= basereg & 7;	/* &7 to sanity check */
	    
	    /* Put in indexreg, checking for none case */
	    if (indexreg == REG32_NONE)
		*sib |= 040;
		/* Any scale field is valid, just leave at 0. */
	    else {
		*sib |= (indexreg & 7) << 3;	/* &7 to sanity check */
		/* Set scale field, 1 case -> 0, so don't bother. */
		switch (reg32mult[indexreg]) {
		    case 2:
			*sib |= 0100;
			break;
		    case 4:
			*sib |= 0200;
			break;
		    case 8:
			*sib |= 0300;
			break;
		}
	    }

	    *v_sib = 1;	/* Done with SIB */
	}

	/* Calculate displacement length (if possible) */
	return checkea_calc_displen(ep, 4, basereg == REG32_NONE,
				    basereg == REG32_EBP &&
				    indexreg == REG32_NONE, displen, modrm,
				    v_modrm);
    } else if (*addrsize == 16 && *n_modrm && !*v_modrm) {
	static const unsigned char modrm16[16] = {
	    0006 /* disp16  */, 0007 /* [BX]    */, 0004 /* [SI]    */,
	    0000 /* [BX+SI] */, 0005 /* [DI]    */, 0001 /* [BX+DI] */,
	    0377 /* invalid */, 0377 /* invalid */, 0006 /* [BP]+d  */,
	    0377 /* invalid */, 0002 /* [BP+SI] */, 0377 /* invalid */,
	    0003 /* [BP+DI] */, 0377 /* invalid */, 0377 /* invalid */,
	    0377 /* invalid */
	};
	checkea_reg16_data reg16mult = {0, 0, 0, 0};
	enum {
	    HAVE_NONE = 0,
	    HAVE_BX = 1<<0,
	    HAVE_SI = 1<<1,
	    HAVE_DI = 1<<2,
	    HAVE_BP = 1<<3
	} havereg = HAVE_NONE;

	/* 16-bit cannot have SIB */
	*sib = 0;
	*v_sib = 0;
	*n_sib = 0;

	switch (expr_checkea_getregusage(ep, (int *)NULL, &reg16mult,
					 expr_checkea_get_reg16)) {
	    case 0:
		e = *ep;
		ErrorAt(e->filename, e->line, _("invalid effective address"));
		return 0;
	    case 1:
		return 1;
	    default:
		e = *ep;
		break;
	}

	/* reg multipliers not 0 or 1 are illegal. */
	if (reg16mult.bx & ~1 || reg16mult.si & ~1 || reg16mult.di & ~1 ||
	    reg16mult.bp & ~1) {
	    ErrorAt(e->filename, e->line, _("invalid effective address"));
	    return 0;
	}

	/* Set havereg appropriately */
	if (reg16mult.bx > 0)
	    havereg |= HAVE_BX;
	if (reg16mult.si > 0)
	    havereg |= HAVE_SI;
	if (reg16mult.di > 0)
	    havereg |= HAVE_DI;
	if (reg16mult.bp > 0)
	    havereg |= HAVE_BP;

	/* Check the modrm value for invalid combinations. */
	if (modrm16[havereg] & 0070) {
	    ErrorAt(e->filename, e->line, _("invalid effective address"));
	    return 0;
	}

	/* Set ModRM byte for registers */
	*modrm |= modrm16[havereg];

	/* Calculate displacement length (if possible) */
	return checkea_calc_displen(ep, 2, havereg == HAVE_NONE,
				    havereg == HAVE_BP, displen, modrm,
				    v_modrm);
    }
    return 1;
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
static int
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
