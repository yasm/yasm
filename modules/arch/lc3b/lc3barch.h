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
    unsigned long line;
    /*@keep@*/ /*@null@*/ yasm_expr *imm;
    lc3b_imm_type imm_type;
    /*@null@*/ /*@dependent@*/ yasm_symrec *origin;
    unsigned int opcode;
} lc3b_new_insn_data;

yasm_bytecode *yasm_lc3b__bc_create_insn(lc3b_new_insn_data *d);

void yasm_lc3b__parse_cpu(yasm_arch *arch, const char *cpuid,
			  unsigned long line);

yasm_arch_check_id_retval yasm_lc3b__parse_check_id
    (yasm_arch *arch, unsigned long data[2], const char *id,
     unsigned long line);

/*@null@*/ yasm_bytecode *yasm_lc3b__parse_insn
    (yasm_arch *arch, const unsigned long data[2], int num_operands,
     /*@null@*/ yasm_insn_operands *operands, yasm_bytecode *prev_bc,
     unsigned long line);

int yasm_lc3b__intnum_tobytes
    (yasm_arch *arch, const yasm_intnum *intn, unsigned char *buf,
     size_t destsize, size_t valsize, int shift, const yasm_bytecode *bc,
     int rel, int warn, unsigned long line);
#endif
