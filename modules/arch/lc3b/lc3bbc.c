/*
 * LC-3b bytecode utility functions
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
/*@unused@*/ RCSID("$IdPath$");

#define YASM_LIB_INTERNAL
#define YASM_BC_INTERNAL
#include <libyasm.h>

#include "lc3barch.h"


typedef struct lc3b_insn {
    yasm_bytecode bc;		/* base structure */

    /*@null@*/ yasm_expr *imm;	/* immediate or relative value */
    lc3b_imm_type imm_type;	/* size of the immediate */

    /*@null@*/ /*@dependent@*/ yasm_symrec *origin; /* PC origin if needed */

    unsigned int opcode;	/* opcode */
} lc3b_insn;


/*@-compmempass -mustfree@*/
yasm_bytecode *
yasm_lc3b__bc_new_insn(lc3b_new_insn_data *d)
{
    lc3b_insn *insn;
   
    insn = (lc3b_insn *)yasm_bc_new_common((yasm_bytecode_type)LC3B_BC_INSN,
					   sizeof(lc3b_insn), d->lindex);

    insn->imm = d->imm;
    if (d->imm)
	insn->imm_type = d->imm_type;
    else
	insn->imm_type = LC3B_IMM_NONE;
    insn->origin = d->origin;
    insn->opcode = d->opcode;

    return (yasm_bytecode *)insn;
}
/*@=compmempass =mustfree@*/

void
yasm_lc3b__bc_delete(yasm_bytecode *bc)
{
    lc3b_insn *insn;

    if ((lc3b_bytecode_type)bc->type != LC3B_BC_INSN)
	return;

    insn = (lc3b_insn *)bc;
    if (insn->imm)
	yasm_expr_delete(insn->imm);
}

void
yasm_lc3b__bc_print(FILE *f, int indent_level, const yasm_bytecode *bc)
{
    const lc3b_insn *insn;

    if ((lc3b_bytecode_type)bc->type != LC3B_BC_INSN)
	return;

    insn = (const lc3b_insn *)bc;
    fprintf(f, "%*s_Instruction_\n", indent_level, "");
    fprintf(f, "%*sImmediate Value:", indent_level, "");
    if (!insn->imm)
	fprintf(f, " (nil)\n");
    else {
	indent_level++;
	fprintf(f, "\n%*sVal=", indent_level, "");
	yasm_expr_print(f, insn->imm);
	fprintf(f, "\n%*sType=", indent_level, "");
	switch (insn->imm_type) {
	    case LC3B_IMM_NONE:
		fprintf(f, "NONE-SHOULDN'T HAPPEN");
		break;
	    case LC3B_IMM_4:
		fprintf(f, "4-bit");
		break;
	    case LC3B_IMM_5:
		fprintf(f, "5-bit");
		break;
	    case LC3B_IMM_6_WORD:
		fprintf(f, "6-bit, word-multiple");
		break;
	    case LC3B_IMM_6_BYTE:
		fprintf(f, "6-bit, byte-multiple");
		break;
	    case LC3B_IMM_8:
		fprintf(f, "8-bit, word-multiple");
		break;
	    case LC3B_IMM_9:
		fprintf(f, "9-bit, signed, word-multiple");
		break;
	    case LC3B_IMM_9_PC:
		fprintf(f, "9-bit, signed, word-multiple, PC-relative");
		break;
	}
	indent_level--;
    }
    fprintf(f, "%*sOrigin=", indent_level, "");
    if (insn->origin) {
	fprintf(f, "\n");
	yasm_symrec_print(f, indent_level+1, insn->origin);
    } else
	fprintf(f, "(nil)\n");
    fprintf(f, "%*sOpcode: %04x\n", indent_level, "",
	    (unsigned int)insn->opcode);
}

yasm_bc_resolve_flags
yasm_lc3b__bc_resolve(yasm_bytecode *bc, int save, const yasm_section *sect,
		      yasm_calc_bc_dist_func calc_bc_dist)
{
    lc3b_insn *insn;
    /*@null@*/ yasm_expr *temp;
    /*@dependent@*/ /*@null@*/ const yasm_intnum *num;
    long rel;

    if ((lc3b_bytecode_type)bc->type != LC3B_BC_INSN)
	yasm_internal_error(N_("Didn't handle bytecode type in lc3b arch"));

    insn = (lc3b_insn *)bc;

    /* Fixed size instruction length */
    bc->len = 2;

    /* Only need to worry about out-of-range to PC-relative, and only if we're
     * saving.
     */
    if (insn->imm_type != LC3B_IMM_9_PC || !save)
	return YASM_BC_RESOLVE_MIN_LEN;

    temp = yasm_expr_copy(insn->imm);
    temp = yasm_expr_new(YASM_EXPR_SUB, yasm_expr_expr(temp),
			 yasm_expr_sym(insn->origin), bc->line);
    num = yasm_expr_get_intnum(&temp, calc_bc_dist);
    if (!num) {
	yasm__error(bc->line, N_("target external or out of segment"));
	yasm_expr_delete(temp);
	return YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
    }
    rel = yasm_intnum_get_int(num);
    rel -= 2;
    yasm_expr_delete(temp);
    /* 9-bit signed, word-multiple displacement */
    if (rel < -512 || rel > 512) {
	yasm__error(bc->line, N_("target out of range"));
	return YASM_BC_RESOLVE_ERROR | YASM_BC_RESOLVE_UNKNOWN_LEN;
    }
    return YASM_BC_RESOLVE_MIN_LEN;
}

int
yasm_lc3b__bc_tobytes(yasm_bytecode *bc, unsigned char **bufp,
		      const yasm_section *sect, void *d,
		      yasm_output_expr_func output_expr)
{
    lc3b_insn *insn;
    unsigned int opcode;
    unsigned int val;
    int rel = 0;

    if ((lc3b_bytecode_type)bc->type != LC3B_BC_INSN)
	return 0;

    insn = (lc3b_insn *)bc;

    /* Change into PC-relative if necessary */
    if (insn->imm_type == LC3B_IMM_9_PC) {
	insn->imm = yasm_expr_new(YASM_EXPR_SUB, yasm_expr_expr(insn->imm),
				  yasm_expr_sym(insn->origin), bc->line);
	rel = 1;
    }

    /* Get immediate value */
    if (output_expr(&insn->imm, bufp, 2, 0, sect, bc, rel, d))
	return 1;
    *bufp -= 2;
    YASM_LOAD_16_L(val, *bufp);

    opcode = insn->opcode;

    /* Insert immediate into opcode.  Warn on overflow? */
    switch (insn->imm_type) {
	case LC3B_IMM_NONE:
	    break;
	case LC3B_IMM_4:
	    opcode &= ~0xF;
	    opcode |= (val & 0xF);
	    break;
	case LC3B_IMM_5:
	    opcode &= ~0x1F;
	    opcode |= (val & 0x1F);
	    break;
	case LC3B_IMM_6_WORD:
	    if (val & 1)
		yasm__warning(YASM_WARN_GENERAL, bc->line,
			      N_("Misaligned access, truncating to boundary"));
	    val >>= 1;
	    /*@fallthrough@*/
	case LC3B_IMM_6_BYTE:
	    opcode &= ~0x3F;
	    opcode |= (val & 0x3F);
	    break;
	case LC3B_IMM_8:
	    if (val & 1)
		yasm__warning(YASM_WARN_GENERAL, bc->line,
			      N_("Misaligned access, truncating to boundary"));
	    opcode &= ~0xFF;
	    opcode |= ((val>>1) & 0xFF);
	    break;
	case LC3B_IMM_9_PC:
	    val -= 2;
	    /*@fallthrough@*/
	case LC3B_IMM_9:
	    if (val & 1)
		yasm__warning(YASM_WARN_GENERAL, bc->line,
			      N_("Misaligned access, truncating to boundary"));
	    opcode &= ~0x1FF;
	    opcode |= ((val>>1) & 0x1FF);
	    break;
	default:
	    yasm_internal_error(N_("Unrecognized immediate type"));
    }

    /* Output it */
    YASM_WRITE_16_L(*bufp, opcode);

    return 0;
}

int
yasm_lc3b__intnum_tobytes(const yasm_intnum *intn, unsigned char **bufp,
			  unsigned long valsize, const yasm_expr *e,
			  const yasm_bytecode *bc, int rel)
{
    /* Write value out. */
    yasm_intnum_get_sized(intn, *bufp, (size_t)valsize);
    *bufp += valsize;
    return 0;
}
