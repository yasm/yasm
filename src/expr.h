/* $IdPath$
 * Expression handling header file
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
#ifndef YASM_EXPR_H
#define YASM_EXPR_H

typedef struct ExprItem ExprItem;

/*@only@*/ expr *expr_new(ExprOp, /*@only@*/ ExprItem *,
			  /*@only@*/ /*@null@*/ ExprItem *);

/*@only@*/ ExprItem *ExprSym(/*@keep@*/ symrec *);
/*@only@*/ ExprItem *ExprExpr(/*@keep@*/ expr *);
/*@only@*/ ExprItem *ExprInt(/*@keep@*/ intnum *);
/*@only@*/ ExprItem *ExprFloat(/*@keep@*/ floatnum *);
/*@only@*/ ExprItem *ExprReg(unsigned long reg);

#define expr_new_tree(l,o,r) \
    expr_new ((o), ExprExpr(l), ExprExpr(r))
#define expr_new_branch(o,r) \
    expr_new ((o), ExprExpr(r), (ExprItem *)NULL)
#define expr_new_ident(r) \
    expr_new (EXPR_IDENT, (r), (ExprItem *)NULL)

/* allocates and makes an exact duplicate of e */
expr *expr_copy(const expr *e);

void expr_delete(/*@only@*/ /*@null@*/ expr *e);

/* Simplifies the expression e as much as possible, eliminating extraneous
 * branches and simplifying integer-only subexpressions.
 */
/*@only@*/ /*@null@*/ expr *expr_simplify(/*@returned@*/ /*@only@*/ /*@null@*/
					  expr *e, /*@null@*/
					  calc_bc_dist_func calc_bc_dist);

/* Gets the integer value of e if the expression is just an integer.  If the
 * expression is more complex (contains anything other than integers, ie
 * floats, non-valued labels, registers), returns NULL.
 */
/*@dependent@*/ /*@null@*/ const intnum *expr_get_intnum(expr **ep, /*@null@*/
							 calc_bc_dist_func
							 calc_bc_dist);

/* Gets the float value of e if the expression is just an float.  If the
 * expression is more complex (contains anything other than floats, ie
 * integers, non-valued labels, registers), returns NULL.
 */
/*@dependent@*/ /*@null@*/ const floatnum *expr_get_floatnum(expr **ep);

/* Gets the symrec value of e if the expression is just an symbol.  If the
 * expression is more complex, returns NULL.  Simplifies the expr first if
 * simplify is nonzero.
 */
/*@dependent@*/ /*@null@*/ const symrec *expr_get_symrec(expr **ep,
							 int simplify);

/* Gets the register value of e if the expression is just a register.  If the
 * expression is more complex, returns NULL.  Simplifies the expr first if
 * simplify is nonzero.
 */
/*@dependent@*/ /*@null@*/ const unsigned long *expr_get_reg(expr **ep,
							     int simplify);

void expr_print(FILE *f, /*@null@*/ const expr *);

#endif
