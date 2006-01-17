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
/*@unused@*/ RCSID("$Id$");

#define YASM_LIB_INTERNAL
#define YASM_BC_INTERNAL
#define YASM_EXPR_INTERNAL
#include <libyasm.h>

#include "x86arch.h"


/* Effective address type */

typedef struct x86_effaddr {
    yasm_effaddr ea;		/* base structure */

    /* PC-relative portions are for AMD64 only (RIP addressing) */
    /*@null@*/ /*@dependent@*/ yasm_symrec *origin;    /* pcrel origin */

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

/* Effective address callback function prototypes */

static void x86_ea_destroy(yasm_effaddr *ea);
static void x86_ea_print(const yasm_effaddr *ea, FILE *f, int indent_level);

/* Bytecode callback function prototypes */

static void x86_bc_insn_destroy(void *contents);
static void x86_bc_insn_print(const void *contents, FILE *f,
			      int indent_level);
static int x86_bc_insn_calc_len(yasm_bytecode *bc, unsigned long *long_len,
				yasm_expr **critical, long *neg_thres,
				long *pos_thres);
static void x86_bc_insn_set_long(yasm_bytecode *bc);
static int x86_bc_insn_tobytes(yasm_bytecode *bc, unsigned char **bufp,
			       void *d, yasm_output_expr_func output_expr,
			       /*@null@*/ yasm_output_reloc_func output_reloc);

static void x86_bc_jmp_destroy(void *contents);
static void x86_bc_jmp_print(const void *contents, FILE *f, int indent_level);
static int x86_bc_jmp_calc_len(yasm_bytecode *bc, unsigned long *long_len,
			       yasm_expr **critical, long *neg_thres,
			       long *pos_thres);
static void x86_bc_jmp_set_long(yasm_bytecode *bc);
static int x86_bc_jmp_tobytes(yasm_bytecode *bc, unsigned char **bufp,
			      void *d, yasm_output_expr_func output_expr,
			      /*@null@*/ yasm_output_reloc_func output_reloc);

static void x86_bc_jmpfar_destroy(void *contents);
static void x86_bc_jmpfar_print(const void *contents, FILE *f,
				int indent_level);
static int x86_bc_jmpfar_calc_len(yasm_bytecode *bc, unsigned long *long_len,
				  yasm_expr **critical, long *neg_thres,
				  long *pos_thres);
static int x86_bc_jmpfar_tobytes
    (yasm_bytecode *bc, unsigned char **bufp, void *d,
     yasm_output_expr_func output_expr,
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
    yasm_bc_finalize_common,
    x86_bc_insn_calc_len,
    x86_bc_insn_set_long,
    x86_bc_insn_tobytes
};

static const yasm_bytecode_callback x86_bc_callback_jmp = {
    x86_bc_jmp_destroy,
    x86_bc_jmp_print,
    yasm_bc_finalize_common,
    x86_bc_jmp_calc_len,
    x86_bc_jmp_set_long,
    x86_bc_jmp_tobytes
};

static const yasm_bytecode_callback x86_bc_callback_jmpfar = {
    x86_bc_jmpfar_destroy,
    x86_bc_jmpfar_print,
    yasm_bc_finalize_common,
    x86_bc_jmpfar_calc_len,
    yasm_bc_set_long_common,
    x86_bc_jmpfar_tobytes
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

void
yasm_x86__bc_transform_insn(yasm_bytecode *bc, x86_insn *insn)
{
    yasm_bc_transform(bc, &x86_bc_callback_insn, insn);
}

void
yasm_x86__bc_transform_jmp(yasm_bytecode *bc, x86_jmp *jmp)
{
    yasm_bc_transform(bc, &x86_bc_callback_jmp, jmp);
}

void
yasm_x86__bc_transform_jmpfar(yasm_bytecode *bc, x86_jmpfar *jmpfar)
{
    yasm_bc_transform(bc, &x86_bc_callback_jmpfar, jmpfar);
}

void
yasm_x86__ea_init(yasm_effaddr *ea, unsigned int spare, yasm_symrec *origin)
{
    x86_effaddr *x86_ea = (x86_effaddr *)ea;
    x86_ea->origin = origin;
    x86_ea->modrm &= 0xC7;		    /* zero spare/reg bits */
    x86_ea->modrm |= (spare << 3) & 0x38;   /* plug in provided bits */
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
    x86_ea->ea.strong = 0;
    x86_ea->ea.segreg = 0;
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
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    x86_effaddr *x86_ea;

    x86_ea = yasm_xmalloc(sizeof(x86_effaddr));

    x86_ea->ea.callback = &x86_ea_callback;
    if (arch_x86->parser == X86_PARSER_GAS) {
	/* Need to change foo+rip into foo wrt rip.
	 * Note this assumes a particular ordering coming from the parser
	 * to work (it's not very smart)!
	 */
	if (e->op == YASM_EXPR_ADD && e->terms[0].type == YASM_EXPR_REG
	    && e->terms[0].data.reg == X86_RIP) {
	    /* replace register with 0 */
	    e->terms[0].type = YASM_EXPR_INT;
	    e->terms[0].data.intn = yasm_intnum_create_uint(0);
	    /* build new wrt expression */
	    e = yasm_expr_create(YASM_EXPR_WRT, yasm_expr_expr(e),
				 yasm_expr_reg(X86_RIP), e->line);
	}
    }
    x86_ea->ea.disp = e;
    x86_ea->ea.len = 0;
    x86_ea->ea.nosplit = 0;
    x86_ea->ea.strong = 0;
    x86_ea->ea.segreg = 0;
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
    x86_ea->ea.strong = 0;
    x86_ea->ea.segreg = 0;
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

void
yasm_x86__bc_apply_prefixes(x86_common *common, unsigned char *rex,
			    int num_prefixes, unsigned long **prefixes,
			    unsigned long line)
{
    int i;
    int first = 1;

    for (i=0; i<num_prefixes; i++) {
	switch ((x86_parse_insn_prefix)prefixes[i][0]) {
	    case X86_LOCKREP:
		if (common->lockrep_pre != 0)
		    yasm__warning(YASM_WARN_GENERAL, line,
			N_("multiple LOCK or REP prefixes, using leftmost"));
		common->lockrep_pre = (unsigned char)prefixes[i][1];
		break;
	    case X86_ADDRSIZE:
		common->addrsize = (unsigned char)prefixes[i][1];
		break;
	    case X86_OPERSIZE:
		common->opersize = (unsigned char)prefixes[i][1];
		break;
	    case X86_SEGREG:
		/* This is a hack.. we should really be putting this in the
		 * the effective address!
		 */
		common->lockrep_pre = (unsigned char)prefixes[i][1];
		break;
	    case X86_REX:
		if (!rex)
		    yasm__warning(YASM_WARN_GENERAL, line,
			N_("ignoring REX prefix on jump"));
		else if (*rex == 0xff)
		    yasm__warning(YASM_WARN_GENERAL, line,
			N_("REX prefix not allowed on this instruction, ignoring"));
		else {
		    if (*rex != 0) {
			if (first)
			    yasm__warning(YASM_WARN_GENERAL, line,
				N_("overriding generated REX prefix"));
			else
			    yasm__warning(YASM_WARN_GENERAL, line,
				N_("multiple REX prefixes, using leftmost"));
		    }
		    /* Here we assume that we can't get this prefix in non
		     * 64 bit mode due to checks in parse_check_prefix().
		     */
		    common->mode_bits = 64;
		    *rex = (unsigned char)prefixes[i][1];
		}
		first = 0;
		break;
	}
    }
}

static void
x86_bc_insn_destroy(void *contents)
{
    x86_insn *insn = (x86_insn *)contents;
    if (insn->ea)
	yasm_ea_destroy((yasm_effaddr *)insn->ea);
    if (insn->imm) {
	yasm_expr_destroy(insn->imm->val);
	yasm_xfree(insn->imm);
    }
    yasm_xfree(contents);
}

static void
x86_bc_jmp_destroy(void *contents)
{
    x86_jmp *jmp = (x86_jmp *)contents;
    yasm_expr_destroy(jmp->target);
    yasm_xfree(contents);
}

static void
x86_bc_jmpfar_destroy(void *contents)
{
    x86_jmpfar *jmpfar = (x86_jmpfar *)contents;
    yasm_expr_destroy(jmpfar->segment);
    yasm_expr_destroy(jmpfar->offset);
    yasm_xfree(contents);
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
	    (unsigned int)x86_ea->ea.segreg, (unsigned int)x86_ea->pcrel);
    fprintf(f, "%*sModRM=%03o ValidRM=%u NeedRM=%u\n", indent_level, "",
	    (unsigned int)x86_ea->modrm, (unsigned int)x86_ea->valid_modrm,
	    (unsigned int)x86_ea->need_modrm);
    fprintf(f, "%*sSIB=%03o ValidSIB=%u NeedSIB=%u\n", indent_level, "",
	    (unsigned int)x86_ea->sib, (unsigned int)x86_ea->valid_sib,
	    (unsigned int)x86_ea->need_sib);
}

static void
x86_common_print(const x86_common *common, FILE *f, int indent_level)
{
    fprintf(f, "%*sAddrSize=%u OperSize=%u LockRepPre=%02x BITS=%u\n",
	    indent_level, "",
	    (unsigned int)common->addrsize,
	    (unsigned int)common->opersize,
	    (unsigned int)common->lockrep_pre,
	    (unsigned int)common->mode_bits);
}

static void
x86_opcode_print(const x86_opcode *opcode, FILE *f, int indent_level)
{
    fprintf(f, "%*sOpcode: %02x %02x %02x OpLen=%u\n", indent_level, "",
	    (unsigned int)opcode->opcode[0],
	    (unsigned int)opcode->opcode[1],
	    (unsigned int)opcode->opcode[2],
	    (unsigned int)opcode->len);
}

static void
x86_bc_insn_print(const void *contents, FILE *f, int indent_level)
{
    const x86_insn *insn = (const x86_insn *)contents;

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
    x86_opcode_print(&insn->opcode, f, indent_level);
    x86_common_print(&insn->common, f, indent_level);
    fprintf(f, "%*sSpPre=%02x REX=%03o PostOp=%u\n", indent_level, "",
	    (unsigned int)insn->special_prefix,
	    (unsigned int)insn->rex,
	    (unsigned int)insn->postop);
}

static void
x86_bc_jmp_print(const void *contents, FILE *f, int indent_level)
{
    const x86_jmp *jmp = (const x86_jmp *)contents;

    fprintf(f, "%*s_Jump_\n", indent_level, "");
    fprintf(f, "%*sTarget=", indent_level, "");
    yasm_expr_print(jmp->target, f);
    fprintf(f, "%*sOrigin=\n", indent_level, "");
    yasm_symrec_print(jmp->origin, f, indent_level+1);
    fprintf(f, "\n%*sShort Form:\n", indent_level, "");
    if (jmp->shortop.len == 0)
	fprintf(f, "%*sNone\n", indent_level+1, "");
    else
	x86_opcode_print(&jmp->shortop, f, indent_level+1);
    fprintf(f, "%*sNear Form:\n", indent_level, "");
    if (jmp->nearop.len == 0)
	fprintf(f, "%*sNone\n", indent_level+1, "");
    else
	x86_opcode_print(&jmp->nearop, f, indent_level+1);
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
	default:
	    fprintf(f, "UNKNOWN!!");
	    break;
    }
    x86_common_print(&jmp->common, f, indent_level);
}

static void
x86_bc_jmpfar_print(const void *contents, FILE *f, int indent_level)
{
    const x86_jmpfar *jmpfar = (const x86_jmpfar *)contents;

    fprintf(f, "%*s_Far_Jump_\n", indent_level, "");
    fprintf(f, "%*sSegment=", indent_level, "");
    yasm_expr_print(jmpfar->segment, f);
    fprintf(f, "\n%*sOffset=", indent_level, "");
    yasm_expr_print(jmpfar->offset, f);
    x86_opcode_print(&jmpfar->opcode, f, indent_level);
    x86_common_print(&jmpfar->common, f, indent_level);
}

static unsigned int
x86_common_calc_len(const x86_common *common)
{
    unsigned int len = 0;

    if (common->addrsize != 0 && common->addrsize != common->mode_bits)
	len++;
    if (common->opersize != 0 &&
	((common->mode_bits != 64 && common->opersize != common->mode_bits) ||
	 (common->mode_bits == 64 && common->opersize == 16)))
	len++;
    if (common->lockrep_pre != 0)
	len++;

    return len;
}

static int
x86_bc_insn_calc_len(yasm_bytecode *bc, unsigned long *long_len,
		     yasm_expr **critical, long *neg_thres, long *pos_thres)
{
    x86_insn *insn = (x86_insn *)bc->contents;
    /*@null@*/ yasm_expr *temp;
    x86_effaddr *x86_ea = (x86_effaddr *)insn->ea;
    yasm_effaddr *ea = &x86_ea->ea;
    yasm_immval *imm = insn->imm;
    int retval = 0;
    int common_len = 0;

    if (ea) {
	if (ea->disp) {
	    /* Handle shortmov special-casing */
	    if (insn->postop == X86_POSTOP_SHORTMOV &&
		insn->common.mode_bits == 64 && insn->common.addrsize == 32 &&
		!yasm_expr__contains(ea->disp, YASM_EXPR_REG)) {
		yasm_x86__ea_set_disponly((yasm_effaddr *)x86_ea);
		/* Make the short form permanent. */
		insn->opcode.opcode[0] = insn->opcode.opcode[1];
	    }

	    /* Check validity of effective address and calc R/M bits of
	     * Mod/RM byte and SIB byte.  We won't know the Mod field
	     * of the Mod/RM byte until we know more about the
	     * displacement.
	     */
	    if (yasm_x86__expr_checkea(&ea->disp, &insn->common.addrsize,
		    insn->common.mode_bits, ea->nosplit,
		    insn->postop == X86_POSTOP_ADDRESS16, &ea->len,
		    &x86_ea->modrm, &x86_ea->valid_modrm, &x86_ea->need_modrm,
		    &x86_ea->sib, &x86_ea->valid_sib, &x86_ea->need_sib,
		    &x86_ea->pcrel, &insn->rex))
		/* failed, don't bother checking rest of insn */
		return -1;

	    /* Handle address16 postop case */
	    if (insn->postop == X86_POSTOP_ADDRESS16)
		insn->common.addrsize = 0;

	    if (ea->len == 0xff) {
		/* Handle unknown case, default to byte-sized and set as
		 * critical expression.
		 */
		bc->len += 1;
		long_len += (insn->common.addrsize == 16) ? 2U : 4U;
		*critical = yasm_expr_copy(ea->disp);
		*neg_thres = -128;
		*pos_thres = 127;
		retval = 1;
	    } else
		bc->len += ea->len;
	}

	/* Compute length of ea and add to total */
	common_len += x86_ea->need_modrm + (x86_ea->need_sib ? 1:0);
	common_len += (x86_ea->ea.segreg != 0) ? 1 : 0;
    }

    if (imm) {
	int immlen = imm->len;

	if (imm->val) {
	    /* TODO: check imm->len vs. sized len from expr? */

	    /* Handle signext_imm8 postop special-casing */
	    if (insn->postop == X86_POSTOP_SIGNEXT_IMM8) {
		/*@dependent@*/ /*@null@*/ yasm_intnum *num;
		num = yasm_expr_get_intnum(&imm->val, NULL);
		if (num) {
		    int val = yasm_intnum_get_int(num);
		    if (val >= -128 && val <= 127) {
			/* We can use the sign-extended byte form: shorten
			 * the immediate length to 1.
			 */
			immlen = 1;
		    } else {
			/* We can't use the ,1. */
			insn->opcode.opcode[0] = insn->opcode.opcode[1];
		    }
		    insn->postop = X86_POSTOP_NONE;
		} else {
		    /* Unknown; default to byte form and set as critical
		     * expression.
		     */
		    immlen = 1;
		    long_len += imm->len;
		    *critical = yasm_expr_copy(imm->val);
		    *neg_thres = -128;
		    *pos_thres = 127;
		    retval = 1;
		}
	    }

	    /* Handle shift postop special-casing */
	    if (insn->postop == X86_POSTOP_SHIFT) {
		/*@dependent@*/ /*@null@*/ yasm_intnum *num;
		num = yasm_expr_get_intnum(&imm->val, NULL);
		if (num) {
		    if (yasm_intnum_get_uint(num) == 1) {
			/* We can use the ,1. */
			yasm_expr_destroy(imm->val);
			yasm_xfree(imm);
			insn->imm = NULL;
			immlen = 0;
		    } else {
			/* We can't use the ,1. */
			insn->opcode.opcode[0] = insn->opcode.opcode[1];
		    }
		    insn->postop = X86_POSTOP_NONE;
		} else {
		    /* Unknown; default to ,1 form and set as critical
		     * expression.
		     */
		    immlen = 0;
		    long_len += imm->len;
		    *critical = yasm_expr_copy(imm->val);
		    *neg_thres = 1;
		    *pos_thres = 1;
		    retval = 1;
		}
	    }
	}

	common_len += immlen;
    }

    common_len += insn->opcode.len;
    common_len += x86_common_calc_len(&insn->common);
    common_len += (insn->special_prefix != 0) ? 1:0;
    if (insn->rex != 0xff &&
	(insn->rex != 0 ||
	 (insn->common.mode_bits == 64 && insn->common.opersize == 64 &&
	  insn->def_opersize_64 != 64)))
	common_len++;

    bc->len += common_len;
    if (retval > 0)
	*long_len += common_len;

    return retval;
}

static void
x86_bc_insn_set_long(yasm_bytecode *bc)
{
    x86_insn *insn = (x86_insn *)bc->contents;
    /*@null@*/ yasm_expr *temp;
    x86_effaddr *x86_ea = (x86_effaddr *)insn->ea;
    yasm_effaddr *ea = &x86_ea->ea;
    yasm_immval *imm = insn->imm;

    if (ea && ea->disp) {
	/* Change displacement length into word-sized */
	if (ea->len == 0xff) {
	    ea->len = (insn->common.addrsize == 16) ? 2U : 4U;
	    x86_ea->modrm &= ~0300;
	    x86_ea->modrm |= 0200;
	}
    }

    if (imm && imm->val) {
	/* Handle shift postop special-casing */
	if (insn->postop == X86_POSTOP_SHIFT) {
	    /* We can't use the ,1 form. */
	    insn->opcode.opcode[0] = insn->opcode.opcode[1];
	    insn->postop = X86_POSTOP_NONE;
	}
    }
}

static int
x86_bc_jmp_calc_len(yasm_bytecode *bc, unsigned long *long_len,
		    yasm_expr **critical, long *neg_thres, long *pos_thres)
{
    x86_jmp *jmp = (x86_jmp *)bc->contents;
    int retval = 0;
    unsigned int common_len;
    unsigned char opersize;

    /* As opersize may be 0, figure out its "real" value. */
    opersize = (jmp->common.opersize == 0) ?
	jmp->common.mode_bits : jmp->common.opersize;

    if (jmp->op_sel == JMP_SHORT_FORCED || jmp->nearop.len == 0) {
	if (jmp->shortop.len == 0) {
	    yasm__error(bc->line, N_("short jump does not exist"));
	    return -1;
	}

	/* We want to be sure to error if we exceed short length, so still
	 * put it in as a critical expression, but leave the return value as
	 * 0.
	 */
	bc->len += jmp->shortop.len + 1;
	*critical = yasm_expr_create(YASM_EXPR_SUB,
	    yasm_expr_expr(yasm_expr_copy(jmp->target)),
	    yasm_expr_sym(jmp->origin), bc->line);
	*neg_thres = -128;
	*pos_thres = 127;
    } else if (jmp->op_sel == JMP_NEAR_FORCED || jmp->shortop.len == 0) {
	if (jmp->nearop.len == 0) {
	    yasm__error(bc->line, N_("near jump does not exist"));
	    return -1;
	}

	bc->len += jmp->nearop.len;
	bc->len += (opersize == 16) ? 2 : 4;
    } else {
	/* short length goes into bytecode */
	retval = 1;
	bc->len += jmp->shortop.len + 1;
	*long_len += jmp->nearop.len;
	*long_len += (opersize == 16) ? 2 : 4;
	*critical = yasm_expr_create(YASM_EXPR_SUB,
	    yasm_expr_expr(yasm_expr_copy(jmp->target)),
	    yasm_expr_sym(jmp->origin), bc->line);
	*neg_thres = -128;
	*pos_thres = 127;
    }

    common_len = x86_common_calc_len(&jmp->common);
    bc->len += common_len;
    if (retval > 0)
	*long_len += common_len;

    return retval;
}

static void
x86_bc_jmp_set_long(yasm_bytecode *bc)
{
    x86_jmp *jmp = (x86_jmp *)bc->contents;

    if (jmp->nearop.len == 0) {
	yasm__error(bc->line,
		    N_("short jump out of range"));
	return;
    }
    jmp->op_sel = JMP_NEAR;
}

static int
x86_bc_jmpfar_calc_len(yasm_bytecode *bc, unsigned long *long_len,
		       yasm_expr **critical, long *neg_thres, long *pos_thres)
{
    x86_jmpfar *jmpfar = (x86_jmpfar *)bc->contents;
    unsigned char opersize;
   
    opersize = (jmpfar->common.opersize == 0) ?
	jmpfar->common.mode_bits : jmpfar->common.opersize;

    bc->len += jmpfar->opcode.len;
    bc->len += 2;	/* segment */
    bc->len += (opersize == 16) ? 2 : 4;
    bc->len += x86_common_calc_len(&jmpfar->common);

    return 0;
}

static void
x86_common_tobytes(const x86_common *common, unsigned char **bufp,
		   unsigned int segreg)
{
    if (common->lockrep_pre != 0)
	YASM_WRITE_8(*bufp, common->lockrep_pre);
    if (segreg != 0)
	YASM_WRITE_8(*bufp, (unsigned char)segreg);
    if (common->opersize != 0 &&
	((common->mode_bits != 64 && common->opersize != common->mode_bits) ||
	 (common->mode_bits == 64 && common->opersize == 16)))
	YASM_WRITE_8(*bufp, 0x66);
    if (common->addrsize != 0 && common->addrsize != common->mode_bits)
	YASM_WRITE_8(*bufp, 0x67);
}

static void
x86_opcode_tobytes(const x86_opcode *opcode, unsigned char **bufp)
{
    unsigned int i;
    for (i=0; i<opcode->len; i++)
	YASM_WRITE_8(*bufp, opcode->opcode[i]);
}

static int
x86_bc_insn_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		    yasm_output_expr_func output_expr,
		    /*@unused@*/ yasm_output_reloc_func output_reloc)
{
    x86_insn *insn = (x86_insn *)bc->contents;
    /*@null@*/ x86_effaddr *x86_ea = (x86_effaddr *)insn->ea;
    /*@null@*/ yasm_effaddr *ea = &x86_ea->ea;
    yasm_immval *imm = insn->imm;
    unsigned int i;
    unsigned char *bufp_orig = *bufp;

    /* Prefixes */
    if (insn->special_prefix != 0)
	YASM_WRITE_8(*bufp, insn->special_prefix);
    x86_common_tobytes(&insn->common, bufp, ea ? (ea->segreg>>8) : 0);
    if (insn->rex != 0xff) {
	if (insn->common.mode_bits == 64 && insn->common.opersize == 64 &&
	    insn->def_opersize_64 != 64)
	    insn->rex |= 0x48;
	if (insn->rex != 0) {
	    if (insn->common.mode_bits != 64)
		yasm_internal_error(
		    N_("x86: got a REX prefix in non-64-bit mode"));
	    YASM_WRITE_8(*bufp, insn->rex);
	}
    }

    /* Opcode */
    x86_opcode_tobytes(&insn->opcode, bufp);

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
	    /* Simplify expression */
	    ea->disp = yasm_expr_simplify(ea->disp, yasm_common_calc_bc_dist);
	    if (x86_ea->pcrel) {
		/*@null@*/ yasm_expr *wrt = yasm_expr_extract_wrt(&ea->disp);
		ea->disp =
		    yasm_expr_create(YASM_EXPR_SUB, yasm_expr_expr(ea->disp),
				     yasm_expr_sym(x86_ea->origin), bc->line);
		if (wrt) {
		    ea->disp =
			yasm_expr_create(YASM_EXPR_WRT,
					 yasm_expr_expr(ea->disp),
					 yasm_expr_expr(wrt), bc->line);
		}
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
	     * FIXME: Is this still needed?
	     */
	    for (i=0; i<ea->len; i++)
		YASM_WRITE_8(*bufp, 0);
	}
    }

    /* Immediate (if required) */
    if (imm && imm->val && insn->postop != X86_POSTOP_SHIFT) {
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
    x86_jmp *jmp = (x86_jmp *)bc->contents;
    unsigned char opersize;
    unsigned int i;
    unsigned char *bufp_orig = *bufp;
    /*@null@*/ yasm_expr *wrt;

    /* Prefixes */
    x86_common_tobytes(&jmp->common, bufp, 0);

    /* As opersize may be 0, figure out its "real" value. */
    opersize = (jmp->common.opersize == 0) ?
	jmp->common.mode_bits : jmp->common.opersize;

    /* Check here again to see if forms are actually legal. */
    switch (jmp->op_sel) {
	case JMP_SHORT_FORCED:
	case JMP_SHORT:
	    /* 1 byte relative displacement */
	    if (jmp->shortop.len == 0)
		yasm_internal_error(N_("short jump does not exist"));

	    /* Opcode */
	    x86_opcode_tobytes(&jmp->shortop, bufp);

	    /* Relative displacement */
	    wrt = yasm_expr_extract_wrt(&jmp->target);
	    jmp->target =
		yasm_expr_create(YASM_EXPR_SUB, yasm_expr_expr(jmp->target),
				 yasm_expr_sym(jmp->origin), bc->line);
	    if (wrt)
		jmp->target = yasm_expr_create_tree(jmp->target,
						    YASM_EXPR_WRT, wrt,
						    bc->line);
	    if (output_expr(&jmp->target, *bufp, 1, 8, 0,
			    (unsigned long)(*bufp-bufp_orig), bc, 1, 1, d))
		return 1;
	    *bufp += 1;
	    break;
	case JMP_NEAR_FORCED:
	case JMP_NEAR:
	    /* 2/4 byte relative displacement (depending on operand size) */
	    if (jmp->nearop.len == 0) {
		yasm__error(bc->line, N_("near jump does not exist"));
		return 1;
	    }

	    /* Opcode */
	    x86_opcode_tobytes(&jmp->nearop, bufp);

	    /* Relative displacement */
	    wrt = yasm_expr_extract_wrt(&jmp->target);
	    jmp->target =
		yasm_expr_create(YASM_EXPR_SUB, yasm_expr_expr(jmp->target),
				 yasm_expr_sym(jmp->origin), bc->line);
	    if (wrt)
		jmp->target = yasm_expr_create_tree(jmp->target,
						    YASM_EXPR_WRT, wrt,
						    bc->line);
	    i = (opersize == 16) ? 2 : 4;
	    if (output_expr(&jmp->target, *bufp, i, i*8, 0,
			    (unsigned long)(*bufp-bufp_orig), bc, 1, 1, d))
		return 1;
	    *bufp += i;
	    break;
	default:
	    yasm_internal_error(N_("unrecognized relative jump op_sel"));
    }
    return 0;
}

static int
x86_bc_jmpfar_tobytes(yasm_bytecode *bc, unsigned char **bufp, void *d,
		      yasm_output_expr_func output_expr,
		      /*@unused@*/ yasm_output_reloc_func output_reloc)
{
    x86_jmpfar *jmpfar = (x86_jmpfar *)bc->contents;
    unsigned int i;
    unsigned char *bufp_orig = *bufp;
    unsigned char opersize;

    x86_common_tobytes(&jmpfar->common, bufp, 0);
    x86_opcode_tobytes(&jmpfar->opcode, bufp);

    /* As opersize may be 0, figure out its "real" value. */
    opersize = (jmpfar->common.opersize == 0) ?
	jmpfar->common.mode_bits : jmpfar->common.opersize;

    /* Absolute displacement: segment and offset */
    i = (opersize == 16) ? 2 : 4;
    if (output_expr(&jmpfar->offset, *bufp, i, i*8, 0,
		    (unsigned long)(*bufp-bufp_orig), bc, 0, 1, d))
	return 1;
    *bufp += i;
    if (output_expr(&jmpfar->segment, *bufp, 2, 2*8, 0,
		    (unsigned long)(*bufp-bufp_orig), bc, 0, 1, d))
	return 1;
    *bufp += 2;

    return 0;
}

int
yasm_x86__intnum_fixup_rel(yasm_arch *arch, yasm_intnum *intn, size_t valsize,
			   const yasm_bytecode *bc, unsigned long line)
{
    yasm_intnum *delta;
    if (valsize != 8 && valsize != 16 && valsize != 32)
	yasm_internal_error(
	    N_("tried to do PC-relative offset from invalid sized value"));
    delta = yasm_intnum_create_uint(bc->len);
    yasm_intnum_calc(intn, YASM_EXPR_SUB, delta, line);
    yasm_intnum_destroy(delta);
    return 0;
}

int
yasm_x86__intnum_tobytes(yasm_arch *arch, const yasm_intnum *intn,
			 unsigned char *buf, size_t destsize, size_t valsize,
			 int shift, const yasm_bytecode *bc, int warn,
			 unsigned long line)
{
    /* Write value out. */
    yasm_intnum_get_sized(intn, buf, destsize, valsize, shift, 0, warn,
			  line);
    return 0;
}
