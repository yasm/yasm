/* $IdPath$
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util.h"

#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif

#include <libintl.h>
#define _(String)	gettext(String)

#include "globals.h"
#include "errwarn.h"
#include "floatnum.h"
#include "expr.h"

#include "bytecode.h"
#include "section.h"

RCSID("$IdPath$");

/* Static structures for when NULL is passed to conversion functions. */
/*  for Convert*ToImm() */
static immval im_static;

/*  for Convert*ToBytes() */
unsigned char bytes_static[16];

static bytecode *bytecode_new_common(void);

effaddr *
effaddr_new_reg(unsigned long reg)
{
    effaddr *ea = xmalloc(sizeof(effaddr));

    ea->len = 0;
    ea->segment = 0;
    ea->modrm = 0xC0 | (reg & 0x07);	/* Mod=11, R/M=Reg, Reg=0 */
    ea->valid_modrm = 1;
    ea->need_modrm = 1;
    ea->valid_sib = 0;
    ea->need_sib = 0;

    return ea;
}

effaddr *
effaddr_new_expr(expr *expr_ptr)
{
    effaddr *ea = xmalloc(sizeof(effaddr));

    ea->segment = 0;

    ea->valid_modrm = 0;
    ea->need_modrm = 1;
    ea->valid_sib = 0;
    ea->need_sib = 0;

    ea->disp = expr_ptr;

    return ea;
}

effaddr *
effaddr_new_imm(immval *im_ptr, unsigned char im_len)
{
    effaddr *ea = xmalloc(sizeof(effaddr));

    ea->disp = im_ptr->val;
    if (im_ptr->len > im_len)
	Warning(_("%s value exceeds bounds"), "word");
    ea->len = im_len;
    ea->segment = 0;
    ea->valid_modrm = 0;
    ea->need_modrm = 0;
    ea->valid_sib = 0;
    ea->need_sib = 0;

    return ea;
}

immval *
ConvertIntToImm(immval *ptr, unsigned long int_val)
{
    if (!ptr)
	ptr = &im_static;

    /* FIXME: this will leak expr's if static is used */
    ptr->val = expr_new_ident(EXPR_INT, ExprInt(int_val));

    if ((int_val & 0xFF) == int_val)
	ptr->len = 1;
    else if ((int_val & 0xFFFF) == int_val)
	ptr->len = 2;
    else
	ptr->len = 4;

    ptr->isneg = 0;

    return ptr;
}

immval *
ConvertExprToImm(immval *ptr, expr *expr_ptr)
{
    if (!ptr)
	ptr = &im_static;

    ptr->val = expr_ptr;

    ptr->isneg = 0;

    return ptr;
}

void
SetEASegment(effaddr *ptr, unsigned char segment)
{
    if (!ptr)
	return;

    if (ptr->segment != 0)
	Warning(_("multiple segment overrides, using leftmost"));

    ptr->segment = segment;
}

void
SetEALen(effaddr *ptr, unsigned char len)
{
    if (!ptr)
	return;

    /* Currently don't warn if length truncated, as this is called only from
     * an explicit override, where we expect the user knows what they're doing.
     */

    ptr->len = len;
}

void
SetInsnOperSizeOverride(bytecode *bc, unsigned char opersize)
{
    if (!bc)
	return;

    switch (bc->type) {
	case BC_INSN:
	    bc->data.insn.opersize = opersize;
	    break;
	case BC_JMPREL:
	    bc->data.jmprel.opersize = opersize;
	    break;
	default:
	    InternalError(__LINE__, __FILE__,
			  _("OperSize override applied to non-instruction"));
	    return;
    }
}

void
SetInsnAddrSizeOverride(bytecode *bc, unsigned char addrsize)
{
    if (!bc)
	return;

    switch (bc->type) {
	case BC_INSN:
	    bc->data.insn.addrsize = addrsize;
	    break;
	case BC_JMPREL:
	    bc->data.jmprel.addrsize = addrsize;
	    break;
	default:
	    InternalError(__LINE__, __FILE__,
			  _("AddrSize override applied to non-instruction"));
	    return;
    }
}

void
SetInsnLockRepPrefix(bytecode *bc, unsigned char prefix)
{
    unsigned char *lockrep_pre = (unsigned char *)NULL;

    if (!bc)
	return;

    switch (bc->type) {
	case BC_INSN:
	    lockrep_pre = &bc->data.insn.lockrep_pre;
	    break;
	case BC_JMPREL:
	    lockrep_pre = &bc->data.jmprel.lockrep_pre;
	    break;
	default:
	    InternalError(__LINE__, __FILE__,
			  _("LockRep prefix applied to non-instruction"));
	    return;
    }

    if (*lockrep_pre != 0)
	Warning(_("multiple LOCK or REP prefixes, using leftmost"));

    *lockrep_pre = prefix;
}

void
SetInsnShiftFlag(bytecode *bc)
{
    if (!bc)
	return;

    if (bc->type != BC_INSN)
	InternalError(__LINE__, __FILE__,
		      _("Attempted to set shift flag on non-instruction"));

    bc->data.insn.shift_op = 1;
}

void
SetOpcodeSel(jmprel_opcode_sel *old_sel, jmprel_opcode_sel new_sel)
{
    if (!old_sel)
	return;

    if ((*old_sel == JR_SHORT_FORCED) || (*old_sel == JR_NEAR_FORCED))
	Warning(_("multiple SHORT or NEAR specifiers, using leftmost"));
    *old_sel = new_sel;
}

static bytecode *
bytecode_new_common(void)
{
    bytecode *bc = xmalloc(sizeof(bytecode));

    bc->len = 0;

    bc->filename = xstrdup(in_filename);
    bc->lineno = line_number;

    bc->offset = 0;
    bc->mode_bits = mode_bits;

    return bc;
}

bytecode *
bytecode_new_insn(unsigned char  opersize,
		  unsigned char  opcode_len,
		  unsigned char  op0,
		  unsigned char  op1,
		  unsigned char  op2,
		  effaddr       *ea_ptr,
		  unsigned char  spare,
		  immval        *im_ptr,
		  unsigned char  im_len,
		  unsigned char  im_sign)
{
    bytecode *bc = bytecode_new_common();

    bc->type = BC_INSN;

    bc->data.insn.ea = ea_ptr;
    if (ea_ptr) {
	bc->data.insn.ea->modrm &= 0xC7;	/* zero spare/reg bits */
	bc->data.insn.ea->modrm |= (spare << 3) & 0x38;	/* plug in provided bits */
    }

    if (im_ptr) {
	bc->data.insn.imm = *im_ptr;
	bc->data.insn.imm.f_sign = im_sign;
	bc->data.insn.imm.f_len = im_len;
    } else {
	bc->data.insn.imm.len = 0;
	bc->data.insn.imm.f_sign = 0;
	bc->data.insn.imm.f_len = 0;
    }

    bc->data.insn.opcode[0] = op0;
    bc->data.insn.opcode[1] = op1;
    bc->data.insn.opcode[2] = op2;
    bc->data.insn.opcode_len = opcode_len;

    bc->data.insn.addrsize = 0;
    bc->data.insn.opersize = opersize;
    bc->data.insn.lockrep_pre = 0;
    bc->data.insn.shift_op = 0;

    return bc;
}

bytecode *
bytecode_new_jmprel(targetval     *target,
		    unsigned char  short_opcode_len,
		    unsigned char  short_op0,
		    unsigned char  short_op1,
		    unsigned char  short_op2,
		    unsigned char  near_opcode_len,
		    unsigned char  near_op0,
		    unsigned char  near_op1,
		    unsigned char  near_op2,
		    unsigned char  addrsize)
{
    bytecode *bc = bytecode_new_common();

    bc->type = BC_JMPREL;

    bc->data.jmprel.target = target->val;
    bc->data.jmprel.op_sel = target->op_sel;

    if ((target->op_sel == JR_SHORT_FORCED) && (near_opcode_len == 0))
	Error(_("no SHORT form of that jump instruction exists"));
    if ((target->op_sel == JR_NEAR_FORCED) && (short_opcode_len == 0))
	Error(_("no NEAR form of that jump instruction exists"));

    bc->data.jmprel.shortop.opcode[0] = short_op0;
    bc->data.jmprel.shortop.opcode[1] = short_op1;
    bc->data.jmprel.shortop.opcode[2] = short_op2;
    bc->data.jmprel.shortop.opcode_len = short_opcode_len;

    bc->data.jmprel.nearop.opcode[0] = near_op0;
    bc->data.jmprel.nearop.opcode[1] = near_op1;
    bc->data.jmprel.nearop.opcode[2] = near_op2;
    bc->data.jmprel.nearop.opcode_len = near_opcode_len;

    bc->data.jmprel.addrsize = addrsize;
    bc->data.jmprel.opersize = 0;
    bc->data.jmprel.lockrep_pre = 0;

    return bc;
}

bytecode *
bytecode_new_data(datavalhead *datahead, unsigned long size)
{
    bytecode *bc;
    dataval *cur;

    /* First check to see if all the data elements are valid for the size
     * being set.
     * Validity table:
     *  db (1)  -> expr, string
     *  dw (2)  -> expr, string
     *  dd (4)  -> expr, float, string
     *  dq (8)  -> expr, float, string
     *  dt (10) -> float, string
     *
     * Once we calculate expr we'll have to validate it against the size
     * and warn/error appropriately (symbol constants versus labels:
     * constants (equ's) should always be legal, but labels should raise
     * warnings when used in db or dq context at the minimum).
     */
    STAILQ_FOREACH(cur, datahead, link) {
	switch (cur->type) {
	    case DV_EMPTY:
	    case DV_STRING:
		/* string is valid in every size */
		break;
	    case DV_FLOAT:
		if (size == 1)
		    Error(_("floating-point constant encountered in `%s'"),
			  "DB");
		else if (size == 2)
		    Error(_("floating-point constant encountered in `%s'"),
			  "DW");
		break;
	    case DV_EXPR:
		if (size == 10)
		    Error(_("non-floating-point value encountered in `%s'"),
			  "DT");
		break;
	}
    }

    bc = bytecode_new_common();

    bc->type = BC_DATA;

    bc->data.data.datahead = *datahead;
    bc->data.data.size = size;

    return bc;
}

bytecode *
bytecode_new_reserve(expr *numitems, unsigned long itemsize)
{
    bytecode *bc = bytecode_new_common();

    bc->type = BC_RESERVE;

    bc->data.reserve.numitems = numitems;
    bc->data.reserve.itemsize = itemsize;

    return bc;
}

int
bytecode_get_offset(section *sect, bytecode *bc, unsigned long *ret_val)
{
    return 0;	/* TODO */
}

void
bytecode_print(bytecode *bc)
{
    switch (bc->type) {
	case BC_EMPTY:
	    printf("_Empty_\n");
	    break;
	case BC_INSN:
	    printf("_Instruction_\n");
	    printf("Effective Address:");
	    if (!bc->data.insn.ea)
		printf(" (nil)\n");
	    else {
		printf("\n Disp=");
		expr_print(bc->data.insn.ea->disp);
		printf("\n");
		printf(" Len=%u SegmentOv=%2x\n",
		       (unsigned int)bc->data.insn.ea->len,
		       (unsigned int)bc->data.insn.ea->segment);
		printf(" ModRM=%2x ValidRM=%u NeedRM=%u\n",
		       (unsigned int)bc->data.insn.ea->modrm,
		       (unsigned int)bc->data.insn.ea->valid_modrm,
		       (unsigned int)bc->data.insn.ea->need_modrm);
		printf(" SIB=%2x ValidSIB=%u NeedSIB=%u\n",
		       (unsigned int)bc->data.insn.ea->sib,
		       (unsigned int)bc->data.insn.ea->valid_sib,
		       (unsigned int)bc->data.insn.ea->need_sib);
	    }
	    printf("Immediate Value:\n");
	    printf(" Val=");
	    if (!bc->data.insn.imm.val)
		printf("(nil)");
	    else
		expr_print(bc->data.insn.imm.val);
	    printf("\n");
	    printf(" Len=%u, IsNeg=%u\n",
		   (unsigned int)bc->data.insn.imm.len,
		   (unsigned int)bc->data.insn.imm.isneg);
	    printf(" FLen=%u, FSign=%u\n",
		   (unsigned int)bc->data.insn.imm.f_len,
		   (unsigned int)bc->data.insn.imm.f_sign);
	    printf("Opcode: %2x %2x %2x OpLen=%u\n",
		   (unsigned int)bc->data.insn.opcode[0],
		   (unsigned int)bc->data.insn.opcode[1],
		   (unsigned int)bc->data.insn.opcode[2],
		   (unsigned int)bc->data.insn.opcode_len);
	    printf("AddrSize=%u OperSize=%u LockRepPre=%2x\n",
		   (unsigned int)bc->data.insn.addrsize,
		   (unsigned int)bc->data.insn.opersize,
		   (unsigned int)bc->data.insn.lockrep_pre);
	    break;
	case BC_JMPREL:
	    printf("_Relative Jump_\n");
	    printf("Target=");
	    expr_print(bc->data.jmprel.target);
	    printf("\nShort Form:\n");
	    if (!bc->data.jmprel.shortop.opcode_len == 0)
		printf(" None\n");
	    else
		printf(" Opcode: %2x %2x %2x OpLen=%u\n",
		       (unsigned int)bc->data.jmprel.shortop.opcode[0],
		       (unsigned int)bc->data.jmprel.shortop.opcode[1],
		       (unsigned int)bc->data.jmprel.shortop.opcode[2],
		       (unsigned int)bc->data.jmprel.shortop.opcode_len);
	    if (!bc->data.jmprel.nearop.opcode_len == 0)
		printf(" None\n");
	    else
		printf(" Opcode: %2x %2x %2x OpLen=%u\n",
		       (unsigned int)bc->data.jmprel.nearop.opcode[0],
		       (unsigned int)bc->data.jmprel.nearop.opcode[1],
		       (unsigned int)bc->data.jmprel.nearop.opcode[2],
		       (unsigned int)bc->data.jmprel.nearop.opcode_len);
	    printf("OpSel=");
	    switch (bc->data.jmprel.op_sel) {
		case JR_NONE:
		    printf("None");
		    break;
		case JR_SHORT:
		    printf("Short");
		    break;
		case JR_NEAR:
		    printf("Near");
		    break;
		case JR_SHORT_FORCED:
		    printf("Forced Short");
		    break;
		case JR_NEAR_FORCED:
		    printf("Forced Near");
		    break;
		default:
		    printf("UNKNOWN!!");
		    break;
	    }
	    printf("\nAddrSize=%u OperSize=%u LockRepPre=%2x\n",
		   (unsigned int)bc->data.jmprel.addrsize,
		   (unsigned int)bc->data.jmprel.opersize,
		   (unsigned int)bc->data.jmprel.lockrep_pre);
	    break;
	case BC_DATA:
	    printf("_Data_\n");
	    printf("Final Element Size=%u\n",
		   (unsigned int)bc->data.data.size);
	    printf("Elements:\n");
	    dataval_print(&bc->data.data.datahead);
	    break;
	case BC_RESERVE:
	    printf("_Reserve_\n");
	    printf("Num Items=");
	    expr_print(bc->data.reserve.numitems);
	    printf("\nItem Size=%u\n",
		   (unsigned int)bc->data.reserve.itemsize);
	    break;
	default:
	    printf("_Unknown_\n");
    }
    printf("Length=%lu\n", bc->len);
    printf("Filename=\"%s\" Line Number=%u\n",
	   bc->filename ? bc->filename : "<UNKNOWN>", bc->lineno);
    printf("Offset=%lx BITS=%u\n", bc->offset, bc->mode_bits);
}

bytecode *
bytecodes_append(bytecodehead *headp, bytecode *bc)
{
    if (bc) {
	if (bc->type != BC_EMPTY) {
	    STAILQ_INSERT_TAIL(headp, bc, link);
	    return bc;
	} else {
	    free(bc);
	}
    }
    return (bytecode *)NULL;
}

dataval *
dataval_new_expr(expr *expn)
{
    dataval *retval = xmalloc(sizeof(dataval));

    retval->type = DV_EXPR;
    retval->data.expn = expn;

    return retval;
}

dataval *
dataval_new_float(floatnum *flt)
{
    dataval *retval = xmalloc(sizeof(dataval));

    retval->type = DV_FLOAT;
    retval->data.flt = flt;

    return retval;
}

dataval *
dataval_new_string(char *str_val)
{
    dataval *retval = xmalloc(sizeof(dataval));

    retval->type = DV_STRING;
    retval->data.str_val = str_val;

    return retval;
}

void
dataval_print(datavalhead *head)
{
    dataval *cur;

    STAILQ_FOREACH(cur, head, link) {
	switch (cur->type) {
	    case DV_EMPTY:
		printf(" Empty\n");
		break;
	    case DV_EXPR:
		printf(" Expr=");
		expr_print(cur->data.expn);
		printf("\n");
		break;
	    case DV_FLOAT:
		printf(" Float=");
		floatnum_print(cur->data.flt);
		printf("\n");
		break;
	    case DV_STRING:
		printf(" String=%s\n", cur->data.str_val);
		break;
	}
    }
}
