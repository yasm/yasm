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

typedef struct yasm_expr__item yasm_expr__item;

void yasm_expr_initialize(yasm_arch *a);

/*@only@*/ yasm_expr *yasm_expr_new(yasm_expr_op, /*@only@*/ yasm_expr__item *,
				    /*@only@*/ /*@null@*/ yasm_expr__item *,
				    unsigned long lindex);

/*@only@*/ yasm_expr__item *yasm_expr_sym(/*@keep@*/ yasm_symrec *);
/*@only@*/ yasm_expr__item *yasm_expr_expr(/*@keep@*/ yasm_expr *);
/*@only@*/ yasm_expr__item *yasm_expr_int(/*@keep@*/ yasm_intnum *);
/*@only@*/ yasm_expr__item *yasm_expr_float(/*@keep@*/ yasm_floatnum *);
/*@only@*/ yasm_expr__item *yasm_expr_reg(unsigned long reg);

#define yasm_expr_new_tree(l,o,r,i) \
    yasm_expr_new ((o), yasm_expr_expr(l), yasm_expr_expr(r), i)
#define yasm_expr_new_branch(o,r,i) \
    yasm_expr_new ((o), yasm_expr_expr(r), (yasm_expr__item *)NULL, i)
#define yasm_expr_new_ident(r,i) \
    yasm_expr_new (YASM_EXPR_IDENT, (r), (yasm_expr__item *)NULL, i)

/* allocates and makes an exact duplicate of e */
yasm_expr *yasm_expr_copy(const yasm_expr *e);

void yasm_expr_delete(/*@only@*/ /*@null@*/ yasm_expr *e);

/* "Extra" transformation function that may be inserted into an
 * expr_level_tree() invocation.
 * Inputs: e, the expression being simplified
 *         d, data provided as expr_xform_extra_data to expr_level_tree()
 * Returns updated e.
 */
typedef /*@only@*/ yasm_expr * (*yasm_expr_xform_func)
    (/*@returned@*/ /*@only@*/ yasm_expr *e, /*@null@*/ void *d);

typedef SLIST_HEAD(yasm__exprhead, yasm__exprentry) yasm__exprhead;
/* Level an entire expn tree.  Call with eh = NULL */
/*@only@*/ /*@null@*/ yasm_expr *yasm_expr__level_tree
    (/*@returned@*/ /*@only@*/ /*@null@*/ yasm_expr *e, int fold_const,
     int simplify_ident, /*@null@*/ yasm_calc_bc_dist_func calc_bc_dist,
     /*@null@*/ yasm_expr_xform_func expr_xform_extra,
     /*@null@*/ void *expr_xform_extra_data, /*@null@*/ yasm__exprhead *eh);

/* Simplifies the expression e as much as possible, eliminating extraneous
 * branches and simplifying integer-only subexpressions.
 */
#define yasm_expr_simplify(e, cbd) \
    yasm_expr__level_tree(e, 1, 1, cbd, NULL, NULL, NULL)

/* Extracts a single symrec out of an expression, replacing it with the
 * symrec's value (if it's a label).  Returns NULL if it's unable to extract a
 * symrec (too complex of expr, none present, etc).
 */
/*@dependent@*/ /*@null@*/ yasm_symrec *yasm_expr_extract_symrec
    (yasm_expr **ep, yasm_calc_bc_dist_func calc_bc_dist);

/* Gets the integer value of e if the expression is just an integer.  If the
 * expression is more complex (contains anything other than integers, ie
 * floats, non-valued labels, registers), returns NULL.
 */
/*@dependent@*/ /*@null@*/ const yasm_intnum *yasm_expr_get_intnum
    (yasm_expr **ep, /*@null@*/ yasm_calc_bc_dist_func calc_bc_dist);

/* Gets the float value of e if the expression is just an float.  If the
 * expression is more complex (contains anything other than floats, ie
 * integers, non-valued labels, registers), returns NULL.
 */
/*@dependent@*/ /*@null@*/ const yasm_floatnum *yasm_expr_get_floatnum
    (yasm_expr **ep);

/* Gets the symrec value of e if the expression is just an symbol.  If the
 * expression is more complex, returns NULL.  Simplifies the expr first if
 * simplify is nonzero.
 */
/*@dependent@*/ /*@null@*/ const yasm_symrec *yasm_expr_get_symrec
    (yasm_expr **ep, int simplify);

/* Gets the register value of e if the expression is just a register.  If the
 * expression is more complex, returns NULL.  Simplifies the expr first if
 * simplify is nonzero.
 */
/*@dependent@*/ /*@null@*/ const unsigned long *yasm_expr_get_reg
    (yasm_expr **ep, int simplify);

void yasm_expr_print(FILE *f, /*@null@*/ const yasm_expr *);

#endif
