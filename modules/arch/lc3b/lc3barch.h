/* $IdPath$
 * LC-3b Architecture header file
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
#ifndef YASM_LC3BARCH_H
#define YASM_LC3BARCH_H

typedef enum {
    LC3B_BC_INSN = YASM_BYTECODE_TYPE_BASE
} lc3b_bytecode_type;
#define LC3B_BYTECODE_TYPE_MAX	LC3B_BC_INSN+1

/* Types of immediate.  All immediates are stored in the LSBs of the insn. */
typedef enum lc3b_imm_type {
    LC3B_IMM_NONE = 0,	/* no immediate */
    LC3B_IMM_4,		/* 4-bit */
    LC3B_IMM_5,		/* 5-bit */
    LC3B_IMM_6_WORD,	/* 6-bit, word-multiple (byte>>1) */
    LC3B_IMM_6_BYTE,	/* 6-bit, byte-multiple */
    LC3B_IMM_8,		/* 8-bit, word-multiple (byte>>1) */
    LC3B_IMM_9,		/* 9-bit, signed, word-multiple (byte>>1) */
    LC3B_IMM_9_PC	/* 9-bit, signed, word-multiple, PC relative */
} lc3b_imm_type;

/* Structure with *all* inputs passed to lc3b_bytecode_new_insn().
 * IMPORTANT: im_ptr cannot be reused or freed after calling the function
 * (it doesn't make a copy).
 */
typedef struct lc3b_new_insn_data {
    unsigned long lindex;
    /*@keep@*/ /*@null@*/ yasm_expr *imm;
    lc3b_imm_type imm_type;
    /*@null@*/ /*@dependent@*/ yasm_symrec *origin;
    unsigned int opcode;
} lc3b_new_insn_data;

yasm_bytecode *yasm_lc3b__bc_new_insn(lc3b_new_insn_data *d);

void yasm_lc3b__bc_delete(yasm_bytecode *bc);
void yasm_lc3b__bc_print(FILE *f, int indent_level, const yasm_bytecode *bc);
yasm_bc_resolve_flags yasm_lc3b__bc_resolve
    (yasm_bytecode *bc, int save, const yasm_section *sect,
     yasm_calc_bc_dist_func calc_bc_dist);
int yasm_lc3b__bc_tobytes(yasm_bytecode *bc, unsigned char **bufp,
			  const yasm_section *sect, void *d,
			  yasm_output_expr_func output_expr);

void yasm_lc3b__parse_cpu(const char *cpuid, unsigned long lindex);

yasm_arch_check_id_retval yasm_lc3b__parse_check_id
    (unsigned long data[2], const char *id, unsigned long lindex);

int yasm_lc3b__parse_directive(const char *name, yasm_valparamhead *valparams,
			       /*@null@*/ yasm_valparamhead *objext_valparams,
			       yasm_sectionhead *headp, unsigned long lindex);

/*@null@*/ yasm_bytecode *yasm_lc3b__parse_insn
    (const unsigned long data[2], int num_operands,
     /*@null@*/ yasm_insn_operandhead *operands, yasm_section *cur_section,
     /*@null@*/ yasm_bytecode *prev_bc, unsigned long lindex);

int yasm_lc3b__intnum_tobytes(const yasm_intnum *intn, unsigned char **bufp,
			      unsigned long valsize, const yasm_expr *e,
			      const yasm_bytecode *bc, int rel);

unsigned int yasm_lc3b__get_reg_size(unsigned long reg);

void yasm_lc3b__reg_print(FILE *f, unsigned long reg);

#endif
