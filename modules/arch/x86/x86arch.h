/* $IdPath$
 * x86 Architecture header file
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
#ifndef YASM_X86ARCH_H
#define YASM_X86ARCH_H

typedef enum {
    X86_BC_INSN = BYTECODE_TYPE_BASE,
    X86_BC_JMPREL
} x86_bytecode_type;
#define X86_BYTECODE_TYPE_MAX	X86_BC_JMPREL+1

typedef enum {
    JR_NONE,
    JR_SHORT,
    JR_NEAR,
    JR_SHORT_FORCED,
    JR_NEAR_FORCED
} x86_jmprel_opcode_sel;

typedef struct x86_targetval {
    expr *val;

    x86_jmprel_opcode_sel op_sel;
} x86_targetval;

void x86_ea_set_segment(/*@null@*/ effaddr *ea, unsigned char segment);
effaddr *x86_ea_new_reg(unsigned char reg);
effaddr *x86_ea_new_imm(immval *imm, unsigned char im_len);
effaddr *x86_ea_new_expr(/*@keep@*/ expr *e);

/*@null@*/ effaddr *x86_bc_insn_get_ea(bytecode *bc);

void x86_bc_insn_opersize_override(bytecode *bc, unsigned char opersize);
void x86_bc_insn_addrsize_override(bytecode *bc, unsigned char addrsize);
void x86_bc_insn_set_lockrep_prefix(bytecode *bc, unsigned char prefix);
void x86_bc_insn_set_shift_flag(bytecode *bc);

void x86_set_jmprel_opcode_sel(x86_jmprel_opcode_sel *old_sel,
			       x86_jmprel_opcode_sel new_sel);

/* Structure with *all* inputs passed to x86_bytecode_new_insn().
 * IMPORTANT: ea_ptr and im_ptr cannot be reused or freed after calling the
 * function (it doesn't make a copy).
 */
typedef struct x86_new_insn_data {
    /*@keep@*/ /*@null@*/ effaddr *ea;
    /*@keep@*/ /*@null@*/ immval *imm;
    unsigned char opersize;
    unsigned char op_len;
    unsigned char op[3];
    unsigned char spare;	/* bits to go in 'spare' field of ModRM */
    unsigned char im_len;
    unsigned char im_sign;
} x86_new_insn_data;

bytecode *x86_bc_new_insn(x86_new_insn_data *d);

/* Structure with *all* inputs passed to x86_bytecode_new_jmprel().
 * Pass 0 for the opcode_len if that version of the opcode doesn't exist.
 */
typedef struct x86_new_jmprel_data {
    /*@keep@*/ x86_targetval *target;
    unsigned char short_op_len;
    unsigned char short_op[3];
    unsigned char near_op_len;
    unsigned char near_op[3];
    unsigned char addrsize;
} x86_new_jmprel_data;

bytecode *x86_bc_new_jmprel(x86_new_jmprel_data *d);

extern unsigned char x86_mode_bits;

#endif
