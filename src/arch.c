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


static /*@dependent@*/ arch *cur_arch;

void
arch_common_initialize(arch *a)
{
    cur_arch = a;
}

insn_operand *
operand_new_reg(unsigned long reg)
{
    insn_operand *retval = xmalloc(sizeof(insn_operand));

    retval->type = INSN_OPERAND_REG;
    retval->data.reg = reg;
    retval->targetmod = 0;
    retval->size = 0;

    return retval;
}

insn_operand *
operand_new_segreg(unsigned long segreg)
{
    insn_operand *retval = xmalloc(sizeof(insn_operand));

    retval->type = INSN_OPERAND_SEGREG;
    retval->data.reg = segreg;
    retval->targetmod = 0;
    retval->size = 0;

    return retval;
}

insn_operand *
operand_new_mem(/*@only@*/ effaddr *ea)
{
    insn_operand *retval = xmalloc(sizeof(insn_operand));

    retval->type = INSN_OPERAND_MEMORY;
    retval->data.ea = ea;
    retval->targetmod = 0;
    retval->size = 0;

    return retval;
}

insn_operand *
operand_new_imm(/*@only@*/ expr *val)
{
    insn_operand *retval;
    const unsigned long *reg;

    reg = expr_get_reg(&val, 0);
    if (reg) {
	retval = operand_new_reg(*reg);
	expr_delete(val);
    } else {
	retval = xmalloc(sizeof(insn_operand));
	retval->type = INSN_OPERAND_IMM;
	retval->data.val = val;
	retval->targetmod = 0;
	retval->size = 0;
    }

    return retval;
}

void
operand_print(FILE *f, int indent_level, const insn_operand *op)
{
    switch (op->type) {
	case INSN_OPERAND_REG:
	    fprintf(f, "%*sReg=", indent_level, "");
	    cur_arch->reg_print(f, op->data.reg);
	    fprintf(f, "\n");
	    break;
	case INSN_OPERAND_SEGREG:
	    fprintf(f, "%*sSegReg=", indent_level, "");
	    cur_arch->segreg_print(f, op->data.reg);
	    fprintf(f, "\n");
	    break;
	case INSN_OPERAND_MEMORY:
	    fprintf(f, "%*sMemory=\n", indent_level, "");
	    ea_print(f, indent_level, op->data.ea);
	    break;
	case INSN_OPERAND_IMM:
	    fprintf(f, "%*sImm=", indent_level, "");
	    expr_print(f, op->data.val);
	    fprintf(f, "\n");
	    break;
    }
    fprintf(f, "%*sTargetMod=%lx\n", indent_level+1, "", op->targetmod);
    fprintf(f, "%*sSize=%u\n", indent_level+1, "", op->size);
}

void
ops_delete(insn_operandhead *headp, int content)
{
    insn_operand *cur, *next;

    cur = STAILQ_FIRST(headp);
    while (cur) {
	next = STAILQ_NEXT(cur, link);
	if (content)
	    switch (cur->type) {
		case INSN_OPERAND_MEMORY:
		    ea_delete(cur->data.ea);
		    break;
		case INSN_OPERAND_IMM:
		    expr_delete(cur->data.val);
		    break;
		default:
		    break;
	    }
	xfree(cur);
	cur = next;
    }
    STAILQ_INIT(headp);
}

/*@null@*/ insn_operand *
ops_append(insn_operandhead *headp, /*@returned@*/ /*@null@*/ insn_operand *op)
{
    if (op) {
	STAILQ_INSERT_TAIL(headp, op, link);
	return op;
    }
    return (insn_operand *)NULL;
}

void
ops_print(FILE *f, int indent_level, const insn_operandhead *headp)
{
    insn_operand *cur;

    STAILQ_FOREACH (cur, headp, link)
	operand_print(f, indent_level, cur);
}
