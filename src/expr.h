/* $IdPath$
 * Expression handling header file
 *
 *  Copyright (C) 2001  Michael Urman
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

typedef enum {
    EXPR_ADD,
    EXPR_SUB,
    EXPR_MUL,
    EXPR_DIV,
    EXPR_MOD,
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

typedef enum {
    EXPR_NONE,			/* for left side of a NOT, NEG, etc. */
    EXPR_NUM,
    EXPR_EXPR,
    EXPR_SYM
} ExprType;

typedef union expritem_u {
    struct symrec_s *sym;
    struct expr_s *expr;
    unsigned long num;
} ExprItem;

typedef struct expr_s {
    ExprType ltype, rtype;
    ExprItem left, right;
    ExprOp op;
} expr;

expr *expr_new(ExprType, ExprItem, ExprOp, ExprType, ExprItem);

ExprItem ExprSym(struct symrec_s *);
ExprItem ExprExpr(expr *);
ExprItem ExprNum(unsigned long);
ExprItem ExprNone(void);

#define expr_new_tree(l,o,r) \
    expr_new (EXPR_EXPR, ExprExpr(l), (o), EXPR_EXPR, ExprExpr(r))
#define expr_new_branch(o,r) \
    expr_new (EXPR_NONE, ExprNone(), (o), EXPR_EXPR, ExprExpr(r))
#define expr_new_ident(t,r) \
    expr_new (EXPR_NONE, ExprNone(), EXPR_IDENT, (ExprType)(t), (r))

int expr_simplify(expr *);
void expr_print(expr *);

/* get the value if possible.  return value is IF POSSIBLE, not the val */
int expr_get_value(expr *, unsigned long *);

#endif
