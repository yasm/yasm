/* $Id: bytecode.h,v 1.4 2001/05/21 20:17:51 peter Exp $
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
    unsigned char len;		/* length of offset (in bytes), 0 if none */
    unsigned char addrsize;	/* 16 or 32, 0 indicates no override */
    unsigned char segment;	/* segment override, 0 if none */
    unsigned char modrm;
    unsigned char need_modrm;	/* 1 if Mod/RM byte needed, 0 if not */
    unsigned char sib;
    unsigned char need_sib;	/* 1 if SIB byte needed, 0 if not */
} effaddr;

typedef struct immval_s {
    unsigned long val;
    unsigned char len;		/* length of val (in bytes), 0 if none */
    unsigned char isrel;
    unsigned char isneg;	/* the value has been explicitly negated */
} immval;

typedef struct bytecode_s {
    struct bytecode_s *next;

    enum { BC_INSN, BC_DATA, BC_RESERVE } type;

    union {
	struct {
	    effaddr ea;			/* effective address */

	    immval imm;			/* immediate or relative value */
	    unsigned char f_len_imm;	/* final imm length */
	    unsigned char f_rel_imm;	/* 1 if final imm should be rel */
	    unsigned char f_sign_imm;	/* 1 if final imm should be signed */

	    unsigned char opcode[2];	/* opcode */
	    unsigned char opcode_len;

	    unsigned char opersize;	/* 0 indicates no override */
	    unsigned char lockrep_pre;	/* 0 indicates no prefix */
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
effaddr *ConvertImmToEA(effaddr *ptr, immval *im_ptr, unsigned char im_len);

immval *ConvertIntToImm(immval *ptr, unsigned long int_val);

void BuildBC_Insn(bytecode      *bc,
		  unsigned char  opersize,
		  unsigned char  opcode_len,
		  unsigned char  op0,
		  unsigned char  op1,
		  effaddr       *ea_ptr,
		  unsigned char  spare,
		  immval        *im_ptr,
		  unsigned char  im_len,
		  unsigned char  im_sign,
		  unsigned char  im_rel);

unsigned char *ConvertBCInsnToBytes(unsigned char *ptr, bytecode *bc, int *len);

void DebugPrintBC(bytecode *bc);

#endif
