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

/* EXTERN and COMMON are mutually exclusive */
typedef enum {
    SYM_LOCAL = 0,		/* default, local only */
    SYM_GLOBAL = 1 << 0,	/* if it's declared GLOBAL */
    SYM_COMMON = 1 << 1,	/* if it's declared COMMON */
    SYM_EXTERN = 1 << 2		/* if it's declared EXTERN */
} SymVisibility;

/* Resolves a label into an offset, if possible.
 * Inputs: sym, the label to resolve.
 *         sect, the section returned by symrec_get_label() on sym.
 *         precbc, the preceding bytecode returned by symrec_get_label() on sym.
 *         bc, the bytecode following precbc (if any).
 *         startval, the section start (should always be added to return value)
 * Returns an intnum value containing the offset, or NULL if it was not
 * possible to resolve (such as an external value).
 */
typedef /*@null@*/ intnum *
    (*resolve_label_func) (symrec *sym, section *sect,
			   /*@null@*/ bytecode *precbc,
			   /*@null@*/ bytecode *bc, unsigned long startval);

/* Called before attempted resolution of a label.
 * Inputs: sect, the section returned by symrec_get_label() on the label.
 */
typedef void (*resolve_precall_func) (section *sect);

/* Converts an expr to its byte representation.  Usually implemented by
 * object formats to keep track of relocations and verify legal expressions.
 * Inputs:
 *  ep      - (double) pointer to the expression to output
 *  bufp    - (double) pointer to buffer to contain byte representation
 *  valsize - the size (in bytes) to be used for the byte rep
 *  sect    - current section (usually passed into higher-level calling fct)
 *  bc      - current bytecode (usually passed into higher-level callign fct)
 *  rel     - should the expr be treated as PC/IP-relative? (nonzero=yes)
 *  d       - objfmt-specific data (passed into higher-level calling fct)
 * Returns nonzero if an error occurred, 0 otherwise
 */
typedef int (*output_expr_func) (expr **ep, unsigned char **bufp,
				 unsigned long valsize,
				 /*@observer@*/ const section *sect,
				 /*@observer@*/ const bytecode *bc, int rel,
				 /*@null@*/ void *d)
    /*@uses *ep@*/ /*@sets **bufp@*/;

/* Converts a objfmt data bytecode into its byte representation.  Usually
 * implemented by object formats to output their own generated data.
 * Inputs:
 *  type - objfmt-specific type
 *  data - objfmt-specific data
 *  bufp - (double) pointer to buffer to contain byte representation
 * bufp is guaranteed to have enough space to store the data into (as given
 * by the original bc_new_objfmt_data() call).
 * Returns nonzero if an error occurred, 0 otherwise.
 */
typedef int (*output_bc_objfmt_data_func) (unsigned int type,
					   /*@observer@*/ void *data,
					   unsigned char **bufp)
    /*@sets **bufp@*/;
#endif
