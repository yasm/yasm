/* $IdPath$
 * Expressions internal structures header file
 *
 *  Copyright (C) 2001  Peter Johnson
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
