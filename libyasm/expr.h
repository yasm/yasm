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

#ifndef YASM_SYMREC
#define YASM_SYMREC
typedef struct symrec symrec;
#endif

#ifndef YASM_FLOATNUM
#define YASM_FLOATNUM
typedef struct floatnum floatnum;
#endif

#ifndef YASM_INTNUM
#define YASM_INTNUM
typedef struct intnum intnum;
#endif

#ifndef YASM_EXPROP
#define YASM_EXPROP
typedef enum {
    EXPR_ADD,
    EXPR_SUB,
    EXPR_MUL,
    EXPR_DIV,
    EXPR_SIGNDIV,
    EXPR_MOD,
    EXPR_SIGNMOD,
    EXPR_NEG,
    EXPR_NOT,
    EXPR_OR,
    EXPR_AND,
    EXPR_XOR,
    EXPR_SHL,
    EXPR_SHR,
    EXPR_LOR,
    EXPR_LAND,
    EXPR_LNOT,
    EXPR_LT,
    EXPR_GT,
    EXPR_EQ,
    EXPR_LE,
    EXPR_GE,
    EXPR_NE,
    EXPR_IDENT	    /* no operation, just a value */
} ExprOp;
#endif

typedef struct ExprItem ExprItem;

#ifndef YASM_EXPR
#define YASM_EXPR
typedef struct expr expr;
#endif

expr *expr_new(ExprOp, ExprItem *, ExprItem *);

ExprItem *ExprSym(symrec *);
ExprItem *ExprExpr(expr *);
ExprItem *ExprInt(intnum *);
ExprItem *ExprFloat(floatnum *);
ExprItem *ExprReg(unsigned char reg, unsigned char size);

#define expr_new_tree(l,o,r) \
    expr_new ((o), ExprExpr(l), ExprExpr(r))
#define expr_new_branch(o,r) \
    expr_new ((o), ExprExpr(r), (ExprItem *)NULL)
#define expr_new_ident(r) \
    expr_new (EXPR_IDENT, (r), (ExprItem *)NULL)

/* allocates and makes an exact duplicate of e */
expr *expr_copy(const expr *e);

void expr_delete(expr *e);

int expr_checkea(expr **ep, unsigned char *addrsize, unsigned char bits,
		 unsigned char *displen, unsigned char *modrm,
		 unsigned char *v_modrm, unsigned char *n_modrm,
		 unsigned char *sib, unsigned char *v_sib,
		 unsigned char *n_sib);

/* Expands all (symrec) equ's in the expression into full expression
 * instances.
 */
void expr_expand_equ(expr *e);

/* Simplifies the expression e as much as possible, eliminating extraneous
 * branches and simplifying integer-only subexpressions.
 */
expr *expr_simplify(expr *e);

/* Gets the integer value of e if the expression is just an integer.  If the
 * expression is more complex (contains anything other than integers, ie
 * floats, non-valued labels, registers), returns NULL.
 */
const intnum *expr_get_intnum(expr **ep);

void expr_print(expr *);

#endif
