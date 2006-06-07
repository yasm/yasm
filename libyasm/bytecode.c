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
#include "symrec.h"

#include "bytecode.h"

#include "bc-int.h"
#include "expr-int.h"


void
yasm_bc_set_multiple(yasm_bytecode *bc, yasm_expr *e)
{
    if (bc->multiple)
	bc->multiple = yasm_expr_create_tree(bc->multiple, YASM_EXPR_MUL, e,
					     e->line);
    else
	bc->multiple = e;
}

void
yasm_bc_finalize_common(yasm_bytecode *bc, yasm_bytecode *prev_bc)
{
}

int
yasm_bc_expand_common(yasm_bytecode *bc, int span, long old_val, long new_val,
		      /*@out@*/ long *neg_thres, /*@out@*/ long *pos_thres)
{
    yasm_internal_error(N_("bytecode does not have any dependent spans"));
    /*@unreached@*/
    return 0;
}

void
yasm_bc_transform(yasm_bytecode *bc, const yasm_bytecode_callback *callback,
		  void *contents)
{
    if (bc->callback)
	bc->callback->destroy(bc->contents);
    bc->callback = callback;
    bc->contents = contents;
}

yasm_bytecode *
yasm_bc_create_common(const yasm_bytecode_callback *callback, void *contents,
		      unsigned long line)
{
    yasm_bytecode *bc = yasm_xmalloc(sizeof(yasm_bytecode));

    bc->callback = callback;

    bc->section = NULL;

    bc->multiple = (yasm_expr *)NULL;
    bc->len = 0;

    bc->line = line;

    bc->offset = ~0UL;

    bc->opt_flags = 0;

    bc->symrecs = NULL;

    bc->contents = contents;

    return bc;
}

yasm_section *
yasm_bc_get_section(yasm_bytecode *bc)
{
    return bc->section;
}

void
yasm_bc__add_symrec(yasm_bytecode *bc, yasm_symrec *sym)
{
    if (!bc->symrecs) {
	bc->symrecs = yasm_xmalloc(2*sizeof(yasm_symrec *));
	bc->symrecs[0] = sym;
	bc->symrecs[1] = NULL;
    } else {
	/* Very inefficient implementation for large numbers of symbols.  But
	 * that would be very unusual, so use the simple algorithm instead.
	 */
	size_t count = 1;
	while (bc->symrecs[count])
	    count++;
	bc->symrecs = yasm_xrealloc(bc->symrecs,
				    (count+2)*sizeof(yasm_symrec *));
	bc->symrecs[count] = sym;
	bc->symrecs[count+1] = NULL;
    }
}

void
yasm_bc_destroy(yasm_bytecode *bc)
{
    if (!bc)
	return;

    if (bc->callback)
	bc->callback->destroy(bc->contents);
    yasm_expr_destroy(bc->multiple);
    if (bc->symrecs)
	yasm_xfree(bc->symrecs);
    yasm_xfree(bc);
}

void
yasm_bc_print(const yasm_bytecode *bc, FILE *f, int indent_level)
{
    if (!bc->callback)
	fprintf(f, "%*s_Empty_\n", indent_level, "");
    else
	bc->callback->print(bc->contents, f, indent_level);
    fprintf(f, "%*sMultiple=", indent_level, "");
    if (!bc->multiple)
	fprintf(f, "nil (1)");
    else
	yasm_expr_print(bc->multiple, f);
    fprintf(f, "\n%*sLength=%lu\n", indent_level, "", bc->len);
    fprintf(f, "%*sLine Index=%lu\n", indent_level, "", bc->line);
    fprintf(f, "%*sOffset=%lx\n", indent_level, "", bc->offset);
}

void
yasm_bc_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc)
{
    if (bc->callback)
	bc->callback->finalize(bc, prev_bc);
    if (bc->multiple) {
	yasm_value val;

	if (yasm_value_finalize_expr(&val, bc->multiple, 0))
	    yasm_error_set(YASM_ERROR_TOO_COMPLEX,
			   N_("multiple expression too complex"));
	else if (val.rel)
	    yasm_error_set(YASM_ERROR_NOT_ABSOLUTE,
			   N_("multiple expression not absolute"));
	bc->multiple = val.abs;
    }
}

/*@null@*/ yasm_intnum *
yasm_calc_bc_dist(yasm_bytecode *precbc1, yasm_bytecode *precbc2)
{
    unsigned long dist;
    yasm_intnum *intn;

    if (precbc1->section != precbc2->section)
	return NULL;

    dist = precbc2->offset + precbc2->len;
    if (dist < precbc1->offset + precbc1->len) {
	intn = yasm_intnum_create_uint(precbc1->offset + precbc1->len - dist);
	yasm_intnum_calc(intn, YASM_EXPR_NEG, NULL);
	return intn;
    }
    dist -= precbc1->offset + precbc1->len;
    return yasm_intnum_create_uint(dist);
}

int
yasm_bc_calc_len(yasm_bytecode *bc, yasm_bc_add_span_func add_span,
		 void *add_span_data)
{
    int retval = 0;

    bc->len = 0;

    if (!bc->callback)
	yasm_internal_error(N_("got empty bytecode in bc_resolve"));
    else
	retval = bc->callback->calc_len(bc, add_span, add_span_data);
#if 0
    /* Check for multiples */
    if (bc->multiple) {
	/*@dependent@*/ /*@null@*/ const yasm_intnum *num;

	num = yasm_expr_get_intnum(&bc->multiple, NULL);
	if (!num) {
	    if (yasm_expr__contains(bc->multiple, YASM_EXPR_FLOAT)) {
		yasm_error_set(YASM_ERROR_VALUE,
		    N_("expression must not contain floating point value"));
		retval = -1;
	    } else {
		/* FIXME: Non-constant currently not allowed. */
		yasm__error(bc->line,
			    N_("attempt to use non-constant multiple"));
		retval = -1;
	    }
	}
    }
#endif
    /* If we got an error somewhere along the line, clear out any calc len */
    if (retval < 0)
	bc->len = 0;

    return retval;
}

int
yasm_bc_expand(yasm_bytecode *bc, int span, long old_val, long new_val,
	       /*@out@*/ long *neg_thres, /*@out@*/ long *pos_thres)
{
    if (!bc->callback) {
	yasm_internal_error(N_("got empty bytecode in bc_set_long"));
	/*@unreached@*/
	return 0;
    } else
	return bc->callback->expand(bc, span, old_val, new_val, neg_thres,
				    pos_thres);
}

/*@null@*/ /*@only@*/ unsigned char *
yasm_bc_tobytes(yasm_bytecode *bc, unsigned char *buf, unsigned long *bufsize,
		/*@out@*/ int *gap, void *d,
		yasm_output_value_func output_value,
		/*@null@*/ yasm_output_reloc_func output_reloc)
    /*@sets *buf@*/
{
    /*@only@*/ /*@null@*/ unsigned char *mybuf = NULL;
    unsigned char *origbuf, *destbuf;
    unsigned long datasize, multiple, i;
    int error = 0;

    if (yasm_bc_get_multiple(bc, &multiple, 1) || multiple == 0) {
	*bufsize = 0;
	return NULL;
    }

    /* special case for reserve bytecodes */
    if (bc->callback->reserve) {
	*bufsize = bc->len;
	*gap = 1;
	return NULL;	/* we didn't allocate a buffer */
    }
    *gap = 0;

    if (*bufsize < bc->len) {
	mybuf = yasm_xmalloc(bc->len);
	destbuf = mybuf;
    } else
	destbuf = buf;

    *bufsize = bc->len;
    datasize = bc->len / multiple;

    if (!bc->callback)
	yasm_internal_error(N_("got empty bytecode in bc_tobytes"));
    else for (i=0; i<multiple; i++) {
	origbuf = destbuf;
	error = bc->callback->tobytes(bc, &destbuf, d, output_value,
				      output_reloc);

	if (!error && ((unsigned long)(destbuf - origbuf) != datasize))
	    yasm_internal_error(
		N_("written length does not match optimized length"));
    }

    return mybuf;
}

int
yasm_bc_get_multiple(yasm_bytecode *bc, unsigned long *multiple,
		     int calc_bc_dist)
{
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;

    *multiple = 1;
    if (bc->multiple) {
	num = yasm_expr_get_intnum(&bc->multiple, calc_bc_dist);
	if (!num) {
	    yasm_error_set(YASM_ERROR_VALUE,
			   N_("could not determine multiple"));
	    return 1;
	}
	if (yasm_intnum_sign(num) < 0) {
	    yasm_error_set(YASM_ERROR_VALUE, N_("multiple is negative"));
	    return 1;
	}
	*multiple = yasm_intnum_get_uint(num);
    }
    return 0;
}
