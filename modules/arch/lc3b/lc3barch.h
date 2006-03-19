/* $Id$
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

/* Bytecode types */

typedef struct lc3b_insn {
    yasm_value imm;		/* immediate or relative value */
    lc3b_imm_type imm_type;	/* size of the immediate */

    /* PC origin if needed */
    /*@null@*/ /*@dependent@*/ yasm_bytecode *origin_prevbc;

    unsigned int opcode;	/* opcode */
} lc3b_insn;

void yasm_lc3b__bc_transform_insn(yasm_bytecode *bc, lc3b_insn *insn);

void yasm_lc3b__parse_cpu(yasm_arch *arch, const char *cpuid, size_t cpuid_len,
			  unsigned long line);

yasm_arch_insnprefix yasm_lc3b__parse_check_insnprefix
    (yasm_arch *arch, unsigned long data[4], const char *id, size_t id_len,
     unsigned long line);
yasm_arch_regtmod yasm_lc3b__parse_check_regtmod
    (yasm_arch *arch, unsigned long *data, const char *id, size_t id_len,
     unsigned long line);

void yasm_lc3b__finalize_insn
    (yasm_arch *arch, yasm_bytecode *bc, yasm_bytecode *prev_bc,
     const unsigned long data[4], int num_operands,
     /*@null@*/ yasm_insn_operands *operands, int num_prefixes,
     unsigned long **prefixes, int num_segregs, const unsigned long *segregs);

int yasm_lc3b__intnum_tobytes
    (yasm_arch *arch, const yasm_intnum *intn, unsigned char *buf,
     size_t destsize, size_t valsize, int shift, const yasm_bytecode *bc,
     int warn, unsigned long line);
#endif
