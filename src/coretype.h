/* $IdPath$
 * Core (used by many modules/header files) type definitions.
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
#ifndef YASM_CORETYPE_H
#define YASM_CORETYPE_H

typedef struct arch arch;

typedef struct preproc preproc;
typedef struct parser parser;
typedef struct optimizer optimizer;
typedef struct objfmt objfmt;

typedef struct bytecode bytecode;
typedef /*@reldef@*/ STAILQ_HEAD(bytecodehead, bytecode) bytecodehead;

typedef struct section section;
typedef /*@reldef@*/ STAILQ_HEAD(sectionhead, section) sectionhead;

typedef struct symrec symrec;

typedef struct expr expr;
typedef struct intnum intnum;
typedef struct floatnum floatnum;

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
