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
#include <util.h>
/*@unused@*/ RCSID("$IdPath$");

#define YASM_LIB_INTERNAL
#define YASM_BC_INTERNAL
#include <libyasm.h>

#include "x86arch.h"


/* Effective address type */

typedef struct x86_effaddr {
    yasm_effaddr ea;		/* base structure */

    /* PC-relative portions are for AMD64 only (RIP addressing) */
    /*@null@*/ /*@dependent@*/ yasm_symrec *origin;    /* pcrel origin */

    unsigned char segment;	/* segment override, 0 if none */

    /* How the spare (register) bits in Mod/RM are handled:
     * Even if valid_modrm=0, the spare bits are still valid (don't overwrite!)
     * They're set in bytecode_create_insn().
     */
    unsigned char modrm;
    unsigned char valid_modrm;	/* 1 if Mod/RM byte currently valid, 0 if not */
    unsigned char need_modrm;	/* 1 if Mod/RM byte needed, 0 if not */

    unsigned char sib;
    unsigned char valid_sib;	/* 1 if SIB byte currently valid, 0 if not */
    unsigned char need_sib;	/* 1 if SIB byte needed, 0 if not,
				   0xff if unknown */

    unsigned char pcrel;	/* 1 if PC-relative transformation needed */
} x86_effaddr;

/* Bytecode types */

typedef struct x86_insn {
    yasm_bytecode bc;		/* base structure */

    /*@null@*/ x86_effaddr *ea;	/* effective address */

    /*@null@*/ yasm_immval *imm;/* immediate or relative value */

    unsigned char opcode[3];	/* opcode */
    unsigned char opcode_len;

    unsigned char addrsize;	/* 0 or =mode_bits => no override */
    unsigned char opersize;	/* 0 or =mode_bits => no override */
    unsigned char lockrep_pre;	/* 0 indicates no prefix */

    unsigned char def_opersize_64;  /* default operand size in 64-bit mode */
    unsigned char special_prefix;   /* "special" prefix (0=none) */

    unsigned char rex;		/* REX AMD64 extension, 0 if none,
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

typedef struct x86_jmp {
    yasm_bytecode bc;		/* base structure */

    yasm_expr *target;		/* target location */
    /*@dependent@*/ yasm_symrec *origin;    /* jump origin */

    struct {
	unsigned char opcode[3];
	unsigned char opcode_len;   /* 0 = no opc for this version */
    } shortop, nearop, farop;

    /* which opcode are we using? */
    /* The *FORCED forms are specified in the source as such */
    x86_jmp_opcode_sel op_sel;

    unsigned char addrsize;	/* 0 or =mode_bits => no override */
    unsigned char opersize;	/* 0 indicates no override */
    unsigned char lockrep_pre;	/* 0 indicates no prefix */

    unsigned char mode_bits;
} x86_jmp;

/* Effective address callback function prototypes */

static void x86_ea_destroy(yasm_effaddr *ea);
static void x86_ea_print(const yasm_effaddr *ea, FILE *f, int indent_level);

/* Bytecode callback function prototypes */

static void x86_bc_insn_destroy(yasm_bytecode *bc);
static void x86_bc_insn_print(const yasm_bytecode *bc, FILE *f,
			      int indent_level);
static yasm_bc_resolve_flags x86_bc_insn_resolve
    (yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
static int x86_bc_insn_tobytes(yasm_bytecode *bc, unsigned char **bufp,
			       void *d, yasm_output_expr_func output_expr,
			       /*@null@*/ yasm_output_reloc_func output_reloc);

static void x86_bc_jmp_destroy(yasm_bytecode *bc);
static void x86_bc_jmp_print(const yasm_bytecode *bc, FILE *f,
			     int indent_level);
static yasm_bc_resolve_flags x86_bc_jmp_resolve
    (yasm_bytecode *bc, int save, yasm_calc_bc_dist_func calc_bc_dist);
static int x86_bc_jmp_tobytes(yasm_bytecode *bc, unsigned char **bufp,
			      void *d, yasm_output_expr_func output_expr,
			      /*@null@*/ yasm_output_reloc_func output_reloc);

/* Effective address callback structures */

static const yasm_effaddr_callback x86_ea_callback = {
    x86_ea_destroy,
    x86_ea_print
};

/* Bytecode callback structures */

static const yasm_bytecode_callback x86_bc_callback_insn = {
    x86_bc_insn_destroy,
    x86_bc_insn_print,
    x86_bc_insn_resolve,
    x86_bc_insn_tobytes
};

static const yasm_bytecode_callback x86_bc_callback_jmp = {
    x86_bc_jmp_destroy,
    x86_bc_jmp_print,
    x86_bc_jmp_resolve,
    x86_bc_jmp_tobytes
};

int
yasm_x86__set_rex_from_reg(unsigned char *rex, unsigned char *low3,
			   unsigned long reg, unsigned int bits,
			   x86_rex_bit_pos rexbit)
{
    *low3 = (unsigned char)(reg&7);

    if (bits == 64) {
	x86_expritem_reg_size size = (x86_expritem_reg_size)(reg & ~0xFUL);

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
yasm_x86__bc_create_insn(yasm_arch *arch, x86_new_insn_data *d)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    x86_insn *insn;
   
    insn = (x86_insn *)yasm_bc_create_common(&x86_bc_callback_insn,
					     sizeof(x86_insn), d->line);

    insn->ea = (x86_effaddr *)d->ea;
    if (d->ea) {
	insn->ea->origin = d->ea_origin;
	insn->ea->modrm &= 0xC7;	/* zero spare/reg bits */
	insn->ea->modrm |= (d->spare << 3) & 0x38;  /* plug in provided bits */
    }

    if (d->imm) {
	insn->imm = yasm_imm_create_expr(d->imm);
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
    insn->def_opersize_64 = d->def_opersize_64;
    insn->special_prefix = d->special_prefix;
    insn->lockrep_pre = 0;
    insn->rex = d->rex;
    insn->shift_op = d->shift_op;
    insn->signext_imm8_op = d->signext_imm8_op;

    insn->mode_bits = arch_x86->mode_bits;

    return (yasm_bytecode *)insn;
}
/*@=compmempass =mustfree@*/

/*@-compmempass -mustfree@*/
yasm_bytecode *
yasm_x86__bc_create_jmp(yasm_arch *arch, x86_new_jmp_data *d)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    x86_jmp *jmp;

    jmp = (x86_jmp *) yasm_bc_create_common(&x86_bc_callback_jmp,
					    sizeof(x86_jmp), d->line);

    jmp->target = d->target;
    jmp->origin = d->origin;
    jmp->op_sel = d->op_sel;

    if ((d->op_sel == JMP_SHORT_FORCED) && (d->near_op_len == 0))
	yasm__error(d->line,
		    N_("no SHORT form of that jump instruction exists"));
    if ((d->op_sel == JMP_NEAR_FORCED) && (d->short_op_len == 0))
	yasm__error(d->line,
		    N_("no NEAR form of that jump instruction exists"));

    jmp->shortop.opcode[0] = d->short_op[0];
    jmp->shortop.opcode[1] = d->short_op[1];
    jmp->shortop.opcode[2] = d->short_op[2];
    jmp->shortop.opcode_len = d->short_op_len;

    jmp->nearop.opcode[0] = d->near_op[0];
    jmp->nearop.opcode[1] = d->near_op[1];
    jmp->nearop.opcode[2] = d->near_op[2];
    jmp->nearop.opcode_len = d->near_op_len;

    jmp->farop.opcode[0] = d->far_op[0];
    jmp->farop.opcode[1] = d->far_op[1];
    jmp->farop.opcode[2] = d->far_op[2];
    jmp->farop.opcode_len = d->far_op_len;

    jmp->addrsize = d->addrsize;
    jmp->opersize = d->opersize;
    jmp->lockrep_pre = 0;

    jmp->mode_bits = arch_x86->mode_bits;

    return (yasm_bytecode *)jmp;
}
/*@=compmempass =mustfree@*/

void
yasm_x86__ea_set_segment(yasm_effaddr *ea, unsigned int segment,
			 unsigned long line)
{
    x86_effaddr *x86_ea = (x86_effaddr *)ea;

    if (!ea)
	return;

    if (segment != 0 && x86_ea->segment != 0)
	yasm__warning(YASM_WARN_GENERAL, line,
		      N_("multiple segment overrides, using leftmost"));

    x86_ea->segment = (unsigned char)segment;
}

void
yasm_x86__ea_set_disponly(yasm_effaddr *ea)
{
    x86_effaddr *x86_ea = (x86_effaddr *)ea;

    x86_ea->valid_modrm = 0;
    x86_ea->need_modrm = 0;
    x86_ea->valid_sib = 0;
    x86_ea->need_sib = 0;
    x86_ea->pcrel = 0;
}

yasm_effaddr *
yasm_x86__ea_create_reg(unsigned long reg, unsigned char *rex,
			unsigned int bits)
{
    x86_effaddr *x86_ea;
    unsigned char rm;

    if (yasm_x86__set_rex_from_reg(rex, &rm, reg, bits, X86_REX_B))
	return NULL;

    x86_ea = yasm_xmalloc(sizeof(x86_effaddr));

    x86_ea->ea.callback = &x86_ea_callback;
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
    x86_ea->pcrel = 0;

    return (yasm_effaddr *)x86_ea;
}

yasm_effaddr *
yasm_x86__ea_create_expr(yasm_arch *arch, yasm_expr *e)
{
    x86_effaddr *x86_ea;

    x86_ea = yasm_xmalloc(sizeof(x86_effaddr));

    x86_ea->ea.callback = &x86_ea_callback;
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
    x86_ea->pcrel = 0;

    return (yasm_effaddr *)x86_ea;
}

/*@-compmempass@*/
yasm_effaddr *
yasm_x86__ea_create_imm(yasm_expr *imm, unsigned int im_len)
{
    x86_effaddr *x86_ea;

    x86_ea = yasm_xmalloc(sizeof(x86_effaddr));

    x86_ea->ea.callback = &x86_ea_callback;
    x86_ea->ea.disp = imm;
    x86_ea->ea.len = (unsigned char)im_len;
    x86_ea->ea.nosplit = 0;
    x86_ea->segment = 0;
    x86_ea->modrm = 0;
    x86_ea->valid_modrm = 0;
    x86_ea->need_modrm = 0;
    x86_ea->sib = 0;
    x86_ea->valid_sib = 0;
    x86_ea->need_sib = 0;
    x86_ea->pcrel = 0;

    return (yasm_effaddr *)x86_ea;
}
/*@=compmempass@*/

yasm_effaddr *
yasm_x86__bc_insn_get_ea(yasm_bytecode *bc)
{
    if (!bc)
	return NULL;

    if (bc->callback != &x86_bc_callback_insn)
	yasm_internal_error(N_("Trying to get EA of non-instruction"));

    return (yasm_effaddr *)(((x86_insn *)bc)->ea);
}

void
yasm_x86__bc_insn_opersize_override(yasm_bytecode *bc, unsigned int opersize)
{
    if (!bc)
	return;

    if (bc->callback == &x86_bc_callback_insn) {
	x86_insn *insn = (x86_insn *)bc;
	insn->opersize = (unsigned char)opersize;
    } else if (bc->callback == &x86_bc_callback_jmp) {
	x86_jmp *jmp = (x86_jmp *)bc;
	jmp->opersize = (unsigned char)opersize;
    } else
	yasm_internal_error(N_("OperSize override applied to non-instruction"));
}

void
yasm_x86__bc_insn_addrsize_override(yasm_bytecode *bc, unsigned int addrsize)
{
    if (!bc)
	return;

    if (bc->callback == &x86_bc_callback_insn) {
	x86_insn *insn = (x86_insn *)bc;
	insn->addrsize = (unsigned char)addrsize;
    } else if (bc->callback == &x86_bc_callback_jmp) {
	x86_jmp *jmp = (x86_jmp *)bc;
	jmp->addrsize = (unsigned char)addrsize;
    } else
	yasm_internal_error(N_("AddrSize override applied to non-instruction"));
}

void
yasm_x86__bc_insn_set_lockrep_prefix(yasm_bytecode *bc, unsigned int prefix,
				     unsigned long line)
{
    unsigned char *lockrep_pre = (unsigned char *)NULL;

    if (!bc)
	return;

    if (bc->callback == &x86_bc_callback_insn) {
	x86_insn *insn = (x86_insn *)bc;
	lockrep_pre = &insn->lockrep_pre;
    } else if (bc->callback == &x86_bc_callback_jmp) {
	x86_jmp *jmp = (x86_jmp *)bc;
	lockrep_pre = &jmp->lockrep_pre;
    } else
	yasm_internal_error(N_("LockRep prefix applied to non-instruction"));

    if (*lockrep_pre != 0)
	yasm__warning(YASM_WARN_GENERAL, line,
		      N_("multiple LOCK or REP prefixes, using leftmost"));

    *lockrep_pre = (unsigned char)prefix;
}

static void
x86_bc_insn_destroy(yasm_bytecode *bc)
{
    x86_insn *insn = (x86_insn *)bc;
    if (insn->ea)
	yasm_ea_destroy((yasm_effaddr *)insn->ea);
    if (insn->imm) {
	yasm_expr_destroy(insn->imm->val);
	yasm_xfree(insn->imm);
    }
}

static void
x86_bc_jmp_destroy(yasm_bytecode *bc)
{
    x86_jmp *jmp = (x86_jmp *)bc;
    yasm_expr_destroy(jmp->target);
}

static void
x86_ea_destroy(yasm_effaddr *ea)
{
}

static void
x86_ea_print(const yasm_effaddr *ea, FILE *f, int indent_level)
{
    const x86_effaddr *x86_ea = (const x86_effaddr *)ea;
    fprintf(f, "%*sSegmentOv=%02x PCRel=%u\n", indent_level, "",
	    (unsigned int)x86_ea->segment, (unsigned int)x86_ea->pcrel);
    fprintf(f, "%*sModRM=%03o ValidRM=%u NeedRM=%u\n", indent_level, "",
	    (unsigned int)x86_ea->modrm, (unsigned int)x86_ea->valid_modrm,
	    (unsigned int)x86_ea->need_modrm);
    fprintf(f, "%*sSIB=%03o ValidSIB=%u NeedSIB=%u\n", indent_level, "",
	    (unsigned int)x86_ea->sib, (unsigned int)x86_ea->valid_sib,
	    (unsigned int)x86_ea->need_sib);
}

static void
x86_bc_insn_print(const yasm_bytecode *bc, FILE *f, int indent_level)
{
    const x86_insn *insn = (const x86_insn *)bc;

    fprintf(f, "%*s_Instruction_\n", indent_level, "");
    fprintf(f, "%*sEffective Address:", indent_level, "");
    if (insn->ea) {
	fprintf(f, "\n");
	yasm_ea_print((yasm_effaddr *)insn->ea, f, indent_level+1);
    } else
	fprintf(f, " (nil)\n");
    fprintf(f, "%*sImmediate Value:", indent_level, "");
    if (!insn->imm)
	fprintf(f, " (nil)\n");
    else {
	indent_level++;
	fprintf(f, "\n%*sVal=", indent_level, "");
	if (insn->imm->val)
	    yasm_expr_print(insn->imm->val, f);
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
	    "%*sAddrSize=%u OperSize=%u LockRepPre=%02x SpPre=%02x REX=%03o\n",
	    indent_level, "",
	    (unsigned int)insn->addrsize,
	    (unsigned int)insn->opersize,
	    (unsigned int)insn->lockrep_pre,
	    (unsigned int)insn->special_prefix,
	    (unsigned int)insn->rex);
    fprintf(f, "%*sShiftOp=%u BITS=%u\n", indent_level, "",
	    (unsigned int)insn->shift_op,
	    (unsigned int)insn->mode_bits);
}

static void
x86_bc_jmp_print(const yasm_bytecode *bc, FILE *f, int indent_level)
{
    const x86_jmp *jmp = (const x86_jmp *)bc;

    fprintf(f, "%*s_Jump_\n", indent_level, "");
    fprintf(f, "%*sTarget=", indent_level, "");
    yasm_expr_print(jmp->target, f);
    fprintf(f, "%*sOrigin=\n", indent_level, "");
    yasm_symrec_print(jmp->origin, f, indent_level+1);
    fprintf(f, "\n%*sShort Form:\n", indent_level, "");
    if (jmp->shortop.opcode_len == 0)
	fprintf(f, "%*sNone\n", indent_level+1, "");
    else
	fprintf(f, "%*sOpcode: %02x %02x %02x OpLen=%u\n",
		indent_level+1, "",
		(unsigned int)jmp->shortop.opcode[0],
		(unsigned int)jmp->shortop.opcode[1],
		(unsigned int)jmp->shortop.opcode[2],
		(unsigned int)jmp->shortop.opcode_len);
    fprintf(f, "%*sNear Form:\n", indent_level, "");
    if (jmp->nearop.opcode_len == 0)
	fprintf(f, "%*sNone\n", indent_level+1, "");
    else
	fprintf(f, "%*sOpcode: %02x %02x %02x OpLen=%u\n",
		indent_level+1, "",
		(unsigned int)jmp->nearop.opcode[0],
		(unsigned int)jmp->nearop.opcode[1],
		(unsigned int)jmp->nearop.opcode[2],
		(unsigned int)jmp->nearop.opcode_len);
    fprintf(f, "%*sFar Form:\n", indent_level, "");
    if (jmp->farop.opcode_len == 0)
	fprintf(f, "%*sNone\n", indent_level+1, "");
    else
	fprintf(f, "%*sOpcode: %02x %02x %02x OpLen=%u\n",
		indent_level+1, "",
		(unsigned int)jmp->farop.opcode[0],
		(unsigned int)jmp->farop.opcode[1],
		(unsigned int)jmp->farop.opcode[2],
		(unsigned int)jmp->farop.opcode_len);
    fprintf(f, "%*sOpSel=", indent_level, "");
    switch (jmp->op_sel) {
	case JMP_NONE:
	    fprintf(f, "None");
	    break;
	case JMP_SHORT:
	    fprintf(f, "Short");
	    break;
	case JMP_NEAR:
	    fprintf(f, "Near");
	    break;
	case JMP_SHORT_FORCED:
	    fprintf(f, "Forced Short");
	    break;
	case JMP_NEAR_FORCED:
	    fprintf(f, "Forced Near");
	    break;
	case JMP_FAR:
	    fprintf(f, "Far");
	    break;
	default:
	    fprintf(f, "UNKNOWN!!");
	    break;
    }
    fprintf(f, "\n%*sAddrSize=%u OperSize=%u LockRepPre=%02x\n",
	    indent_level, "",
	    (unsigned int)jmp->addrsize,
	    (unsigned int)jmp->opersize,
	    (unsigned int)jmp->lockrep_pre);
    fprintf(f, "%*sBITS=%u\n", indent_level, "",
	    (unsigned int)jmp->mode_bits);
}

static yasm_bc_resolve_flags
x86_bc_insn_resolve(yasm_bytecode *bc, int save,
		    yasm_calc_bc_dist_func calc_bc_dist)
{
    x86_insn *insn = (x86_insn *)bc;
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
	    switch (yasm_x86__expr_checkea(&temp, &insn->addrsize,
		    insn->mode_bits, ea->nosplit, &displen, &eat.modrm,
		    &eat.valid_modrm, &eat.need_modrm, &eat.sib,
		    &eat.valid_sib, &eat.need_sib, &eat.pcrel, &insn->rex,
		    calc_bc_dist)) {
		case 1:
		    yasm_expr_destroy(temp);
		    /* failed, don't bother checking rest of insn */
		    return YASM_BC_RESOLVE_UNKNOWN_LEN|YASM_BC_RESOLVE_ERROR;
		case 2:
		    yasm_expr_destroy(temp);
		    /* failed, don't bother checking rest of insn */
		    return YASM_BC_RESOLVE_UNKNOWN_LEN;
		default:
		    yasm_expr_destroy(temp);
		    /* okay */
		    break;
	    }

	    if (displen != 1) {
		/* Fits into a word/dword, or unknown. */
		retval = YASM_BC_RESOLVE_NONE;  /* may not be smallest size */

		/* Handle unknown case, make displen word-sized */
		if (displen == 0xff)
		    displen = (insn->addrsize == 16) ? 2U : 4U;
	    }

	    if (save) {
		*x86_ea = eat;	/* structure copy */
		ea->len = displen;
		if (displen == 0 && ea->disp) {
		    yasm_expr_destroy(ea->disp);
		    ea->disp = NULL;
		}
	    }
	}

	/* Compute length of ea and add to total */
	bc->len += eat.need_modrm + (eat.need_sib ? 1:0) + displen;
	bc->len += (eat.segment != 0) ? 1 : 0;
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
		    bc->len -= imm->len;

		    if (save) {
			/* Make the ,1 form permanent. */
			insn->opcode[0] = insn->opcode[1];
			/* Delete imm, as it's not needed. */
			yasm_expr_destroy(imm->val);
			yasm_xfree(imm);
			insn->imm = (yasm_immval *)NULL;
		    }
		} else
		    retval = YASM_BC_RESOLVE_NONE;  /* we could still get ,1 */

		/* Not really necessary, but saves confusion over it. */
		if (save)
		    insn->shift_op = 0;
	    }

	    yasm_expr_destroy(temp);
	}

	bc->len += immlen;
    }

    bc->len += insn->opcode_len;
    bc->len += (insn->addrsize != 0 && insn->addrsize != insn->mode_bits) ? 1:0;
    if (insn->opersize != 0 &&
	((insn->mode_bits != 64 && insn->opersize != insn->mode_bits) ||
	 (insn->mode_bits == 64 && insn->opersize == 16)))
	bc->len++;
    bc->len += (insn->special_prefix != 0) ? 1:0;
    bc->len += (insn->lockrep_pre != 0) ? 1:0;
    if (insn->rex != 0xff &&
	(insn->rex != 0 ||
	 (insn->mode_bits == 64 && insn->opersize == 64 &&
	  insn->def_opersize_64 != 64)))
	bc->len++;

    return retval;
}

static yasm_bc_resolve_flags
x86_bc_jmp_resolve(yasm_bytecode *bc, int save,
		   yasm_calc_bc_dist_func calc_bc_dist)
{
    x86_jmp *jmp = (x86_jmp *)bc;
    yasm_bc_resolve_flags retval = YASM_BC_RESOLVE_MIN_LEN;
    /*@null@*/ yasm_expr *temp;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;
    long rel;
    unsigned char opersize;
    x86_jmp_opcode_sel jrtype = JMP_NONE;

    /* As opersize may be 0, figure out its "real" value. */
    opersize = (jmp->opersize == 0) ? jmp->mode_bits : jmp->opersize;

    /* We only check to see if forced forms are actually legal if we're in
     * save mode.  Otherwise we assume that they are legal.
     */
    switch (jmp->op_sel) {
	case JMP_SHORT_FORCED:
	    /* 1 byte relative displacement */
	    jrtype = JMP_SHORT;
	    if (save) {
		temp = yasm_expr_copy(jmp->target);
		temp = yasm_expr_create(YASM_EXPR_SUB, yasm_expr_expr(temp),
					yasm_expr_sym(jmp->origin), bc->line);
		num = yasm_expr_get_intnum(&temp, calc_bc_dist);
		if (!num) {
		    yasm__error(bc->line,
			N_("short jump target external or out of segment"));
		    yasm_expr_destroy(temp);
		    return YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
		} else {
		    rel = yasm_intnum_get_int(num);
		    rel -= jmp->shortop.opcode_len+1;
		    yasm_expr_destroy(temp);
		    /* does a short form exist? */
		    if (jmp->shortop.opcode_len == 0) {
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
	case JMP_NEAR_FORCED:
	    /* 2/4 byte relative displacement (depending on operand size) */
	    jrtype = JMP_NEAR;
	    if (save) {
		if (jmp->nearop.opcode_len == 0) {
		    yasm__error(bc->line, N_("near jump does not exist"));
		    return YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
		}
	    }
	    break;
	default:
	    temp = yasm_expr_copy(jmp->target);
	    temp = yasm_expr_simplify(temp, NULL);

	    /* Check for far displacement (seg:off). */
	    if (yasm_expr_is_op(temp, YASM_EXPR_SEGOFF)) {
		jrtype = JMP_FAR;
		break;	    /* length handled below */
	    } else if (jmp->op_sel == JMP_FAR) {
		yasm__error(bc->line,
			    N_("far jump does not have a far displacement"));
		return YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
	    }

	    /* Try to find shortest displacement based on difference between
	     * target expr value and our (this bytecode's) offset.  Note this
	     * requires offset to be set BEFORE calling calc_len in order for
	     * this test to be valid.
	     */
	    temp = yasm_expr_create(YASM_EXPR_SUB, yasm_expr_expr(temp),
				    yasm_expr_sym(jmp->origin), bc->line);
	    num = yasm_expr_get_intnum(&temp, calc_bc_dist);
	    if (num) {
		rel = yasm_intnum_get_int(num);
		rel -= jmp->shortop.opcode_len+1;
		/* short displacement must fit within -128 <= rel <= +127 */
		if (jmp->shortop.opcode_len != 0 && rel >= -128 &&
		    rel <= 127) {
		    /* It fits into a short displacement. */
		    jrtype = JMP_SHORT;
		} else if (jmp->nearop.opcode_len != 0) {
		    /* Near for now, but could get shorter in the future if
		     * there's a short form available.
		     */
		    jrtype = JMP_NEAR;
		    if (jmp->shortop.opcode_len != 0)
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
		    jrtype = JMP_SHORT;
		}
	    } else {
		/* It's unknown.  Thus, assume near displacement.  If a near
		 * opcode is not available, use a short opcode instead.
		 * If we're saving, error if a near opcode is not available.
		 */
		if (jmp->nearop.opcode_len != 0) {
		    if (jmp->shortop.opcode_len != 0)
			retval = YASM_BC_RESOLVE_NONE;
		    jrtype = JMP_NEAR;
		} else {
		    if (save) {
			yasm__error(bc->line,
			    N_("short jump out of range (near jump does not exist)"));
			return YASM_BC_RESOLVE_ERROR |
			    YASM_BC_RESOLVE_UNKNOWN_LEN;
		    }
		    jrtype = JMP_SHORT;
		}
	    }
	    yasm_expr_destroy(temp);
	    break;
    }

    switch (jrtype) {
	case JMP_SHORT:
	    if (save)
		jmp->op_sel = JMP_SHORT;
	    if (jmp->shortop.opcode_len == 0)
		return YASM_BC_RESOLVE_UNKNOWN_LEN; /* size not available */

	    bc->len += jmp->shortop.opcode_len + 1;
	    break;
	case JMP_NEAR:
	    if (save)
		jmp->op_sel = JMP_NEAR;
	    if (jmp->nearop.opcode_len == 0)
		return YASM_BC_RESOLVE_UNKNOWN_LEN; /* size not available */

	    bc->len += jmp->nearop.opcode_len;
	    bc->len += (opersize == 16) ? 2 : 4;
	    break;
	case JMP_FAR:
	    if (save)
		jmp->op_sel = JMP_FAR;
	    if (jmp->farop.opcode_len == 0)
		return YASM_BC_RESOLVE_UNKNOWN_LEN; /* size not available */

	    bc->len += jmp->farop.opcode_len;
	    bc->len += 2;	/* segment */
	    bc->len += (opersize == 16) ? 2 : 4;
	    break;
	default:
	    yasm_internal_error(N_("unknown jump type"));
    }
    bc->len += (jmp->addrsize != 0 && jmp->addrsize != jmp->mode_bits) ? 1:0;
    bc->len += (jmp->opersize != 0 && jmp->opersize != jmp->mode_bits) ? 1:0;
    bc->len += (jmp->lockrep_pre != 0) ? 1:0;

    return retval;
}

static int
x86_bc_insn_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		    yasm_output_expr_func output_expr,
		    /*@unused@*/ yasm_output_reloc_func output_reloc)
{
    x86_insn *insn = (x86_insn *)bc;
    /*@null@*/ x86_effaddr *x86_ea = insn->ea;
    /*@null@*/ yasm_effaddr *ea = &x86_ea->ea;
    yasm_immval *imm = insn->imm;
    unsigned int i;
    unsigned char *bufp_orig = *bufp;

    /* Prefixes */
    if (insn->special_prefix != 0)
	YASM_WRITE_8(*bufp, insn->special_prefix);
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
    if (insn->rex != 0xff) {
	if (insn->mode_bits == 64 && insn->opersize == 64 &&
	    insn->def_opersize_64 != 64)
	    insn->rex |= 0x48;
	if (insn->rex != 0) {
	    if (insn->mode_bits != 64)
		yasm_internal_error(
		    N_("x86: got a REX prefix in non-64-bit mode"));
	    YASM_WRITE_8(*bufp, insn->rex);
	}
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
	    if (yasm_x86__expr_checkea(&ea->disp, &addrsize, insn->mode_bits,
				       ea->nosplit, &displen, &eat.modrm,
				       &eat.valid_modrm, &eat.need_modrm,
				       &eat.sib, &eat.valid_sib,
				       &eat.need_sib, &eat.pcrel, &insn->rex,
				       yasm_common_calc_bc_dist))
		yasm_internal_error(N_("checkea failed"));

	    if (ea->disp) {
		if (eat.pcrel) {
		    ea->disp =
			yasm_expr_create(YASM_EXPR_SUB,
					 yasm_expr_expr(ea->disp),
					 yasm_expr_sym(eat.origin), bc->line);
		    if (output_expr(&ea->disp, *bufp, ea->len,
				    (size_t)(ea->len*8), 0,
				    (unsigned long)(*bufp-bufp_orig), bc, 1, 1,
				    d))
			return 1;
		} else {
		    if (output_expr(&ea->disp, *bufp, ea->len,
				    (size_t)(ea->len*8), 0,
				    (unsigned long)(*bufp-bufp_orig), bc, 0, 1,
				    d))
			return 1;
		}
		*bufp += ea->len;
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
	if (output_expr(&imm->val, *bufp, imm->len, (size_t)(imm->len*8), 0,
			(unsigned long)(*bufp-bufp_orig), bc, 0, 1, d))
	    return 1;
	*bufp += imm->len;
    }

    return 0;
}

static int
x86_bc_jmp_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		   yasm_output_expr_func output_expr,
		   /*@unused@*/ yasm_output_reloc_func output_reloc)
{
    x86_jmp *jmp = (x86_jmp *)bc;
    unsigned char opersize;
    unsigned int i;
    unsigned char *bufp_orig = *bufp;
    /*@null@*/ yasm_expr *targetseg;

    /* Prefixes */
    if (jmp->lockrep_pre != 0)
	YASM_WRITE_8(*bufp, jmp->lockrep_pre);
    /* FIXME: branch hints! */
    if (jmp->opersize != 0 && jmp->opersize != jmp->mode_bits)
	YASM_WRITE_8(*bufp, 0x66);
    if (jmp->addrsize != 0 && jmp->addrsize != jmp->mode_bits)
	YASM_WRITE_8(*bufp, 0x67);

    /* As opersize may be 0, figure out its "real" value. */
    opersize = (jmp->opersize == 0) ? jmp->mode_bits : jmp->opersize;

    /* Check here to see if forced forms are actually legal. */
    switch (jmp->op_sel) {
	case JMP_SHORT_FORCED:
	case JMP_SHORT:
	    /* 1 byte relative displacement */
	    if (jmp->shortop.opcode_len == 0)
		yasm_internal_error(N_("short jump does not exist"));

	    /* Opcode */
	    for (i=0; i<jmp->shortop.opcode_len; i++)
		YASM_WRITE_8(*bufp, jmp->shortop.opcode[i]);

	    /* Relative displacement */
	    jmp->target =
		yasm_expr_create(YASM_EXPR_SUB, yasm_expr_expr(jmp->target),
				 yasm_expr_sym(jmp->origin), bc->line);
	    if (output_expr(&jmp->target, *bufp, 1, 8, 0,
			    (unsigned long)(*bufp-bufp_orig), bc, 1, 1, d))
		return 1;
	    *bufp += 1;
	    break;
	case JMP_NEAR_FORCED:
	case JMP_NEAR:
	    /* 2/4 byte relative displacement (depending on operand size) */
	    if (jmp->nearop.opcode_len == 0) {
		yasm__error(bc->line, N_("near jump does not exist"));
		return 1;
	    }

	    /* Opcode */
	    for (i=0; i<jmp->nearop.opcode_len; i++)
		YASM_WRITE_8(*bufp, jmp->nearop.opcode[i]);

	    /* Relative displacement */
	    jmp->target =
		yasm_expr_create(YASM_EXPR_SUB, yasm_expr_expr(jmp->target),
				 yasm_expr_sym(jmp->origin), bc->line);
	    i = (opersize == 16) ? 2 : 4;
	    if (output_expr(&jmp->target, *bufp, i, i*8, 0,
			    (unsigned long)(*bufp-bufp_orig), bc, 1, 1, d))
		return 1;
	    *bufp += i;
	    break;
	case JMP_FAR:
	    /* far absolute (4/6 byte depending on operand size) */
	    if (jmp->farop.opcode_len == 0) {
		yasm__error(bc->line, N_("far jump does not exist"));
		return 1;
	    }

	    /* Opcode */
	    for (i=0; i<jmp->farop.opcode_len; i++)
		YASM_WRITE_8(*bufp, jmp->farop.opcode[i]);

	    /* Absolute displacement: segment and offset */
	    jmp->target = yasm_expr_simplify(jmp->target, NULL);
	    targetseg = yasm_expr_extract_segment(&jmp->target);
	    if (!targetseg)
		yasm_internal_error(N_("could not extract segment for far jump"));
	    i = (opersize == 16) ? 2 : 4;
	    if (output_expr(&jmp->target, *bufp, i, i*8, 0,
			    (unsigned long)(*bufp-bufp_orig), bc, 0, 1, d))
		return 1;
	    *bufp += i;
	    if (output_expr(&targetseg, *bufp, 2, 2*8, 0,
			    (unsigned long)(*bufp-bufp_orig), bc, 0, 1, d))
		return 1;
	    *bufp += 2;

	    break;
	default:
	    yasm_internal_error(N_("unrecognized relative jump op_sel"));
    }
    return 0;
}

int
yasm_x86__intnum_tobytes(yasm_arch *arch, const yasm_intnum *intn,
			 unsigned char *buf, size_t destsize, size_t valsize,
			 int shift, const yasm_bytecode *bc, int rel, int warn,
			 unsigned long line)
{
    if (rel) {
	yasm_intnum *relnum, *delta;
	if (valsize != 8 && valsize != 16 && valsize != 32)
	    yasm_internal_error(
		N_("tried to do PC-relative offset from invalid sized value"));
	relnum = yasm_intnum_copy(intn);
	delta = yasm_intnum_create_uint(bc->len);
	yasm_intnum_calc(relnum, YASM_EXPR_SUB, delta, line);
	yasm_intnum_destroy(delta);
	yasm_intnum_get_sized(relnum, buf, destsize, valsize, shift, 0, warn,
			      line);
	yasm_intnum_destroy(relnum);
    } else {
	/* Write value out. */
	yasm_intnum_get_sized(intn, buf, destsize, valsize, shift, 0, warn,
			      line);
    }
    return 0;
}
