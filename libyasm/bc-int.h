/* $Id$
 * Bytecode internal structures header file
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
#ifndef YASM_BC_INT_H
#define YASM_BC_INT_H

typedef struct yasm_effaddr_callback {
    void (*destroy) (yasm_effaddr *ea);
    void (*print) (const yasm_effaddr *ea, FILE *f, int indent_level);
} yasm_effaddr_callback;

struct yasm_effaddr {
    const yasm_effaddr_callback *callback;	/* callback functions */

    /*@only@*/ /*@null@*/ yasm_expr *disp;	/* address displacement */
    unsigned char len;		/* length of disp (in bytes), 0 if unknown,
				 * 0xff if unknown and required to be >0.
				 */
    unsigned char nosplit;	/* 1 if reg*2 should not be split into
				   reg+reg. (0 if not) */
};

struct yasm_immval {
    /*@only@*/ /*@null@*/ yasm_expr *val;

    unsigned char len;		/* final length (in bytes), 0 if unknown */
    unsigned char sign;		/* 1 if final imm is treated as signed */
};

typedef struct yasm_bytecode_callback {
    void (*destroy) (yasm_bytecode *bc);
    void (*print) (const yasm_bytecode *bc, FILE *f, int indent_level);
    yasm_bc_resolve_flags (*resolve)
	(yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
    int (*tobytes) (yasm_bytecode *bc, unsigned char **bufp, void *d,
		    yasm_output_expr_func output_expr,
		    /*@null@*/ yasm_output_reloc_func output_reloc);
} yasm_bytecode_callback;

struct yasm_bytecode {
    /*@reldef@*/ STAILQ_ENTRY(yasm_bytecode) link;

    /*@null@*/ const yasm_bytecode_callback *callback;

    /* Pointer to section containing bytecode; NULL if not part of a section. */
    /*@dependent@*/ /*@null@*/ yasm_section *section;

    /* number of times bytecode is repeated, NULL=1. */
    /*@only@*/ /*@null@*/ yasm_expr *multiple;

    unsigned long len;		/* total length of entire bytecode (including
				   multiple copies), 0 if unknown */

    /* where it came from */
    unsigned long line;

    /* other assembler state info */
    unsigned long offset;	/* 0 if unknown */

    /* storage for optimizer flags */
    unsigned long opt_flags;

    /* NULL-terminated array of labels that point to this bytecode (as the
     * bytecode previous to the label).  NULL if no labels point here. */
    /*@null@*/ yasm_symrec **symrecs;
};

/** Create a bytecode of any specified type.
 * \param callback	bytecode callback functions, if NULL, creates empty
 *			bytecode (may not be resolved or output)
 * \param datasize	size of type-specific data (in bytes)
 * \param line		virtual line (from yasm_linemap)
 * \return Newly allocated bytecode of the specified type.
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_common
    (/*@null@*/ const yasm_bytecode_callback *callback, size_t datasize,
     unsigned long line);

#define yasm_bc__next(x)		STAILQ_NEXT(x, link)

#endif
