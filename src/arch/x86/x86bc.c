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

#include "file.h"

#include "globals.h"
#include "errwarn.h"
#include "intnum.h"
#include "expr.h"

#include "bytecode.h"
#include "arch.h"

#include "x86arch.h"

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

    if (d->imm) {
	insn->imm = imm_new_expr(d->imm);
	insn->imm->len = d->im_len;
	insn->imm->sign = d->im_sign;
    } else
	insn->imm = NULL;

    insn->opcode[0] = d->op[0];
    insn->opcode[1] = d->op[1];
    insn->opcode[2] = d->op[2];
    insn->opcode_len = d->op_len;

    insn->addrsize = 0;
    insn->opersize = d->opersize;
    insn->lockrep_pre = 0;
    insn->shift_op = d->shift_op;
    insn->signext_imm8_op = d->signext_imm8_op;

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

    jmprel->target = d->target;
    jmprel->op_sel = d->op_sel;

    if ((d->op_sel == JR_SHORT_FORCED) && (d->near_op_len == 0))
	Error(_("no SHORT form of that jump instruction exists"));
    if ((d->op_sel == JR_NEAR_FORCED) && (d->short_op_len == 0))
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
    jmprel->opersize = d->opersize;
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
    ead->sib = 0;
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
    ead->sib = 0;
    ead->valid_sib = 0;
    ead->need_sib = 0xff;   /* we won't know until we know more about expr and
			       the BITS/address override setting */

    return ea;
}

/*@-compmempass@*/
effaddr *
x86_ea_new_imm(expr *imm, unsigned char im_len)
{
    effaddr *ea = xmalloc(sizeof(effaddr)+sizeof(x86_effaddr_data));
    x86_effaddr_data *ead = ea_get_data(ea);

    ea->disp = imm;
    ea->len = im_len;
    ea->nosplit = 0;
    ead->segment = 0;
    ead->modrm = 0;
    ead->valid_modrm = 0;
    ead->need_modrm = 0;
    ead->sib = 0;
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
	    if (insn->ea)
		ea_delete(insn->ea);
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
x86_ea_data_print(FILE *f, const effaddr *ea)
{
    const x86_effaddr_data *ead = ea_get_const_data(ea);
    fprintf(f, "%*sSegmentOv=%02x\n", indent_level, "",
	    (unsigned int)ead->segment);
    fprintf(f, "%*sModRM=%03o ValidRM=%u NeedRM=%u\n", indent_level, "",
	    (unsigned int)ead->modrm, (unsigned int)ead->valid_modrm,
	    (unsigned int)ead->need_modrm);
    fprintf(f, "%*sSIB=%03o ValidSIB=%u NeedSIB=%u\n", indent_level, "",
	    (unsigned int)ead->sib, (unsigned int)ead->valid_sib,
	    (unsigned int)ead->need_sib);
}

void
x86_bc_print(FILE *f, const bytecode *bc)
{
    const x86_insn *insn;
    const x86_jmprel *jmprel;

    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    insn = bc_get_const_data(bc);
	    fprintf(f, "%*s_Instruction_\n", indent_level, "");
	    fprintf(f, "%*sEffective Address:", indent_level, "");
	    if (insn->ea) {
		fprintf(f, "\n");
		indent_level++;
		ea_print(f, insn->ea);
		indent_level--;
	    } else
		fprintf(f, " (nil)\n");
	    fprintf(f, "%*sImmediate Value:", indent_level, "");
	    if (!insn->imm)
		fprintf(f, " (nil)\n");
	    else {
		indent_level++;
		fprintf(f, "\n%*sVal=", indent_level, "");
		if (insn->imm->val)
		    expr_print(f, insn->imm->val);
		else
		    fprintf(f, "(nil-SHOULDN'T HAPPEN)");
		fprintf(f, "\n");
		fprintf(f, "%*sLen=%u, Sign=%u\n", indent_level, "",
			(unsigned int)insn->imm->len,
			(unsigned int)insn->imm->sign);
		indent_level--;
	    }
	    fprintf(f, "%*sOpcode: %02x %02x %02x OpLen=%u\n", indent_level,
		    "", (unsigned int)insn->opcode[0],
		    (unsigned int)insn->opcode[1],
		    (unsigned int)insn->opcode[2],
		    (unsigned int)insn->opcode_len);
	    fprintf(f,
		    "%*sAddrSize=%u OperSize=%u LockRepPre=%02x ShiftOp=%u\n",
		    indent_level, "",
		    (unsigned int)insn->addrsize,
		    (unsigned int)insn->opersize,
		    (unsigned int)insn->lockrep_pre,
		    (unsigned int)insn->shift_op);
	    fprintf(f, "%*sBITS=%u\n", indent_level, "",
		    (unsigned int)insn->mode_bits);
	    break;
	case X86_BC_JMPREL:
	    jmprel = bc_get_const_data(bc);
	    fprintf(f, "%*s_Relative Jump_\n", indent_level, "");
	    fprintf(f, "%*sTarget=", indent_level, "");
	    expr_print(f, jmprel->target);
	    fprintf(f, "\n%*sShort Form:\n", indent_level, "");
	    indent_level++;
	    if (jmprel->shortop.opcode_len == 0)
		fprintf(f, "%*sNone\n", indent_level, "");
	    else
		fprintf(f, "%*sOpcode: %02x %02x %02x OpLen=%u\n",
			indent_level, "",
			(unsigned int)jmprel->shortop.opcode[0],
			(unsigned int)jmprel->shortop.opcode[1],
			(unsigned int)jmprel->shortop.opcode[2],
			(unsigned int)jmprel->shortop.opcode_len);
	    indent_level--;
	    fprintf(f, "%*sNear Form:\n", indent_level, "");
	    indent_level++;
	    if (jmprel->nearop.opcode_len == 0)
		fprintf(f, "%*sNone\n", indent_level, "");
	    else
		fprintf(f, "%*sOpcode: %02x %02x %02x OpLen=%u\n",
			indent_level, "",
			(unsigned int)jmprel->nearop.opcode[0],
			(unsigned int)jmprel->nearop.opcode[1],
			(unsigned int)jmprel->nearop.opcode[2],
			(unsigned int)jmprel->nearop.opcode_len);
	    indent_level--;
	    fprintf(f, "%*sOpSel=", indent_level, "");
	    switch (jmprel->op_sel) {
		case JR_NONE:
		    fprintf(f, "None");
		    break;
		case JR_SHORT:
		    fprintf(f, "Short");
		    break;
		case JR_NEAR:
		    fprintf(f, "Near");
		    break;
		case JR_SHORT_FORCED:
		    fprintf(f, "Forced Short");
		    break;
		case JR_NEAR_FORCED:
		    fprintf(f, "Forced Near");
		    break;
		default:
		    fprintf(f, "UNKNOWN!!");
		    break;
	    }
	    fprintf(f, "\n%*sAddrSize=%u OperSize=%u LockRepPre=%02x\n",
		    indent_level, "",
		    (unsigned int)jmprel->addrsize,
		    (unsigned int)jmprel->opersize,
		    (unsigned int)jmprel->lockrep_pre);
	    fprintf(f, "%*sBITS=%u\n", indent_level, "",
		    (unsigned int)jmprel->mode_bits);
	    break;
    }
}

static bc_resolve_flags
x86_bc_resolve_insn(x86_insn *insn, unsigned long *len, int save,
		    const section *sect, calc_bc_dist_func calc_bc_dist)
{
    /*@null@*/ expr *temp;
    effaddr *ea = insn->ea;
    x86_effaddr_data *ead = ea_get_data(ea);
    immval *imm = insn->imm;
    bc_resolve_flags retval = BC_RESOLVE_MIN_LEN;

    if (ea) {
	/* Create temp copy of disp, etc. */
	x86_effaddr_data ead_t = *ead;  /* structure copy */
	unsigned char displen = ea->len;

	if (ea->disp) {
	    temp = expr_copy(ea->disp);
	    assert(temp != NULL);

	    /* Check validity of effective address and calc R/M bits of
	     * Mod/RM byte and SIB byte.  We won't know the Mod field
	     * of the Mod/RM byte until we know more about the
	     * displacement.
	     */
	    if (!x86_expr_checkea(&temp, &insn->addrsize, insn->mode_bits,
				  ea->nosplit, &displen, &ead_t.modrm,
				  &ead_t.valid_modrm, &ead_t.need_modrm,
				  &ead_t.sib, &ead_t.valid_sib,
				  &ead_t.need_sib, calc_bc_dist)) {
		expr_delete(temp);
		/* failed, don't bother checking rest of insn */
		return BC_RESOLVE_UNKNOWN_LEN|BC_RESOLVE_ERROR;
	    }

	    expr_delete(temp);

	    if (displen != 1) {
		/* Fits into a word/dword, or unknown. */
		retval = BC_RESOLVE_NONE;    /* may not be smallest size */

		/* Handle unknown case, make displen word-sized */
		if (displen == 0xff)
		    displen = (insn->addrsize == 32) ? 4U : 2U;
	    }

	    if (save) {
		*ead = ead_t;	/* structure copy */
		ea->len = displen;
		if (displen == 0 && ea->disp) {
		    expr_delete(ea->disp);
		    ea->disp = NULL;
		}
	    }
	}

	/* Compute length of ea and add to total */
	*len += ead_t.need_modrm + ead_t.need_sib + displen;
	*len += (ead_t.segment != 0) ? 1 : 0;
    }

    if (imm) {
	const intnum *num;
	unsigned int immlen = imm->len;

	if (imm->val) {
	    temp = expr_copy(imm->val);
	    assert(temp != NULL);

	    /* TODO: check imm->len vs. sized len from expr? */

	    /* Handle shift_op special-casing */
	    if (insn->shift_op && temp &&
		(num = expr_get_intnum(&temp, calc_bc_dist))) {
		if (num && intnum_get_uint(num) == 1) {
		    /* We can use the ,1 form: subtract out the imm len
		     * (as we add it back in below).
		     */
		    *len -= imm->len;

		    if (save) {
			/* Make the ,1 form permanent. */
			insn->opcode[0] = insn->opcode[1];
			/* Delete imm, as it's not needed. */
			expr_delete(imm->val);
			xfree(imm);
			insn->imm = (immval *)NULL;
		    }
		} else
		    retval = BC_RESOLVE_NONE;	    /* we could still get ,1 */

		/* Not really necessary, but saves confusion over it. */
		if (save)
		    insn->shift_op = 0;
	    }

	    expr_delete(temp);
	}

	*len += immlen;
    }

    *len += insn->opcode_len;
    *len += (insn->addrsize != 0 && insn->addrsize != insn->mode_bits) ? 1:0;
    *len += (insn->opersize != 0 && insn->opersize != insn->mode_bits) ? 1:0;
    *len += (insn->lockrep_pre != 0) ? 1:0;

    return retval;
}

static bc_resolve_flags
x86_bc_resolve_jmprel(x86_jmprel *jmprel, unsigned long *len, int save,
		      const bytecode *bc, const section *sect,
		      calc_bc_dist_func calc_bc_dist)
{
    bc_resolve_flags retval = BC_RESOLVE_MIN_LEN;
    /*@null@*/ expr *temp;
    /*@dependent@*/ /*@null@*/ const intnum *num;
    long rel;
    unsigned char opersize;
    int jrshort = 0;

    /* As opersize may be 0, figure out its "real" value. */
    opersize = (jmprel->opersize == 0) ? jmprel->mode_bits :
	jmprel->opersize;

    /* We only check to see if forced forms are actually legal if we're in
     * save mode.  Otherwise we assume that they are legal.
     */
    switch (jmprel->op_sel) {
	case JR_SHORT_FORCED:
	    /* 1 byte relative displacement */
	    jrshort = 1;
	    if (save) {
		temp = expr_copy(jmprel->target);
		num = expr_get_intnum(&temp, calc_bc_dist);
		if (!num) {
		    ErrorAt(bc->line,
			    _("short jump target external or out of segment"));
		    return BC_RESOLVE_ERROR | BC_RESOLVE_UNKNOWN_LEN;
		} else {
		    rel = intnum_get_int(num);
		    rel -= jmprel->shortop.opcode_len+1;
		    /* does a short form exist? */
		    if (jmprel->shortop.opcode_len == 0) {
			ErrorAt(bc->line, _("short jump does not exist"));
			return BC_RESOLVE_ERROR | BC_RESOLVE_UNKNOWN_LEN;
		    }
		    /* short displacement must fit in -128 <= rel <= +127 */
		    if (rel < -128 || rel > 127) {
			ErrorAt(bc->line, _("short jump out of range"));
			return BC_RESOLVE_ERROR | BC_RESOLVE_UNKNOWN_LEN;
		    }
		}
	    }
	    break;
	case JR_NEAR_FORCED:
	    /* 2/4 byte relative displacement (depending on operand size) */
	    jrshort = 0;
	    if (save) {
		if (jmprel->nearop.opcode_len == 0) {
		    ErrorAt(bc->line, _("near jump does not exist"));
		    return BC_RESOLVE_ERROR | BC_RESOLVE_UNKNOWN_LEN;
		}
	    }
	    break;
	default:
	    /* Try to find shortest displacement based on difference between
	     * target expr value and our (this bytecode's) offset.  Note this
	     * requires offset to be set BEFORE calling calc_len in order for
	     * this test to be valid.
	     */
	    temp = expr_copy(jmprel->target);
	    num = expr_get_intnum(&temp, calc_bc_dist);
	    if (num) {
		rel = intnum_get_int(num);
		rel -= jmprel->shortop.opcode_len+1;
		/* short displacement must fit within -128 <= rel <= +127 */
		if (jmprel->shortop.opcode_len != 0 && rel >= -128 &&
		    rel <= 127) {
		    /* It fits into a short displacement. */
		    jrshort = 1;
		} else if (jmprel->nearop.opcode_len != 0) {
		    /* Near for now, but could get shorter in the future if
		     * there's a short form available.
		     */
		    jrshort = 0;
		    if (jmprel->shortop.opcode_len != 0)
			retval = BC_RESOLVE_NONE;
		} else {
		    /* Doesn't fit into short, and there's no near opcode.
		     * Error out if saving, otherwise just make it a short
		     * (in the hopes that a short might make it possible for
		     * it to actually be within short range).
		     */
		    if (save) {
			ErrorAt(bc->line, _("short jump out of range (near jump does not exist)"));
			return BC_RESOLVE_ERROR | BC_RESOLVE_UNKNOWN_LEN;
		    }
		    jrshort = 1;
		}
	    } else {
		/* It's unknown.  Thus, assume near displacement.  If a near
		 * opcode is not available, use a short opcode instead.
		 * If we're saving, error if a near opcode is not available.
		 */
		if (jmprel->nearop.opcode_len != 0) {
		    if (jmprel->shortop.opcode_len != 0)
			retval = BC_RESOLVE_NONE;
		    jrshort = 0;
		} else {
		    if (save) {
			ErrorAt(bc->line,
				_("short jump out of range (near jump does not exist)"));
			return BC_RESOLVE_ERROR | BC_RESOLVE_UNKNOWN_LEN;
		    }
		    jrshort = 1;
		}
	    }
	    expr_delete(temp);
	    break;
    }

    if (jrshort) {
	if (save)
	    jmprel->op_sel = JR_SHORT;
	if (jmprel->shortop.opcode_len == 0)
	    return BC_RESOLVE_UNKNOWN_LEN; /* uh-oh, that size not available */

	*len += jmprel->shortop.opcode_len + 1;
    } else {
	if (save)
	    jmprel->op_sel = JR_NEAR;
	if (jmprel->nearop.opcode_len == 0)
	    return BC_RESOLVE_UNKNOWN_LEN; /* uh-oh, that size not available */

	*len += jmprel->nearop.opcode_len;
	*len += (opersize == 32) ? 4 : 2;
    }
    *len += (jmprel->addrsize != 0 && jmprel->addrsize != jmprel->mode_bits) ?
	1:0;
    *len += (jmprel->opersize != 0 && jmprel->opersize != jmprel->mode_bits) ?
	1:0;
    *len += (jmprel->lockrep_pre != 0) ? 1:0;

    return retval;
}

bc_resolve_flags
x86_bc_resolve(bytecode *bc, int save, const section *sect,
	       calc_bc_dist_func calc_bc_dist)
{
    x86_insn *insn;
    x86_jmprel *jmprel;

    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    insn = bc_get_data(bc);
	    return x86_bc_resolve_insn(insn, &bc->len, save, sect,
				       calc_bc_dist);
	case X86_BC_JMPREL:
	    jmprel = bc_get_data(bc);
	    return x86_bc_resolve_jmprel(jmprel, &bc->len, save, bc, sect,
					 calc_bc_dist);
	default:
	    break;
    }
    InternalError(_("Didn't handle bytecode type in x86 arch"));
    /*@notreached@*/
    return BC_RESOLVE_UNKNOWN_LEN;
}

static int
x86_bc_tobytes_insn(x86_insn *insn, unsigned char **bufp, const section *sect,
		    const bytecode *bc, void *d, output_expr_func output_expr)
{
    /*@null@*/ effaddr *ea = insn->ea;
    x86_effaddr_data *ead = ea_get_data(ea);
    immval *imm = insn->imm;
    unsigned int i;

    /* Prefixes */
    if (insn->lockrep_pre != 0)
	WRITE_BYTE(*bufp, insn->lockrep_pre);
    if (ea && ead->segment != 0)
	WRITE_BYTE(*bufp, ead->segment);
    if (insn->opersize != 0 && insn->opersize != insn->mode_bits)
	WRITE_BYTE(*bufp, 0x66);
    if (insn->addrsize != 0 && insn->addrsize != insn->mode_bits)
	WRITE_BYTE(*bufp, 0x67);

    /* Opcode */
    for (i=0; i<insn->opcode_len; i++)
	WRITE_BYTE(*bufp, insn->opcode[i]);

    /* Effective address: ModR/M (if required), SIB (if required), and
     * displacement (if required).
     */
    if (ea) {
	if (ead->need_modrm) {
	    if (!ead->valid_modrm)
		InternalError(_("invalid Mod/RM in x86 tobytes_insn"));
	    WRITE_BYTE(*bufp, ead->modrm);
	}

	if (ead->need_sib) {
	    if (!ead->valid_sib)
		InternalError(_("invalid SIB in x86 tobytes_insn"));
	    WRITE_BYTE(*bufp, ead->sib);
	}

	if (ea->disp) {
	    x86_effaddr_data ead_t = *ead;  /* structure copy */
	    unsigned char displen = ea->len;
	    unsigned char addrsize = insn->addrsize;

	    ead_t.valid_modrm = 0;  /* force checkea to actually run */

	    /* Call checkea() to simplify the registers out of the
	     * displacement.  Throw away all of the return values except for
	     * the modified expr.
	     */
	    if (!x86_expr_checkea(&ea->disp, &addrsize, insn->mode_bits,
				  ea->nosplit, &displen, &ead_t.modrm,
				  &ead_t.valid_modrm, &ead_t.need_modrm,
				  &ead_t.sib, &ead_t.valid_sib,
				  &ead_t.need_sib, common_calc_bc_dist))
		InternalError(_("checkea failed"));

	    if (ea->disp) {
		if (output_expr(&ea->disp, bufp, ea->len, sect, bc, 0, d))
		    return 1;
	    } else {
		/* 0 displacement, but we didn't know it before, so we have to
		 * write out 0 value.
		 */
		for (i=0; i<ea->len; i++)
		    WRITE_BYTE(*bufp, 0);
	    }
	}
    }

    /* Immediate (if required) */
    if (imm && imm->val) {
	/* TODO: check imm->len vs. sized len from expr? */
	if (output_expr(&imm->val, bufp, imm->len, sect, bc, 0, d))
	    return 1;
    }

    return 0;
}

static int
x86_bc_tobytes_jmprel(x86_jmprel *jmprel, unsigned char **bufp,
		      const section *sect, const bytecode *bc, void *d,
		      output_expr_func output_expr)
{
    unsigned char opersize;
    unsigned int i;

    /* Prefixes */
    if (jmprel->lockrep_pre != 0)
	WRITE_BYTE(*bufp, jmprel->lockrep_pre);
    /* FIXME: branch hints! */
    if (jmprel->opersize != 0 && jmprel->opersize != jmprel->mode_bits)
	WRITE_BYTE(*bufp, 0x66);
    if (jmprel->addrsize != 0 && jmprel->addrsize != jmprel->mode_bits)
	WRITE_BYTE(*bufp, 0x67);

    /* As opersize may be 0, figure out its "real" value. */
    opersize = (jmprel->opersize == 0) ? jmprel->mode_bits :
	jmprel->opersize;

    /* Check here to see if forced forms are actually legal. */
    switch (jmprel->op_sel) {
	case JR_SHORT_FORCED:
	case JR_SHORT:
	    /* 1 byte relative displacement */
	    if (jmprel->shortop.opcode_len == 0)
		InternalError(_("short jump does not exist"));

	    /* Opcode */
	    for (i=0; i<jmprel->shortop.opcode_len; i++)
		WRITE_BYTE(*bufp, jmprel->shortop.opcode[i]);

	    /* Relative displacement */
	    if (output_expr(&jmprel->target, bufp, 1, sect, bc, 1, d))
		return 1;
	    break;
	case JR_NEAR_FORCED:
	case JR_NEAR:
	    /* 2/4 byte relative displacement (depending on operand size) */
	    if (jmprel->nearop.opcode_len == 0) {
		ErrorAt(bc->line, _("near jump does not exist"));
		return 1;
	    }

	    /* Opcode */
	    for (i=0; i<jmprel->nearop.opcode_len; i++)
		WRITE_BYTE(*bufp, jmprel->nearop.opcode[i]);

	    /* Relative displacement */
	    if (output_expr(&jmprel->target, bufp,
			    (opersize == 32) ? 4UL : 2UL, sect, bc, 1, d))
		return 1;
	    break;
	default:
	    InternalError(_("unrecognized relative jump op_sel"));
    }
    return 0;
}

int
x86_bc_tobytes(bytecode *bc, unsigned char **bufp, const section *sect,
	       void *d, output_expr_func output_expr)
{
    x86_insn *insn;
    x86_jmprel *jmprel;

    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    insn = bc_get_data(bc);
	    return x86_bc_tobytes_insn(insn, bufp, sect, bc, d, output_expr);
	case X86_BC_JMPREL:
	    jmprel = bc_get_data(bc);
	    return x86_bc_tobytes_jmprel(jmprel, bufp, sect, bc, d,
					 output_expr);
	default:
	    break;
    }
    return 0;
}

