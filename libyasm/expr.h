/**
 * \file libyasm/expr.h
 * \brief YASM expression interface.
 *
 * \rcs
 * $Id$
 * \endrcs
 *
 * \license
 *  Copyright (C) 2001  Michael Urman, Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
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
 * \endlicense
 */
#ifndef YASM_EXPR_H
#define YASM_EXPR_H

/** Expression item (opaque type).  \internal */
typedef struct yasm_expr__item yasm_expr__item;

/** Create a new expression e=a op b.
 * \param op	    operation
 * \param a	    expression item a
 * \param b	    expression item b (optional depending on op)
 * \param line	    virtual line (where expression defined)
 * \return Newly allocated expression.
 */
/*@only@*/ yasm_expr *yasm_expr_create
    (yasm_expr_op op, /*@only@*/ yasm_expr__item *a,
     /*@only@*/ /*@null@*/ yasm_expr__item *b, unsigned long line);

/** Create a new symbol expression item.
 * \param sym	    symbol
 * \return Newly allocated expression item.
 */
/*@only@*/ yasm_expr__item *yasm_expr_sym(/*@keep@*/ yasm_symrec *sym);

/** Create a new expression expression item.
 * \param e	    expression
 * \return Newly allocated expression item.
 */
/*@only@*/ yasm_expr__item *yasm_expr_expr(/*@keep@*/ yasm_expr *e);

/** Create a new intnum expression item.
 * \param intn	    intnum
 * \return Newly allocated expression item.
 */
/*@only@*/ yasm_expr__item *yasm_expr_int(/*@keep@*/ yasm_intnum *intn);

/** Create a new floatnum expression item.
 * \param flt	    floatnum
 * \return Newly allocated expression item.
 */
/*@only@*/ yasm_expr__item *yasm_expr_float(/*@keep@*/ yasm_floatnum *flt);

/** Create a new register expression item.
 * \param reg	    register
 * \return Newly allocated expression item.
 */
/*@only@*/ yasm_expr__item *yasm_expr_reg(unsigned long reg);

/** Create a new expression tree e=l op r.
 * \param l	expression for left side of new expression
 * \param o	operation
 * \param r	expression for right side of new expression
 * \param i	line index
 * \return Newly allocated expression.
 */
#define yasm_expr_create_tree(l,o,r,i) \
    yasm_expr_create ((o), yasm_expr_expr(l), yasm_expr_expr(r), i)

/** Create a new expression branch e=op r.
 * \param o	operation
 * \param r	expression for right side of new expression
 * \param i	line index
 * \return Newly allocated expression.
 */
#define yasm_expr_create_branch(o,r,i) \
    yasm_expr_create ((o), yasm_expr_expr(r), (yasm_expr__item *)NULL, i)

/** Create a new expression identity e=r.
 * \param r	expression for identity within new expression
 * \param i	line index
 * \return Newly allocated expression.
 */
#define yasm_expr_create_ident(r,i) \
    yasm_expr_create (YASM_EXPR_IDENT, (r), (yasm_expr__item *)NULL, i)

/** Duplicate an expression.
 * \param e	expression
 * \return Newly allocated expression identical to e.
 */
yasm_expr *yasm_expr_copy(const yasm_expr *e);

/** Destroy (free allocated memory for) an expression.
 * \param e	expression
 */
void yasm_expr_destroy(/*@only@*/ /*@null@*/ yasm_expr *e);

/** Determine if an expression is a specified operation (at the top level).
 * \param e		expression
 * \param op		operator
 * \return Nonzero if the expression was the specified operation at the top
 *         level, zero otherwise.
 */
int yasm_expr_is_op(const yasm_expr *e, yasm_expr_op op);

/** Extra transformation function for yasm_expr__level_tree().
 * \param e	expression being simplified
 * \param d	data provided as expr_xform_extra_data to
 *		yasm_expr__level_tree()
 * \return Transformed e.
 */
typedef /*@only@*/ yasm_expr * (*yasm_expr_xform_func)
    (/*@returned@*/ /*@only@*/ yasm_expr *e, /*@null@*/ void *d);

/** Linked list of expression entries.
 * \internal
 * Used internally by yasm_expr__level_tree().
 */
typedef struct yasm__exprhead yasm__exprhead;
#ifdef YASM_LIB_INTERNAL
SLIST_HEAD(yasm__exprhead, yasm__exprentry);
#endif

/** Level an entire expression tree.
 * \internal
 * \param e		    expression
 * \param fold_const	    enable constant folding if nonzero
 * \param simplify_ident    simplify identities
 * \param calc_bc_dist	    bytecode distance-calculation function
 * \param expr_xform_extra  extra transformation function
 * \param expr_xform_extra_data	data to pass to expr_xform_extra
 * \param eh		    call with NULL (for internal use in recursion)
 * \return Leveled expression.
 */
/*@only@*/ /*@null@*/ yasm_expr *yasm_expr__level_tree
    (/*@returned@*/ /*@only@*/ /*@null@*/ yasm_expr *e, int fold_const,
     int simplify_ident, /*@null@*/ yasm_calc_bc_dist_func calc_bc_dist,
     /*@null@*/ yasm_expr_xform_func expr_xform_extra,
     /*@null@*/ void *expr_xform_extra_data, /*@null@*/ yasm__exprhead *eh);

/** Simplify an expression as much as possible.  Eliminates extraneous
 * branches and simplifies integer-only subexpressions.  Simplified version
 * of yasm_expr__level_tree().
 * \param e	expression
 * \param cbd	bytecode distance-calculation function
 * \return Simplified expression.
 */
#define yasm_expr_simplify(e, cbd) \
    yasm_expr__level_tree(e, 1, 1, cbd, NULL, NULL, NULL)

/** Relocation actions for yasm_expr_extract_symrec(). */
typedef enum yasm_symrec_relocate_action {
    YASM_SYMREC_REPLACE_ZERO = 0,   /**< Replace the symbol with 0 */
    YASM_SYMREC_REPLACE_VALUE,	    /**< Replace with symbol's value (offset)
				     * if symbol is a label.
				     */
    YASM_SYMREC_REPLACE_VALUE_IF_LOCAL	/**< Replace with symbol's value only
					 * if symbol is label and declared
					 * local.
					 */
} yasm_symrec_relocate_action;

/** Extract a single symbol out of an expression.  Replaces it with 0, or
 * optionally the symbol's value (if it's a label).
 * \param ep		    expression (pointer to)
 * \param relocate_action   replacement action to take on symbol in expr
 * \param calc_bc_dist	    bytecode distance-calculation function
 * \return NULL if unable to extract a symbol (too complex of expr, none
 *         present, etc); otherwise returns the extracted symbol.
 */
/*@dependent@*/ /*@null@*/ yasm_symrec *yasm_expr_extract_symrec
    (yasm_expr **ep, yasm_symrec_relocate_action relocate_action,
     yasm_calc_bc_dist_func calc_bc_dist);

/** Remove the SEG unary operator if present, leaving the lower level
 * expression.
 * \param ep		expression (pointer to)
 * \return NULL if the expression does not have a top-level operator of SEG;
 * otherwise the modified input expression (without the SEG).
 */
/*@only@*/ /*@null@*/ yasm_expr *yasm_expr_extract_seg(yasm_expr **ep);

/** Extract the segment portion of a SEG:OFF expression, leaving the offset.
 * \param ep		expression (pointer to)
 * \return NULL if unable to extract a segment (YASM_EXPR_SEGOFF not the
 *         top-level operator), otherwise the segment expression.  The input
 *         expression is modified such that on return, it's the offset
 *         expression.
 */
/*@only@*/ /*@null@*/ yasm_expr *yasm_expr_extract_segoff(yasm_expr **ep);

/** Extract the right portion (y) of a x WRT y expression, leaving the left
 * portion (x).
 * \param ep		expression (pointer to)
 * \return NULL if unable to extract (YASM_EXPR_WRT not the top-level
 *         operator), otherwise the right side of the WRT expression.  The
 *         input expression is modified such that on return, it's the left side
 *         of the WRT expression.
 */
/*@only@*/ /*@null@*/ yasm_expr *yasm_expr_extract_wrt(yasm_expr **ep);

/** Extract the right portion (y) of a x SHR y expression, leaving the left
 * portion (x).
 * \param ep		expression (pointer to)
 * \return NULL if unable to extract (YASM_EXPR_SHR not the top-level
 *         operator), otherwise the right side of the SHR expression.  The
 *         input expression is modified such that on return, it's the left side
 *         of the SHR expression.
 */
/*@only@*/ /*@null@*/ yasm_expr *yasm_expr_extract_shr(yasm_expr **ep);

/** Get the integer value of an expression if it's just an integer.
 * \param ep		expression (pointer to)
 * \param calc_bc_dist	bytecode distance-calculation function
 * \return NULL if the expression is too complex (contains anything other than
 *         integers, ie floats, non-valued labels, registers); otherwise the
 *         intnum value of the expression.
 */
/*@dependent@*/ /*@null@*/ yasm_intnum *yasm_expr_get_intnum
    (yasm_expr **ep, /*@null@*/ yasm_calc_bc_dist_func calc_bc_dist);

/** Get the floating point value of an expression if it's just an floatnum.
 * \param ep		expression (pointer to)
 * \return NULL if the expression is too complex (contains anything other than
 *         floats, ie integers, non-valued labels, registers); otherwise the
 *         floatnum value of the expression.
 */
/*@dependent@*/ /*@null@*/ const yasm_floatnum *yasm_expr_get_floatnum
    (yasm_expr **ep);

/** Get the symbol value of an expression if it's just a symbol.
 * \param ep		expression (pointer to)
 * \param simplify	if nonzero, simplify the expression first
 * \return NULL if the expression is too complex; otherwise the symbol value of
 *         the expression.
 */
/*@dependent@*/ /*@null@*/ const yasm_symrec *yasm_expr_get_symrec
    (yasm_expr **ep, int simplify);

/** Get the register value of an expression if it's just a register.
 * \param ep		expression (pointer to)
 * \param simplify	if nonzero, simplify the expression first
 * \return NULL if the expression is too complex; otherwise the register value
 *         of the expression.
 */
/*@dependent@*/ /*@null@*/ const unsigned long *yasm_expr_get_reg
    (yasm_expr **ep, int simplify);

/** Print an expression.  For debugging purposes.
 * \param e	expression
 * \param f	file
 */
void yasm_expr_print(/*@null@*/ const yasm_expr *e, FILE *f);

#endif
