/* $Id: bytecode.c,v 1.5 2001/05/21 21:04:54 peter Exp $
 * Bytecode utility functions
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
#include <stdio.h>
#include "globals.h"
#include "bytecode.h"
#include "errwarn.h"

static effaddr eff_static;
static immval im_static;
unsigned char bytes_static[16];

/* FIXME: converting int to EA, but we don't know addrsize yet? 
   currently assumes BITS setting.. probably better to always do calculations
   in 32 bits in case BITS 16 is going to be overridden later in parsing. */

effaddr *ConvertIntToEA(effaddr *ptr, unsigned long int_val)
{
    if(!ptr)
	ptr = &eff_static;

    ptr->addrsize = 0;
    ptr->segment = 0;

    if(mode_bits == 32) {
	ptr->offset = int_val;
	ptr->len = 4;
	ptr->modrm = 0x05;		    /* Mod=00 R/M=disp32, Reg=0 */
	ptr->need_modrm = 1;
	ptr->need_sib = 0;
    } else if(mode_bits == 16) {
	ptr->offset = int_val & 0xFFFF;
	ptr->len = 2;
	ptr->modrm = 0x06;		    /* Mod=00 R/M=disp16, Reg=0 */
	ptr->need_modrm = 1;
	ptr->need_sib = 0;
    } else
	return (effaddr *)NULL;

    return ptr;
}

effaddr *ConvertRegToEA(effaddr *ptr, unsigned long reg)
{
    if(!ptr)
	ptr = &eff_static;

    ptr->len = 0;
    ptr->addrsize = 0;
    ptr->segment = 0;
    ptr->modrm = 0xC0 | (reg & 0x07);	    /* Mod=11, R/M=Reg, Reg=0 */
    ptr->need_modrm = 1;
    ptr->need_sib = 0;

    return ptr;
}

effaddr *ConvertImmToEA(effaddr *ptr, immval *im_ptr, unsigned char im_len)
{
    if(!ptr)
	ptr = &eff_static;

    ptr->offset = im_ptr->val;
    if(im_ptr->len > im_len)
	Warning(WARN_VALUE_EXCEEDS_BOUNDS, (char *)NULL, "word");
    ptr->len = im_len;
    ptr->addrsize = 0;
    ptr->segment = 0;
    ptr->need_modrm = 0;
    ptr->need_sib = 0;

    return ptr;
}

immval *ConvertIntToImm(immval *ptr, unsigned long int_val)
{
    if(!ptr)
	ptr = &im_static;

    ptr->val = int_val;

    if((int_val & 0xFF) == int_val)
	ptr->len = 1;
    else if((int_val & 0xFFFF) == int_val)
	ptr->len = 2;
    else
	ptr->len = 4;

    ptr->isrel = 0;
    ptr->isneg = 0;

    return ptr;
}

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
		  unsigned char  im_rel)
{
    bc->next = (bytecode *)NULL;
    bc->type = BC_INSN;

    if(ea_ptr) {
	bc->data.insn.ea = *ea_ptr;
	bc->data.insn.ea.modrm &= 0xC7;		/* zero spare/reg bits */
	bc->data.insn.ea.modrm |= (spare << 3) & 0x38;	/* plug in provided bits */
    } else {
	bc->data.insn.ea.len = 0;
	bc->data.insn.ea.addrsize = 0;
	bc->data.insn.ea.segment = 0;
	bc->data.insn.ea.need_modrm = 0;
	bc->data.insn.ea.need_sib = 0;
    }

    if(im_ptr) {
	bc->data.insn.imm = *im_ptr;
    } else {
	bc->data.insn.imm.len = 0;
    }
    bc->data.insn.f_rel_imm = im_rel;
    bc->data.insn.f_sign_imm = im_sign;
    bc->data.insn.f_len_imm = im_len;

    bc->data.insn.opcode[0] = op0;
    bc->data.insn.opcode[1] = op1;
    bc->data.insn.opcode_len = opcode_len;

    bc->data.insn.opersize = opersize;

    bc->len = 0;

    bc->filename = (char *)NULL;
    bc->lineno = line_number;

    bc->offset = 0;
    bc->mode_bits = mode_bits;
}
 
/* TODO: implement.  Shouldn't be difficult. */
unsigned char *ConvertBCInsnToBytes(unsigned char *ptr, bytecode *bc, int *len)
{
    if(bc->type != BC_INSN)
	return (unsigned char *)NULL;
    return (unsigned char *)NULL;
}

void DebugPrintBC(bytecode *bc)
{
    unsigned long i;

    switch(bc->type) {
	case BC_INSN:
	    printf("_Instruction_\n");
	    printf("Effective Address:\n");
	    printf(" Offset=%lx Len=%u\n", bc->data.insn.ea.offset,
		(unsigned int)bc->data.insn.ea.len);
	    printf(" AddrSize=%u SegmentOv=%2x\n",
		(unsigned int)bc->data.insn.ea.addrsize,
		(unsigned int)bc->data.insn.ea.segment);
	    printf(" ModRM=%2x NeedRM=%u SIB=%2x NeedSIB=%u\n",
		(unsigned int)bc->data.insn.ea.modrm,
		(unsigned int)bc->data.insn.ea.need_modrm,
		(unsigned int)bc->data.insn.ea.sib,
		(unsigned int)bc->data.insn.ea.need_sib);
	    printf("Immediate/Relative Value:\n");
	    printf(" Val=%lx Len=%u, IsRel=%u\n", bc->data.insn.imm.val,
		(unsigned int)bc->data.insn.imm.len,
		(unsigned int)bc->data.insn.imm.isrel);
	    printf(" FLen=%u, FRel=%u, FSign=%u\n",
		(unsigned int)bc->data.insn.f_len_imm,
		(unsigned int)bc->data.insn.f_rel_imm,
		(unsigned int)bc->data.insn.f_sign_imm);
	    printf("Opcode: %2x %2x OpLen=%u\n",
		(unsigned int)bc->data.insn.opcode[0],
		(unsigned int)bc->data.insn.opcode[1],
		(unsigned int)bc->data.insn.opcode_len);
	    printf("OperSize=%u LockRepPre=%2x\n",
		(unsigned int)bc->data.insn.opersize,
		(unsigned int)bc->data.insn.lockrep_pre);
	    break;
	case BC_DATA:
	    printf("_Data_\n");
	    for(i=0; i<bc->len; i++) {
		printf("%2x ", (unsigned int)bc->data.data.data[i]);
		if((i & ~0xFFFF) == i)
		    printf("\n");
	    }
	    break;
	case BC_RESERVE:
	    printf("_Reserve_\n");
	    break;
	default:
	    printf("_Unknown_\n");
    }
    printf("Length=%lu\n", bc->len);
    printf("Filename=\"%s\" Line Number=%u\n",
	bc->filename ? bc->filename : "<UNKNOWN>", bc->lineno);
    printf("Offset=%lx BITS=%u\n", bc->offset, bc->mode_bits);
}

