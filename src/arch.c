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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "expr.h"

#include "bytecode.h"

#include "arch.h"


static /*@dependent@*/ yasm_arch *cur_arch;

void
yasm_arch_common_initialize(yasm_arch *a)
{
    cur_arch = a;
}

yasm_insn_operand *
yasm_operand_new_reg(unsigned long reg)
{
    yasm_insn_operand *retval = yasm_xmalloc(sizeof(yasm_insn_operand));

    retval->type = YASM_INSN__OPERAND_REG;
    retval->data.reg = reg;
    retval->targetmod = 0;
    retval->size = 0;

    return retval;
}

yasm_insn_operand *
yasm_operand_new_segreg(unsigned long segreg)
{
    yasm_insn_operand *retval = yasm_xmalloc(sizeof(yasm_insn_operand));

    retval->type = YASM_INSN__OPERAND_SEGREG;
    retval->data.reg = segreg;
    retval->targetmod = 0;
    retval->size = 0;

    return retval;
}

yasm_insn_operand *
yasm_operand_new_mem(/*@only@*/ yasm_effaddr *ea)
{
    yasm_insn_operand *retval = yasm_xmalloc(sizeof(yasm_insn_operand));

    retval->type = YASM_INSN__OPERAND_MEMORY;
    retval->data.ea = ea;
    retval->targetmod = 0;
    retval->size = 0;

    return retval;
}

yasm_insn_operand *
yasm_operand_new_imm(/*@only@*/ yasm_expr *val)
{
    yasm_insn_operand *retval;
    const unsigned long *reg;

    reg = yasm_expr_get_reg(&val, 0);
    if (reg) {
	retval = yasm_operand_new_reg(*reg);
	yasm_expr_delete(val);
    } else {
	retval = yasm_xmalloc(sizeof(yasm_insn_operand));
	retval->type = YASM_INSN__OPERAND_IMM;
	retval->data.val = val;
	retval->targetmod = 0;
	retval->size = 0;
    }

    return retval;
}

void
yasm_operand_print(FILE *f, int indent_level, const yasm_insn_operand *op)
{
    switch (op->type) {
	case YASM_INSN__OPERAND_REG:
	    fprintf(f, "%*sReg=", indent_level, "");
	    cur_arch->reg_print(f, op->data.reg);
	    fprintf(f, "\n");
	    break;
	case YASM_INSN__OPERAND_SEGREG:
	    fprintf(f, "%*sSegReg=", indent_level, "");
	    cur_arch->segreg_print(f, op->data.reg);
	    fprintf(f, "\n");
	    break;
	case YASM_INSN__OPERAND_MEMORY:
	    fprintf(f, "%*sMemory=\n", indent_level, "");
	    yasm_ea_print(f, indent_level, op->data.ea);
	    break;
	case YASM_INSN__OPERAND_IMM:
	    fprintf(f, "%*sImm=", indent_level, "");
	    yasm_expr_print(f, op->data.val);
	    fprintf(f, "\n");
	    break;
    }
    fprintf(f, "%*sTargetMod=%lx\n", indent_level+1, "", op->targetmod);
    fprintf(f, "%*sSize=%u\n", indent_level+1, "", op->size);
}

void
yasm_ops_delete(yasm_insn_operandhead *headp, int content)
{
    yasm_insn_operand *cur, *next;

    cur = STAILQ_FIRST(headp);
    while (cur) {
	next = STAILQ_NEXT(cur, link);
	if (content)
	    switch (cur->type) {
		case YASM_INSN__OPERAND_MEMORY:
		    yasm_ea_delete(cur->data.ea);
		    break;
		case YASM_INSN__OPERAND_IMM:
		    yasm_expr_delete(cur->data.val);
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
yasm_ops_append(yasm_insn_operandhead *headp,
		/*@returned@*/ /*@null@*/ yasm_insn_operand *op)
{
    if (op) {
	STAILQ_INSERT_TAIL(headp, op, link);
	return op;
    }
    return (yasm_insn_operand *)NULL;
}

void
yasm_ops_print(FILE *f, int indent_level, const yasm_insn_operandhead *headp)
{
    yasm_insn_operand *cur;

    STAILQ_FOREACH (cur, headp, link)
	yasm_operand_print(f, indent_level, cur);
}
