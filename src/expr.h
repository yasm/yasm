/* $IdPath$
 * Expression handling header file
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
#ifndef YASM_EXPR_H
#define YASM_EXPR_H

typedef struct ExprItem ExprItem;

void expr_initialize(arch *a, errwarn *we);

/*@only@*/ expr *expr_new(ExprOp, /*@only@*/ ExprItem *,
			  /*@only@*/ /*@null@*/ ExprItem *,
			  unsigned long lindex);

/*@only@*/ ExprItem *ExprSym(/*@keep@*/ symrec *);
/*@only@*/ ExprItem *ExprExpr(/*@keep@*/ expr *);
/*@only@*/ ExprItem *ExprInt(/*@keep@*/ intnum *);
/*@only@*/ ExprItem *ExprFloat(/*@keep@*/ floatnum *);
/*@only@*/ ExprItem *ExprReg(unsigned long reg);

#define expr_new_tree(l,o,r,i) \
    expr_new ((o), ExprExpr(l), ExprExpr(r), i)
#define expr_new_branch(o,r,i) \
    expr_new ((o), ExprExpr(r), (ExprItem *)NULL, i)
#define expr_new_ident(r,i) \
    expr_new (EXPR_IDENT, (r), (ExprItem *)NULL, i)

/* allocates and makes an exact duplicate of e */
expr *expr_copy(const expr *e);

void expr_delete(/*@only@*/ /*@null@*/ expr *e);

/* "Extra" transformation function that may be inserted into an expr_level_tree()
 * invocation.
 * Inputs: e, the expression being simplified
 *         d, data provided as expr_xform_extra_data to expr_level_tree()
 * Returns updated e.
 */
typedef /*@only@*/ expr * (*expr_xform_func) (/*@returned@*/ /*@only@*/ expr *e,
					      /*@null@*/ void *d);

typedef SLIST_HEAD(exprhead, exprentry) exprhead;
/* Level an entire expn tree.  Call with eh = NULL */
/*@only@*/ /*@null@*/ expr *expr_level_tree(
    /*@returned@*/ /*@only@*/ /*@null@*/ expr *e, int fold_const,
    int simplify_ident, /*@null@*/ calc_bc_dist_func calc_bc_dist,
    /*@null@*/ expr_xform_func expr_xform_extra,
    /*@null@*/ void *expr_xform_extra_data, /*@null@*/ exprhead *eh);

/* Simplifies the expression e as much as possible, eliminating extraneous
 * branches and simplifying integer-only subexpressions.
 */
#define expr_simplify(e, cbd)	expr_level_tree(e, 1, 1, cbd, NULL, NULL, NULL)

/* Extracts a single symrec out of an expression, replacing it with the
 * symrec's value (if it's a label).  Returns NULL if it's unable to extract a
 * symrec (too complex of expr, none present, etc).
 */
/*@dependent@*/ /*@null@*/ symrec *expr_extract_symrec(expr **ep,
						       calc_bc_dist_func
						       calc_bc_dist);

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
