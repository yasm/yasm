/*
 * x86 bytecode utility functions
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

#include "errwarn.h"
#include "intnum.h"
#include "expr.h"

#include "bytecode.h"
#include "arch.h"

#include "x86-int.h"

#include "bc-int.h"


/*@-compmempass -mustfree@*/
bytecode *
x86_bc_new_insn(x86_new_insn_data *d)
{
    bytecode *bc;
    x86_insn *insn;
   
    bc = bc_new_common((bytecode_type)X86_BC_INSN, sizeof(x86_insn));
    insn = bc_get_data(bc);

    insn->ea = d->ea;
    if (d->ea) {
	x86_effaddr_data *ead = ea_get_data(d->ea);
	ead->modrm &= 0xC7;	/* zero spare/reg bits */
	ead->modrm |= (d->spare << 3) & 0x38;	/* plug in provided bits */
    }

    insn->imm = d->imm;
    if (d->imm) {
	insn->imm->f_len = d->im_len;
	insn->imm->f_sign = d->im_sign;
    }

    insn->opcode[0] = d->op[0];
    insn->opcode[1] = d->op[1];
    insn->opcode[2] = d->op[2];
    insn->opcode_len = d->op_len;

    insn->addrsize = 0;
    insn->opersize = d->opersize;
    insn->lockrep_pre = 0;
    insn->shift_op = 0;

    insn->mode_bits = x86_mode_bits;

    return bc;
}
/*@=compmempass =mustfree@*/

/*@-compmempass -mustfree@*/
bytecode *
x86_bc_new_jmprel(x86_new_jmprel_data *d)
{
    bytecode *bc;
    x86_jmprel *jmprel;

    bc = bc_new_common((bytecode_type)X86_BC_JMPREL, sizeof(x86_jmprel));
    jmprel = bc_get_data(bc);

    jmprel->target = d->target->val;
    jmprel->op_sel = d->target->op_sel;

    if ((d->target->op_sel == JR_SHORT_FORCED) && (d->near_op_len == 0))
	Error(_("no SHORT form of that jump instruction exists"));
    if ((d->target->op_sel == JR_NEAR_FORCED) && (d->short_op_len == 0))
	Error(_("no NEAR form of that jump instruction exists"));

    jmprel->shortop.opcode[0] = d->short_op[0];
    jmprel->shortop.opcode[1] = d->short_op[1];
    jmprel->shortop.opcode[2] = d->short_op[2];
    jmprel->shortop.opcode_len = d->short_op_len;

    jmprel->nearop.opcode[0] = d->near_op[0];
    jmprel->nearop.opcode[1] = d->near_op[1];
    jmprel->nearop.opcode[2] = d->near_op[2];
    jmprel->nearop.opcode_len = d->near_op_len;

    jmprel->addrsize = d->addrsize;
    jmprel->opersize = 0;
    jmprel->lockrep_pre = 0;

    jmprel->mode_bits = x86_mode_bits;

    return bc;
}
/*@=compmempass =mustfree@*/

void
x86_ea_set_segment(effaddr *ea, unsigned char segment)
{
    x86_effaddr_data *ead;

    if (!ea)
	return;

    ead = ea_get_data(ea);

    if (segment != 0 && ead->segment != 0)
	Warning(_("multiple segment overrides, using leftmost"));

    ead->segment = segment;
}

effaddr *
x86_ea_new_reg(unsigned char reg)
{
    effaddr *ea = xmalloc(sizeof(effaddr)+sizeof(x86_effaddr_data));
    x86_effaddr_data *ead = ea_get_data(ea);

    ea->disp = (expr *)NULL;
    ea->len = 0;
    ea->nosplit = 0;
    ead->segment = 0;
    ead->modrm = 0xC0 | (reg & 0x07);	/* Mod=11, R/M=Reg, Reg=0 */
    ead->valid_modrm = 1;
    ead->need_modrm = 1;
    ead->valid_sib = 0;
    ead->need_sib = 0;

    return ea;
}

effaddr *
x86_ea_new_expr(expr *e)
{
    effaddr *ea = xmalloc(sizeof(effaddr)+sizeof(x86_effaddr_data));
    x86_effaddr_data *ead = ea_get_data(ea);

    ea->disp = e;
    ea->len = 0;
    ea->nosplit = 0;
    ead->segment = 0;
    ead->modrm = 0;
    ead->valid_modrm = 0;
    ead->need_modrm = 1;
    ead->valid_sib = 0;
    ead->need_sib = 0xff;   /* we won't know until we know more about expr and
			       the BITS/address override setting */

    return ea;
}

/*@-compmempass@*/
effaddr *
x86_ea_new_imm(immval *imm, unsigned char im_len)
{
    effaddr *ea = xmalloc(sizeof(effaddr)+sizeof(x86_effaddr_data));
    x86_effaddr_data *ead = ea_get_data(ea);

    ea->disp = imm->val;
    ea->len = im_len;
    ea->nosplit = 0;
    ead->segment = 0;
    ead->modrm = 0;
    ead->valid_modrm = 0;
    ead->need_modrm = 0;
    ead->valid_sib = 0;
    ead->need_sib = 0;

    return ea;
}
/*@=compmempass@*/

effaddr *
x86_bc_insn_get_ea(bytecode *bc)
{
    x86_insn *insn = bc_get_data(bc);

    if (!bc)
	return NULL;

    if ((x86_bytecode_type)bc->type != X86_BC_INSN)
	InternalError(_("Trying to get EA of non-instruction"));

    return insn->ea;
}

void
x86_bc_insn_opersize_override(bytecode *bc, unsigned char opersize)
{
    x86_insn *insn;
    x86_jmprel *jmprel;

    if (!bc)
	return;

    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    insn = bc_get_data(bc);
	    insn->opersize = opersize;
	    break;
	case X86_BC_JMPREL:
	    jmprel = bc_get_data(bc);
	    jmprel->opersize = opersize;
	    break;
	default:
	    InternalError(_("OperSize override applied to non-instruction"));
    }
}

void
x86_bc_insn_addrsize_override(bytecode *bc, unsigned char addrsize)
{
    x86_insn *insn;
    x86_jmprel *jmprel;

    if (!bc)
	return;

    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    insn = bc_get_data(bc);
	    insn->addrsize = addrsize;
	    break;
	case X86_BC_JMPREL:
	    jmprel = bc_get_data(bc);
	    jmprel->addrsize = addrsize;
	    break;
	default:
	    InternalError(_("AddrSize override applied to non-instruction"));
    }
}

void
x86_bc_insn_set_lockrep_prefix(bytecode *bc, unsigned char prefix)
{
    x86_insn *insn;
    x86_jmprel *jmprel;
    unsigned char *lockrep_pre = (unsigned char *)NULL;

    if (!bc)
	return;

    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    insn = bc_get_data(bc);
	    lockrep_pre = &insn->lockrep_pre;
	    break;
	case X86_BC_JMPREL:
	    jmprel = bc_get_data(bc);
	    lockrep_pre = &jmprel->lockrep_pre;
	    break;
	default:
	    InternalError(_("LockRep prefix applied to non-instruction"));
    }

    if (*lockrep_pre != 0)
	Warning(_("multiple LOCK or REP prefixes, using leftmost"));

    *lockrep_pre = prefix;
}

void
x86_bc_insn_set_shift_flag(bytecode *bc)
{
    x86_insn *insn;

    if (!bc)
	return;

    if ((x86_bytecode_type)bc->type != X86_BC_INSN)
	InternalError(_("Attempted to set shift flag on non-instruction"));

    insn = bc_get_data(bc);

    insn->shift_op = 1;
}

void
x86_set_jmprel_opcode_sel(x86_jmprel_opcode_sel *old_sel,
			  x86_jmprel_opcode_sel new_sel)
{
    if (!old_sel)
	return;

    if (new_sel != JR_NONE && ((*old_sel == JR_SHORT_FORCED) ||
			       (*old_sel == JR_NEAR_FORCED)))
	Warning(_("multiple SHORT or NEAR specifiers, using leftmost"));
    *old_sel = new_sel;
}

void
x86_bc_delete(bytecode *bc)
{
    x86_insn *insn;
    x86_jmprel *jmprel;

    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    insn = bc_get_data(bc);
	    if (insn->ea) {
		expr_delete(insn->ea->disp);
		xfree(insn->ea);
	    }
	    if (insn->imm) {
		expr_delete(insn->imm->val);
		xfree(insn->imm);
	    }
	    break;
	case X86_BC_JMPREL:
	    jmprel = bc_get_data(bc);
	    expr_delete(jmprel->target);
	    break;
    }
}

void
x86_bc_print(const bytecode *bc)
{
    const x86_insn *insn;
    const x86_jmprel *jmprel;
    x86_effaddr_data *ead;

    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    insn = bc_get_const_data(bc);
	    printf("_Instruction_\n");
	    printf("Effective Address:");
	    if (!insn->ea)
		printf(" (nil)\n");
	    else {
		printf("\n Disp=");
		if (insn->ea->disp)
		    expr_print(insn->ea->disp);
		else
		    printf("(nil)");
		printf("\n");
		ead = ea_get_data(insn->ea);
		printf(" Len=%u SegmentOv=%02x NoSplit=%u\n",
		       (unsigned int)insn->ea->len,
		       (unsigned int)ead->segment,
		       (unsigned int)insn->ea->nosplit);
		printf(" ModRM=%03o ValidRM=%u NeedRM=%u\n",
		       (unsigned int)ead->modrm,
		       (unsigned int)ead->valid_modrm,
		       (unsigned int)ead->need_modrm);
		printf(" SIB=%03o ValidSIB=%u NeedSIB=%u\n",
		       (unsigned int)ead->sib,
		       (unsigned int)ead->valid_sib,
		       (unsigned int)ead->need_sib);
	    }
	    printf("Immediate Value:");
	    if (!insn->imm)
		printf(" (nil)\n");
	    else {
		printf("\n Val=");
		if (insn->imm->val)
		    expr_print(insn->imm->val);
		else
		    printf("(nil-SHOULDN'T HAPPEN)");
		printf("\n");
		printf(" Len=%u, IsNeg=%u\n",
		       (unsigned int)insn->imm->len,
		       (unsigned int)insn->imm->isneg);
		printf(" FLen=%u, FSign=%u\n",
		       (unsigned int)insn->imm->f_len,
		       (unsigned int)insn->imm->f_sign);
	    }
	    printf("Opcode: %02x %02x %02x OpLen=%u\n",
		   (unsigned int)insn->opcode[0],
		   (unsigned int)insn->opcode[1],
		   (unsigned int)insn->opcode[2],
		   (unsigned int)insn->opcode_len);
	    printf("AddrSize=%u OperSize=%u LockRepPre=%02x ShiftOp=%u\n",
		   (unsigned int)insn->addrsize,
		   (unsigned int)insn->opersize,
		   (unsigned int)insn->lockrep_pre,
		   (unsigned int)insn->shift_op);
	    printf("BITS=%u\n", (unsigned int)insn->mode_bits);
	    break;
	case X86_BC_JMPREL:
	    jmprel = bc_get_const_data(bc);
	    printf("_Relative Jump_\n");
	    printf("Target=");
	    expr_print(jmprel->target);
	    printf("\nShort Form:\n");
	    if (!jmprel->shortop.opcode_len == 0)
		printf(" None\n");
	    else
		printf(" Opcode: %02x %02x %02x OpLen=%u\n",
		       (unsigned int)jmprel->shortop.opcode[0],
		       (unsigned int)jmprel->shortop.opcode[1],
		       (unsigned int)jmprel->shortop.opcode[2],
		       (unsigned int)jmprel->shortop.opcode_len);
	    if (!jmprel->nearop.opcode_len == 0)
		printf(" None\n");
	    else
		printf(" Opcode: %02x %02x %02x OpLen=%u\n",
		       (unsigned int)jmprel->nearop.opcode[0],
		       (unsigned int)jmprel->nearop.opcode[1],
		       (unsigned int)jmprel->nearop.opcode[2],
		       (unsigned int)jmprel->nearop.opcode_len);
	    printf("OpSel=");
	    switch (jmprel->op_sel) {
		case JR_NONE:
		    printf("None");
		    break;
		case JR_SHORT:
		    printf("Short");
		    break;
		case JR_NEAR:
		    printf("Near");
		    break;
		case JR_SHORT_FORCED:
		    printf("Forced Short");
		    break;
		case JR_NEAR_FORCED:
		    printf("Forced Near");
		    break;
		default:
		    printf("UNKNOWN!!");
		    break;
	    }
	    printf("BITS=%u\nAddrSize=%u OperSize=%u LockRepPre=%02x\n",
		   (unsigned int)jmprel->mode_bits,
		   (unsigned int)jmprel->addrsize,
		   (unsigned int)jmprel->opersize,
		   (unsigned int)jmprel->lockrep_pre);
	    break;
    }
}

static void
x86_bc_parser_finalize_insn(x86_insn *insn)
{
    effaddr *ea = insn->ea;
    x86_effaddr_data *ead = ea_get_data(ea);
    immval *imm = insn->imm;

    if (ea) {
	if ((ea->disp) && ((!ead->valid_sib && ead->need_sib) ||
			   (!ead->valid_modrm && ead->need_modrm))) {
	    /* First expand equ's */
	    expr_expand_equ(ea->disp);

	    /* Check validity of effective address and calc R/M bits of
	     * Mod/RM byte and SIB byte.  We won't know the Mod field
	     * of the Mod/RM byte until we know more about the
	     * displacement.
	     */
	    if (!x86_expr_checkea(&ea->disp, &insn->addrsize, insn->mode_bits,
				  ea->nosplit, &ea->len, &ead->modrm,
				  &ead->valid_modrm, &ead->need_modrm,
				  &ead->sib, &ead->valid_sib, &ead->need_sib))
		return;	    /* failed, don't bother checking rest of insn */
	}
    }

    if (imm) {
	const intnum *num;

	if (imm->val) {
	    expr_expand_equ(imm->val);
	    imm->val = expr_simplify(imm->val);
	}
	/* TODO: check imm f_len vs. len? */

	/* Handle shift_op special-casing */
	/*@-nullstate@*/
	if (insn->shift_op && (num = expr_get_intnum(&imm->val))) {
	/*@=nullstate@*/
	    if (num) {
		if (intnum_get_uint(num) == 1) {
		    /* Use ,1 form: first copy ,1 opcode. */
		    insn->opcode[0] = insn->opcode[1];
		    /* Delete ModRM, as it's no longer needed */
		    xfree(ea);
		    insn->ea = (effaddr *)NULL;
		    /* Delete Imm, as it's not needed */
		    expr_delete(imm->val);
		    xfree(imm);
		    insn->imm = (immval *)NULL;
		}
		insn->shift_op = 0;
	    }
	}
    }

    
}

void
x86_bc_parser_finalize(bytecode *bc)
{
    x86_insn *insn;

    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    insn = bc_get_data(bc);
	    x86_bc_parser_finalize_insn(insn);
	    break;
	default:
	    break;
    }
}

