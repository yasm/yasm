/*
 * Bytecode utility functions
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
#define YASM_LIB_INTERNAL
#include "util.h"
/*@unused@*/ RCSID("$Id$");

#include "coretype.h"

#include "errwarn.h"
#include "intnum.h"
#include "expr.h"
#include "value.h"

#include "bytecode.h"

#include "bc-int.h"
#include "expr-int.h"


typedef struct bytecode_reserve {
    /*@only@*/ /*@null@*/ yasm_expr *numitems; /* number of items to reserve */
    unsigned char itemsize;	    /* size of each item (in bytes) */
} bytecode_reserve;

static void bc_reserve_destroy(void *contents);
static void bc_reserve_print(const void *contents, FILE *f, int indent_level);
static void bc_reserve_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc);
static int bc_reserve_calc_len(yasm_bytecode *bc,
			       yasm_bc_add_span_func add_span,
			       void *add_span_data);
static int bc_reserve_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
			      yasm_output_value_func output_value,
			      /*@null@*/ yasm_output_reloc_func output_reloc);

static const yasm_bytecode_callback bc_reserve_callback = {
    bc_reserve_destroy,
    bc_reserve_print,
    bc_reserve_finalize,
    bc_reserve_calc_len,
    yasm_bc_expand_common,
    bc_reserve_tobytes,
    1
};


static void
bc_reserve_destroy(void *contents)
{
    bytecode_reserve *reserve = (bytecode_reserve *)contents;
    yasm_expr_destroy(reserve->numitems);
    yasm_xfree(contents);
}

static void
bc_reserve_print(const void *contents, FILE *f, int indent_level)
{
    const bytecode_reserve *reserve = (const bytecode_reserve *)contents;
    fprintf(f, "%*s_Reserve_\n", indent_level, "");
    fprintf(f, "%*sNum Items=", indent_level, "");
    yasm_expr_print(reserve->numitems, f);
    fprintf(f, "\n%*sItem Size=%u\n", indent_level, "",
	    (unsigned int)reserve->itemsize);
}

static void
bc_reserve_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc)
{
    bytecode_reserve *reserve = (bytecode_reserve *)bc->contents;
    yasm_value val;

    if (yasm_value_finalize_expr(&val, reserve->numitems, 0))
	yasm_error_set(YASM_ERROR_TOO_COMPLEX,
		       N_("reserve expression too complex"));
    else if (val.rel)
	yasm_error_set(YASM_ERROR_NOT_ABSOLUTE,
		       N_("reserve expression not absolute"));
    else if (val.abs && yasm_expr__contains(val.abs, YASM_EXPR_FLOAT))
	yasm_error_set(YASM_ERROR_VALUE,
		       N_("expression must not contain floating point value"));
    reserve->numitems = val.abs;
}

static int
bc_reserve_calc_len(yasm_bytecode *bc, yasm_bc_add_span_func add_span,
		    void *add_span_data)
{
    bytecode_reserve *reserve = (bytecode_reserve *)bc->contents;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;

    if (!reserve->numitems)
	return 0;

    num = yasm_expr_get_intnum(&reserve->numitems, NULL);
    if (!num) {
	/* Check for use of floats first. */
	if (reserve->numitems &&
	    yasm_expr__contains(reserve->numitems, YASM_EXPR_FLOAT)) {
	    yasm_error_set(YASM_ERROR_VALUE,
		N_("expression must not contain floating point value"));
	    return -1;
	}
	/* FIXME: Non-constant currently not allowed. */
	yasm_error_set(YASM_ERROR_NOT_CONSTANT,
		       N_("attempt to reserve non-constant quantity of space"));
	return -1;
    }
    
    bc->len += yasm_intnum_get_uint(num)*reserve->itemsize;
    return 0;
}

static int
bc_reserve_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		   yasm_output_value_func output_value,
		   /*@unused@*/ yasm_output_reloc_func output_reloc)
{
    yasm_internal_error(N_("bc_reserve_tobytes called"));
    /*@notreached@*/
    return 1;
}

yasm_bytecode *
yasm_bc_create_reserve(yasm_expr *numitems, unsigned int itemsize,
		       unsigned long line)
{
    bytecode_reserve *reserve = yasm_xmalloc(sizeof(bytecode_reserve));

    /*@-mustfree@*/
    reserve->numitems = numitems;
    /*@=mustfree@*/
    reserve->itemsize = (unsigned char)itemsize;

    return yasm_bc_create_common(&bc_reserve_callback, reserve, line);
}
