/*
 * x86 bytecode utility functions
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
#define YASM_BC_INTERNAL
#include "libyasm.h"
/*@unused@*/ RCSID("$IdPath$");

#include "x86arch.h"


typedef struct x86_effaddr {
    yasm_effaddr ea;		/* base structure */

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
    yasm_bytecode bc;		/* base structure */

    /*@null@*/ x86_effaddr *ea;	/* effective address */

    /*@null@*/ yasm_immval *imm;/* immediate or relative value */

    unsigned char opcode[3];	/* opcode */
    unsigned char opcode_len;

    unsigned char addrsize;	/* 0 or =mode_bits => no override */
    unsigned char opersize;	/* 0 indicates no override */
    unsigned char lockrep_pre;	/* 0 indicates no prefix */

    unsigned char rex;		/* REX x86-64 extension, 0 if none,
				   0xff if not allowed (high 8 bit reg used) */

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
    yasm_bytecode bc;		/* base structure */

    yasm_expr *target;		/* target location */

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


int
yasm_x86__set_rex_from_reg(unsigned char *rex, unsigned char *low3,
			   unsigned long reg, unsigned char bits,
			   x86_rex_bit_pos rexbit)
{
    *low3 = (unsigned char)(reg&7);

    if (bits == 64) {
	x86_expritem_reg_size size = (x86_expritem_reg_size)(reg & ~0xF);

	if (size == X86_REG8X || (reg & 0xF) >= 8) {
	    if (*rex == 0xff)
		return 1;
	    *rex |= 0x40 | (((reg & 8) >> 3) << rexbit);
	} else if (size == X86_REG8 && (reg & 7) >= 4) {
	    /* AH/BH/CH/DH, so no REX allowed */
	    if (*rex != 0 && *rex != 0xff)
		return 1;
	    *rex = 0xff;
	}
    }

    return 0;
}

/*@-compmempass -mustfree@*/
yasm_bytecode *
yasm_x86__bc_new_insn(x86_new_insn_data *d)
{
    x86_insn *insn;
   
    insn = (x86_insn *)yasm_bc_new_common((yasm_bytecode_type)X86_BC_INSN,
					  sizeof(x86_insn), d->lindex);

    insn->ea = (x86_effaddr *)d->ea;
    if (d->ea) {
	insn->ea->modrm &= 0xC7;	/* zero spare/reg bits */
	insn->ea->modrm |= (d->spare << 3) & 0x38;  /* plug in provided bits */
    }

    if (d->imm) {
	insn->imm = yasm_imm_new_expr(d->imm);
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
    insn->rex = d->rex;
    insn->shift_op = d->shift_op;
    insn->signext_imm8_op = d->signext_imm8_op;

    insn->mode_bits = yasm_x86_LTX_mode_bits;

    return (yasm_bytecode *)insn;
}
/*@=compmempass =mustfree@*/

/*@-compmempass -mustfree@*/
yasm_bytecode *
yasm_x86__bc_new_jmprel(x86_new_jmprel_data *d)
{
    x86_jmprel *jmprel;

    jmprel = (x86_jmprel *)
	yasm_bc_new_common((yasm_bytecode_type)X86_BC_JMPREL,
			   sizeof(x86_jmprel), d->lindex);

    jmprel->target = d->target;
    jmprel->op_sel = d->op_sel;

    if ((d->op_sel == JR_SHORT_FORCED) && (d->near_op_len == 0))
	yasm__error(d->lindex,
		    N_("no SHORT form of that jump instruction exists"));
    if ((d->op_sel == JR_NEAR_FORCED) && (d->short_op_len == 0))
	yasm__error(d->lindex,
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

    return (yasm_bytecode *)jmprel;
}
/*@=compmempass =mustfree@*/

void
yasm_x86__ea_set_segment(yasm_effaddr *ea, unsigned char segment,
			 unsigned long lindex)
{
    x86_effaddr *x86_ea = (x86_effaddr *)ea;

    if (!ea)
	return;

    if (segment != 0 && x86_ea->segment != 0)
	yasm__warning(YASM_WARN_GENERAL, lindex,
		      N_("multiple segment overrides, using leftmost"));

    x86_ea->segment = segment;
}

void
yasm_x86__ea_set_disponly(yasm_effaddr *ea)
{
    x86_effaddr *x86_ea = (x86_effaddr *)ea;

    x86_ea->valid_modrm = 0;
    x86_ea->need_modrm = 0;
    x86_ea->valid_sib = 0;
    x86_ea->need_sib = 0;
}

yasm_effaddr *
yasm_x86__ea_new_reg(unsigned long reg, unsigned char *rex, unsigned char bits)
{
    x86_effaddr *x86_ea;
    unsigned char rm;

    if (yasm_x86__set_rex_from_reg(rex, &rm, reg, bits, X86_REX_B))
	return NULL;

    x86_ea = yasm_xmalloc(sizeof(x86_effaddr));

    x86_ea->ea.disp = (yasm_expr *)NULL;
    x86_ea->ea.len = 0;
    x86_ea->ea.nosplit = 0;
    x86_ea->segment = 0;
    x86_ea->modrm = 0xC0 | rm;	/* Mod=11, R/M=Reg, Reg=0 */
    x86_ea->valid_modrm = 1;
    x86_ea->need_modrm = 1;
    x86_ea->sib = 0;
    x86_ea->valid_sib = 0;
    x86_ea->need_sib = 0;

    return (yasm_effaddr *)x86_ea;
}

yasm_effaddr *
yasm_x86__ea_new_expr(yasm_expr *e)
{
    x86_effaddr *x86_ea;

    x86_ea = yasm_xmalloc(sizeof(x86_effaddr));

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

    return (yasm_effaddr *)x86_ea;
}

/*@-compmempass@*/
yasm_effaddr *
yasm_x86__ea_new_imm(yasm_expr *imm, unsigned char im_len)
{
    x86_effaddr *x86_ea;

    x86_ea = yasm_xmalloc(sizeof(x86_effaddr));

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

    return (yasm_effaddr *)x86_ea;
}
/*@=compmempass@*/

yasm_effaddr *
yasm_x86__bc_insn_get_ea(yasm_bytecode *bc)
{
    if (!bc)
	return NULL;

    if ((x86_bytecode_type)bc->type != X86_BC_INSN)
	yasm_internal_error(N_("Trying to get EA of non-instruction"));

    return (yasm_effaddr *)(((x86_insn *)bc)->ea);
}

void
yasm_x86__bc_insn_opersize_override(yasm_bytecode *bc, unsigned char opersize)
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
	    yasm_internal_error(
		N_("OperSize override applied to non-instruction"));
    }
}

void
yasm_x86__bc_insn_addrsize_override(yasm_bytecode *bc, unsigned char addrsize)
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
	    yasm_internal_error(
		N_("AddrSize override applied to non-instruction"));
    }
}

void
yasm_x86__bc_insn_set_lockrep_prefix(yasm_bytecode *bc, unsigned char prefix,
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
	    yasm_internal_error(
		N_("LockRep prefix applied to non-instruction"));
    }

    if (*lockrep_pre != 0)
	yasm__warning(YASM_WARN_GENERAL, lindex,
		      N_("multiple LOCK or REP prefixes, using leftmost"));

    *lockrep_pre = prefix;
}

void
yasm_x86__bc_delete(yasm_bytecode *bc)
{
    x86_insn *insn;
    x86_jmprel *jmprel;

    switch ((x86_bytecode_type)bc->type) {
	case X86_BC_INSN:
	    insn = (x86_insn *)bc;
	    if (insn->ea)
		yasm_ea_delete((yasm_effaddr *)insn->ea);
	    if (insn->imm) {
		yasm_expr_delete(insn->imm->val);
		yasm_xfree(insn->imm);
	    }
	    break;
	case X86_BC_JMPREL:
	    jmprel = (x86_jmprel *)bc;
	    yasm_expr_delete(jmprel->target);
	    break;
    }
}

void
yasm_x86__ea_data_print(FILE *f, int indent_level, const yasm_effaddr *ea)
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
yasm_x86__bc_print(FILE *f, int indent_level, const yasm_bytecode *bc)
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
		yasm_ea_print(f, indent_level+1, (yasm_effaddr *)insn->ea);
	    } else
		fprintf(f, " (nil)\n");
	    fprintf(f, "%*sImmediate Value:", indent_level, "");
	    if (!insn->imm)
		fprintf(f, " (nil)\n");
	    else {
		indent_level++;
		fprintf(f, "\n%*sVal=", indent_level, "");
		if (insn->imm->val)
		    yasm_expr_print(f, insn->imm->val);
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
	    yasm_expr_print(f, jmprel->target);
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

static yasm_bc_resolve_flags
x86_bc_resolve_insn(x86_insn *insn, unsigned long *len, int save,
		    const yasm_section *sect,
		    yasm_calc_bc_dist_func calc_bc_dist)
{
    /*@null@*/ yasm_expr *temp;
    x86_effaddr *x86_ea = insn->ea;
    yasm_effaddr *ea = &x86_ea->ea;
    yasm_immval *imm = insn->imm;
    yasm_bc_resolve_flags retval = YASM_BC_RESOLVE_MIN_LEN;

    if (ea) {
	/* Create temp copy of disp, etc. */
	x86_effaddr eat = *x86_ea;  /* structure copy */
	unsigned char displen = ea->len;

	if (ea->disp) {
	    temp = yasm_expr_copy(ea->disp);
	    assert(temp != NULL);

	    /* Check validity of effective address and calc R/M bits of
	     * Mod/RM byte and SIB byte.  We won't know the Mod field
	     * of the Mod/RM byte until we know more about the
	     * displacement.
	     */
	    if (!yasm_x86__expr_checkea(&temp, &insn->addrsize,
		    insn->mode_bits, ea->nosplit, &displen, &eat.modrm,
		    &eat.valid_modrm, &eat.need_modrm, &eat.sib,
		    &eat.valid_sib, &eat.need_sib, &insn->rex, calc_bc_dist)) {
		yasm_expr_delete(temp);
		/* failed, don't bother checking rest of insn */
		return YASM_BC_RESOLVE_UNKNOWN_LEN|YASM_BC_RESOLVE_ERROR;
	    }

	    yasm_expr_delete(temp);

	    if (displen != 1) {
		/* Fits into a word/dword, or unknown. */
		retval = YASM_BC_RESOLVE_NONE;  /* may not be smallest size */

		/* Handle unknown case, make displen word-sized */
		if (displen == 0xff)
		    displen = (insn->addrsize == 32) ? 4U : 2U;
	    }

	    if (save) {
		*x86_ea = eat;	/* structure copy */
		ea->len = displen;
		if (displen == 0 && ea->disp) {
		    yasm_expr_delete(ea->disp);
		    ea->disp = NULL;
		}
	    }
	}

	/* Compute length of ea and add to total */
	*len += eat.need_modrm + eat.need_sib + displen;
	*len += (eat.segment != 0) ? 1 : 0;
    }

    if (imm) {
	const yasm_intnum *num;
	unsigned int immlen = imm->len;

	if (imm->val) {
	    temp = yasm_expr_copy(imm->val);
	    assert(temp != NULL);

	    /* TODO: check imm->len vs. sized len from expr? */

	    /* Handle shift_op special-casing */
	    if (insn->shift_op && temp &&
		(num = yasm_expr_get_intnum(&temp, calc_bc_dist))) {
		if (num && yasm_intnum_get_uint(num) == 1) {
		    /* We can use the ,1 form: subtract out the imm len
		     * (as we add it back in below).
		     */
		    *len -= imm->len;

		    if (save) {
			/* Make the ,1 form permanent. */
			insn->opcode[0] = insn->opcode[1];
			/* Delete imm, as it's not needed. */
			yasm_expr_delete(imm->val);
			yasm_xfree(imm);
			insn->imm = (yasm_immval *)NULL;
		    }
		} else
		    retval = YASM_BC_RESOLVE_NONE;  /* we could still get ,1 */

		/* Not really necessary, but saves confusion over it. */
		if (save)
		    insn->shift_op = 0;
	    }

	    yasm_expr_delete(temp);
	}

	*len += immlen;
    }

    *len += insn->opcode_len;
    *len += (insn->addrsize != 0 && insn->addrsize != insn->mode_bits) ? 1:0;
    if (insn->opersize != 0 &&
	((insn->mode_bits != 64 && insn->opersize != insn->mode_bits) ||
	 (insn->mode_bits == 64 && insn->opersize == 16)))
	(*len)++;
    *len += (insn->lockrep_pre != 0) ? 1:0;
    *len += (insn->rex != 0 && insn->rex != 0xff) ? 1:0;

    return retval;
}

static yasm_bc_resolve_flags
x86_bc_resolve_jmprel(x86_jmprel *jmprel, unsigned long *len, int save,
		      const yasm_bytecode *bc, const yasm_section *sect,
		      yasm_calc_bc_dist_func calc_bc_dist)
{
    yasm_bc_resolve_flags retval = YASM_BC_RESOLVE_MIN_LEN;
    /*@null@*/ yasm_expr *temp;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;
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
		temp = yasm_expr_copy(jmprel->target);
		num = yasm_expr_get_intnum(&temp, calc_bc_dist);
		if (!num) {
		    yasm__error(bc->line,
			N_("short jump target external or out of segment"));
		    yasm_expr_delete(temp);
		    return YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
		} else {
		    rel = yasm_intnum_get_int(num);
		    rel -= jmprel->shortop.opcode_len+1;
		    yasm_expr_delete(temp);
		    /* does a short form exist? */
		    if (jmprel->shortop.opcode_len == 0) {
			yasm__error(bc->line, N_("short jump does not exist"));
			return YASM_BC_RESOLVE_ERROR |
			    YASM_BC_RESOLVE_UNKNOWN_LEN;
		    }
		    /* short displacement must fit in -128 <= rel <= +127 */
		    if (rel < -128 || rel > 127) {
			yasm__error(bc->line, N_("short jump out of range"));
			return YASM_BC_RESOLVE_ERROR |
			    YASM_BC_RESOLVE_UNKNOWN_LEN;
		    }
		}
	    }
	    break;
	case JR_NEAR_FORCED:
	    /* 2/4 byte relative displacement (depending on operand size) */
	    jrshort = 0;
	    if (save) {
		if (jmprel->nearop.opcode_len == 0) {
		    yasm__error(bc->line, N_("near jump does not exist"));
		    return YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
		}
	    }
	    break;
	default:
	    /* Try to find shortest displacement based on difference between
	     * target expr value and our (this bytecode's) offset.  Note this
	     * requires offset to be set BEFORE calling calc_len in order for
	     * this test to be valid.
	     */
	    temp = yasm_expr_copy(jmprel->target);
	    num = yasm_expr_get_intnum(&temp, calc_bc_dist);
	    if (num) {
		rel = yasm_intnum_get_int(num);
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
			retval = YASM_BC_RESOLVE_NONE;
		} else {
		    /* Doesn't fit into short, and there's no near opcode.
		     * Error out if saving, otherwise just make it a short
		     * (in the hopes that a short might make it possible for
		     * it to actually be within short range).
		     */
		    if (save) {
			yasm__error(bc->line,
			    N_("short jump out of range (near jump does not exist)"));
			return YASM_BC_RESOLVE_ERROR |
			    YASM_BC_RESOLVE_UNKNOWN_LEN;
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
			retval = YASM_BC_RESOLVE_NONE;
		    jrshort = 0;
		} else {
		    if (save) {
			yasm__error(bc->line,
			    N_("short jump out of range (near jump does not exist)"));
			return YASM_BC_RESOLVE_ERROR |
			    YASM_BC_RESOLVE_UNKNOWN_LEN;
		    }
		    jrshort = 1;
		}
	    }
	    yasm_expr_delete(temp);
	    break;
    }

    if (jrshort) {
	if (save)
	    jmprel->op_sel = JR_SHORT;
	if (jmprel->shortop.opcode_len == 0)
	    return YASM_BC_RESOLVE_UNKNOWN_LEN; /* that size not available */

	*len += jmprel->shortop.opcode_len + 1;
    } else {
	if (save)
	    jmprel->op_sel = JR_NEAR;
	if (jmprel->nearop.opcode_len == 0)
	    return YASM_BC_RESOLVE_UNKNOWN_LEN; /* that size not available */

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

yasm_bc_resolve_flags
yasm_x86__bc_resolve(yasm_bytecode *bc, int save, const yasm_section *sect,
		     yasm_calc_bc_dist_func calc_bc_dist)
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
    yasm_internal_error(N_("Didn't handle bytecode type in x86 arch"));
    /*@notreached@*/
    return YASM_BC_RESOLVE_UNKNOWN_LEN;
}

static int
x86_bc_tobytes_insn(x86_insn *insn, unsigned char **bufp,
		    const yasm_section *sect, const yasm_bytecode *bc, void *d,
		    yasm_output_expr_func output_expr)
{
    /*@null@*/ x86_effaddr *x86_ea = insn->ea;
    /*@null@*/ yasm_effaddr *ea = &x86_ea->ea;
    yasm_immval *imm = insn->imm;
    unsigned int i;
    unsigned char *bufp_orig = *bufp;

    /* Prefixes */
    if (insn->lockrep_pre != 0)
	YASM_WRITE_8(*bufp, insn->lockrep_pre);
    if (x86_ea && x86_ea->segment != 0)
	YASM_WRITE_8(*bufp, x86_ea->segment);
    if (insn->opersize != 0 &&
	((insn->mode_bits != 64 && insn->opersize != insn->mode_bits) ||
	 (insn->mode_bits == 64 && insn->opersize == 16)))
	YASM_WRITE_8(*bufp, 0x66);
    if (insn->addrsize != 0 && insn->addrsize != insn->mode_bits)
	YASM_WRITE_8(*bufp, 0x67);
    if (insn->rex != 0 && insn->rex != 0xff) {
	if (insn->mode_bits != 64)
	    yasm_internal_error(
		N_("x86: got a REX prefix in non-64-bit mode"));
	YASM_WRITE_8(*bufp, insn->rex);
    }

    /* Opcode */
    for (i=0; i<insn->opcode_len; i++)
	YASM_WRITE_8(*bufp, insn->opcode[i]);

    /* Effective address: ModR/M (if required), SIB (if required), and
     * displacement (if required).
     */
    if (ea) {
	if (x86_ea->need_modrm) {
	    if (!x86_ea->valid_modrm)
		yasm_internal_error(N_("invalid Mod/RM in x86 tobytes_insn"));
	    YASM_WRITE_8(*bufp, x86_ea->modrm);
	}

	if (x86_ea->need_sib) {
	    if (!x86_ea->valid_sib)
		yasm_internal_error(N_("invalid SIB in x86 tobytes_insn"));
	    YASM_WRITE_8(*bufp, x86_ea->sib);
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
	    if (!yasm_x86__expr_checkea(&ea->disp, &addrsize, insn->mode_bits,
					ea->nosplit, &displen, &eat.modrm,
					&eat.valid_modrm, &eat.need_modrm,
					&eat.sib, &eat.valid_sib,
					&eat.need_sib, &insn->rex,
					yasm_common_calc_bc_dist))
		yasm_internal_error(N_("checkea failed"));

	    if (ea->disp) {
		if (output_expr(&ea->disp, bufp, ea->len, *bufp-bufp_orig,
				sect, bc, 0, d))
		    return 1;
	    } else {
		/* 0 displacement, but we didn't know it before, so we have to
		 * write out 0 value.
		 */
		for (i=0; i<ea->len; i++)
		    YASM_WRITE_8(*bufp, 0);
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
		      const yasm_section *sect, const yasm_bytecode *bc,
		      void *d, yasm_output_expr_func output_expr)
{
    unsigned char opersize;
    unsigned int i;
    unsigned char *bufp_orig = *bufp;

    /* Prefixes */
    if (jmprel->lockrep_pre != 0)
	YASM_WRITE_8(*bufp, jmprel->lockrep_pre);
    /* FIXME: branch hints! */
    if (jmprel->opersize != 0 && jmprel->opersize != jmprel->mode_bits)
	YASM_WRITE_8(*bufp, 0x66);
    if (jmprel->addrsize != 0 && jmprel->addrsize != jmprel->mode_bits)
	YASM_WRITE_8(*bufp, 0x67);

    /* As opersize may be 0, figure out its "real" value. */
    opersize = (jmprel->opersize == 0) ? jmprel->mode_bits :
	jmprel->opersize;

    /* Check here to see if forced forms are actually legal. */
    switch (jmprel->op_sel) {
	case JR_SHORT_FORCED:
	case JR_SHORT:
	    /* 1 byte relative displacement */
	    if (jmprel->shortop.opcode_len == 0)
		yasm_internal_error(N_("short jump does not exist"));

	    /* Opcode */
	    for (i=0; i<jmprel->shortop.opcode_len; i++)
		YASM_WRITE_8(*bufp, jmprel->shortop.opcode[i]);

	    /* Relative displacement */
	    if (output_expr(&jmprel->target, bufp, 1, *bufp-bufp_orig, sect,
			    bc, 1, d))
		return 1;
	    break;
	case JR_NEAR_FORCED:
	case JR_NEAR:
	    /* 2/4 byte relative displacement (depending on operand size) */
	    if (jmprel->nearop.opcode_len == 0) {
		yasm__error(bc->line, N_("near jump does not exist"));
		return 1;
	    }

	    /* Opcode */
	    for (i=0; i<jmprel->nearop.opcode_len; i++)
		YASM_WRITE_8(*bufp, jmprel->nearop.opcode[i]);

	    /* Relative displacement */
	    if (output_expr(&jmprel->target, bufp,
			    (opersize == 32) ? 4UL : 2UL, *bufp-bufp_orig,
			    sect, bc, 1, d))
		return 1;
	    break;
	default:
	    yasm_internal_error(N_("unrecognized relative jump op_sel"));
    }
    return 0;
}

int
yasm_x86__bc_tobytes(yasm_bytecode *bc, unsigned char **bufp,
		     const yasm_section *sect, void *d,
		     yasm_output_expr_func output_expr)
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
yasm_x86__intnum_tobytes(const yasm_intnum *intn, unsigned char **bufp,
			 unsigned long valsize, const yasm_expr *e,
			 const yasm_bytecode *bc, int rel)
{
    if (rel) {
	long val;
	if (valsize != 1 && valsize != 2 && valsize != 4)
	    yasm_internal_error(
		N_("tried to do PC-relative offset from invalid sized value"));
	val = yasm_intnum_get_uint(intn);
	val -= bc->len;
	switch (valsize) {
	    case 1:
		YASM_WRITE_8(*bufp, val);
		break;
	    case 2:
		YASM_WRITE_16_L(*bufp, val);
		break;
	    case 4:
		YASM_WRITE_32_L(*bufp, val);
		break;
	}
    } else {
	/* Write value out. */
	yasm_intnum_get_sized(intn, *bufp, (size_t)valsize);
	*bufp += valsize;
    }
    return 0;
}
