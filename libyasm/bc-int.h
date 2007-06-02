/* $Id$
 * Bytecode internal structures header file
 *
 *  Copyright (C) 2001-2007  Peter Johnson
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

typedef struct yasm_bytecode_callback {
    void (*destroy) (/*@only@*/ void *contents);
    void (*print) (const void *contents, FILE *f, int indent_level);
    void (*finalize) (yasm_bytecode *bc, yasm_bytecode *prev_bc);
    int (*calc_len) (yasm_bytecode *bc, yasm_bc_add_span_func add_span,
		     void *add_span_data);
    int (*expand) (yasm_bytecode *bc, int span, long old_val, long new_val,
		   /*@out@*/ long *neg_thres, /*@out@*/ long *pos_thres);
    int (*tobytes) (yasm_bytecode *bc, unsigned char **bufp, void *d,
		    yasm_output_value_func output_value,
		    /*@null@*/ yasm_output_reloc_func output_reloc);
    enum yasm_bytecode_special_type {
	YASM_BC_SPECIAL_NONE = 0,
	YASM_BC_SPECIAL_RESERVE,/* Reserves space instead of outputting data */
	YASM_BC_SPECIAL_OFFSET	/* Adjusts offset instead of calculating len */
    } special;
} yasm_bytecode_callback;

struct yasm_bytecode {
    /*@reldef@*/ STAILQ_ENTRY(yasm_bytecode) link;

    /*@null@*/ const yasm_bytecode_callback *callback;

    /* Pointer to section containing bytecode; NULL if not part of a section. */
    /*@dependent@*/ /*@null@*/ yasm_section *section;

    /* number of times bytecode is repeated, NULL=1. */
    /*@only@*/ /*@null@*/ yasm_expr *multiple;

    unsigned long len;		/* total length of entire bytecode
				   (not including multiple copies) */
    long mult_int;		/* number of copies: integer version */

    /* where it came from */
    unsigned long line;

    /* other assembler state info */
    unsigned long offset;	/* ~0UL if unknown */
    unsigned long bc_index;

    /* NULL-terminated array of labels that point to this bytecode (as the
     * bytecode previous to the label).  NULL if no labels point here. */
    /*@null@*/ yasm_symrec **symrecs;

    /* bytecode-type-specific data (type identified by callback) */
    void *contents;
};

/** Create a bytecode of any specified type.
 * \param callback	bytecode callback functions, if NULL, creates empty
 *			bytecode (may not be resolved or output)
 * \param contents	type-specific data
 * \param line		virtual line (from yasm_linemap)
 * \return Newly allocated bytecode of the specified type.
 */
/*@only@*/ yasm_bytecode *yasm_bc_create_common
    (/*@null@*/ const yasm_bytecode_callback *callback,
     /*@only@*/ /*@null@*/ void *contents, unsigned long line);

/** Transform a bytecode of any type into a different type.
 * \param bc		bytecode to transform
 * \param callback	new bytecode callback function
 * \param contents	new type-specific data
 */
void yasm_bc_transform(yasm_bytecode *bc,
		       const yasm_bytecode_callback *callback,
		       void *contents);

/** Common bytecode callback finalize function, for where no finalization
 * is ever required for this type of bytecode.
 */
void yasm_bc_finalize_common(yasm_bytecode *bc, yasm_bytecode *prev_bc);

/** Common bytecode callback expand function, for where the bytecode is
 * always short (calc_len never calls add_span).  Causes an internal
 * error if called.
 */
int yasm_bc_expand_common
    (yasm_bytecode *bc, int span, long old_val, long new_val,
     /*@out@*/ long *neg_thres, /*@out@*/ long *pos_thres);

#define yasm_bc__next(x)		STAILQ_NEXT(x, link)

#endif
