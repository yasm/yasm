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
#include "intnum.h"
#include "floatnum.h"
#include "expr.h"

#include "bytecode.h"
#include "section.h"

RCSID("$IdPath$");

struct effaddr {
    expr *disp;			/* address displacement */
    unsigned char len;		/* length of disp (in bytes), 0 if unknown,
				 * 0xff if unknown and required to be >0.
				 */

    unsigned char segment;	/* segment override, 0 if none */

    /* How the spare (register) bits in Mod/RM are handled:
     * Even if valid_modrm=0, the spare bits are still valid (don't overwrite!)
     * They're set in bytecode_new_insn().
     */
    unsigned char modrm;
    unsigned char valid_modrm;	/* 1 if Mod/RM byte currently valid, 0 if not */
    unsigned char need_modrm;	/* 1 if Mod/RM byte needed, 0 if not */

    unsigned char sib;
    unsigned char valid_sib;	/* 1 if SIB byte currently valid, 0 if not */
    unsigned char need_sib;	/* 1 if SIB byte needed, 0 if not,
				   0xff if unknown */
};

struct immval {
    expr *val;

    unsigned char len;		/* length of val (in bytes), 0 if unknown */
    unsigned char isneg;	/* the value has been explicitly negated */

    unsigned char f_len;	/* final imm length */
    unsigned char f_sign;	/* 1 if final imm should be signed */
};

struct dataval {
    STAILQ_ENTRY(dataval) link;

    enum { DV_EMPTY, DV_EXPR, DV_STRING } type;

    union {
	expr *expn;
	char *str_val;
    } data;
};

struct bytecode {
    STAILQ_ENTRY(bytecode) link;

    enum { BC_EMPTY, BC_INSN, BC_JMPREL, BC_DATA, BC_RESERVE } type;

    /* This union has been somewhat tweaked to get it as small as possible
     * on the 4-byte-aligned x86 architecture (without resorting to
     * bitfields).  In particular, insn and jmprel are the largest structures
     * in the union, and are also the same size (after padding).  jmprel
     * can have another unsigned char added to the end without affecting
     * its size.
     *
     * Don't worry about this too much, but keep it in mind when changing
     * this structure.  We care about the size of bytecode in particular
     * because it accounts for the majority of the memory usage in the
     * assembler when assembling a large file.
     */
    union {
	struct {
	    effaddr *ea;	/* effective address */

	    immval *imm;	/* immediate or relative value */

	    unsigned char opcode[3];	/* opcode */
	    unsigned char opcode_len;

	    unsigned char addrsize;	/* 0 or =mode_bits => no override */
	    unsigned char opersize;	/* 0 indicates no override */
	    unsigned char lockrep_pre;	/* 0 indicates no prefix */

	    /* HACK, but a space-saving one: shift opcodes have an immediate
	     * form and a ,1 form (with no immediate).  In the parser, we
	     * set this and opcode_len=1, but store the ,1 version in the
	     * second byte of the opcode array.  We then choose between the
	     * two versions once we know the actual value of imm (because we
	     * don't know it in the parser module).
	     *
	     * A override to force the imm version should just leave this at
	     * 0.  Then later code won't know the ,1 version even exists.
	     * TODO: Figure out how this affects CPU flags processing.
	     *
	     * Call SetInsnShiftFlag() to set this flag to 1.
	     */
	    unsigned char shift_op;
	} insn;
	struct {
	    expr *target;		/* target location */

	    struct {
		unsigned char opcode[3];
		unsigned char opcode_len;   /* 0 = no opc for this version */
	    } shortop, nearop;

	    /* which opcode are we using? */
	    /* The *FORCED forms are specified in the source as such */
	    jmprel_opcode_sel op_sel;

	    unsigned char addrsize;	/* 0 or =mode_bits => no override */
	    unsigned char opersize;	/* 0 indicates no override */
	    unsigned char lockrep_pre;	/* 0 indicates no prefix */
	} jmprel;
	struct {
	    /* non-converted data (linked list) */
	    datavalhead datahead;

	    /* final (converted) size of each element (in bytes) */
	    unsigned char size;
	} data;
	struct {
	    expr *numitems;		/* number of items to reserve */
	    unsigned char itemsize;	/* size of each item (in bytes) */
	} reserve;
    } data;

    expr *multiple;		/* number of times bytecode is repeated,
				   NULL=1 */

    unsigned long len;		/* total length of entire bytecode (including
				   multiple copies), 0 if unknown */

    /* where it came from */
    char *filename;
    unsigned int lineno;

    /* other assembler state info */
    unsigned long offset;	/* 0 if unknown */
    unsigned char mode_bits;
};

/* Static structures for when NULL is passed to conversion functions. */
/*  for Convert*ToBytes() */
unsigned char bytes_static[16];

static bytecode *bytecode_new_common(void);

effaddr *
effaddr_new_reg(unsigned long reg)
{
    effaddr *ea = xmalloc(sizeof(effaddr));

    ea->disp = (expr *)NULL;
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

    ea->disp = expr_ptr;
    ea->len = 0;
    ea->segment = 0;
    ea->modrm = 0;
    ea->valid_modrm = 0;
    ea->need_modrm = 1;
    ea->valid_sib = 0;
    ea->need_sib = 0xff;    /* we won't know until we know more about expr and
			       the BITS/address override setting */

    return ea;
}

effaddr *
effaddr_new_imm(immval *im_ptr, unsigned char im_len)
{
    effaddr *ea = xmalloc(sizeof(effaddr));

    ea->disp = im_ptr->val;
    ea->len = im_len;
    ea->segment = 0;
    ea->modrm = 0;
    ea->valid_modrm = 0;
    ea->need_modrm = 0;
    ea->valid_sib = 0;
    ea->need_sib = 0;

    return ea;
}

immval *
immval_new_int(unsigned long int_val)
{
    immval *im = xmalloc(sizeof(immval));

    im->val = expr_new_ident(ExprInt(intnum_new_int(int_val)));

    if ((int_val & 0xFF) == int_val)
	im->len = 1;
    else if ((int_val & 0xFFFF) == int_val)
	im->len = 2;
    else
	im->len = 4;

    im->isneg = 0;

    return im;
}

immval *
immval_new_expr(expr *expr_ptr)
{
    immval *im = xmalloc(sizeof(immval));

    im->val = expr_ptr;
    im->len = 0;
    im->isneg = 0;

    return im;
}

void
SetEASegment(effaddr *ptr, unsigned char segment)
{
    if (!ptr)
	return;

    if (segment != 0 && ptr->segment != 0)
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

effaddr *
GetInsnEA(bytecode *bc)
{
    if (!bc)
	return NULL;

    if (bc->type != BC_INSN)
	InternalError(__LINE__, __FILE__,
		      _("Trying to get EA of non-instruction"));

    return bc->data.insn.ea;
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

    if (new_sel != JR_NONE && ((*old_sel == JR_SHORT_FORCED) ||
			       (*old_sel == JR_NEAR_FORCED)))
	Warning(_("multiple SHORT or NEAR specifiers, using leftmost"));
    *old_sel = new_sel;
}

void
SetBCMultiple(bytecode *bc, expr *e)
{
    if (bc->multiple)
	bc->multiple = expr_new_tree(bc->multiple, EXPR_MUL, e);
    else
	bc->multiple = e;
}

static bytecode *
bytecode_new_common(void)
{
    bytecode *bc = xmalloc(sizeof(bytecode));

    bc->multiple = (expr *)NULL;
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

    bc->data.insn.imm = im_ptr;
    if (im_ptr) {
	bc->data.insn.imm->f_sign = im_sign;
	bc->data.insn.imm->f_len = im_len;
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
    bytecode *bc = bytecode_new_common();

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
bytecode_print(const bytecode *bc)
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
		if (bc->data.insn.ea->disp)
		    expr_print(bc->data.insn.ea->disp);
		else
		    printf("(nil)");
		printf("\n");
		printf(" Len=%u SegmentOv=%02x\n",
		       (unsigned int)bc->data.insn.ea->len,
		       (unsigned int)bc->data.insn.ea->segment);
		printf(" ModRM=%03o ValidRM=%u NeedRM=%u\n",
		       (unsigned int)bc->data.insn.ea->modrm,
		       (unsigned int)bc->data.insn.ea->valid_modrm,
		       (unsigned int)bc->data.insn.ea->need_modrm);
		printf(" SIB=%03o ValidSIB=%u NeedSIB=%u\n",
		       (unsigned int)bc->data.insn.ea->sib,
		       (unsigned int)bc->data.insn.ea->valid_sib,
		       (unsigned int)bc->data.insn.ea->need_sib);
	    }
	    printf("Immediate Value:\n");
	    printf(" Val=");
	    if (!bc->data.insn.imm)
		printf("(nil)\n");
	    else {
		expr_print(bc->data.insn.imm->val);
		printf("\n");
		printf(" Len=%u, IsNeg=%u\n",
		       (unsigned int)bc->data.insn.imm->len,
		       (unsigned int)bc->data.insn.imm->isneg);
		printf(" FLen=%u, FSign=%u\n",
		       (unsigned int)bc->data.insn.imm->f_len,
		       (unsigned int)bc->data.insn.imm->f_sign);
	    }
	    printf("Opcode: %02x %02x %02x OpLen=%u\n",
		   (unsigned int)bc->data.insn.opcode[0],
		   (unsigned int)bc->data.insn.opcode[1],
		   (unsigned int)bc->data.insn.opcode[2],
		   (unsigned int)bc->data.insn.opcode_len);
	    printf("AddrSize=%u OperSize=%u LockRepPre=%02x ShiftOp=%u\n",
		   (unsigned int)bc->data.insn.addrsize,
		   (unsigned int)bc->data.insn.opersize,
		   (unsigned int)bc->data.insn.lockrep_pre,
		   (unsigned int)bc->data.insn.shift_op);
	    break;
	case BC_JMPREL:
	    printf("_Relative Jump_\n");
	    printf("Target=");
	    expr_print(bc->data.jmprel.target);
	    printf("\nShort Form:\n");
	    if (!bc->data.jmprel.shortop.opcode_len == 0)
		printf(" None\n");
	    else
		printf(" Opcode: %02x %02x %02x OpLen=%u\n",
		       (unsigned int)bc->data.jmprel.shortop.opcode[0],
		       (unsigned int)bc->data.jmprel.shortop.opcode[1],
		       (unsigned int)bc->data.jmprel.shortop.opcode[2],
		       (unsigned int)bc->data.jmprel.shortop.opcode_len);
	    if (!bc->data.jmprel.nearop.opcode_len == 0)
		printf(" None\n");
	    else
		printf(" Opcode: %02x %02x %02x OpLen=%u\n",
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
	    printf("\nAddrSize=%u OperSize=%u LockRepPre=%02x\n",
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
    printf("Multiple=");
    if (!bc->multiple)
	printf("1");
    else
	expr_print(bc->multiple);
    printf("\n");
    printf("Length=%lu\n", bc->len);
    printf("Filename=\"%s\" Line Number=%u\n",
	   bc->filename ? bc->filename : "<UNKNOWN>", bc->lineno);
    printf("Offset=%lx BITS=%u\n", bc->offset, bc->mode_bits);
}

static void
bytecode_parser_finalize_insn(bytecode *bc)
{
    effaddr *ea = bc->data.insn.ea;

    if (ea) {
	if ((ea->disp) && ((!ea->valid_sib && ea->need_sib) ||
			   (!ea->valid_modrm && ea->need_modrm))) {
	    /* First expand equ's */
	    expr_expand_equ(ea->disp);

	    /* Check validity of effective address and calc R/M bits of
	     * Mod/RM byte and SIB byte.  We won't know the Mod field
	     * of the Mod/RM byte until we know more about the
	     * displacement.
	     */
	    if (!expr_checkea(&ea->disp, &bc->data.insn.addrsize,
			      bc->mode_bits, &ea->len, &ea->modrm,
			      &ea->valid_modrm, &ea->need_modrm, &ea->sib,
			      &ea->valid_sib, &ea->need_sib))
		return;	    /* failed, don't bother checking rest of insn */
	}
    }
}

void
bytecode_parser_finalize(bytecode *bc)
{
    switch (bc->type) {
	case BC_EMPTY:
	    /* FIXME: delete it (probably in bytecodes_ level, not here */
	    InternalError(__LINE__, __FILE__,
			  _("got empty bytecode in parser_finalize"));
	    break;
	case BC_INSN:
	    bytecode_parser_finalize_insn(bc);
	    break;
	default:
	    break;
    }
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

void
bytecodes_print(const bytecodehead *headp)
{
    bytecode *cur;

    STAILQ_FOREACH(cur, headp, link) {
	printf("---Next Bytecode---\n");
	bytecode_print(cur);
    }
}

void
bytecodes_parser_finalize(bytecodehead *headp)
{
    bytecode *cur;

    STAILQ_FOREACH(cur, headp, link)
	bytecode_parser_finalize(cur);
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
dataval_new_string(char *str_val)
{
    dataval *retval = xmalloc(sizeof(dataval));

    retval->type = DV_STRING;
    retval->data.str_val = str_val;

    return retval;
}

dataval *
datavals_append(datavalhead *headp, dataval *dv)
{
    if (dv) {
	STAILQ_INSERT_TAIL(headp, dv, link);
	return dv;
    }
    return (dataval *)NULL;
}

void
dataval_print(const datavalhead *head)
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
	    case DV_STRING:
		printf(" String=%s\n", cur->data.str_val);
		break;
	}
    }
}
