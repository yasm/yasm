/* $IdPath$
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
#ifndef YASM_BYTECODE_H
#define YASM_BYTECODE_H

struct section_s;

typedef struct effaddr_s {
    struct expr_s *disp;	/* address displacement */
    unsigned char len;		/* length of disp (in bytes), 0 if none */

    unsigned char segment;	/* segment override, 0 if none */

    unsigned char modrm;
    unsigned char valid_modrm;	/* 1 if Mod/RM byte currently valid, 0 if not */
    unsigned char need_modrm;	/* 1 if Mod/RM byte needed, 0 if not */

    unsigned char sib;
    unsigned char valid_sib;	/* 1 if SIB byte currently valid, 0 if not */
    unsigned char need_sib;	/* 1 if SIB byte needed, 0 if not */
} effaddr;

typedef struct immval_s {
    struct expr_s *val;

    unsigned char len;		/* length of val (in bytes), 0 if none */
    unsigned char isneg;	/* the value has been explicitly negated */

    unsigned char f_len;	/* final imm length */
    unsigned char f_sign;	/* 1 if final imm should be signed */
} immval;

typedef STAILQ_HEAD(datavalhead_s, dataval_s) datavalhead;

typedef struct dataval_s {
    STAILQ_ENTRY(dataval_s) link;

    enum { DV_EMPTY, DV_EXPR, DV_FLOAT, DV_STRING } type;

    union {
	struct expr_s *exp;
	struct floatnum_s *flt;
	char *str_val;
    } data;
} dataval;

typedef enum jmprel_opcode_sel_e {
    JR_NONE,
    JR_SHORT,
    JR_NEAR,
    JR_SHORT_FORCED,
    JR_NEAR_FORCED
} jmprel_opcode_sel;

typedef struct targetval_s {
    struct expr_s *val;

    jmprel_opcode_sel op_sel;
} targetval;

typedef STAILQ_HEAD(bytecodehead_s, bytecode_s) bytecodehead;

typedef struct bytecode_s {
    STAILQ_ENTRY(bytecode_s) link;

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

	    immval imm;		/* immediate or relative value */

	    unsigned char opcode[3];	/* opcode */
	    unsigned char opcode_len;

	    unsigned char addrsize;	/* 0 indicates no override */
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
	    struct expr_s *target;	/* target location */

	    struct {
		unsigned char opcode[3];
		unsigned char opcode_len;   /* 0 = no opc for this version */
	    } shortop, nearop;

	    /* which opcode are we using? */
	    /* The *FORCED forms are specified in the source as such */
	    jmprel_opcode_sel op_sel;

	    unsigned char addrsize;	/* 0 indicates no override */
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
	    struct expr_s *numitems;	/* number of items to reserve */
	    unsigned char itemsize;	/* size of each item (in bytes) */
	} reserve;
    } data;

    unsigned long len;		/* total length of entire bytecode */

    /* where it came from */
    char *filename;
    unsigned int lineno;

    /* other assembler state info */
    unsigned long offset;
    unsigned char mode_bits;
} bytecode;

effaddr *effaddr_new_reg(unsigned long reg);
effaddr *effaddr_new_imm(immval *im_ptr, unsigned char im_len);
effaddr *effaddr_new_expr(struct expr_s *expr_ptr);

immval *ConvertIntToImm(immval *ptr, unsigned long int_val);
immval *ConvertExprToImm(immval *ptr, struct expr_s *expr_ptr);

void SetEASegment(effaddr *ptr, unsigned char segment);
void SetEALen(effaddr *ptr, unsigned char len);

void SetInsnOperSizeOverride(bytecode *bc, unsigned char opersize);
void SetInsnAddrSizeOverride(bytecode *bc, unsigned char addrsize);
void SetInsnLockRepPrefix(bytecode *bc, unsigned char prefix);
void SetInsnShiftFlag(bytecode *bc);

void SetOpcodeSel(jmprel_opcode_sel *old_sel, jmprel_opcode_sel new_sel);

/* IMPORTANT: ea_ptr cannot be reused or freed after calling this function
 * (it doesn't make a copy).  im_ptr, on the other hand, can be.
 */
bytecode *bytecode_new_insn(unsigned char  opersize,
			    unsigned char  opcode_len,
			    unsigned char  op0,
			    unsigned char  op1,
			    unsigned char  op2,
			    effaddr       *ea_ptr,
			    unsigned char  spare,
			    immval        *im_ptr,
			    unsigned char  im_len,
			    unsigned char  im_sign);

/* Pass 0 for the opcode_len if that version of the opcode doesn't exist. */
bytecode *bytecode_new_jmprel(targetval     *target,
			      unsigned char  short_opcode_len,
			      unsigned char  short_op0,
			      unsigned char  short_op1,
			      unsigned char  short_op2,
			      unsigned char  near_opcode_len,
			      unsigned char  near_op0,
			      unsigned char  near_op1,
			      unsigned char  near_op2,
			      unsigned char  addrsize);

bytecode *bytecode_new_data(datavalhead *datahead, unsigned long size);

bytecode *bytecode_new_reserve(struct expr_s *numitems,
			       unsigned long  itemsize);

/* Gets the offset of the bytecode specified by bc if possible.
 * Return value is IF POSSIBLE, not the value.
 */
int bytecode_get_offset(struct section_s *sect, bytecode *bc,
			unsigned long *ret_val);

void bytecode_print(bytecode *bc);

/* void bytecodes_initialize(bytecodehead *headp); */
#define	bytecodes_initialize(headp)	STAILQ_INIT(headp)

/* Adds bc to the list of bytecodes headp.
 * NOTE: Does not make a copy of bc; so don't pass this function
 * static or local variables, and discard the bc pointer after calling
 * this function.  If bc was actually appended (it wasn't NULL or empty),
 * then returns bc, otherwise returns NULL.
 */
bytecode *bytecodes_append(bytecodehead *headp, bytecode *bc);

dataval *dataval_new_expr(struct expr_s *exp);
dataval *dataval_new_float(struct floatnum_s *flt);
dataval *dataval_new_string(char *str_val);

void dataval_print(datavalhead *head);

#endif
