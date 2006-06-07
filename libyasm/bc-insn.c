/*
 * Insn bytecode
 *
 *  Copyright (C) 2005-2006  Peter Johnson
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
#include "expr.h"
#include "value.h"

#include "bytecode.h"
#include "arch.h"

#include "bc-int.h"


typedef struct bytecode_insn {
    /*@dependent@*/ yasm_arch *arch;
    unsigned long insn_data[4];

    int num_operands;
    /*@null@*/ yasm_insn_operands operands;

    /* array of 4-element prefix_data arrays */
    int num_prefixes;
    /*@null@*/ unsigned long **prefixes;

    /* array of segment prefixes */
    int num_segregs;
    /*@null@*/ unsigned long *segregs;
} bytecode_insn;

static void bc_insn_destroy(void *contents);
static void bc_insn_print(const void *contents, FILE *f, int indent_level);
static void bc_insn_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc);
static int bc_insn_calc_len(yasm_bytecode *bc, yasm_bc_add_span_func add_span,
			    void *add_span_data);
static int bc_insn_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
			   yasm_output_value_func output_value,
			   /*@null@*/ yasm_output_reloc_func output_reloc);

static const yasm_bytecode_callback bc_insn_callback = {
    bc_insn_destroy,
    bc_insn_print,
    bc_insn_finalize,
    bc_insn_calc_len,
    yasm_bc_expand_common,
    bc_insn_tobytes,
    0
};


yasm_immval *
yasm_imm_create_expr(yasm_expr *e)
{
    yasm_immval *im = yasm_xmalloc(sizeof(yasm_immval));

    if (yasm_value_finalize_expr(&im->val, e, 0))
	yasm_error_set(YASM_ERROR_TOO_COMPLEX,
		       N_("immediate expression too complex"));
    im->sign = 0;

    return im;
}

const yasm_expr *
yasm_ea_get_disp(const yasm_effaddr *ea)
{
    return ea->disp.abs;
}

void
yasm_ea_set_len(yasm_effaddr *ptr, unsigned int len)
{
    if (!ptr)
	return;

    /* Currently don't warn if length truncated, as this is called only from
     * an explicit override, where we expect the user knows what they're doing.
     */

    ptr->disp.size = (unsigned char)len;
}

void
yasm_ea_set_nosplit(yasm_effaddr *ptr, unsigned int nosplit)
{
    if (!ptr)
	return;

    ptr->nosplit = (unsigned char)nosplit;
}

void
yasm_ea_set_strong(yasm_effaddr *ptr, unsigned int strong)
{
    if (!ptr)
	return;

    ptr->strong = (unsigned char)strong;
}

void
yasm_ea_set_segreg(yasm_effaddr *ea, unsigned long segreg)
{
    if (!ea)
	return;

    if (segreg != 0 && ea->segreg != 0)
	yasm_warn_set(YASM_WARN_GENERAL,
		      N_("multiple segment overrides, using leftmost"));

    ea->segreg = segreg;
}

/*@-nullstate@*/
void
yasm_ea_destroy(yasm_effaddr *ea)
{
    ea->callback->destroy(ea);
    yasm_value_delete(&ea->disp);
    yasm_xfree(ea);
}
/*@=nullstate@*/

/*@-nullstate@*/
void
yasm_ea_print(const yasm_effaddr *ea, FILE *f, int indent_level)
{
    fprintf(f, "%*sDisp:\n", indent_level, "");
    yasm_value_print(&ea->disp, f, indent_level+1);
    fprintf(f, "%*sNoSplit=%u\n", indent_level, "", (unsigned int)ea->nosplit);
    ea->callback->print(ea, f, indent_level);
}
/*@=nullstate@*/

static void
bc_insn_destroy(void *contents)
{
    bytecode_insn *insn = (bytecode_insn *)contents;
    if (insn->num_operands > 0)
	yasm_ops_delete(&insn->operands, 0);
    if (insn->num_prefixes > 0) {
	int i;
	for (i=0; i<insn->num_prefixes; i++)
	    yasm_xfree(insn->prefixes[i]);
	yasm_xfree(insn->prefixes);
    }
    if (insn->num_segregs > 0)
	yasm_xfree(insn->segregs);
    yasm_xfree(contents);
}

static void
bc_insn_print(const void *contents, FILE *f, int indent_level)
{
}

static void
bc_insn_finalize(yasm_bytecode *bc, yasm_bytecode *prev_bc)
{
    bytecode_insn *insn = (bytecode_insn *)bc->contents;
    int i;
    yasm_insn_operand *op;
    yasm_error_class eclass;
    char *str, *xrefstr;
    unsigned long xrefline;

    /* Simplify the operands' expressions first. */
    for (i = 0, op = yasm_ops_first(&insn->operands);
	 op && i<insn->num_operands; op = yasm_operand_next(op), i++) {
	/* Check operand type */
	switch (op->type) {
	    case YASM_INSN__OPERAND_MEMORY:
		/* Don't get over-ambitious here; some archs' memory expr
		 * parser are sensitive to the presence of *1, etc, so don't
		 * simplify reg*1 identities.
		 */
		if (op->data.ea)
		    op->data.ea->disp.abs =
			yasm_expr__level_tree(op->data.ea->disp.abs, 1, 1, 0,
					      0, NULL, NULL, NULL);
		if (yasm_error_occurred()) {
		    /* Add a pointer to where it was used to the error */
		    yasm_error_fetch(&eclass, &str, &xrefline, &xrefstr);
		    if (xrefstr) {
			yasm_error_set_xref(xrefline, "%s", xrefstr);
			yasm_xfree(xrefstr);
		    }
		    if (str) {
			yasm_error_set(eclass, "%s in memory expression", str);
			yasm_xfree(str);
		    }
		    return;
		}
		break;
	    case YASM_INSN__OPERAND_IMM:
		op->data.val =
		    yasm_expr__level_tree(op->data.val, 1, 1, 1, 0, NULL, NULL,
					  NULL);
		if (yasm_error_occurred()) {
		    /* Add a pointer to where it was used to the error */
		    yasm_error_fetch(&eclass, &str, &xrefline, &xrefstr);
		    if (xrefstr) {
			yasm_error_set_xref(xrefline, "%s", xrefstr);
			yasm_xfree(xrefstr);
		    }
		    if (str) {
			yasm_error_set(eclass, "%s in immediate expression",
				       str);
			yasm_xfree(str);
		    }
		    return;
		}
		break;
	    default:
		break;
	}
    }

    yasm_arch_finalize_insn(insn->arch, bc, prev_bc, insn->insn_data,
			    insn->num_operands, &insn->operands,
			    insn->num_prefixes, insn->prefixes,
			    insn->num_segregs, insn->segregs);
}

static int
bc_insn_calc_len(yasm_bytecode *bc, yasm_bc_add_span_func add_span,
		 void *add_span_data)
{
    yasm_internal_error(N_("bc_insn_calc_len() is not implemented"));
    /*@notreached@*/
    return 0;
}

static int
bc_insn_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		yasm_output_value_func output_value,
		/*@unused@*/ yasm_output_reloc_func output_reloc)
{
    yasm_internal_error(N_("bc_insn_tobytes() is not implemented"));
    /*@notreached@*/
    return 1;
}

yasm_bytecode *
yasm_bc_create_insn(yasm_arch *arch, const unsigned long insn_data[4],
		    int num_operands, /*@null@*/ yasm_insn_operands *operands,
		    unsigned long line)
{
    bytecode_insn *insn = yasm_xmalloc(sizeof(bytecode_insn));

    insn->arch = arch;
    insn->insn_data[0] = insn_data[0];
    insn->insn_data[1] = insn_data[1];
    insn->insn_data[2] = insn_data[2];
    insn->insn_data[3] = insn_data[3];
    insn->num_operands = num_operands;
    if (operands)
	insn->operands = *operands;	/* structure copy */
    else
	yasm_ops_initialize(&insn->operands);
    insn->num_prefixes = 0;
    insn->prefixes = NULL;
    insn->num_segregs = 0;
    insn->segregs = NULL;

    return yasm_bc_create_common(&bc_insn_callback, insn, line);
}

yasm_bytecode *
yasm_bc_create_empty_insn(yasm_arch *arch, unsigned long line)
{
    bytecode_insn *insn = yasm_xmalloc(sizeof(bytecode_insn));

    insn->arch = arch;
    insn->insn_data[0] = 0;
    insn->insn_data[1] = 0;
    insn->insn_data[2] = 0;
    insn->insn_data[3] = 0;
    insn->num_operands = 0;
    yasm_ops_initialize(&insn->operands);
    insn->num_prefixes = 0;
    insn->prefixes = NULL;
    insn->num_segregs = 0;
    insn->segregs = NULL;

    return yasm_bc_create_common(&bc_insn_callback, insn, line);
}

void
yasm_bc_insn_add_prefix(yasm_bytecode *bc, const unsigned long prefix_data[4])
{
    bytecode_insn *insn = (bytecode_insn *)bc->contents;

    assert(bc->callback == bc_insn_callback);

    insn->prefixes =
	yasm_xrealloc(insn->prefixes,
		      (insn->num_prefixes+1)*sizeof(unsigned long *));
    insn->prefixes[insn->num_prefixes] =
	yasm_xmalloc(4*sizeof(unsigned long));
    insn->prefixes[insn->num_prefixes][0] = prefix_data[0];
    insn->prefixes[insn->num_prefixes][1] = prefix_data[1];
    insn->prefixes[insn->num_prefixes][2] = prefix_data[2];
    insn->prefixes[insn->num_prefixes][3] = prefix_data[3];
    insn->num_prefixes++;
}

void
yasm_bc_insn_add_seg_prefix(yasm_bytecode *bc, unsigned long segreg)
{
    bytecode_insn *insn = (bytecode_insn *)bc->contents;

    assert(bc->callback == bc_insn_callback);

    insn->segregs =
	yasm_xrealloc(insn->segregs,
		      (insn->num_segregs+1)*sizeof(unsigned long));
    insn->segregs[insn->num_segregs] = segreg;
    insn->num_segregs++;
}
