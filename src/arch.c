/*
 * Architecture interface
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
