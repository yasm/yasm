/* $IdPath$
 * x86 Architecture header file
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef YASM_X86ARCH_H
#define YASM_X86ARCH_H

/* Available CPU feature flags */
#define CPU_Any	    (0UL)	/* Any old cpu will do */
#define CPU_086	    CPU_Any
#define CPU_186	    (1UL<<0)	/* i186 or better required */
#define CPU_286	    (1UL<<1)	/* i286 or better required */
#define CPU_386	    (1UL<<2)	/* i386 or better required */
#define CPU_486	    (1UL<<3)	/* i486 or better required */
#define CPU_586	    (1UL<<4)	/* i585 or better required */
#define CPU_686	    (1UL<<5)	/* i686 or better required */
#define CPU_P3	    (1UL<<6)	/* Pentium3 or better required */
#define CPU_P4	    (1UL<<7)	/* Pentium4 or better required */
#define CPU_IA64    (1UL<<8)	/* IA-64 or better required */
#define CPU_K6	    (1UL<<9)	/* AMD K6 or better required */
#define CPU_Athlon  (1UL<<10)	/* AMD Athlon or better required */
#define CPU_Hammer  (1UL<<11)	/* AMD Sledgehammer or better required */
#define CPU_FPU	    (1UL<<12)	/* FPU support required */
#define CPU_MMX	    (1UL<<13)	/* MMX support required */
#define CPU_SSE	    (1UL<<14)	/* Streaming SIMD extensions required */
#define CPU_SSE2    (1UL<<15)	/* Streaming SIMD extensions 2 required */
#define CPU_3DNow   (1UL<<16)	/* 3DNow! support required */
#define CPU_Cyrix   (1UL<<17)	/* Cyrix-specific instruction */
#define CPU_AMD	    (1UL<<18)	/* AMD-specific inst. (older than K6) */
#define CPU_SMM	    (1UL<<19)	/* System Management Mode instruction */
#define CPU_Prot    (1UL<<20)	/* Protected mode only instruction */
#define CPU_Undoc   (1UL<<21)	/* Undocumented instruction */
#define CPU_Obs	    (1UL<<22)	/* Obsolete instruction */
#define CPU_Priv    (1UL<<23)	/* Priveleged instruction */

/* Technically not CPU capabilities, they do affect what instructions are
 * available.  These are tested against BITS==64.
 */
#define CPU_64	    (1UL<<24)	/* Only available in 64-bit mode */
#define CPU_Not64   (1UL<<25)	/* Not available (invalid) in 64-bit mode */

/* What instructions/features are enabled? */
extern unsigned long yasm_x86__cpu_enabled;

typedef enum {
    X86_BC_INSN = YASM_BYTECODE_TYPE_BASE,
    X86_BC_JMP
} x86_bytecode_type;
#define X86_BYTECODE_TYPE_MAX	X86_BC_JMP+1

/* 0-15 (low 4 bits) used for register number, stored in same data area.
 * Note 8-15 are only valid for some registers, and only in 64-bit mode.
 */
typedef enum {
    X86_REG8 = 0x1<<4,
    X86_REG8X = 0x2<<4,	    /* 64-bit mode only, REX prefix version of REG8 */
    X86_REG16 = 0x3<<4,
    X86_REG32 = 0x4<<4,
    X86_REG64 = 0x5<<4,	    /* 64-bit mode only */
    X86_FPUREG = 0x6<<4,
    X86_MMXREG = 0x7<<4,
    X86_XMMREG = 0x8<<4,
    X86_CRREG = 0x9<<4,
    X86_DRREG = 0xA<<4,
    X86_TRREG = 0xB<<4,
    X86_RIP = 0xC<<4	    /* 64-bit mode only, always RIP (regnum ignored) */
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
    JMP_NONE,
    JMP_SHORT,
    JMP_NEAR,
    JMP_SHORT_FORCED,
    JMP_NEAR_FORCED,
    JMP_FAR		    /* not really relative, but fits here */
} x86_jmp_opcode_sel;

typedef enum {
    X86_REX_W = 3,
    X86_REX_R = 2,
    X86_REX_X = 1,
    X86_REX_B = 0
} x86_rex_bit_pos;

/* Sets REX (4th bit) and 3 LS bits from register size/number.  Returns 1 if
 * impossible to fit reg into REX, otherwise returns 0.  Input parameter rexbit
 * indicates bit of REX to use if REX is needed.  Will not modify REX if not
 * in 64-bit mode or if it wasn't needed to express reg.
 */
int yasm_x86__set_rex_from_reg(unsigned char *rex, unsigned char *low3,
			       unsigned long reg, unsigned int bits,
			       x86_rex_bit_pos rexbit);

void yasm_x86__ea_set_segment(/*@null@*/ yasm_effaddr *ea,
			      unsigned int segment, unsigned long lindex);
void yasm_x86__ea_set_disponly(yasm_effaddr *ea);
yasm_effaddr *yasm_x86__ea_new_reg(unsigned long reg, unsigned char *rex,
				   unsigned int bits);
yasm_effaddr *yasm_x86__ea_new_imm(/*@keep@*/ yasm_expr *imm,
				   unsigned int im_len);
yasm_effaddr *yasm_x86__ea_new_expr(/*@keep@*/ yasm_expr *e);

/*@observer@*/ /*@null@*/ yasm_effaddr *yasm_x86__bc_insn_get_ea
    (/*@null@*/ yasm_bytecode *bc);

void yasm_x86__bc_insn_opersize_override(yasm_bytecode *bc,
					 unsigned int opersize);
void yasm_x86__bc_insn_addrsize_override(yasm_bytecode *bc,
					 unsigned int addrsize);
void yasm_x86__bc_insn_set_lockrep_prefix(yasm_bytecode *bc,
					  unsigned int prefix,
					  unsigned long lindex);

/* Structure with *all* inputs passed to x86_bytecode_new_insn().
 * IMPORTANT: ea_ptr and im_ptr cannot be reused or freed after calling the
 * function (it doesn't make a copy).
 */
typedef struct x86_new_insn_data {
    unsigned long lindex;
    /*@keep@*/ /*@null@*/ yasm_effaddr *ea;
    /*@keep@*/ /*@null@*/ yasm_expr *imm;
    unsigned char opersize;
    unsigned char op_len;
    unsigned char op[3];
    unsigned char spare;	/* bits to go in 'spare' field of ModRM */
    unsigned char rex;
    unsigned char im_len;
    unsigned char im_sign;
    unsigned char shift_op;
    unsigned char signext_imm8_op;
} x86_new_insn_data;

yasm_bytecode *yasm_x86__bc_new_insn(x86_new_insn_data *d);

/* Structure with *all* inputs passed to x86_bytecode_new_jmp().
 * Pass 0 for the opcode_len if that version of the opcode doesn't exist.
 */
typedef struct x86_new_jmp_data {
    unsigned long lindex;
    /*@keep@*/ yasm_expr *target;
    /*@dependent@*/ yasm_symrec *origin;
    x86_jmp_opcode_sel op_sel;
    unsigned char short_op_len;
    unsigned char short_op[3];
    unsigned char near_op_len;
    unsigned char near_op[3];
    unsigned char far_op_len;
    unsigned char far_op[3];
    unsigned char addrsize;
    unsigned char opersize;
} x86_new_jmp_data;

yasm_bytecode *yasm_x86__bc_new_jmp(x86_new_jmp_data *d);

extern unsigned char yasm_x86_LTX_mode_bits;

void yasm_x86__bc_delete(yasm_bytecode *bc);
void yasm_x86__bc_print(FILE *f, int indent_level, const yasm_bytecode *bc);
yasm_bc_resolve_flags yasm_x86__bc_resolve
    (yasm_bytecode *bc, int save, const yasm_section *sect,
     yasm_calc_bc_dist_func calc_bc_dist);
int yasm_x86__bc_tobytes(yasm_bytecode *bc, unsigned char **bufp,
			 const yasm_section *sect, void *d,
			 yasm_output_expr_func output_expr);

/* Check an effective address.  Returns 0 if EA was successfully determined,
 * 1 if invalid EA, or 2 if indeterminate EA.
 */
int yasm_x86__expr_checkea
    (yasm_expr **ep, unsigned char *addrsize, unsigned int bits,
     unsigned int nosplit, unsigned char *displen, unsigned char *modrm,
     unsigned char *v_modrm, unsigned char *n_modrm, unsigned char *sib,
     unsigned char *v_sib, unsigned char *n_sib, unsigned char *rex,
     yasm_calc_bc_dist_func calc_bc_dist);

void yasm_x86__parse_cpu(const char *cpuid, unsigned long lindex);

yasm_arch_check_id_retval yasm_x86__parse_check_id
    (unsigned long data[2], const char *id, unsigned long lindex);

int yasm_x86__parse_directive(const char *name, yasm_valparamhead *valparams,
			      /*@null@*/ yasm_valparamhead *objext_valparams,
			      yasm_sectionhead *headp, unsigned long lindex);

/*@null@*/ yasm_bytecode *yasm_x86__parse_insn
    (const unsigned long data[2], int num_operands,
     /*@null@*/ yasm_insn_operandhead *operands, yasm_section *cur_section,
     /*@null@*/ yasm_bytecode *prev_bc, unsigned long lindex);

void yasm_x86__parse_prefix(yasm_bytecode *bc, const unsigned long data[4],
			    unsigned long lindex);

void yasm_x86__parse_seg_prefix(yasm_bytecode *bc, unsigned long segreg,
				unsigned long lindex);

void yasm_x86__parse_seg_override(yasm_effaddr *ea, unsigned long segreg,
				  unsigned long lindex);

int yasm_x86__floatnum_tobytes(const yasm_floatnum *flt, unsigned char *buf,
			       size_t destsize, size_t valsize, size_t shift,
			       int warn, unsigned long lindex);
int yasm_x86__intnum_tobytes(const yasm_intnum *intn, unsigned char *buf,
			     size_t destsize, size_t valsize, int shift,
			     const yasm_bytecode *bc, int rel, int warn,
			     unsigned long lindex);

unsigned int yasm_x86__get_reg_size(unsigned long reg);

void yasm_x86__reg_print(FILE *f, unsigned long reg);

void yasm_x86__segreg_print(FILE *f, unsigned long segreg);

void yasm_x86__ea_data_print(FILE *f, int indent_level,
			     const yasm_effaddr *ea);

#endif
