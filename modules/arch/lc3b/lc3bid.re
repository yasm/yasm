/*
 * LC-3b identifier recognition and instruction handling
 *
 *  Copyright (C) 2003  Peter Johnson
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
RCSID("$IdPath$");

#define YASM_LIB_INTERNAL
#define YASM_BC_INTERNAL
#define YASM_EXPR_INTERNAL
#include <libyasm.h>

#include "modules/arch/lc3b/lc3barch.h"


/* Opcode modifiers.  The opcode bytes are in "reverse" order because the
 * parameters are read from the arch-specific data in LSB->MSB order.
 * (only for asthetic reasons in the lexer code below, no practical reason).
 */
#define MOD_OpHAdd  (1UL<<0)	/* Parameter adds to upper 8 bits of insn */
#define MOD_OpLAdd  (1UL<<1)	/* Parameter adds to lower 8 bits of insn */

/* Operand types.  These are more detailed than the "general" types for all
 * architectures, as they include the size, for instance.
 * Bit Breakdown (from LSB to MSB):
 *  - 1 bit = general type (must be exact match, except for =3):
 *            0 = immediate
 *            1 = register
 *
 * MSBs than the above are actions: what to do with the operand if the
 * instruction matches.  Essentially describes what part of the output bytecode
 * gets the operand.  This may require conversion (e.g. a register going into
 * an ea field).  Naturally, only one of each of these may be contained in the
 * operands of a single insn_info structure.
 *  - 2 bits = action:
 *             0 = does nothing (operand data is discarded)
 *             1 = DR field
 *             2 = SR field
 *             3 = immediate
 *
 * Immediate operands can have different sizes.
 *  - 3 bits = size:
 *             0 = no immediate
 *             1 = 4-bit immediate
 *             2 = 5-bit immediate
 *             3 = 6-bit index, word (16 bit)-multiple
 *             4 = 6-bit index, byte-multiple
 *             5 = 8-bit immediate, word-multiple
 *             6 = 9-bit signed immediate, word-multiple
 *             7 = 9-bit signed offset from next PC ($+2), word-multiple
 */
#define OPT_Imm		0x0
#define OPT_Reg		0x1
#define OPT_MASK	0x1

#define OPA_None	(0<<1)
#define OPA_DR		(1<<1)
#define OPA_SR		(2<<1)
#define OPA_Imm		(3<<1)
#define OPA_MASK	(3<<1)

#define OPI_None	(LC3B_IMM_NONE<<3)
#define OPI_4		(LC3B_IMM_4<<3)
#define OPI_5		(LC3B_IMM_5<<3)
#define OPI_6W		(LC3B_IMM_6_WORD<<3)
#define OPI_6B		(LC3B_IMM_6_BYTE<<3)
#define OPI_8		(LC3B_IMM_8<<3)
#define OPI_9		(LC3B_IMM_9<<3)
#define OPI_9PC		(LC3B_IMM_9_PC<<3)
#define OPI_MASK	(7<<3)

typedef struct lc3b_insn_info {
    /* Opcode modifiers for variations of instruction.  As each modifier reads
     * its parameter in LSB->MSB order from the arch-specific data[1] from the
     * lexer data, and the LSB of the arch-specific data[1] is reserved for the
     * count of insn_info structures in the instruction grouping, there can
     * only be a maximum of 3 modifiers.
     */
    unsigned int modifiers;

    /* The basic 2 byte opcode */
    unsigned int opcode;

    /* The number of operands this form of the instruction takes */
    unsigned char num_operands;

    /* The types of each operand, see above */
    unsigned int operands[3];
} lc3b_insn_info;

/* Define lexer arch-specific data with 0-3 modifiers. */
#define DEF_INSN_DATA(group, mod)	do { \
    data[0] = (unsigned long)group##_insn; \
    data[1] = ((mod)<<8) | \
    	      ((unsigned char)(sizeof(group##_insn)/sizeof(lc3b_insn_info))); \
    } while (0)

#define RET_INSN(group, mod)	do { \
    DEF_INSN_DATA(group, mod); \
    return YASM_ARCH_CHECK_ID_INSN; \
    } while (0)

/*
 * Instruction groupings
 */

static const lc3b_insn_info addand_insn[] = {
    { MOD_OpHAdd, 0x1000, 3,
      {OPT_Reg|OPA_DR, OPT_Reg|OPA_SR, OPT_Reg|OPA_Imm|OPI_5} },
    { MOD_OpHAdd, 0x1020, 3,
      {OPT_Reg|OPA_DR, OPT_Reg|OPA_SR, OPT_Imm|OPA_Imm|OPI_5} }
};

static const lc3b_insn_info br_insn[] = {
    { MOD_OpHAdd, 0x0000, 1, {OPT_Imm|OPA_Imm|OPI_9PC, 0, 0} }
};

static const lc3b_insn_info jmp_insn[] = {
    { 0, 0xC000, 2, {OPT_Reg|OPA_DR, OPT_Imm|OPA_Imm|OPI_9, 0} }
};

static const lc3b_insn_info lea_insn[] = {
    { 0, 0xE000, 2, {OPT_Reg|OPA_DR, OPT_Imm|OPA_Imm|OPI_9PC, 0} }
};

static const lc3b_insn_info ldst_insn[] = {
    { MOD_OpHAdd, 0x0000, 3,
      {OPT_Reg|OPA_DR, OPT_Reg|OPA_SR, OPT_Imm|OPA_Imm|OPI_6W} }
};

static const lc3b_insn_info ldstb_insn[] = {
    { MOD_OpHAdd, 0x0000, 3,
      {OPT_Reg|OPA_DR, OPT_Reg|OPA_SR, OPT_Imm|OPA_Imm|OPI_6B} }
};

static const lc3b_insn_info not_insn[] = {
    { 0, 0x903F, 2, {OPT_Reg|OPA_DR, OPT_Reg|OPA_SR, 0} }
};

static const lc3b_insn_info nooperand_insn[] = {
    { MOD_OpHAdd, 0x0000, 0, {0, 0, 0} }
};

static const lc3b_insn_info shift_insn[] = {
    { MOD_OpLAdd, 0xD000, 3,
      {OPT_Reg|OPA_DR, OPT_Reg|OPA_SR, OPT_Imm|OPA_Imm|OPI_4} }
};

static const lc3b_insn_info trap_insn[] = {
    { 0, 0xF000, 1, {OPT_Imm|OPA_Imm|OPI_8, 0, 0} }
};

yasm_bytecode *
yasm_lc3b__parse_insn(yasm_arch *arch, const unsigned long data[4],
		      int num_operands, yasm_insn_operands *operands,
		      yasm_bytecode *prev_bc, unsigned long line)
{
    lc3b_new_insn_data d;
    int num_info = (int)(data[1]&0xFF);
    lc3b_insn_info *info = (lc3b_insn_info *)data[0];
    unsigned long mod_data = data[1] >> 8;
    int found = 0;
    yasm_insn_operand *op;
    int i;

    /* Just do a simple linear search through the info array for a match.
     * First match wins.
     */
    for (; num_info>0 && !found; num_info--, info++) {
	int mismatch = 0;

	/* Match # of operands */
	if (num_operands != info->num_operands)
	    continue;

	if (!operands) {
	    found = 1;	    /* no operands -> must have a match here. */
	    break;
	}

	/* Match each operand type and size */
	for(i = 0, op = yasm_ops_first(operands); op && i<info->num_operands &&
	    !mismatch; op = yasm_operand_next(op), i++) {
	    /* Check operand type */
	    switch ((int)(info->operands[i] & OPT_MASK)) {
		case OPT_Imm:
		    if (op->type != YASM_INSN__OPERAND_IMM)
			mismatch = 1;
		    break;
		case OPT_Reg:
		    if (op->type != YASM_INSN__OPERAND_REG)
			mismatch = 1;
		    break;
		default:
		    yasm_internal_error(N_("invalid operand type"));
	    }

	    if (mismatch)
		break;
	}

	if (!mismatch) {
	    found = 1;
	    break;
	}
    }

    if (!found) {
	/* Didn't find a matching one */
	yasm__error(line, N_("invalid combination of opcode and operands"));
	return NULL;
    }

    /* Copy what we can from info */
    d.line = line;
    d.imm = NULL;
    d.imm_type = LC3B_IMM_NONE;
    d.origin = NULL;
    d.opcode = info->opcode;

    /* Apply modifiers */
    if (info->modifiers & MOD_OpHAdd) {
	d.opcode += ((unsigned int)(mod_data & 0xFF))<<8;
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_OpLAdd) {
	d.opcode += (unsigned int)(mod_data & 0xFF);
	/*mod_data >>= 8;*/
    }

    /* Go through operands and assign */
    if (operands) {
	for(i = 0, op = yasm_ops_first(operands); op && i<info->num_operands;
	    op = yasm_operand_next(op), i++) {
	    switch ((int)(info->operands[i] & OPA_MASK)) {
		case OPA_None:
		    /* Throw away the operand contents */
		    if (op->type == YASM_INSN__OPERAND_IMM)
			yasm_expr_destroy(op->data.val);
		    break;
		case OPA_DR:
		    if (op->type != YASM_INSN__OPERAND_REG)
			yasm_internal_error(N_("invalid operand conversion"));
		    d.opcode |= ((unsigned int)(op->data.reg & 0x7)) << 9;
		    break;
		case OPA_SR:
		    if (op->type != YASM_INSN__OPERAND_REG)
			yasm_internal_error(N_("invalid operand conversion"));
		    d.opcode |= ((unsigned int)(op->data.reg & 0x7)) << 6;
		    break;
		case OPA_Imm:
		    switch (op->type) {
			case YASM_INSN__OPERAND_IMM:
			    d.imm = op->data.val;
			    break;
			case YASM_INSN__OPERAND_REG:
			    d.imm = yasm_expr_create_ident(yasm_expr_int(
				yasm_intnum_create_uint(op->data.reg & 0x7)),
				line);
			    break;
			default:
			    yasm_internal_error(N_("invalid operand conversion"));
		    }
		    break;
		default:
		    yasm_internal_error(N_("unknown operand action"));
	    }

	    d.imm_type = (info->operands[i] & OPI_MASK)>>3;
	    if (d.imm_type == LC3B_IMM_9_PC)
		d.origin = yasm_symtab_define_label2("$", prev_bc, 0, line);
	}
    }

    /* Create the bytecode and return it */
    return yasm_lc3b__bc_create_insn(&d);
}


#define YYCTYPE		char
#define YYCURSOR	id
#define YYLIMIT		id
#define YYMARKER	marker
#define YYFILL(n)

/*!re2c
  any = [\000-\377];
  A = [aA];
  B = [bB];
  C = [cC];
  D = [dD];
  E = [eE];
  F = [fF];
  G = [gG];
  H = [hH];
  I = [iI];
  J = [jJ];
  K = [kK];
  L = [lL];
  M = [mM];
  N = [nN];
  O = [oO];
  P = [pP];
  Q = [qQ];
  R = [rR];
  S = [sS];
  T = [tT];
  U = [uU];
  V = [vV];
  W = [wW];
  X = [xX];
  Y = [yY];
  Z = [zZ];
*/

void
yasm_lc3b__parse_cpu(yasm_arch *arch, const char *id, unsigned long line)
{
}

yasm_arch_check_id_retval
yasm_lc3b__parse_check_id(yasm_arch *arch, unsigned long data[4],
			  const char *id, unsigned long line)
{
    const char *oid = id;
    /*const char *marker;*/
    /*!re2c

	/* integer registers */
	R [0-7]	{
	    data[0] = (oid[1]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}

	/* instructions */

	A D D { RET_INSN(addand, 0x00); }
	A N D { RET_INSN(addand, 0x40); }

	B R { RET_INSN(br, 0x00); }
	B R N { RET_INSN(br, 0x08); }
	B R Z { RET_INSN(br, 0x04); }
	B R P { RET_INSN(br, 0x02); }
	B R N Z { RET_INSN(br, 0x0C); }
	B R N P { RET_INSN(br, 0x0A); }
	B R Z P { RET_INSN(br, 0x06); }
	B R N Z P { RET_INSN(br, 0x0E); }
	J S R { RET_INSN(br, 0x40); }

	J M P { RET_INSN(jmp, 0); }

	L E A { RET_INSN(lea, 0); }

	L D { RET_INSN(ldst, 0x20); }
	L D I { RET_INSN(ldst, 0xA0); }
	S T { RET_INSN(ldst, 0x30); }
	S T I { RET_INSN(ldst, 0xB0); }

	L D B { RET_INSN(ldstb, 0x60); }
	S T B { RET_INSN(ldstb, 0x70); }

	N O T { RET_INSN(not, 0); }

	R E T { RET_INSN(nooperand, 0xCE); }
	R T I { RET_INSN(nooperand, 0x80); }

	L S H F { RET_INSN(shift, 0x00); }
	R S H F L { RET_INSN(shift, 0x10); }
	R S H F A { RET_INSN(shift, 0x30); }

	T R A P { RET_INSN(trap, 0); }

	/* catchalls */
	[\001-\377]+	{
	    return YASM_ARCH_CHECK_ID_NONE;
	}
	[\000]	{
	    return YASM_ARCH_CHECK_ID_NONE;
	}
    */
}
