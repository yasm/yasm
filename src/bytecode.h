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

#ifndef YASM_SECTION
#define YASM_SECTION
typedef struct section section;
#endif

#ifndef YASM_EXPR
#define YASM_EXPR
typedef struct expr expr;
#endif

#ifndef YASM_FLOATNUM
#define YASM_FLOATNUM
typedef struct floatnum floatnum;
#endif

typedef struct effaddr effaddr;
typedef struct immval immval;
typedef STAILQ_HEAD(datavalhead, dataval) datavalhead;
typedef struct dataval dataval;
typedef STAILQ_HEAD(bytecodehead, bytecode) bytecodehead;

#ifndef YASM_BYTECODE
#define YASM_BYTECODE
typedef struct bytecode bytecode;
#endif

typedef enum {
    JR_NONE,
    JR_SHORT,
    JR_NEAR,
    JR_SHORT_FORCED,
    JR_NEAR_FORCED
} jmprel_opcode_sel;

typedef struct targetval_s {
    expr *val;

    jmprel_opcode_sel op_sel;
} targetval;

effaddr *effaddr_new_reg(unsigned long reg);
effaddr *effaddr_new_imm(immval *im_ptr, unsigned char im_len);
effaddr *effaddr_new_expr(expr *expr_ptr);

immval *immval_new_int(unsigned long int_val);
immval *immval_new_expr(expr *expr_ptr);

void SetEASegment(effaddr *ptr, unsigned char segment);
void SetEALen(effaddr *ptr, unsigned char len);
void SetEANosplit(effaddr *ptr, unsigned char nosplit);

effaddr *GetInsnEA(bytecode *bc);

void SetInsnOperSizeOverride(bytecode *bc, unsigned char opersize);
void SetInsnAddrSizeOverride(bytecode *bc, unsigned char addrsize);
void SetInsnLockRepPrefix(bytecode *bc, unsigned char prefix);
void SetInsnShiftFlag(bytecode *bc);

void SetOpcodeSel(jmprel_opcode_sel *old_sel, jmprel_opcode_sel new_sel);

void SetBCMultiple(bytecode *bc, expr *e);

/* IMPORTANT: ea_ptr and im_ptr cannot be reused or freed after calling this
 * function (it doesn't make a copy).
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

bytecode *bytecode_new_reserve(expr *numitems, unsigned long itemsize);

/* Gets the offset of the bytecode specified by bc if possible.
 * Return value is IF POSSIBLE, not the value.
 */
int bytecode_get_offset(section *sect, bytecode *bc, unsigned long *ret_val);

void bytecode_print(const bytecode *bc);

void bytecode_parser_finalize(bytecode *bc);

/* void bytecodes_initialize(bytecodehead *headp); */
#define	bytecodes_initialize(headp)	STAILQ_INIT(headp)

/* Adds bc to the list of bytecodes headp.
 * NOTE: Does not make a copy of bc; so don't pass this function
 * static or local variables, and discard the bc pointer after calling
 * this function.  If bc was actually appended (it wasn't NULL or empty),
 * then returns bc, otherwise returns NULL.
 */
bytecode *bytecodes_append(bytecodehead *headp, bytecode *bc);

void bytecodes_print(const bytecodehead *headp);

void bytecodes_parser_finalize(bytecodehead *headp);

dataval *dataval_new_expr(expr *expn);
dataval *dataval_new_float(floatnum *flt);
dataval *dataval_new_string(char *str_val);

/* void datavals_initialize(datavalhead *headp); */
#define	datavals_initialize(headp)	STAILQ_INIT(headp)

/* Adds dv to the list of datavals headp.
 * NOTE: Does not make a copy of dv; so don't pass this function
 * static or local variables, and discard the dv pointer after calling
 * this function.  If dv was actually appended (it wasn't NULL), then
 * returns dv, otherwise returns NULL.
 */
dataval *datavals_append(datavalhead *headp, dataval *dv);

void dataval_print(const datavalhead *head);

#endif
