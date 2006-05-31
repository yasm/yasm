/* $Id$
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
    YASM_EXPR_NONE = 0,
    YASM_EXPR_REG = 1<<0,
    YASM_EXPR_INT = 1<<1,
    YASM_EXPR_FLOAT = 1<<2,
    YASM_EXPR_SYM = 1<<3,
    YASM_EXPR_SYMEXP = 1<<4, /* post-expanded sym (due to absolute expansion) */
    YASM_EXPR_EXPR = 1<<5
} yasm_expr__type;

struct yasm_expr__item {
    yasm_expr__type type;
    union {
	yasm_symrec *sym;
	yasm_expr *expn;
	yasm_intnum *intn;
	yasm_floatnum *flt;
	unsigned long reg;
    } data;
};

/* Some operations may allow more than two operand terms:
 * ADD, MUL, OR, AND, XOR
 */
struct yasm_expr {
    yasm_expr_op op;
    unsigned long line;
    int numterms;
    yasm_expr__item terms[2];	/* structure may be extended to include more */
};

/* Traverse over expression tree in order, calling func for each leaf
 * (non-operation).  The data pointer d is passed to each func call.
 *
 * Stops early (and returns 1) if func returns 1.  Otherwise returns 0.
 */
int yasm_expr__traverse_leaves_in_const
    (const yasm_expr *e, /*@null@*/ void *d,
     int (*func) (/*@null@*/ const yasm_expr__item *ei, /*@null@*/ void *d));
int yasm_expr__traverse_leaves_in
    (yasm_expr *e, /*@null@*/ void *d,
     int (*func) (/*@null@*/ yasm_expr__item *ei, /*@null@*/ void *d));

/* Reorder terms of e into canonical order.  Only reorders if reordering
 * doesn't change meaning of expression.  (eg, doesn't reorder SUB).
 * Canonical order: REG, INT, FLOAT, SYM, EXPR.
 * Multiple terms of a single type are kept in the same order as in
 * the original expression.
 * NOTE: Only performs reordering on *one* level (no recursion).
 */
void yasm_expr__order_terms(yasm_expr *e);

/* Copy entire expression EXCEPT for index "except" at *top level only*. */
yasm_expr *yasm_expr__copy_except(const yasm_expr *e, int except);

int yasm_expr__contains(const yasm_expr *e, yasm_expr__type t);

#endif
