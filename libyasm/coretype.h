/* $IdPath$
 * YASM core (used by many modules/header files) type definitions.
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
#ifndef YASM_CORETYPE_H
#define YASM_CORETYPE_H

typedef struct yasm_arch yasm_arch;

typedef struct yasm_preproc yasm_preproc;
typedef struct yasm_parser yasm_parser;
typedef struct yasm_optimizer yasm_optimizer;
typedef struct yasm_objfmt yasm_objfmt;
typedef struct yasm_dbgfmt yasm_dbgfmt;

typedef struct yasm_bytecode yasm_bytecode;
typedef struct yasm_bytecodehead yasm_bytecodehead;

typedef struct yasm_section yasm_section;
typedef struct yasm_sectionhead yasm_sectionhead;

typedef struct yasm_symrec yasm_symrec;

typedef struct yasm_expr yasm_expr;
typedef struct yasm_intnum yasm_intnum;
typedef struct yasm_floatnum yasm_floatnum;

typedef struct yasm_linemgr yasm_linemgr;

typedef struct yasm_valparam yasm_valparam;
typedef struct yasm_valparamhead yasm_valparamhead;

typedef enum {
    YASM_EXPR_ADD,
    YASM_EXPR_SUB,
    YASM_EXPR_MUL,
    YASM_EXPR_DIV,
    YASM_EXPR_SIGNDIV,
    YASM_EXPR_MOD,
    YASM_EXPR_SIGNMOD,
    YASM_EXPR_NEG,
    YASM_EXPR_NOT,
    YASM_EXPR_OR,
    YASM_EXPR_AND,
    YASM_EXPR_XOR,
    YASM_EXPR_SHL,
    YASM_EXPR_SHR,
    YASM_EXPR_LOR,
    YASM_EXPR_LAND,
    YASM_EXPR_LNOT,
    YASM_EXPR_LT,
    YASM_EXPR_GT,
    YASM_EXPR_EQ,
    YASM_EXPR_LE,
    YASM_EXPR_GE,
    YASM_EXPR_NE,
    YASM_EXPR_SEG,
    YASM_EXPR_WRT,
    YASM_EXPR_SEGOFF,	/* The ':' in SEG:OFF */
    YASM_EXPR_IDENT	/* no operation, just a value */
} yasm_expr_op;

/* EXTERN and COMMON are mutually exclusive */
typedef enum {
    YASM_SYM_LOCAL = 0,		/* default, local only */
    YASM_SYM_GLOBAL = 1 << 0,	/* if it's declared GLOBAL */
    YASM_SYM_COMMON = 1 << 1,	/* if it's declared COMMON */
    YASM_SYM_EXTERN = 1 << 2	/* if it's declared EXTERN */
} yasm_sym_vis;

/* Determines the distance, in bytes, between the starting offsets of two
 * bytecodes in a section.
 * Inputs: sect, the section in which the bytecodes reside.
 *         precbc1, preceding bytecode to the first bytecode
 *         precbc2, preceding bytecode to the second bytecode
 * Outputs: dist, the distance in bytes (bc2-bc1)
 * Returns distance in bytes (bc2-bc1), or NULL if the distance was
 *  indeterminate.
 */
typedef /*@null@*/ yasm_intnum * (*yasm_calc_bc_dist_func)
    (yasm_section *sect, /*@null@*/ yasm_bytecode *precbc1,
     /*@null@*/ yasm_bytecode *precbc2);

/* Converts an expr to its byte representation.  Usually implemented by
 * object formats to keep track of relocations and verify legal expressions.
 * Inputs:
 *  ep      - (double) pointer to the expression to output
 *  bufp    - (double) pointer to buffer to contain byte representation
 *  valsize - the size (in bytes) to be used for the byte rep
 *  offset  - the offset (in bytes) of the expr contents from the bc start
 *  sect    - current section (usually passed into higher-level calling fct)
 *  bc      - current bytecode (usually passed into higher-level calling fct)
 *  rel     - should the expr be treated as PC/IP-relative? (nonzero=yes)
 *  d       - objfmt-specific data (passed into higher-level calling fct)
 * Returns nonzero if an error occurred, 0 otherwise
 */
typedef int (*yasm_output_expr_func)
    (yasm_expr **ep, unsigned char **bufp, unsigned long valsize,
     unsigned long offset, /*@observer@*/ const yasm_section *sect,
     /*@observer@*/ const yasm_bytecode *bc, int rel, /*@null@*/ void *d)
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
typedef int (*yasm_output_bc_objfmt_data_func)
    (unsigned int type, /*@observer@*/ void *data, unsigned char **bufp)
    /*@sets **bufp@*/;
#endif
