/* $IdPath$
 * Expressions internal structures header file
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
#ifndef YASM_EXPR_INT_H
#define YASM_EXPR_INT_H

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
	unsigned long reg;
    } data;
};

/* Some operations may allow more than two operand terms:
 * ADD, MUL, OR, AND, XOR
 */
struct expr {
    ExprOp op;
    unsigned long line;
    int numterms;
    ExprItem terms[2];		/* structure may be extended to include more */
};

/* Traverse over expression tree in order, calling func for each leaf
 * (non-operation).  The data pointer d is passed to each func call.
 *
 * Stops early (and returns 1) if func returns 1.  Otherwise returns 0.
 */
int expr_traverse_leaves_in_const(const expr *e, /*@null@*/ void *d,
				  int (*func) (/*@null@*/ const ExprItem *ei,
					       /*@null@*/ void *d));
int expr_traverse_leaves_in(expr *e, /*@null@*/ void *d,
			    int (*func) (/*@null@*/ ExprItem *ei,
					 /*@null@*/ void *d));

/* Transform negatives throughout an entire expn tree */
/*@only@*/ /*@null@*/ expr *expr_xform_neg_tree(/*@returned@*/ /*@only@*/
						/*@null@*/ expr *e);

/* Level an entire expn tree */
/*@only@*/ /*@null@*/ expr *expr_level_tree(/*@returned@*/ /*@only@*/
					    /*@null@*/ expr *e,
					    int fold_const,
					    int simplify_ident, /*@null@*/
					    calc_bc_dist_func calc_bc_dist);

/* Reorder terms of e into canonical order.  Only reorders if reordering
 * doesn't change meaning of expression.  (eg, doesn't reorder SUB).
 * Canonical order: REG, INT, FLOAT, SYM, EXPR.
 * Multiple terms of a single type are kept in the same order as in
 * the original expression.
 * NOTE: Only performs reordering on *one* level (no recursion).
 */
void expr_order_terms(expr *e);

/* Copy entire expression EXCEPT for index "except" at *top level only*. */
expr *expr_copy_except(const expr *e, int except);

int expr_contains(const expr *e, ExprType t);

#endif
