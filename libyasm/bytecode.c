/* $Id: bytecode.c,v 1.10 2001/07/05 09:32:58 mu Exp $
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
#include "expr.h"

static effaddr eff_static;
static immval im_static;
unsigned char bytes_static[16];

effaddr *ConvertIntToEA(effaddr *ptr, unsigned long int_val)
{
    if(!ptr)
	ptr = &eff_static;

    ptr->segment = 0;

    ptr->valid_modrm = 0;
    ptr->need_modrm = 1;
    ptr->valid_sib = 0;
    ptr->need_sib = 0;

    ptr->disp = int_val;

    if((int_val & 0xFF) == int_val)
	ptr->len = 1;
    else if((int_val & 0xFFFF) == int_val)
	ptr->len = 2;
    else
	ptr->len = 4;

    return ptr;
}

effaddr *ConvertRegToEA(effaddr *ptr, unsigned long reg)
{
    if(!ptr)
	ptr = &eff_static;

    ptr->len = 0;
    ptr->segment = 0;
    ptr->modrm = 0xC0 | (reg & 0x07);	    /* Mod=11, R/M=Reg, Reg=0 */
    ptr->valid_modrm = 1;
    ptr->need_modrm = 1;
    ptr->valid_sib = 0;
    ptr->need_sib = 0;

    return ptr;
}

effaddr *ConvertImmToEA(effaddr *ptr, immval *im_ptr, unsigned char im_len)
{
    int gotexprval;
    if(!ptr)
	ptr = &eff_static;

    /* FIXME: warn when gotexprval is 0, and/or die */
    gotexprval = expr_get_value (im_ptr->val, &ptr->disp);
    if(im_ptr->len > im_len)
	Warning(WARN_VALUE_EXCEEDS_BOUNDS, (char *)NULL, "word");
    ptr->len = im_len;
    ptr->segment = 0;
    ptr->valid_modrm = 0;
    ptr->need_modrm = 0;
    ptr->valid_sib = 0;
    ptr->need_sib = 0;

    return ptr;
}

immval *ConvertIntToImm(immval *ptr, unsigned long int_val)
{
    if(!ptr)
	ptr = &im_static;

    /* FIXME: this will leak expr's if static is used */
    ptr->val = expr_new_ident(EXPR_NUM, ExprNum(int_val));

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

immval *ConvertExprToImm(immval *ptr, expr *expr_ptr)
{
    if(!ptr)
	ptr = &im_static;

    ptr->val = expr_ptr;

    ptr->isrel = 0;
    ptr->isneg = 0;

    return ptr;
}

void SetEASegment(effaddr *ptr, unsigned char segment)
{
    if(!ptr)
	return;

    if(ptr->segment != 0)
	Warning(WARN_MULT_SEG_OVERRIDE, (char *)NULL);

    ptr->segment = segment;
}

void SetEALen(effaddr *ptr, unsigned char len)
{
    if(!ptr)
	return;

    /* Currently don't warn if length truncated, as this is called only from
     * an explicit override, where we expect the user knows what they're doing.
     */

    ptr->len = len;
}

void SetInsnOperSizeOverride(bytecode *bc, unsigned char opersize)
{
    if(!bc)
	return;

    bc->data.insn.opersize = opersize;
}

void SetInsnAddrSizeOverride(bytecode *bc, unsigned char addrsize)
{
    if(!bc)
	return;

    bc->data.insn.addrsize = addrsize;
}

void SetInsnLockRepPrefix(bytecode *bc, unsigned char prefix)
{
    if(!bc)
	return;

    if(bc->data.insn.lockrep_pre != 0)
	Warning(WARN_MULT_LOCKREP_PREFIX, (char *)NULL);

    bc->data.insn.lockrep_pre = prefix;
}

void BuildBC_Insn(bytecode      *bc,
		  unsigned char  opersize,
		  unsigned char  opcode_len,
		  unsigned char  op0,
		  unsigned char  op1,
		  unsigned char  op2,
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
	bc->data.insn.ea.segment = 0;
	bc->data.insn.ea.need_modrm = 0;
	bc->data.insn.ea.need_sib = 0;
    }

    if(im_ptr) {
	bc->data.insn.imm = *im_ptr;
	bc->data.insn.imm.f_rel = im_rel;
	bc->data.insn.imm.f_sign = im_sign;
	bc->data.insn.imm.f_len = im_len;
    } else {
	bc->data.insn.imm.len = 0;
	bc->data.insn.imm.f_rel = 0;
	bc->data.insn.imm.f_sign = 0;
	bc->data.insn.imm.f_len = 0;
    }

    bc->data.insn.opcode[0] = op0;
    bc->data.insn.opcode[1] = op1;
    bc->data.insn.opcode[2] = op2;
    bc->data.insn.opcode_len = opcode_len;

    bc->data.insn.addrsize = 0;
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
	    printf(" Disp=%lx Len=%u SegmentOv=%2x\n", bc->data.insn.ea.disp,
		(unsigned int)bc->data.insn.ea.len,
		(unsigned int)bc->data.insn.ea.segment);
	    printf(" ModRM=%2x ValidRM=%u NeedRM=%u\n",
		(unsigned int)bc->data.insn.ea.modrm,
		(unsigned int)bc->data.insn.ea.valid_modrm,
		(unsigned int)bc->data.insn.ea.need_modrm);
	    printf(" SIB=%2x ValidSIB=%u NeedSIB=%u\n",
		(unsigned int)bc->data.insn.ea.sib,
		(unsigned int)bc->data.insn.ea.valid_sib,
		(unsigned int)bc->data.insn.ea.need_sib);
	    printf("Immediate/Relative Value:\n");
	    printf(" Val=");
	    expr_print(bc->data.insn.imm.val);
	    printf("\n");
	    printf(" Len=%u, IsRel=%u, IsNeg=%u\n",
		(unsigned int)bc->data.insn.imm.len,
		(unsigned int)bc->data.insn.imm.isrel,
		(unsigned int)bc->data.insn.imm.isneg);
	    printf(" FLen=%u, FRel=%u, FSign=%u\n",
		(unsigned int)bc->data.insn.imm.f_len,
		(unsigned int)bc->data.insn.imm.f_rel,
		(unsigned int)bc->data.insn.imm.f_sign);
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

