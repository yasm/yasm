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

/* 0-7 (low 3 bits) used for register number, stored in same data area */
typedef enum {
    X86_REG8 = 0x8,
    X86_REG16 = 0x10,
    X86_REG32 = 0x20,
    X86_MMXREG = 0x40,
    X86_XMMREG = 0x80,
    X86_CRREG = 0xC0,
    X86_DRREG = 0xC8,
    X86_TRREG = 0xF0,
    X86_FPUREG = 0xF8
} x86_expritem_reg_size;

typedef enum {
    X86_LOCKREP = 1,
    X86_ADDRSIZE,
    X86_OPERSIZE
} x86_parse_insn_prefix;

typedef enum {
    X86_NEAR = 1,
    X86_SHORT,
    X86_FAR,
    X86_TO
} x86_parse_targetmod;

typedef enum {
    JR_NONE,
    JR_SHORT,
    JR_NEAR,
    JR_SHORT_FORCED,
    JR_NEAR_FORCED
} x86_jmprel_opcode_sel;

void x86_ea_set_segment(/*@null@*/ effaddr *ea, unsigned char segment);
void x86_ea_set_disponly(effaddr *ea);
effaddr *x86_ea_new_reg(unsigned char reg);
effaddr *x86_ea_new_imm(/*@keep@*/expr *imm, unsigned char im_len);
effaddr *x86_ea_new_expr(/*@keep@*/ expr *e);

/*@observer@*/ /*@null@*/ effaddr *x86_bc_insn_get_ea(/*@null@*/ bytecode *bc);

void x86_bc_insn_opersize_override(bytecode *bc, unsigned char opersize);
void x86_bc_insn_addrsize_override(bytecode *bc, unsigned char addrsize);
void x86_bc_insn_set_lockrep_prefix(bytecode *bc, unsigned char prefix);

void x86_set_jmprel_opcode_sel(x86_jmprel_opcode_sel *old_sel,
			       x86_jmprel_opcode_sel new_sel);

/* Structure with *all* inputs passed to x86_bytecode_new_insn().
 * IMPORTANT: ea_ptr and im_ptr cannot be reused or freed after calling the
 * function (it doesn't make a copy).
 */
typedef struct x86_new_insn_data {
    /*@keep@*/ /*@null@*/ effaddr *ea;
    /*@keep@*/ /*@null@*/ expr *imm;
    unsigned char opersize;
    unsigned char op_len;
    unsigned char op[3];
    unsigned char spare;	/* bits to go in 'spare' field of ModRM */
    unsigned char im_len;
    unsigned char im_sign;
    unsigned char shift_op;
    unsigned char signext_imm8_op;
} x86_new_insn_data;

bytecode *x86_bc_new_insn(x86_new_insn_data *d);

/* Structure with *all* inputs passed to x86_bytecode_new_jmprel().
 * Pass 0 for the opcode_len if that version of the opcode doesn't exist.
 */
typedef struct x86_new_jmprel_data {
    /*@keep@*/ expr *target;
    x86_jmprel_opcode_sel op_sel;
    unsigned char short_op_len;
    unsigned char short_op[3];
    unsigned char near_op_len;
    unsigned char near_op[3];
    unsigned char addrsize;
    unsigned char opersize;
} x86_new_jmprel_data;

bytecode *x86_bc_new_jmprel(x86_new_jmprel_data *d);

extern unsigned char yasm_x86_LTX_mode_bits;

void x86_bc_delete(bytecode *bc);
void x86_bc_print(FILE *f, int indent_level, const bytecode *bc);
bc_resolve_flags x86_bc_resolve(bytecode *bc, int save, const section *sect,
				calc_bc_dist_func calc_bc_dist);
int x86_bc_tobytes(bytecode *bc, unsigned char **bufp, const section *sect,
		   void *d, output_expr_func output_expr);

int x86_expr_checkea(expr **ep, unsigned char *addrsize, unsigned char bits,
		     unsigned char nosplit, unsigned char *displen,
		     unsigned char *modrm, unsigned char *v_modrm,
		     unsigned char *n_modrm, unsigned char *sib,
		     unsigned char *v_sib, unsigned char *n_sib,
		     calc_bc_dist_func calc_bc_dist);

void x86_switch_cpu(const char *cpuid);

arch_check_id_retval x86_check_identifier(unsigned long data[2],
					  const char *id);

int x86_directive(const char *name, valparamhead *valparams,
		  /*@null@*/ valparamhead *objext_valparams,
		  sectionhead *headp);

/*@null@*/ bytecode *x86_new_insn(const unsigned long data[2],
				  int num_operands,
				  /*@null@*/ insn_operandhead *operands,
				  section *cur_section,
				  /*@null@*/ bytecode *prev_bc);

void x86_handle_prefix(bytecode *bc, const unsigned long data[4]);

void x86_handle_seg_prefix(bytecode *bc, unsigned long segreg);

void x86_handle_seg_override(effaddr *ea, unsigned long segreg);

int x86_floatnum_tobytes(const floatnum *flt, unsigned char **bufp,
			 unsigned long valsize, const expr *e);
int x86_intnum_tobytes(const intnum *intn, unsigned char **bufp,
		       unsigned long valsize, const expr *e,
		       const bytecode *bc, int rel);

unsigned int x86_get_reg_size(unsigned long reg);

void x86_reg_print(FILE *f, unsigned long reg);

void x86_segreg_print(FILE *f, unsigned long segreg);

void x86_ea_data_print(FILE *f, int indent_level, const effaddr *ea);

#endif
