/* $Id: bytecode.h,v 1.1 2001/05/15 05:28:06 peter Exp $
 * Bytecode utility functions header file
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
#ifndef _BYTECODE_H_
#define _BYTECODE_H_

typedef struct effaddr_s {
    unsigned long offset;
    unsigned char len;		/* length of offset + 1 (for RM) (in bytes) */
    unsigned char addrsize;	/* 8, 16, or 32, 0 indicates no override */
    unsigned char segment;	/* segment override, 0 if none */
    unsigned char modrm;
    unsigned char sib;
    unsigned char need_sib;
} effaddr;

typedef struct immval_s {
    unsigned long val;
    unsigned char len;		/* length of val (in bytes) */
} immval;

typedef struct relval_s {
    unsigned long val;
    unsigned char len;		/* length of val (in bytes) */
} relval;

typedef struct bytecode_s {
    struct bytecode_s *next;

    enum { BC_INSN, BC_DATA, BC_RESERVE } type;

    union {
	struct {
	    effaddr ea;			/* effective address */
	    union {
		immval im;		/* immediate value */
		relval rel;
	    } imm;
	    unsigned char opcode[2];	/* opcode */
	    unsigned char opcode_len;
	    unsigned char opersize;	/* 0 indicates no override */
	    unsigned char lockrep_pre;	/* 0 indicates no prefix */
	    unsigned char isrel;
	} insn;
	struct {
	    unsigned char *data;
	} data;
    } data;

    unsigned long len;	/* total length of entire bytecode */

    /* where it came from */
    char *filename;
    unsigned int lineno;

    /* other assembler state info */
    unsigned long offset;
    unsigned int mode_bits;
} bytecode;

effaddr *ConvertIntToEA(effaddr *ptr, unsigned long int_val);
effaddr *ConvertRegToEA(effaddr *ptr, unsigned long reg);

immval *ConvertIntToImm(immval *ptr, unsigned long int_val);

relval *ConvertImmToRel(relval *ptr, immval *im_ptr,
    unsigned char rel_forcelen);

void BuildBC_Insn(bytecode *bc, unsigned char opersize,
    unsigned char opcode_len, unsigned char op0, unsigned char op1,
    effaddr *ea_ptr, unsigned char spare, immval *im_ptr,
    unsigned char im_forcelen);

void BuildBC_Insn_Rel(bytecode *bc, unsigned char opersize,
    unsigned char opcode_len, unsigned char op0, unsigned char op1,
    effaddr *ea_ptr, unsigned char spare, relval *rel_ptr);

unsigned char *ConvertBCInsnToBytes(unsigned char *ptr, bytecode *bc, int *len);

void DebugPrintBC(bytecode *bc);

#endif
