/* $IdPath$
 * x86 internals header file
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
#ifndef YASM_X86_INT_H
#define YASM_X86_INT_H

typedef struct x86_effaddr_data {
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
} x86_effaddr_data;

typedef struct x86_insn {
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
     * Call x86_SetInsnShiftFlag() to set this flag to 1.
     */
    unsigned char shift_op;

    unsigned char mode_bits;
} x86_insn;

typedef struct x86_jmprel {
    expr *target;		/* target location */

    struct {
	unsigned char opcode[3];
	unsigned char opcode_len;   /* 0 = no opc for this version */
    } shortop, nearop;

    /* which opcode are we using? */
    /* The *FORCED forms are specified in the source as such */
    x86_jmprel_opcode_sel op_sel;

    unsigned char addrsize;	/* 0 or =mode_bits => no override */
    unsigned char opersize;	/* 0 indicates no override */
    unsigned char lockrep_pre;	/* 0 indicates no prefix */

    unsigned char mode_bits;
} x86_jmprel;

void x86_bc_delete(bytecode *bc);
void x86_bc_print(const bytecode *bc);
void x86_bc_parser_finalize(bytecode *bc);

#endif
