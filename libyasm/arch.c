/*
 * Architecture interface
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
#define YASM_ARCH_INTERNAL
#include "util.h"
/*@unused@*/ RCSID("$Id$");

#include "coretype.h"

#include "expr.h"

#include "bytecode.h"

#include "arch.h"


yasm_insn_operand *
yasm_operand_create_reg(unsigned long reg)
{
    yasm_insn_operand *retval = yasm_xmalloc(sizeof(yasm_insn_operand));

    retval->type = YASM_INSN__OPERAND_REG;
    retval->data.reg = reg;
    retval->targetmod = 0;
    retval->size = 0;
    retval->deref = 0;
    retval->strict = 0;

    return retval;
}

yasm_insn_operand *
yasm_operand_create_segreg(unsigned long segreg)
{
    yasm_insn_operand *retval = yasm_xmalloc(sizeof(yasm_insn_operand));

    retval->type = YASM_INSN__OPERAND_SEGREG;
    retval->data.reg = segreg;
    retval->targetmod = 0;
    retval->size = 0;
    retval->deref = 0;
    retval->strict = 0;

    return retval;
}

yasm_insn_operand *
yasm_operand_create_mem(/*@only@*/ yasm_effaddr *ea)
{
    yasm_insn_operand *retval = yasm_xmalloc(sizeof(yasm_insn_operand));

    retval->type = YASM_INSN__OPERAND_MEMORY;
    retval->data.ea = ea;
    retval->targetmod = 0;
    retval->size = 0;
    retval->deref = 0;
    retval->strict = 0;

    return retval;
}

yasm_insn_operand *
yasm_operand_create_imm(/*@only@*/ yasm_expr *val)
{
    yasm_insn_operand *retval;
    const unsigned long *reg;

    reg = yasm_expr_get_reg(&val, 0);
    if (reg) {
	retval = yasm_operand_create_reg(*reg);
	yasm_expr_destroy(val);
    } else {
	retval = yasm_xmalloc(sizeof(yasm_insn_operand));
	retval->type = YASM_INSN__OPERAND_IMM;
	retval->data.val = val;
	retval->targetmod = 0;
	retval->size = 0;
	retval->deref = 0;
	retval->strict = 0;
    }

    return retval;
}

void
yasm_operand_print(const yasm_insn_operand *op, FILE *f, int indent_level,
		   yasm_arch *arch)
{
    switch (op->type) {
	case YASM_INSN__OPERAND_REG:
	    fprintf(f, "%*sReg=", indent_level, "");
	    yasm_arch_reg_print(arch, op->data.reg, f);
	    fprintf(f, "\n");
	    break;
	case YASM_INSN__OPERAND_SEGREG:
	    fprintf(f, "%*sSegReg=", indent_level, "");
	    yasm_arch_segreg_print(arch, op->data.reg, f);
	    fprintf(f, "\n");
	    break;
	case YASM_INSN__OPERAND_MEMORY:
	    fprintf(f, "%*sMemory=\n", indent_level, "");
	    yasm_ea_print(op->data.ea, f, indent_level);
	    break;
	case YASM_INSN__OPERAND_IMM:
	    fprintf(f, "%*sImm=", indent_level, "");
	    yasm_expr_print(op->data.val, f);
	    fprintf(f, "\n");
	    break;
    }
    fprintf(f, "%*sTargetMod=%lx\n", indent_level+1, "", op->targetmod);
    fprintf(f, "%*sSize=%u\n", indent_level+1, "", op->size);
    fprintf(f, "%*sDeref=%d, Strict=%d\n", indent_level+1, "", (int)op->deref,
	    (int)op->strict);
}

void
yasm_ops_delete(yasm_insn_operands *headp, int content)
{
    yasm_insn_operand *cur, *next;

    cur = STAILQ_FIRST(headp);
    while (cur) {
	next = STAILQ_NEXT(cur, link);
	if (content)
	    switch (cur->type) {
		case YASM_INSN__OPERAND_MEMORY:
		    yasm_ea_destroy(cur->data.ea);
		    break;
		case YASM_INSN__OPERAND_IMM:
		    yasm_expr_destroy(cur->data.val);
		    break;
		default:
		    break;
	    }
	yasm_xfree(cur);
	cur = next;
    }
    STAILQ_INIT(headp);
}

/*@null@*/ yasm_insn_operand *
yasm_ops_append(yasm_insn_operands *headp,
		/*@returned@*/ /*@null@*/ yasm_insn_operand *op)
{
    if (op) {
	STAILQ_INSERT_TAIL(headp, op, link);
	return op;
    }
    return (yasm_insn_operand *)NULL;
}

void
yasm_ops_print(const yasm_insn_operands *headp, FILE *f, int indent_level,
	       yasm_arch *arch)
{
    yasm_insn_operand *cur;

    STAILQ_FOREACH (cur, headp, link)
	yasm_operand_print(cur, f, indent_level, arch);
}

yasm_insn_operands *
yasm_ops_create(void)
{
    yasm_insn_operands *headp = yasm_xmalloc(sizeof(yasm_insn_operands));
    yasm_ops_initialize(headp);
    return headp;
}

void
yasm_ops_destroy(yasm_insn_operands *headp, int content)
{
    yasm_ops_delete(headp, content);
    yasm_xfree(headp);
}

/* Non-macro yasm_ops_first() for non-YASM_LIB_INTERNAL users. */
#undef yasm_ops_first
yasm_insn_operand *
yasm_ops_first(yasm_insn_operands *headp)
{
    return STAILQ_FIRST(headp);
}

/* Non-macro yasm_operand_next() for non-YASM_LIB_INTERNAL users. */
#undef yasm_operand_next
yasm_insn_operand *
yasm_operand_next(yasm_insn_operand *cur)
{
    return STAILQ_NEXT(cur, link);
}
