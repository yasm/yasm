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
    EXPR_IDENT			/* if right is IDENT, then the entire expr is just a num */
} ExprOp;
#endif

typedef struct ExprItem ExprItem;

#ifndef YASM_EXPR
#define YASM_EXPR
typedef struct expr expr;
#endif

expr *expr_new(ExprItem *, ExprOp, ExprItem *);

ExprItem *ExprSym(symrec *);
ExprItem *ExprExpr(expr *);
ExprItem *ExprInt(intnum *);
ExprItem *ExprFloat(floatnum *);

#define expr_new_tree(l,o,r) \
    expr_new (ExprExpr(l), (o), ExprExpr(r))
#define expr_new_branch(o,r) \
    expr_new ((ExprItem *)NULL, (o), ExprExpr(r))
#define expr_new_ident(r) \
    expr_new ((ExprItem *)NULL, EXPR_IDENT, (r))

int expr_simplify(expr *);
void expr_print(expr *);

#endif
