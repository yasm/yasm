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

#include "errwarn.h"
#include "intnum.h"
#include "expr.h"

#include "bytecode.h"
#include "arch.h"

#include "x86arch.h"

#include "bc-int.h"


typedef struct x86_effaddr {
    effaddr ea;	    /* base structure */

    unsigned char segment;	/* segment override, 0 if none */

    /* How the spare (register) bits in Mod/RM are handled:
     * Even if valid_modrm=0, the spare bits are still valid (don't overwrite!)
     * They're set in bytecode_new_insn().
     */
    unsigned char modrm;
    unsigned char valid_modrm;	/* 1 if Mod/RM byte currently valid, 0 if not */
    unsigned char need_modrm;	/* 1 if Mod/RM byte needed, 0 if not */

    unsigned char sib;
    unsigned char valid_sib;	/* 1 if SIB byte currently valid, 0 if not */
    unsigned char need_sib;	/* 1 if SIB byte needed, 0 if not,
				   0xff if unknown */
} x86_effaddr;

typedef struct x86_insn {
    bytecode bc;    /* base structure */

    /*@null@*/ x86_effaddr *ea;	/* effective address */

    /*@null@*/ immval *imm;	/* immediate or relative value */

    unsigned char opcode[3];	/* opcode */
    unsigned char opcode_len;

    unsigned char addrsize;	/* 0 or =mode_bits => no override */
    unsigned char opersize;	/* 0 indicates no override */
    unsigned char lockrep_pre;	/* 0 indicates no prefix */

    unsigned char rex;		/* REX x86-64 extension, 0 if none */

    /* HACK, but a space-saving one: shift opcodes have an immediate
     * form and a ,1 form (with no immediate).  In the parser, we
     * set this and opcode_len=1, but store the ,1 version in the
     * second byte of the opcode array.  We then choose between the
     * two versions once we know the actual value of imm (because we
     * don't know it in the parser module).
     *
     * A override to force the imm version should just leave this at
     * 0.  Then later code won't know the ,1 version even exists.
     * TODO: Figure out how this affects CPU flags processing.
     *
     * Call x86_SetInsnShiftFlag() to set this flag to 1.
     */
    unsigned char shift_op;

    /* HACK, similar to that for shift_op above, for optimizing instructions
     * that take a sign-extended imm8 as well as imm values (eg, the arith
     * instructions and a subset of the imul instructions).
     */
    unsigned char signext_imm8_op;

    unsigned char mode_bits;
} x86_insn;

typedef struct x86_jmprel {
    bytecode bc;    /* base structure */

    expr *target;		/* target location */

    struct {
	unsigned char opcode[3];
	unsigned char opcode_len;   /* 0 = no opc for this version */
    } shortop, nearop;

    /* which opcode are we using? */
    /* The *FORCED forms are specified in the source as such */
    x86_jmprel_opcode_sel op_sel;

    unsigned char addrsize;	/* 0 or =mode_bits => no override */
    unsigned char opersize;	/* 0 indicates no override */
    unsigned char lockrep_pre;	/* 0 indicates no prefix */

    unsigned char mode_bits;
} x86_jmprel;


/*@-compmempass -mustfree@*/
bytecode *
x86_bc_new_insn(x86_new_insn_data *d)
{
    x86_insn *insn;
   
    insn = (x86_insn *)bc_new_common((bytecode_type)X86_BC_INSN,
				     sizeof(x86_insn), d->lindex);

    insn->ea = (x86_effaddr *)d->ea;
    if (d->ea) {
	insn->ea->modrm &= 0xC7;	/* zero spare/reg bits */
	insn->ea->modrm |= (d->spare << 3) & 0x38;  /* plug in provided bits */
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
    insn->rex = 0;
    insn->shift_op = d->shift_op;
    insn->signext_imm8_op = d->signext_imm8_op;

    insn->mode_bits = yasm_x86_LTX_mode_bits;

    return (bytecode *)insn;
}
/*@=compmempass =mustfree@*/

/*@-compmempass -mustfree@*/
bytecode *
x86_bc_new_jmprel(x86_new_jmprel_data *d)
{
    x86_jmprel *jmprel;

    jmprel = (x86_jmprel *)bc_new_common((bytecode_type)X86_BC_JMPREL,
					 sizeof(x86_jmprel), d->lindex);

    jmprel->target = d->target;
    jmprel->op_sel = d->op_sel;

    if ((d->op_sel == JR_SHORT_FORCED) && (d->near_op_len == 0))
	cur_we->error(d->lindex,
		      N_("no SHORT form of that jump instruction exists"));
    if ((d->op_sel == JR_NEAR_FORCED) && (d->short_op_len == 0))
	cur_we->error(d->lindex,
		      N_("no NEAR form of that jump instruction exists"));

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

    jmprel->mode_bits = yasm_x86_LTX_mode_bits;

    return (bytecode *)jmprel;
}
/*@=compmempass =mustfree@*/

void
x86_ea_set_segment(effaddr *ea, unsigned char segment, unsigned long lindex)
{
    x86_effaddr *x86_ea = (x86_effaddr *)ea;

    if (!ea)
	return;

    if (segment != 0 && x86_ea->segment != 0)
	cur_we->warning(WARN_GENERAL, lindex,
			N_("multiple segment overrides, using leftmost"));

    x86_ea->segment = segment;
}

void
x86_ea_set_disponly(effaddr *ea)
{
    x86_effaddr *x86_ea = (x86_effaddr *)ea;

    x86_ea->valid_modrm = 0;
    x86_ea->need_modrm = 0;
    x86_ea->valid_sib = 0;
    x86_ea->need_sib = 0;
}

effaddr *
x86_ea_new_reg(unsigned char reg)
{
    x86_effaddr *x86_ea;

    x86_ea = xmalloc(sizeof(x86_effaddr));

    x86_ea->ea.disp = (expr *)NULL;
    x86_ea->ea.len = 0;
    x86_ea->ea.nosplit = 0;
    x86_ea->segment = 0;
    x86_ea->modrm = 0xC0 | (reg & 0x07);	/* Mod=11, R/M=Reg, Reg=0 */
    x86_ea->valid_modrm = 1;
    x86_ea->need_modrm = 1;
    x86_ea->sib = 0;
    x86_ea->valid_sib = 0;
    x86_ea->need_sib = 0;

    return (effaddr *)x86_ea;
}

effaddr *
x86_ea_new_expr(expr *e)
{
    x86_effaddr *x86_ea;

    x86_ea = xmalloc(sizeof(x86_effaddr));

    x86_ea->ea.disp = e;
    x86_ea->ea.len = 0;
    x86_ea->ea.nosplit = 0;
    x86_ea->segment = 0;
    x86_ea->modrm = 0;
    x86_ea->valid_modrm = 0;
    x86_ea->need_modrm = 1;
    x86_ea->sib = 0;
    x86_ea->valid_sib = 0;
    /* We won't know whether we need an SIB until we know more about expr and
     * the BITS/address override setting.
     */
    x86_ea->need_sib = 0xff;

    return (effaddr *)x86_ea;
}

/*@-compmempass@*/
effaddr *
x86_ea_new_imm(expr *imm, unsigned char im_len)
{
    x86_effaddr *x86_ea;

    x86_ea = xmalloc(sizeof(x86_effaddr));

    x86_ea->ea.disp = imm;
    x86_ea->ea.len = im_len;
    x86_ea->ea.nosplit = 0;
    x86_ea->segment = 0;
    x86_ea->modrm = 0;
    x86_ea->valid_modrm = 0;
    x86_ea->need_modrm = 0;
    x86_ea->sib = 0;
    x86_ea->valid_sib = 0;
    x86_ea->need_sib = 0;

    return (effaddr *)x86_ea;
}
/*@=compmempass@*/

effaddr *
x86_bc_insn_get_ea(bytecode *bc)
{
    if (!bc)
	return NULL;

    if ((x86_bytecode_type)bc->type != X86_BC_INSN)
	cur_we->internal_error(N_("Trying to get EA of non-instruction"));

    return (effaddr *)(((x86_insn *)bc)->ea);
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
	    insn = (x86_insn *)bc;
	    insn->opersize = opersize;
	    break;
	case X86_BC_JMPREL:
	    jmprel = (x86_jmprel *)bc;
	    jmprel->opersize = opersize;
	    break;
	default:
	    cur_we->internal_error(
		N_("OperSize override applied to non-instruction"));
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
	    insn = (x86_insn *)bc;
	    insn->addrsize = addrsize;
	    break;
	case X86_BC_JMPREL:
	    jmprel = (x86_jmprel *)bc;
	    jmprel->addrsize = addrsize;
	    break;
	default:
	    cur_we->internal_error(
		N_("AddrSize override applied to non-instruction"));
    }
}

void
x86_bc_insn_set_lockrep_prefix(bytecode *bc, unsigned char prefix,
			       unsigned long lindex)
{
    x86_insn *insn;
    x86_jmprel *jmprel;
    unsigned char *lockrep_pre = (unsigned char *)NULL;

    if (!bc)
	return;

    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    insn = (x86_insn *)bc;
	    lockrep_pre = &insn->lockrep_pre;
	    break;
	case X86_BC_JMPREL:
	    jmprel = (x86_jmprel *)bc;
	    lockrep_pre = &jmprel->lockrep_pre;
	    break;
	default:
	    cur_we->internal_error(
		N_("LockRep prefix applied to non-instruction"));
    }

    if (*lockrep_pre != 0)
	cur_we->warning(WARN_GENERAL, lindex,
			N_("multiple LOCK or REP prefixes, using leftmost"));

    *lockrep_pre = prefix;
}

void
x86_bc_delete(bytecode *bc)
{
    x86_insn *insn;
    x86_jmprel *jmprel;

    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    insn = (x86_insn *)bc;
	    if (insn->ea)
		ea_delete((effaddr *)insn->ea);
	    if (insn->imm) {
		expr_delete(insn->imm->val);
		xfree(insn->imm);
	    }
	    break;
	case X86_BC_JMPREL:
	    jmprel = (x86_jmprel *)bc;
	    expr_delete(jmprel->target);
	    break;
    }
}

void
x86_ea_data_print(FILE *f, int indent_level, const effaddr *ea)
{
    const x86_effaddr *x86_ea = (const x86_effaddr *)ea;
    fprintf(f, "%*sSegmentOv=%02x\n", indent_level, "",
	    (unsigned int)x86_ea->segment);
    fprintf(f, "%*sModRM=%03o ValidRM=%u NeedRM=%u\n", indent_level, "",
	    (unsigned int)x86_ea->modrm, (unsigned int)x86_ea->valid_modrm,
	    (unsigned int)x86_ea->need_modrm);
    fprintf(f, "%*sSIB=%03o ValidSIB=%u NeedSIB=%u\n", indent_level, "",
	    (unsigned int)x86_ea->sib, (unsigned int)x86_ea->valid_sib,
	    (unsigned int)x86_ea->need_sib);
}

void
x86_bc_print(FILE *f, int indent_level, const bytecode *bc)
{
    const x86_insn *insn;
    const x86_jmprel *jmprel;

    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    insn = (const x86_insn *)bc;
	    fprintf(f, "%*s_Instruction_\n", indent_level, "");
	    fprintf(f, "%*sEffective Address:", indent_level, "");
	    if (insn->ea) {
		fprintf(f, "\n");
		ea_print(f, indent_level+1, (effaddr *)insn->ea);
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
		    "%*sAddrSize=%u OperSize=%u LockRepPre=%02x REX=%03o\n",
		    indent_level, "",
		    (unsigned int)insn->addrsize,
		    (unsigned int)insn->opersize,
		    (unsigned int)insn->lockrep_pre,
		    (unsigned int)insn->rex);
	    fprintf(f, "%*sShiftOp=%u BITS=%u\n", indent_level, "",
		    (unsigned int)insn->shift_op,
		    (unsigned int)insn->mode_bits);
	    break;
	case X86_BC_JMPREL:
	    jmprel = (const x86_jmprel *)bc;
	    fprintf(f, "%*s_Relative Jump_\n", indent_level, "");
	    fprintf(f, "%*sTarget=", indent_level, "");
	    expr_print(f, jmprel->target);
	    fprintf(f, "\n%*sShort Form:\n", indent_level, "");
	    if (jmprel->shortop.opcode_len == 0)
		fprintf(f, "%*sNone\n", indent_level+1, "");
	    else
		fprintf(f, "%*sOpcode: %02x %02x %02x OpLen=%u\n",
			indent_level+1, "",
			(unsigned int)jmprel->shortop.opcode[0],
			(unsigned int)jmprel->shortop.opcode[1],
			(unsigned int)jmprel->shortop.opcode[2],
			(unsigned int)jmprel->shortop.opcode_len);
	    fprintf(f, "%*sNear Form:\n", indent_level, "");
	    if (jmprel->nearop.opcode_len == 0)
		fprintf(f, "%*sNone\n", indent_level+1, "");
	    else
		fprintf(f, "%*sOpcode: %02x %02x %02x OpLen=%u\n",
			indent_level+1, "",
			(unsigned int)jmprel->nearop.opcode[0],
			(unsigned int)jmprel->nearop.opcode[1],
			(unsigned int)jmprel->nearop.opcode[2],
			(unsigned int)jmprel->nearop.opcode_len);
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
    x86_effaddr *x86_ea = insn->ea;
    effaddr *ea = &x86_ea->ea;
    immval *imm = insn->imm;
    bc_resolve_flags retval = BC_RESOLVE_MIN_LEN;

    if (ea) {
	/* Create temp copy of disp, etc. */
	x86_effaddr eat = *x86_ea;  /* structure copy */
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
				  ea->nosplit, &displen, &eat.modrm,
				  &eat.valid_modrm, &eat.need_modrm,
				  &eat.sib, &eat.valid_sib,
				  &eat.need_sib, calc_bc_dist)) {
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
		*x86_ea = eat;	/* structure copy */
		ea->len = displen;
		if (displen == 0 && ea->disp) {
		    expr_delete(ea->disp);
		    ea->disp = NULL;
		}
	    }
	}

	/* Compute length of ea and add to total */
	*len += eat.need_modrm + eat.need_sib + displen;
	*len += (eat.segment != 0) ? 1 : 0;
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
    *len += (insn->rex != 0) ? 1:0;

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
		    cur_we->error(bc->line,
			N_("short jump target external or out of segment"));
		    expr_delete(temp);
		    return BC_RESOLVE_ERROR | BC_RESOLVE_UNKNOWN_LEN;
		} else {
		    rel = intnum_get_int(num);
		    rel -= jmprel->shortop.opcode_len+1;
		    expr_delete(temp);
		    /* does a short form exist? */
		    if (jmprel->shortop.opcode_len == 0) {
			cur_we->error(bc->line,
				      N_("short jump does not exist"));
			return BC_RESOLVE_ERROR | BC_RESOLVE_UNKNOWN_LEN;
		    }
		    /* short displacement must fit in -128 <= rel <= +127 */
		    if (rel < -128 || rel > 127) {
			cur_we->error(bc->line, N_("short jump out of range"));
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
		    cur_we->error(bc->line, N_("near jump does not exist"));
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
			cur_we->error(bc->line,
			    N_("short jump out of range (near jump does not exist)"));
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
			cur_we->error(bc->line,
			    N_("short jump out of range (near jump does not exist)"));
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
    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    return x86_bc_resolve_insn((x86_insn *)bc, &bc->len, save, sect,
				       calc_bc_dist);
	case X86_BC_JMPREL:
	    return x86_bc_resolve_jmprel((x86_jmprel *)bc, &bc->len, save, bc,
					 sect, calc_bc_dist);
	default:
	    break;
    }
    cur_we->internal_error(N_("Didn't handle bytecode type in x86 arch"));
    /*@notreached@*/
    return BC_RESOLVE_UNKNOWN_LEN;
}

static int
x86_bc_tobytes_insn(x86_insn *insn, unsigned char **bufp, const section *sect,
		    const bytecode *bc, void *d, output_expr_func output_expr)
{
    /*@null@*/ x86_effaddr *x86_ea = insn->ea;
    /*@null@*/ effaddr *ea = &x86_ea->ea;
    immval *imm = insn->imm;
    unsigned int i;
    unsigned char *bufp_orig = *bufp;

    /* Prefixes */
    if (insn->lockrep_pre != 0)
	WRITE_8(*bufp, insn->lockrep_pre);
    if (x86_ea && x86_ea->segment != 0)
	WRITE_8(*bufp, x86_ea->segment);
    if (insn->opersize != 0 && insn->opersize != insn->mode_bits)
	WRITE_8(*bufp, 0x66);
    if (insn->addrsize != 0 && insn->addrsize != insn->mode_bits)
	WRITE_8(*bufp, 0x67);
    if (insn->rex != 0) {
	if (insn->mode_bits != 64)
	    cur_we->internal_error(
		N_("x86: got a REX prefix in non-64-bit mode"));
	WRITE_8(*bufp, insn->rex);
    }

    /* Opcode */
    for (i=0; i<insn->opcode_len; i++)
	WRITE_8(*bufp, insn->opcode[i]);

    /* Effective address: ModR/M (if required), SIB (if required), and
     * displacement (if required).
     */
    if (ea) {
	if (x86_ea->need_modrm) {
	    if (!x86_ea->valid_modrm)
		cur_we->internal_error(
		    N_("invalid Mod/RM in x86 tobytes_insn"));
	    WRITE_8(*bufp, x86_ea->modrm);
	}

	if (x86_ea->need_sib) {
	    if (!x86_ea->valid_sib)
		cur_we->internal_error(N_("invalid SIB in x86 tobytes_insn"));
	    WRITE_8(*bufp, x86_ea->sib);
	}

	if (ea->disp) {
	    x86_effaddr eat = *x86_ea;  /* structure copy */
	    unsigned char displen = ea->len;
	    unsigned char addrsize = insn->addrsize;

	    eat.valid_modrm = 0;    /* force checkea to actually run */

	    /* Call checkea() to simplify the registers out of the
	     * displacement.  Throw away all of the return values except for
	     * the modified expr.
	     */
	    if (!x86_expr_checkea(&ea->disp, &addrsize, insn->mode_bits,
				  ea->nosplit, &displen, &eat.modrm,
				  &eat.valid_modrm, &eat.need_modrm,
				  &eat.sib, &eat.valid_sib,
				  &eat.need_sib, common_calc_bc_dist))
		cur_we->internal_error(N_("checkea failed"));

	    if (ea->disp) {
		if (output_expr(&ea->disp, bufp, ea->len, *bufp-bufp_orig,
				sect, bc, 0, d))
		    return 1;
	    } else {
		/* 0 displacement, but we didn't know it before, so we have to
		 * write out 0 value.
		 */
		for (i=0; i<ea->len; i++)
		    WRITE_8(*bufp, 0);
	    }
	}
    }

    /* Immediate (if required) */
    if (imm && imm->val) {
	/* TODO: check imm->len vs. sized len from expr? */
	if (output_expr(&imm->val, bufp, imm->len, *bufp-bufp_orig, sect, bc,
			0, d))
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
    unsigned char *bufp_orig = *bufp;

    /* Prefixes */
    if (jmprel->lockrep_pre != 0)
	WRITE_8(*bufp, jmprel->lockrep_pre);
    /* FIXME: branch hints! */
    if (jmprel->opersize != 0 && jmprel->opersize != jmprel->mode_bits)
	WRITE_8(*bufp, 0x66);
    if (jmprel->addrsize != 0 && jmprel->addrsize != jmprel->mode_bits)
	WRITE_8(*bufp, 0x67);

    /* As opersize may be 0, figure out its "real" value. */
    opersize = (jmprel->opersize == 0) ? jmprel->mode_bits :
	jmprel->opersize;

    /* Check here to see if forced forms are actually legal. */
    switch (jmprel->op_sel) {
	case JR_SHORT_FORCED:
	case JR_SHORT:
	    /* 1 byte relative displacement */
	    if (jmprel->shortop.opcode_len == 0)
		cur_we->internal_error(N_("short jump does not exist"));

	    /* Opcode */
	    for (i=0; i<jmprel->shortop.opcode_len; i++)
		WRITE_8(*bufp, jmprel->shortop.opcode[i]);

	    /* Relative displacement */
	    if (output_expr(&jmprel->target, bufp, 1, *bufp-bufp_orig, sect,
			    bc, 1, d))
		return 1;
	    break;
	case JR_NEAR_FORCED:
	case JR_NEAR:
	    /* 2/4 byte relative displacement (depending on operand size) */
	    if (jmprel->nearop.opcode_len == 0) {
		cur_we->error(bc->line, N_("near jump does not exist"));
		return 1;
	    }

	    /* Opcode */
	    for (i=0; i<jmprel->nearop.opcode_len; i++)
		WRITE_8(*bufp, jmprel->nearop.opcode[i]);

	    /* Relative displacement */
	    if (output_expr(&jmprel->target, bufp,
			    (opersize == 32) ? 4UL : 2UL, *bufp-bufp_orig,
			    sect, bc, 1, d))
		return 1;
	    break;
	default:
	    cur_we->internal_error(N_("unrecognized relative jump op_sel"));
    }
    return 0;
}

int
x86_bc_tobytes(bytecode *bc, unsigned char **bufp, const section *sect,
	       void *d, output_expr_func output_expr)
{
    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    return x86_bc_tobytes_insn((x86_insn *)bc, bufp, sect, bc, d,
				       output_expr);
	case X86_BC_JMPREL:
	    return x86_bc_tobytes_jmprel((x86_jmprel *)bc, bufp, sect, bc, d,
					 output_expr);
	default:
	    break;
    }
    return 0;
}

int
x86_intnum_tobytes(const intnum *intn, unsigned char **bufp,
		   unsigned long valsize, const expr *e, const bytecode *bc,
		   int rel)
{
    if (rel) {
	long val;
	if (valsize != 1 && valsize != 2 && valsize != 4)
	    cur_we->internal_error(
		N_("tried to do PC-relative offset from invalid sized value"));
	val = intnum_get_uint(intn);
	val -= bc->len;
	switch (valsize) {
	    case 1:
		WRITE_8(*bufp, val);
		break;
	    case 2:
		WRITE_16_L(*bufp, val);
		break;
	    case 4:
		WRITE_32_L(*bufp, val);
		break;
	}
    } else {
	/* Write value out. */
	intnum_get_sized(intn, *bufp, (size_t)valsize);
	*bufp += valsize;
    }
    return 0;
}
