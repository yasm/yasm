/*
 * Expression handling
 *
 *  Copyright (C) 2001  Michael Urman, Peter Johnson
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

#include "bitvect.h"

#include "errwarn.h"
#include "intnum.h"
#include "floatnum.h"
#include "expr.h"
#include "symrec.h"

#include "bytecode.h"
#include "section.h"

#include "arch.h"

#include "expr-int.h"


static int expr_traverse_nodes_post(/*@null@*/ yasm_expr *e,
				    /*@null@*/ void *d,
				    int (*func) (/*@null@*/ yasm_expr *e,
						 /*@null@*/ void *d));

static /*@dependent@*/ yasm_arch *cur_arch;


void
yasm_expr_initialize(yasm_arch *a)
{
    cur_arch = a;
}

/* allocate a new expression node, with children as defined.
 * If it's a unary operator, put the element in left and set right=NULL. */
/*@-compmempass@*/
yasm_expr *
yasm_expr_new(yasm_expr_op op, yasm_expr__item *left, yasm_expr__item *right,
	      unsigned long lindex)
{
    yasm_expr *ptr, *sube;
    ptr = xmalloc(sizeof(yasm_expr));

    ptr->op = op;
    ptr->numterms = 0;
    ptr->terms[0].type = YASM_EXPR_NONE;
    ptr->terms[1].type = YASM_EXPR_NONE;
    if (left) {
	ptr->terms[0] = *left;	/* structure copy */
	xfree(left);
	ptr->numterms++;

	/* Search downward until we find something *other* than an
	 * IDENT, then bring it up to the current level.
	 */
	while (ptr->terms[0].type == YASM_EXPR_EXPR &&
	       ptr->terms[0].data.expn->op == YASM_EXPR_IDENT) {
	    sube = ptr->terms[0].data.expn;
	    ptr->terms[0] = sube->terms[0];	/* structure copy */
	    /*@-usereleased@*/
	    xfree(sube);
	    /*@=usereleased@*/
	}
    } else {
	yasm_internal_error(N_("Right side of expression must exist"));
    }

    if (right) {
	ptr->terms[1] = *right;	/* structure copy */
	xfree(right);
	ptr->numterms++;

	/* Search downward until we find something *other* than an
	 * IDENT, then bring it up to the current level.
	 */
	while (ptr->terms[1].type == YASM_EXPR_EXPR &&
	       ptr->terms[1].data.expn->op == YASM_EXPR_IDENT) {
	    sube = ptr->terms[1].data.expn;
	    ptr->terms[1] = sube->terms[0];	/* structure copy */
	    /*@-usereleased@*/
	    xfree(sube);
	    /*@=usereleased@*/
	}
    }

    ptr->line = lindex;

    return ptr;
}
/*@=compmempass@*/

/* helpers */
yasm_expr__item *
yasm_expr_sym(yasm_symrec *s)
{
    yasm_expr__item *e = xmalloc(sizeof(yasm_expr__item));
    e->type = YASM_EXPR_SYM;
    e->data.sym = s;
    return e;
}

yasm_expr__item *
yasm_expr_expr(yasm_expr *x)
{
    yasm_expr__item *e = xmalloc(sizeof(yasm_expr__item));
    e->type = YASM_EXPR_EXPR;
    e->data.expn = x;
    return e;
}

yasm_expr__item *
yasm_expr_int(yasm_intnum *i)
{
    yasm_expr__item *e = xmalloc(sizeof(yasm_expr__item));
    e->type = YASM_EXPR_INT;
    e->data.intn = i;
    return e;
}

yasm_expr__item *
yasm_expr_float(yasm_floatnum *f)
{
    yasm_expr__item *e = xmalloc(sizeof(yasm_expr__item));
    e->type = YASM_EXPR_FLOAT;
    e->data.flt = f;
    return e;
}

yasm_expr__item *
yasm_expr_reg(unsigned long reg)
{
    yasm_expr__item *e = xmalloc(sizeof(yasm_expr__item));
    e->type = YASM_EXPR_REG;
    e->data.reg = reg;
    return e;
}

/* Transforms instances of symrec-symrec [symrec+(-1*symrec)] into integers if
 * possible.  Also transforms single symrec's that reference absolute sections.
 * Uses a simple n^2 algorithm because n is usually quite small.
 */
static /*@only@*/ yasm_expr *
expr_xform_bc_dist(/*@returned@*/ /*@only@*/ yasm_expr *e,
		   yasm_calc_bc_dist_func calc_bc_dist)
{
    int i;
    /*@dependent@*/ yasm_section *sect;
    /*@dependent@*/ /*@null@*/ yasm_bytecode *precbc;
    /*@null@*/ yasm_intnum *dist;
    int numterms;

    for (i=0; i<e->numterms; i++) {
	/* Transform symrecs that reference absolute sections into
	 * absolute start expr + intnum(dist).
	 */
	if (e->terms[i].type == YASM_EXPR_SYM &&
	    yasm_symrec_get_label(e->terms[i].data.sym, &sect, &precbc) &&
	    yasm_section_is_absolute(sect) &&
	    (dist = calc_bc_dist(sect, NULL, precbc))) {
	    const yasm_expr *start = yasm_section_get_start(sect);
	    e->terms[i].type = YASM_EXPR_EXPR;
	    e->terms[i].data.expn =
		yasm_expr_new(YASM_EXPR_ADD,
			      yasm_expr_expr(yasm_expr_copy(start)),
			      yasm_expr_int(dist), start->line);
	}
    }

    /* Handle symrec-symrec in ADD exprs by looking for (-1*symrec) and
     * symrec term pairs (where both symrecs are in the same segment).
     */
    if (e->op != YASM_EXPR_ADD)
	return e;

    for (i=0; i<e->numterms; i++) {
	int j;
	yasm_expr *sube;
	yasm_intnum *intn;
	yasm_symrec *sym;
	/*@dependent@*/ yasm_section *sect2;
	/*@dependent@*/ /*@null@*/ yasm_bytecode *precbc2;

	/* First look for an (-1*symrec) term */
	if (e->terms[i].type != YASM_EXPR_EXPR)
	    continue;
       	sube = e->terms[i].data.expn;
	if (sube->op != YASM_EXPR_MUL || sube->numterms != 2)
	    continue;

	if (sube->terms[0].type == YASM_EXPR_INT &&
	    sube->terms[1].type == YASM_EXPR_SYM) {
	    intn = sube->terms[0].data.intn;
	    sym = sube->terms[1].data.sym;
	} else if (sube->terms[0].type == YASM_EXPR_SYM &&
		   sube->terms[1].type == YASM_EXPR_INT) {
	    sym = sube->terms[0].data.sym;
	    intn = sube->terms[1].data.intn;
	} else
	    continue;

	if (!yasm_intnum_is_neg1(intn))
	    continue;

	yasm_symrec_get_label(sym, &sect2, &precbc);

	/* Now look for a symrec term in the same segment */
	for (j=0; j<e->numterms; j++) {
	    if (e->terms[j].type == YASM_EXPR_SYM &&
		yasm_symrec_get_label(e->terms[j].data.sym, &sect, &precbc2) &&
		sect == sect2 &&
		(dist = calc_bc_dist(sect, precbc, precbc2))) {
		/* Change the symrec term to an integer */
		e->terms[j].type = YASM_EXPR_INT;
		e->terms[j].data.intn = dist;
		/* Delete the matching (-1*symrec) term */
		yasm_expr_delete(sube);
		e->terms[i].type = YASM_EXPR_NONE;
		break;	/* stop looking for matching symrec term */
	    }
	}
    }

    /* Clean up any deleted (EXPR_NONE) terms */
    numterms = 0;
    for (i=0; i<e->numterms; i++) {
	if (e->terms[i].type != YASM_EXPR_NONE)
	    e->terms[numterms++] = e->terms[i];	/* structure copy */
    }
    if (e->numterms != numterms) {
	e->numterms = numterms;
	e = xrealloc(e, sizeof(yasm_expr)+((numterms<2) ? 0 :
		     sizeof(yasm_expr__item)*(numterms-2)));
	if (numterms == 1)
	    e->op = YASM_EXPR_IDENT;
    }

    return e;
}

/* Negate just a single ExprItem by building a -1*ei subexpression */
static void
expr_xform_neg_item(yasm_expr *e, yasm_expr__item *ei)
{
    yasm_expr *sube = xmalloc(sizeof(yasm_expr));

    /* Build -1*ei subexpression */
    sube->op = YASM_EXPR_MUL;
    sube->line = e->line;
    sube->numterms = 2;
    sube->terms[0].type = YASM_EXPR_INT;
    sube->terms[0].data.intn = yasm_intnum_new_int(-1);
    sube->terms[1] = *ei;	/* structure copy */

    /* Replace original ExprItem with subexp */
    ei->type = YASM_EXPR_EXPR;
    ei->data.expn = sube;
}

/* Negates e by multiplying by -1, with distribution over lower-precedence
 * operators (eg ADD) and special handling to simplify result w/ADD, NEG, and
 * others.
 *
 * Returns a possibly reallocated e.
 */
static /*@only@*/ yasm_expr *
expr_xform_neg_helper(/*@returned@*/ /*@only@*/ yasm_expr *e)
{
    yasm_expr *ne;
    int i;

    switch (e->op) {
	case YASM_EXPR_ADD:
	    /* distribute (recursively if expr) over terms */
	    for (i=0; i<e->numterms; i++) {
		if (e->terms[i].type == YASM_EXPR_EXPR)
		    e->terms[i].data.expn =
			expr_xform_neg_helper(e->terms[i].data.expn);
		else
		    expr_xform_neg_item(e, &e->terms[i]);
	    }
	    break;
	case YASM_EXPR_SUB:
	    /* change op to ADD, and recursively negate left side (if expr) */
	    e->op = YASM_EXPR_ADD;
	    if (e->terms[0].type == YASM_EXPR_EXPR)
		e->terms[0].data.expn =
		    expr_xform_neg_helper(e->terms[0].data.expn);
	    else
		expr_xform_neg_item(e, &e->terms[0]);
	    break;
	case YASM_EXPR_NEG:
	    /* Negating a negated value?  Make it an IDENT. */
	    e->op = YASM_EXPR_IDENT;
	    break;
	case YASM_EXPR_IDENT:
	    /* Negating an ident?  Change it into a MUL w/ -1 if there's no
	     * floatnums present below; if there ARE floatnums, recurse.
	     */
	    if (e->terms[0].type == YASM_EXPR_FLOAT)
		yasm_floatnum_calc(e->terms[0].data.flt, YASM_EXPR_NEG, NULL,
				   e->line);
	    else if (e->terms[0].type == YASM_EXPR_EXPR &&
		yasm_expr__contains(e->terms[0].data.expn, YASM_EXPR_FLOAT))
		    expr_xform_neg_helper(e->terms[0].data.expn);
	    else {
		e->op = YASM_EXPR_MUL;
		e->numterms = 2;
		e->terms[1].type = YASM_EXPR_INT;
		e->terms[1].data.intn = yasm_intnum_new_int(-1);
	    }
	    break;
	default:
	    /* Everything else.  MUL will be combined when it's leveled.
	     * Make a new expr (to replace e) with -1*e.
	     */
	    ne = xmalloc(sizeof(yasm_expr));
	    ne->op = YASM_EXPR_MUL;
	    ne->line = e->line;
	    ne->numterms = 2;
	    ne->terms[0].type = YASM_EXPR_INT;
	    ne->terms[0].data.intn = yasm_intnum_new_int(-1);
	    ne->terms[1].type = YASM_EXPR_EXPR;
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
static /*@only@*/ yasm_expr *
expr_xform_neg(/*@returned@*/ /*@only@*/ yasm_expr *e)
{
    switch (e->op) {
	case YASM_EXPR_NEG:
	    /* Turn -x into -1*x */
	    e->op = YASM_EXPR_IDENT;
	    return expr_xform_neg_helper(e);
	case YASM_EXPR_SUB:
	    /* Turn a-b into a+(-1*b) */

	    /* change op to ADD, and recursively negate right side (if expr) */
	    e->op = YASM_EXPR_ADD;
	    if (e->terms[1].type == YASM_EXPR_EXPR)
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

/* Look for simple identities that make the entire result constant:
 * 0*&x, -1|x, etc.
 */
static int
expr_is_constant(yasm_expr_op op, yasm_intnum *intn)
{
    return ((yasm_intnum_is_zero(intn) && op == YASM_EXPR_MUL) ||
	    (yasm_intnum_is_zero(intn) && op == YASM_EXPR_AND) ||
	    (yasm_intnum_is_neg1(intn) && op == YASM_EXPR_OR));
}

/* Look for simple "left" identities like 0+x, 1*x, etc. */
static int
expr_can_delete_int_left(yasm_expr_op op, yasm_intnum *intn)
{
    return ((yasm_intnum_is_pos1(intn) && op == YASM_EXPR_MUL) ||
	    (yasm_intnum_is_zero(intn) && op == YASM_EXPR_ADD) ||
	    (yasm_intnum_is_neg1(intn) && op == YASM_EXPR_AND) ||
	    (yasm_intnum_is_zero(intn) && op == YASM_EXPR_OR));
}

/* Look for simple "right" identities like x+|-0, x*&/1 */
static int
expr_can_delete_int_right(yasm_expr_op op, yasm_intnum *intn)
{
    return ((yasm_intnum_is_pos1(intn) && op == YASM_EXPR_MUL) ||
	    (yasm_intnum_is_pos1(intn) && op == YASM_EXPR_DIV) ||
	    (yasm_intnum_is_zero(intn) && op == YASM_EXPR_ADD) ||
	    (yasm_intnum_is_zero(intn) && op == YASM_EXPR_SUB) ||
	    (yasm_intnum_is_neg1(intn) && op == YASM_EXPR_AND) ||
	    (yasm_intnum_is_zero(intn) && op == YASM_EXPR_OR) ||
	    (yasm_intnum_is_zero(intn) && op == YASM_EXPR_SHL) ||
	    (yasm_intnum_is_zero(intn) && op == YASM_EXPR_SHR));
}

/* Check for and simplify identities.  Returns new number of expr terms.
 * Sets e->op = EXPR_IDENT if numterms ends up being 1.
 * Uses numterms parameter instead of e->numterms for basis of "new" number
 * of terms.
 * Assumes int_term is *only* integer term in e.
 * NOTE: Really designed to only be used by expr_level_op().
 */
static int
expr_simplify_identity(yasm_expr *e, int numterms, int int_term)
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
	yasm_intnum_delete(e->terms[int_term].data.intn);

	/* Slide everything to its right over by 1 */
	if (int_term != numterms-1) /* if it wasn't last.. */
	    memmove(&e->terms[int_term], &e->terms[int_term+1],
		    (numterms-1-int_term)*sizeof(yasm_expr__item));

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
		    case YASM_EXPR_INT:
			yasm_intnum_delete(e->terms[i].data.intn);
			break;
		    case YASM_EXPR_FLOAT:
			yasm_floatnum_delete(e->terms[i].data.flt);
			break;
		    case YASM_EXPR_EXPR:
			yasm_expr_delete(e->terms[i].data.expn);
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
	e->op = YASM_EXPR_IDENT;

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
/*@-mustfree@*/
static /*@only@*/ yasm_expr *
expr_level_op(/*@returned@*/ /*@only@*/ yasm_expr *e, int fold_const,
	      int simplify_ident)
{
    int i, j, o, fold_numterms, level_numterms, level_fold_numterms;
    int first_int_term = -1;

    /* Determine how many operands will need to be brought up (for leveling).
     * Go ahead and bring up any IDENT'ed values.
     */
    while (e->op == YASM_EXPR_IDENT && e->terms[0].type == YASM_EXPR_EXPR) {
	yasm_expr *sube = e->terms[0].data.expn;
	xfree(e);
	e = sube;
    }
    level_numterms = e->numterms;
    level_fold_numterms = 0;
    for (i=0; i<e->numterms; i++) {
	/* Search downward until we find something *other* than an
	 * IDENT, then bring it up to the current level.
	 */
	while (e->terms[i].type == YASM_EXPR_EXPR &&
	       e->terms[i].data.expn->op == YASM_EXPR_IDENT) {
	    yasm_expr *sube = e->terms[i].data.expn;
	    e->terms[i] = sube->terms[0];
	    xfree(sube);
	}

	if (e->terms[i].type == YASM_EXPR_EXPR &&
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
			if (e->terms[i].data.expn->terms[j].type ==
			    YASM_EXPR_INT)
			    level_fold_numterms++;
	}

	/* Find the first integer term (if one is present) if we're folding
	 * constants.
	 */
	if (fold_const && first_int_term == -1 &&
	    e->terms[i].type == YASM_EXPR_INT)
	    first_int_term = i;
    }

    /* Look for other integer terms if there's one and combine.
     * Also eliminate empty spaces when combining and adjust numterms
     * variables.
     */
    fold_numterms = e->numterms;
    if (first_int_term != -1) {
	for (i=first_int_term+1, o=first_int_term+1; i<e->numterms; i++) {
	    if (e->terms[i].type == YASM_EXPR_INT) {
		yasm_intnum_calc(e->terms[first_int_term].data.intn, e->op,
				 e->terms[i].data.intn);
		fold_numterms--;
		level_numterms--;
		/* make sure to delete folded intnum */
		yasm_intnum_delete(e->terms[i].data.intn);
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
	    e->op = YASM_EXPR_IDENT;
    }

    /* Only level operators that allow more than two operand terms.
     * Also don't bother leveling if it's not necessary to bring up any terms.
     */
    if ((e->op != YASM_EXPR_ADD && e->op != YASM_EXPR_MUL &&
	 e->op != YASM_EXPR_OR && e->op != YASM_EXPR_AND &&
	 e->op != YASM_EXPR_XOR) ||
	level_numterms <= fold_numterms) {
	/* Downsize e if necessary */
	if (fold_numterms < e->numterms && e->numterms > 2)
	    e = xrealloc(e, sizeof(yasm_expr)+((fold_numterms<2) ? 0 :
			 sizeof(yasm_expr__item)*(fold_numterms-2)));
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
    e = xrealloc(e, sizeof(yasm_expr)+((level_numterms<2) ? 0 :
		 sizeof(yasm_expr__item)*(level_numterms-2)));

    /* Copy up ExprItem's.  Iterate from right to left to keep the same
     * ordering as was present originally.
     * Combine integer terms as necessary.
     */
    for (i=e->numterms-1, o=level_numterms-1; i>=0; i--) {
	if (e->terms[i].type == YASM_EXPR_EXPR &&
	    e->terms[i].data.expn->op == e->op) {
	    /* bring up subexpression */
	    yasm_expr *sube = e->terms[i].data.expn;

	    /* copy terms right to left */
	    for (j=sube->numterms-1; j>=0; j--) {
		if (fold_const && sube->terms[j].type == YASM_EXPR_INT) {
		    /* Need to fold it in.. but if there's no int term already,
		     * just copy into a new one.
		     */
		    if (first_int_term == -1) {
			first_int_term = o--;
			e->terms[first_int_term] = sube->terms[j];  /* struc */
		    } else {
			yasm_intnum_calc(e->terms[first_int_term].data.intn,
					 e->op, sube->terms[j].data.intn);
			/* make sure to delete folded intnum */
			yasm_intnum_delete(sube->terms[j].data.intn);
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
	    e->op = YASM_EXPR_IDENT;
    }

    return e;
}
/*@=mustfree@*/

typedef struct yasm__exprentry {
    /*@reldef@*/ SLIST_ENTRY(yasm__exprentry) next;
    /*@null@*/ const yasm_expr *e;
} yasm__exprentry;

/* Level an entire expn tree, expanding equ's as we go */
yasm_expr *
yasm_expr__level_tree(yasm_expr *e, int fold_const, int simplify_ident,
		      yasm_calc_bc_dist_func calc_bc_dist,
		      yasm_expr_xform_func expr_xform_extra,
		      void *expr_xform_extra_data, yasm__exprhead *eh)
{
    int i;
    yasm__exprhead eh_local;
    yasm__exprentry ee;

    if (!e)
	return 0;

    if (!eh) {
	eh = &eh_local;
	SLIST_INIT(eh);
    }

    e = expr_xform_neg(e);

    ee.e = NULL;

    /* traverse terms */
    for (i=0; i<e->numterms; i++) {
	/* First expand equ's */
	if (e->terms[i].type == YASM_EXPR_SYM) {
	    const yasm_expr *equ_expr =
		yasm_symrec_get_equ(e->terms[i].data.sym);
	    if (equ_expr) {
		yasm__exprentry *np;

		/* Check for circular reference */
		SLIST_FOREACH(np, eh, next) {
		    if (np->e == equ_expr) {
			yasm__error(e->line,
				    N_("circular reference detected."));
			return e;
		    }
		}

		e->terms[i].type = YASM_EXPR_EXPR;
		e->terms[i].data.expn = yasm_expr_copy(equ_expr);

		ee.e = equ_expr;
		SLIST_INSERT_HEAD(eh, &ee, next);
	    }
	}

	if (e->terms[i].type == YASM_EXPR_EXPR)
	    e->terms[i].data.expn =
		yasm_expr__level_tree(e->terms[i].data.expn, fold_const,
				      simplify_ident, calc_bc_dist,
				      expr_xform_extra, expr_xform_extra_data,
				      eh);

	if (ee.e) {
	    SLIST_REMOVE_HEAD(eh, next);
	    ee.e = NULL;
	}
    }

    /* do callback */
    e = expr_level_op(e, fold_const, simplify_ident);
    if (calc_bc_dist || expr_xform_extra) {
	if (calc_bc_dist)
	    e = expr_xform_bc_dist(e, calc_bc_dist);
	if (expr_xform_extra)
	    e = expr_xform_extra(e, expr_xform_extra_data);
	e = yasm_expr__level_tree(e, fold_const, simplify_ident, NULL, NULL,
				  NULL, NULL);
    }
    return e;
}

/* Comparison function for expr_order_terms().
 * Assumes ExprType enum is in canonical order.
 */
static int
expr_order_terms_compare(const void *va, const void *vb)
{
    const yasm_expr__item *a = va, *b = vb;
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
yasm_expr__order_terms(yasm_expr *e)
{
    /* don't bother reordering if only one element */
    if (e->numterms == 1)
	return;

    /* only reorder some types of operations */
    switch (e->op) {
	case YASM_EXPR_ADD:
	case YASM_EXPR_MUL:
	case YASM_EXPR_OR:
	case YASM_EXPR_AND:
	case YASM_EXPR_XOR:
	    /* Use mergesort to sort.  It's fast on already sorted values and a
	     * stable sort (multiple terms of same type are kept in the same
	     * order).
	     */
	    mergesort(e->terms, (size_t)e->numterms, sizeof(yasm_expr__item),
		      expr_order_terms_compare);
	    break;
	default:
	    break;
    }
}

/* Copy entire expression EXCEPT for index "except" at *top level only*. */
yasm_expr *
yasm_expr__copy_except(const yasm_expr *e, int except)
{
    yasm_expr *n;
    int i;
    
    n = xmalloc(sizeof(yasm_expr) +
		sizeof(yasm_expr__item)*(e->numterms<2?0:e->numterms-2));

    n->op = e->op;
    n->line = e->line;
    n->numterms = e->numterms;
    for (i=0; i<e->numterms; i++) {
	yasm_expr__item *dest = &n->terms[i];
	const yasm_expr__item *src = &e->terms[i];

	if (i != except) {
	    dest->type = src->type;
	    switch (src->type) {
		case YASM_EXPR_SYM:
		    /* Symbols don't need to be copied */
		    dest->data.sym = src->data.sym;
		    break;
		case YASM_EXPR_EXPR:
		    dest->data.expn =
			yasm_expr__copy_except(src->data.expn, -1);
		    break;
		case YASM_EXPR_INT:
		    dest->data.intn = yasm_intnum_copy(src->data.intn);
		    break;
		case YASM_EXPR_FLOAT:
		    dest->data.flt = yasm_floatnum_copy(src->data.flt);
		    break;
		case YASM_EXPR_REG:
		    dest->data.reg = src->data.reg;
		    break;
		default:
		    break;
	    }
	}
    }

    return n;
}

yasm_expr *
yasm_expr_copy(const yasm_expr *e)
{
    return yasm_expr__copy_except(e, -1);
}

static int
expr_delete_each(/*@only@*/ yasm_expr *e, /*@unused@*/ void *d)
{
    int i;
    for (i=0; i<e->numterms; i++) {
	switch (e->terms[i].type) {
	    case YASM_EXPR_INT:
		yasm_intnum_delete(e->terms[i].data.intn);
		break;
	    case YASM_EXPR_FLOAT:
		yasm_floatnum_delete(e->terms[i].data.flt);
		break;
	    default:
		break;	/* none of the other types needs to be deleted */
	}
    }
    xfree(e);	/* free ourselves */
    return 0;	/* don't stop recursion */
}

/*@-mustfree@*/
void
yasm_expr_delete(yasm_expr *e)
{
    expr_traverse_nodes_post(e, NULL, expr_delete_each);
}
/*@=mustfree@*/

static int
expr_contains_callback(const yasm_expr__item *ei, void *d)
{
    yasm_expr__type *t = d;
    return (ei->type & *t);
}

int
yasm_expr__contains(const yasm_expr *e, yasm_expr__type t)
{
    return yasm_expr__traverse_leaves_in_const(e, &t, expr_contains_callback);
}

/* Traverse over expression tree, calling func for each operation AFTER the
 * branches (if expressions) have been traversed (eg, postorder
 * traversal).  The data pointer d is passed to each func call.
 *
 * Stops early (and returns 1) if func returns 1.  Otherwise returns 0.
 */
static int
expr_traverse_nodes_post(yasm_expr *e, void *d,
			 int (*func) (/*@null@*/ yasm_expr *e,
				      /*@null@*/ void *d))
{
    int i;

    if (!e)
	return 0;

    /* traverse terms */
    for (i=0; i<e->numterms; i++) {
	if (e->terms[i].type == YASM_EXPR_EXPR &&
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
yasm_expr__traverse_leaves_in_const(const yasm_expr *e, void *d,
    int (*func) (/*@null@*/ const yasm_expr__item *ei, /*@null@*/ void *d))
{
    int i;

    if (!e)
	return 0;

    for (i=0; i<e->numterms; i++) {
	if (e->terms[i].type == YASM_EXPR_EXPR) {
	    if (yasm_expr__traverse_leaves_in_const(e->terms[i].data.expn, d,
						    func))
		return 1;
	} else {
	    if (func(&e->terms[i], d))
		return 1;
	}
    }
    return 0;
}

/* Traverse over expression tree in order, calling func for each leaf
 * (non-operation).  The data pointer d is passed to each func call.
 *
 * Stops early (and returns 1) if func returns 1.  Otherwise returns 0.
 */
int
yasm_expr__traverse_leaves_in(yasm_expr *e, void *d,
    int (*func) (/*@null@*/ yasm_expr__item *ei, /*@null@*/ void *d))
{
    int i;

    if (!e)
	return 0;

    for (i=0; i<e->numterms; i++) {
	if (e->terms[i].type == YASM_EXPR_EXPR) {
	    if (yasm_expr__traverse_leaves_in(e->terms[i].data.expn, d, func))
		return 1;
	} else {
	    if (func(&e->terms[i], d))
		return 1;
	}
    }
    return 0;
}

yasm_symrec *
yasm_expr_extract_symrec(yasm_expr **ep, yasm_calc_bc_dist_func calc_bc_dist)
{
    yasm_symrec *sym = NULL;
    int i, symterm = -1;

    switch ((*ep)->op) {
	case YASM_EXPR_IDENT:
	    /* Replace sym with 0 value, return sym */
	    if ((*ep)->terms[0].type == YASM_EXPR_SYM) {
		sym = (*ep)->terms[0].data.sym;
		symterm = 0;
	    }
	    break;
	case YASM_EXPR_ADD:
	    /* Search for sym, if found, delete it from expr and return it */
	    for (i=0; i<(*ep)->numterms; i++) {
		if ((*ep)->terms[i].type == YASM_EXPR_SYM) {
		    sym = (*ep)->terms[i].data.sym;
		    symterm = i;
		    break;
		}
	    }
	    break;
	default:
	    break;
    }
    if (sym) {
	/*@dependent@*/ yasm_section *sect;
	/*@dependent@*/ /*@null@*/ yasm_bytecode *precbc;
	/*@null@*/ yasm_intnum *intn;

	if (yasm_symrec_get_label(sym, &sect, &precbc)) {
	    intn = calc_bc_dist(sect, NULL, precbc);
	    if (!intn)
		return NULL;
	} else
	    intn = yasm_intnum_new_uint(0);
	(*ep)->terms[symterm].type = YASM_EXPR_INT;
	(*ep)->terms[symterm].data.intn = intn;
    }
    return sym;
}

/*@-unqualifiedtrans -nullderef -nullstate -onlytrans@*/
const yasm_intnum *
yasm_expr_get_intnum(yasm_expr **ep, yasm_calc_bc_dist_func calc_bc_dist)
{
    *ep = yasm_expr_simplify(*ep, calc_bc_dist);

    if ((*ep)->op == YASM_EXPR_IDENT && (*ep)->terms[0].type == YASM_EXPR_INT)
	return (*ep)->terms[0].data.intn;
    else
	return (yasm_intnum *)NULL;
}
/*@=unqualifiedtrans =nullderef -nullstate -onlytrans@*/

/*@-unqualifiedtrans -nullderef -nullstate -onlytrans@*/
const yasm_floatnum *
yasm_expr_get_floatnum(yasm_expr **ep)
{
    *ep = yasm_expr_simplify(*ep, NULL);

    if ((*ep)->op == YASM_EXPR_IDENT &&
	(*ep)->terms[0].type == YASM_EXPR_FLOAT)
	return (*ep)->terms[0].data.flt;
    else
	return (yasm_floatnum *)NULL;
}
/*@=unqualifiedtrans =nullderef -nullstate -onlytrans@*/

/*@-unqualifiedtrans -nullderef -nullstate -onlytrans@*/
const yasm_symrec *
yasm_expr_get_symrec(yasm_expr **ep, int simplify)
{
    if (simplify)
	*ep = yasm_expr_simplify(*ep, NULL);

    if ((*ep)->op == YASM_EXPR_IDENT && (*ep)->terms[0].type == YASM_EXPR_SYM)
	return (*ep)->terms[0].data.sym;
    else
	return (yasm_symrec *)NULL;
}
/*@=unqualifiedtrans =nullderef -nullstate -onlytrans@*/

/*@-unqualifiedtrans -nullderef -nullstate -onlytrans@*/
const unsigned long *
yasm_expr_get_reg(yasm_expr **ep, int simplify)
{
    if (simplify)
	*ep = yasm_expr_simplify(*ep, NULL);

    if ((*ep)->op == YASM_EXPR_IDENT && (*ep)->terms[0].type == YASM_EXPR_REG)
	return &((*ep)->terms[0].data.reg);
    else
	return NULL;
}
/*@=unqualifiedtrans =nullderef -nullstate -onlytrans@*/

void
yasm_expr_print(FILE *f, const yasm_expr *e)
{
    char opstr[6];
    int i;

    if (!e) {
	fprintf(f, "(nil)");
	return;
    }

    switch (e->op) {
	case YASM_EXPR_ADD:
	    strcpy(opstr, "+");
	    break;
	case YASM_EXPR_SUB:
	    strcpy(opstr, "-");
	    break;
	case YASM_EXPR_MUL:
	    strcpy(opstr, "*");
	    break;
	case YASM_EXPR_DIV:
	    strcpy(opstr, "/");
	    break;
	case YASM_EXPR_SIGNDIV:
	    strcpy(opstr, "//");
	    break;
	case YASM_EXPR_MOD:
	    strcpy(opstr, "%");
	    break;
	case YASM_EXPR_SIGNMOD:
	    strcpy(opstr, "%%");
	    break;
	case YASM_EXPR_NEG:
	    fprintf(f, "-");
	    opstr[0] = 0;
	    break;
	case YASM_EXPR_NOT:
	    fprintf(f, "~");
	    opstr[0] = 0;
	    break;
	case YASM_EXPR_OR:
	    strcpy(opstr, "|");
	    break;
	case YASM_EXPR_AND:
	    strcpy(opstr, "&");
	    break;
	case YASM_EXPR_XOR:
	    strcpy(opstr, "^");
	    break;
	case YASM_EXPR_SHL:
	    strcpy(opstr, "<<");
	    break;
	case YASM_EXPR_SHR:
	    strcpy(opstr, ">>");
	    break;
	case YASM_EXPR_LOR:
	    strcpy(opstr, "||");
	    break;
	case YASM_EXPR_LAND:
	    strcpy(opstr, "&&");
	    break;
	case YASM_EXPR_LNOT:
	    strcpy(opstr, "!");
	    break;
	case YASM_EXPR_LT:
	    strcpy(opstr, "<");
	    break;
	case YASM_EXPR_GT:
	    strcpy(opstr, ">");
	    break;
	case YASM_EXPR_LE:
	    strcpy(opstr, "<=");
	    break;
	case YASM_EXPR_GE:
	    strcpy(opstr, ">=");
	    break;
	case YASM_EXPR_NE:
	    strcpy(opstr, "!=");
	    break;
	case YASM_EXPR_EQ:
	    strcpy(opstr, "==");
	    break;
	case YASM_EXPR_SEG:
	    fprintf(f, "SEG ");
	    opstr[0] = 0;
	    break;
	case YASM_EXPR_WRT:
	    strcpy(opstr, " WRT ");
	    break;
	case YASM_EXPR_SEGOFF:
	    strcpy(opstr, ":");
	    break;
	case YASM_EXPR_IDENT:
	    opstr[0] = 0;
	    break;
    }
    for (i=0; i<e->numterms; i++) {
	switch (e->terms[i].type) {
	    case YASM_EXPR_SYM:
		fprintf(f, "%s", yasm_symrec_get_name(e->terms[i].data.sym));
		break;
	    case YASM_EXPR_EXPR:
		fprintf(f, "(");
		yasm_expr_print(f, e->terms[i].data.expn);
		fprintf(f, ")");
		break;
	    case YASM_EXPR_INT:
		yasm_intnum_print(f, e->terms[i].data.intn);
		break;
	    case YASM_EXPR_FLOAT:
		yasm_floatnum_print(f, e->terms[i].data.flt);
		break;
	    case YASM_EXPR_REG:
		cur_arch->reg_print(f, e->terms[i].data.reg);
		break;
	    case YASM_EXPR_NONE:
		break;
	}
	if (i < e->numterms-1)
	    fprintf(f, "%s", opstr);
    }
}
