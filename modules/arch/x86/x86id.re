/*
 * x86 identifier recognition and instruction handling
 *
 *  Copyright (C) 2002  Peter Johnson
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
#include <util.h>
RCSID("$IdPath$");

#define YASM_LIB_INTERNAL
#define YASM_BC_INTERNAL
#define YASM_EXPR_INTERNAL
#include <libyasm.h>

#include "modules/arch/x86/x86arch.h"


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

/* What instructions/features are enabled?  Defaults to all. */
static unsigned long cpu_enabled = ~CPU_Any;

/* Opcode modifiers.  The opcode bytes are in "reverse" order because the
 * parameters are read from the arch-specific data in LSB->MSB order.
 * (only for asthetic reasons in the lexer code below, no practical reason).
 */
#define MOD_Op2Add  (1UL<<0)	/* Parameter adds to opcode byte 2 */
#define MOD_Gap0    (1UL<<1)	/* Eats a parameter */
#define MOD_Op1Add  (1UL<<2)	/* Parameter adds to opcode byte 1 */
#define MOD_Gap1    (1UL<<3)	/* Eats a parameter */
#define MOD_Op0Add  (1UL<<4)	/* Parameter adds to opcode byte 0 */
#define MOD_SpAdd   (1UL<<5)	/* Parameter adds to "spare" value */
#define MOD_OpSizeR (1UL<<6)	/* Parameter replaces opersize */
#define MOD_Imm8    (1UL<<7)	/* Parameter is included as immediate byte */
#define MOD_AdSizeR (1UL<<8)	/* Parameter replaces addrsize (jmprel only) */

/* Modifiers that aren't actually used as modifiers.  Rather, if set, bits
 * 20-27 in the modifier are used as an index into an array.
 * Obviously, only one of these may be set at a time.
 */
#define MOD_ExtNone (0UL<<28)	/* No extended modifier */
#define MOD_ExtErr  (1UL<<28)	/* Extended error: index into error strings */
#define MOD_ExtWarn (2UL<<28)	/* Extended warning: index into warning strs */
#define MOD_Ext_MASK (0xFUL<<28)
#define MOD_ExtIndex_SHIFT	20
#define MOD_ExtIndex(indx)	(((unsigned long)(indx))<<MOD_ExtIndex_SHIFT)
#define MOD_ExtIndex_MASK	(0xFFUL<<MOD_ExtIndex_SHIFT)

/* Operand types.  These are more detailed than the "general" types for all
 * architectures, as they include the size, for instance.
 * Bit Breakdown (from LSB to MSB):
 *  - 5 bits = general type (must be exact match, except for =3):
 *             0 = immediate
 *             1 = any general purpose or FPU register
 *             2 = memory
 *             3 = any general purpose or FPU register OR memory
 *             4 = any MMX or XMM register
 *             5 = any MMX or XMM register OR memory
 *             6 = any segment register
 *             7 = any CR register
 *             8 = any DR register
 *             9 = any TR register
 *             A = ST0
 *             B = AL/AX/EAX/RAX (depending on size)
 *             C = CL/CX/ECX/RCX (depending on size)
 *             D = DL/DX/EDX/RDX (depending on size)
 *             E = CS
 *             F = DS
 *             10 = ES
 *             11 = FS
 *             12 = GS
 *             13 = SS
 *             14 = CR4
 *             15 = memory offset (an EA, but with no registers allowed)
 *                  [special case for MOV opcode]
 *  - 3 bits = size (user-specified, or from register size):
 *             0 = any size acceptable/no size spec acceptable (dep. on strict)
 *             1/2/3/4 = 8/16/32/64 bits (from user or reg size)
 *             5/6 = 80/128 bits (from user)
 *  - 1 bit = size implicit or explicit ("strictness" of size matching on
 *            non-registers -- registers are always strictly matched):
 *            0 = user size must exactly match size above.
 *            1 = user size either unspecified or exactly match size above.
 *  - 3 bits = target modification.
 *            0 = no target mod acceptable
 *            1 = NEAR
 *            2 = SHORT
 *            3 = FAR
 *            4 = TO
 *
 * MSBs than the above are actions: what to do with the operand if the
 * instruction matches.  Essentially describes what part of the output bytecode
 * gets the operand.  This may require conversion (e.g. a register going into
 * an ea field).  Naturally, only one of each of these may be contained in the
 * operands of a single insn_info structure.
 *  - 4 bits = action:
 *             0 = does nothing (operand data is discarded)
 *             1 = operand data goes into ea field
 *             2 = operand data goes into imm field
 *             3 = operand data goes into sign-extended imm field
 *             4 = operand data goes into "spare" field
 *             5 = operand data is added to opcode byte 0
 *             6 = operand data is added to opcode byte 1
 *             7 = operand data goes into BOTH ea and spare
 *                 [special case for imul opcode]
 *             8 = relative jump (outputs a jmprel instead of normal insn)
 *             9 = operand size goes into address size (jmprel only)
 * The below describes postponed actions: actions which can't be completed at
 * parse-time due to things like EQU and complex expressions.  For these, some
 * additional data (stored in the second byte of the opcode with a one-byte
 * opcode) is passed to later stages of the assembler with flags set to
 * indicate postponed actions.
 *  - 2 bits = postponed action:
 *             0 = none
 *             1 = shift operation with a ,1 short form (instead of imm8).
 *             2 = large imm16/32 that can become a sign-extended imm8.
 *             3 = can be far jump
 */
#define OPT_Imm		0x0
#define OPT_Reg		0x1
#define OPT_Mem		0x2
#define OPT_RM		0x3
#define OPT_SIMDReg	0x4
#define OPT_SIMDRM	0x5
#define OPT_SegReg	0x6
#define OPT_CRReg	0x7
#define OPT_DRReg	0x8
#define OPT_TRReg	0x9
#define OPT_ST0		0xA
#define OPT_Areg	0xB
#define OPT_Creg	0xC
#define OPT_Dreg	0xD
#define OPT_CS		0xE
#define OPT_DS		0xF
#define OPT_ES		0x10
#define OPT_FS		0x11
#define OPT_GS		0x12
#define OPT_SS		0x13
#define OPT_CR4		0x14
#define OPT_MemOffs	0x15
#define OPT_MASK	0x1F

#define OPS_Any		(0UL<<5)
#define OPS_8		(1UL<<5)
#define OPS_16		(2UL<<5)
#define OPS_32		(3UL<<5)
#define OPS_64		(4UL<<5)
#define OPS_80		(5UL<<5)
#define OPS_128		(6UL<<5)
#define OPS_MASK	(7UL<<5)
#define OPS_SHIFT	5

#define OPS_Relaxed	(1UL<<8)
#define OPS_RMASK	(1UL<<8)

#define OPTM_None	(0UL<<9)
#define OPTM_Near	(1UL<<9)
#define OPTM_Short	(2UL<<9)
#define OPTM_Far	(3UL<<9)
#define OPTM_To		(4UL<<9)
#define OPTM_MASK	(7UL<<9)

#define OPA_None	(0UL<<12)
#define OPA_EA		(1UL<<12)
#define OPA_Imm		(2UL<<12)
#define OPA_SImm	(3UL<<12)
#define OPA_Spare	(4UL<<12)
#define OPA_Op0Add	(5UL<<12)
#define OPA_Op1Add	(6UL<<12)
#define OPA_SpareEA	(7UL<<12)
#define OPA_JmpRel	(8UL<<12)
#define OPA_AdSizeR	(9UL<<12)
#define OPA_MASK	(0xFUL<<12)

#define OPAP_None	(0UL<<16)
#define OPAP_ShiftOp	(1UL<<16)
#define OPAP_SImm8Avail	(2UL<<16)
#define OPAP_JmpFar	(3UL<<16)
#define OPAP_MASK	(3UL<<16)

typedef struct x86_insn_info {
    /* The CPU feature flags needed to execute this instruction.  This is OR'ed
     * with arch-specific data[2].  This combined value is compared with
     * cpu_enabled to see if all bits set here are set in cpu_enabled--if so,
     * the instruction is available on this CPU.
     */
    unsigned long cpu;

    /* Opcode modifiers for variations of instruction.  As each modifier reads
     * its parameter in LSB->MSB order from the arch-specific data[1] from the
     * lexer data, and the LSB of the arch-specific data[1] is reserved for the
     * count of insn_info structures in the instruction grouping, there can
     * only be a maximum of 3 modifiers.
     */
    unsigned long modifiers;

    /* Operand Size */
    unsigned char opersize;

    /* The length of the basic opcode */
    unsigned char opcode_len;

    /* The basic 1-3 byte opcode */
    unsigned char opcode[3];

    /* The 3-bit "spare" value (extended opcode) for the R/M byte field */
    unsigned char spare;

    /* The number of operands this form of the instruction takes */
    unsigned char num_operands;

    /* The types of each operand, see above */
    unsigned long operands[3];
} x86_insn_info;

/* Define lexer arch-specific data with 0-3 modifiers. */
#define DEF_INSN_DATA(group, mod, cpu)	do { \
    data[0] = (unsigned long)group##_insn; \
    data[1] = ((mod)<<8) | \
    	      ((unsigned char)(sizeof(group##_insn)/sizeof(x86_insn_info))); \
    data[2] = cpu; \
    } while (0)

#define RET_INSN(group, mod, cpu)	do { \
    DEF_INSN_DATA(group, mod, cpu); \
    return YASM_ARCH_CHECK_ID_INSN; \
    } while (0)

/*
 * General instruction groupings
 */

/* Placeholder for instructions invalid in 64-bit mode */
static const x86_insn_info not64_insn[] = {
    { CPU_Not64, 0, 0, 0, {0, 0, 0}, 0, 0, {0, 0, 0} }
};

/* One byte opcode instructions with no operands */
static const x86_insn_info onebyte_insn[] = {
    { CPU_Any, MOD_Op0Add|MOD_OpSizeR, 0, 1, {0, 0, 0}, 0, 0, {0, 0, 0} }
};

/* Two byte opcode instructions with no operands */
static const x86_insn_info twobyte_insn[] = {
    { CPU_Any, MOD_Op1Add|MOD_Op0Add, 0, 2, {0, 0, 0}, 0, 0, {0, 0, 0} }
};

/* Three byte opcode instructions with no operands */
static const x86_insn_info threebyte_insn[] = {
    { CPU_Any, MOD_Op2Add|MOD_Op1Add|MOD_Op0Add, 0, 3, {0, 0, 0}, 0, 0,
      {0, 0, 0} }
};

/* One byte opcode instructions with general memory operand */
static const x86_insn_info onebytemem_insn[] = {
    { CPU_Any, MOD_Op0Add|MOD_SpAdd, 0, 1, {0, 0, 0}, 0, 1,
      {OPT_Mem|OPS_Any|OPA_EA, 0, 0} }
};

/* Two byte opcode instructions with general memory operand */
static const x86_insn_info twobytemem_insn[] = {
    { CPU_Any, MOD_Op1Add|MOD_Op0Add|MOD_SpAdd, 0, 1, {0, 0, 0}, 0, 1,
      {OPT_Mem|OPS_Any|OPA_EA, 0, 0} }
};

/* Move instructions */
static const x86_insn_info mov_insn[] = {
    { CPU_Any, 0, 0, 1, {0xA0, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None, OPT_MemOffs|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, 0, 16, 1, {0xA1, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_MemOffs|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, 0, 32, 1, {0xA1, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_MemOffs|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0xA1, 0, 0}, 0, 2,
      {OPT_Areg|OPS_64|OPA_None, OPT_MemOffs|OPS_64|OPS_Relaxed|OPA_EA, 0} },

    { CPU_Any, 0, 0, 1, {0xA2, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_8|OPS_Relaxed|OPA_EA, OPT_Areg|OPS_8|OPA_None, 0} },
    { CPU_Any, 0, 16, 1, {0xA3, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_16|OPS_Relaxed|OPA_EA, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_386, 0, 32, 1, {0xA3, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_32|OPS_Relaxed|OPA_EA, OPT_Areg|OPS_32|OPA_None, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0xA3, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_64|OPS_Relaxed|OPA_EA, OPT_Areg|OPS_64|OPA_None, 0} },

    { CPU_Any, 0, 0, 1, {0x88, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_Any, 0, 16, 1, {0x89, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, 0, 32, 1, {0x89, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x89, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },

    { CPU_Any, 0, 0, 1, {0x8A, 0, 0}, 0, 2,
      {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, 0, 16, 1, {0x8B, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, 0, 32, 1, {0x8B, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x8B, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },

    { CPU_Any, 0, 0, 1, {0x8C, 0, 0}, 0, 2,
      {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA,
       OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, 0} },
    { CPU_Any, 0, 16, 1, {0x8C, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_EA, OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, 0} },
    { CPU_386, 0, 32, 1, {0x8C, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_EA, OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x8C, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_EA, OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, 0} },

    { CPU_Any, 0, 0, 1, {0x8E, 0, 0}, 0, 2,
      {OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare,
       OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, 0, 0, 1, {0x8E, 0, 0}, 0, 2,
      {OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, 0, 0, 1, {0x8E, 0, 0}, 0, 2,
      {OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, OPT_Reg|OPS_64|OPA_EA, 0} },

    { CPU_Any, 0, 0, 1, {0xB0, 0, 0}, 0, 2,
      {OPT_Reg|OPS_8|OPA_Op0Add, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, 0, 16, 1, {0xB8, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Op0Add, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, 0, 32, 1, {0xB8, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Op0Add, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0xB8, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Op0Add, OPT_Imm|OPS_64|OPS_Relaxed|OPA_Imm, 0} },
    /* Need two sets here, one for strictness on left side, one for right. */
    { CPU_Any, 0, 0, 1, {0xC6, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPA_Imm, 0} },
    { CPU_Any, 0, 16, 1, {0xC7, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_16|OPA_Imm, 0} },
    { CPU_386, 0, 32, 1, {0xC7, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0xC7, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_Any, 0, 0, 1, {0xC6, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, 0, 16, 1, {0xC7, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, 0, 32, 1, {0xC7, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0xC7, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },

    { CPU_586|CPU_Priv|CPU_Not64, 0, 0, 2, {0x0F, 0x22, 0}, 0, 2,
      {OPT_CR4|OPS_32|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, 0} },
    { CPU_386|CPU_Priv|CPU_Not64, 0, 0, 2, {0x0F, 0x22, 0}, 0, 2,
      {OPT_CRReg|OPS_32|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, 0} },
    { CPU_Hammer|CPU_Priv|CPU_64, 0, 0, 2, {0x0F, 0x22, 0}, 0, 2,
      {OPT_CRReg|OPS_32|OPA_Spare, OPT_Reg|OPS_64|OPA_EA, 0} },
    { CPU_586|CPU_Priv|CPU_Not64, 0, 0, 2, {0x0F, 0x20, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_EA, OPT_CR4|OPS_32|OPA_Spare, 0} },
    { CPU_386|CPU_Priv|CPU_Not64, 0, 0, 2, {0x0F, 0x20, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_EA, OPT_CRReg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_Priv|CPU_64, 0, 0, 2, {0x0F, 0x20, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_EA, OPT_CRReg|OPS_32|OPA_Spare, 0} },

    { CPU_386|CPU_Priv|CPU_Not64, 0, 0, 2, {0x0F, 0x23, 0}, 0, 2,
      {OPT_DRReg|OPS_32|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, 0} },
    { CPU_Hammer|CPU_Priv|CPU_64, 0, 0, 2, {0x0F, 0x23, 0}, 0, 2,
      {OPT_DRReg|OPS_32|OPA_Spare, OPT_Reg|OPS_64|OPA_EA, 0} },
    { CPU_386|CPU_Priv|CPU_Not64, 0, 0, 2, {0x0F, 0x21, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_EA, OPT_DRReg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_Priv|CPU_64, 0, 0, 2, {0x0F, 0x21, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_EA, OPT_DRReg|OPS_32|OPA_Spare, 0} }
};

/* Move with sign/zero extend */
static const x86_insn_info movszx_insn[] = {
    { CPU_386, MOD_Op1Add, 16, 2, {0x0F, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_Op1Add, 32, 2, {0x0F, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_8|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add, 64, 2, {0x0F, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_8|OPA_EA, 0} },
    { CPU_386, MOD_Op1Add, 32, 2, {0x0F, 1, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_16|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add, 64, 2, {0x0F, 1, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_16|OPA_EA, 0} }
};

/* Move with sign-extend doubleword (64-bit mode only) */
static const x86_insn_info movsxd_insn[] = {
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x63, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_32|OPA_EA, 0} }
};

/* Push instructions */
static const x86_insn_info push_insn[] = {
    { CPU_Any, 0, 16, 1, {0x50, 0, 0}, 0, 1,
      {OPT_Reg|OPS_16|OPA_Op0Add, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 1, {0x50, 0, 0}, 0, 1,
      {OPT_Reg|OPS_32|OPA_Op0Add, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 0, 1, {0x50, 0, 0}, 0, 1,
      {OPT_Reg|OPS_64|OPA_Op0Add, 0, 0} },
    { CPU_Any, 0, 16, 1, {0xFF, 0, 0}, 6, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 1, {0xFF, 0, 0}, 6, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 0, 1, {0xFF, 0, 0}, 6, 1,
      {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 1, {0x6A, 0, 0}, 0, 1, {OPT_Imm|OPS_8|OPA_Imm, 0, 0} },
    { CPU_Any, 0, 16, 1, {0x68, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPA_Imm, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 1, {0x68, 0, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPA_Imm, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x68, 0, 0}, 0, 1,
      {OPT_Imm|OPS_64|OPA_Imm, 0, 0} },
    { CPU_Not64, 0, 0, 1, {0x0E, 0, 0}, 0, 1, {OPT_CS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, 0, 16, 1, {0x0E, 0, 0}, 0, 1, {OPT_CS|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, 0, 32, 1, {0x0E, 0, 0}, 0, 1, {OPT_CS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 1, {0x16, 0, 0}, 0, 1, {OPT_SS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, 0, 16, 1, {0x16, 0, 0}, 0, 1, {OPT_SS|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, 0, 32, 1, {0x16, 0, 0}, 0, 1, {OPT_SS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 1, {0x1E, 0, 0}, 0, 1, {OPT_DS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, 0, 16, 1, {0x1E, 0, 0}, 0, 1, {OPT_DS|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, 0, 32, 1, {0x1E, 0, 0}, 0, 1, {OPT_DS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 1, {0x06, 0, 0}, 0, 1, {OPT_ES|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, 0, 16, 1, {0x06, 0, 0}, 0, 1, {OPT_ES|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, 0, 32, 1, {0x06, 0, 0}, 0, 1, {OPT_ES|OPS_32|OPA_None, 0, 0} },
    { CPU_386, 0, 0, 2, {0x0F, 0xA0, 0}, 0, 1,
      {OPT_FS|OPS_Any|OPA_None, 0, 0} },
    { CPU_386, 0, 16, 2, {0x0F, 0xA0, 0}, 0, 1,
      {OPT_FS|OPS_16|OPA_None, 0, 0} },
    { CPU_386, 0, 32, 2, {0x0F, 0xA0, 0}, 0, 1,
      {OPT_FS|OPS_32|OPA_None, 0, 0} },
    { CPU_386, 0, 0, 2, {0x0F, 0xA8, 0}, 0, 1,
      {OPT_GS|OPS_Any|OPA_None, 0, 0} },
    { CPU_386, 0, 16, 2, {0x0F, 0xA8, 0}, 0, 1,
      {OPT_GS|OPS_16|OPA_None, 0, 0} },
    { CPU_386, 0, 32, 2, {0x0F, 0xA8, 0}, 0, 1,
      {OPT_GS|OPS_32|OPA_None, 0, 0} }
};

/* Pop instructions */
static const x86_insn_info pop_insn[] = {
    { CPU_Any, 0, 16, 1, {0x58, 0, 0}, 0, 1,
      {OPT_Reg|OPS_16|OPA_Op0Add, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 1, {0x58, 0, 0}, 0, 1,
      {OPT_Reg|OPS_32|OPA_Op0Add, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 0, 1, {0x58, 0, 0}, 0, 1,
      {OPT_Reg|OPS_64|OPA_Op0Add, 0, 0} },
    { CPU_Any, 0, 16, 1, {0x8F, 0, 0}, 0, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 1, {0x8F, 0, 0}, 0, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 0, 1, {0x8F, 0, 0}, 0, 1,
      {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    /* POP CS is debateably valid on the 8086, if obsolete and undocumented.
     * We don't include it because it's VERY unlikely it will ever be used
     * anywhere.  If someone really wants it they can db 0x0F it.
     */
    /*{ CPU_Any|CPU_Undoc|CPU_Obs, 0, 0, 1, {0x0F, 0, 0}, 0, 1,
        {OPT_CS|OPS_Any|OPA_None, 0, 0} },*/
    { CPU_Not64, 0, 0, 1, {0x17, 0, 0}, 0, 1, {OPT_SS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, 0, 16, 1, {0x17, 0, 0}, 0, 1, {OPT_SS|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, 0, 32, 1, {0x17, 0, 0}, 0, 1, {OPT_SS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 1, {0x1F, 0, 0}, 0, 1, {OPT_DS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, 0, 16, 1, {0x1F, 0, 0}, 0, 1, {OPT_DS|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, 0, 32, 1, {0x1F, 0, 0}, 0, 1, {OPT_DS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 1, {0x07, 0, 0}, 0, 1, {OPT_ES|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, 0, 16, 1, {0x07, 0, 0}, 0, 1, {OPT_ES|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, 0, 32, 1, {0x07, 0, 0}, 0, 1, {OPT_ES|OPS_32|OPA_None, 0, 0} },
    { CPU_386, 0, 0, 2, {0x0F, 0xA1, 0}, 0, 1,
      {OPT_FS|OPS_Any|OPA_None, 0, 0} },
    { CPU_386, 0, 16, 2, {0x0F, 0xA1, 0}, 0, 1,
      {OPT_FS|OPS_16|OPA_None, 0, 0} },
    { CPU_386, 0, 32, 2, {0x0F, 0xA1, 0}, 0, 1,
      {OPT_FS|OPS_32|OPA_None, 0, 0} },
    { CPU_386, 0, 0, 2, {0x0F, 0xA9, 0}, 0, 1,
      {OPT_GS|OPS_Any|OPA_None, 0, 0} },
    { CPU_386, 0, 16, 2, {0x0F, 0xA9, 0}, 0, 1,
      {OPT_GS|OPS_16|OPA_None, 0, 0} },
    { CPU_386, 0, 32, 2, {0x0F, 0xA9, 0}, 0, 1,
      {OPT_GS|OPS_32|OPA_None, 0, 0} }
};

/* Exchange instructions */
static const x86_insn_info xchg_insn[] = {
    { CPU_Any, 0, 0, 1, {0x86, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_Any, 0, 0, 1, {0x86, 0, 0}, 0, 2,
      {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, 0, 16, 1, {0x90, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_Reg|OPS_16|OPA_Op0Add, 0} },
    { CPU_Any, 0, 16, 1, {0x90, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Op0Add, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_Any, 0, 16, 1, {0x87, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_Any, 0, 16, 1, {0x87, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, 0, 32, 1, {0x90, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_Reg|OPS_32|OPA_Op0Add, 0} },
    { CPU_386, 0, 32, 1, {0x90, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Op0Add, OPT_Areg|OPS_32|OPA_None, 0} },
    { CPU_386, 0, 32, 1, {0x87, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_386, 0, 32, 1, {0x87, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x90, 0, 0}, 0, 2,
      {OPT_Areg|OPS_64|OPA_None, OPT_Reg|OPS_64|OPA_Op0Add, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x90, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Op0Add, OPT_Areg|OPS_64|OPA_None, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x87, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x87, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* In/out from ports */
static const x86_insn_info in_insn[] = {
    { CPU_Any, 0, 0, 1, {0xE4, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, 0, 16, 1, {0xE5, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, 0, 32, 1, {0xE5, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, 0, 0, 1, {0xEC, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None, OPT_Dreg|OPS_16|OPA_None, 0} },
    { CPU_Any, 0, 16, 1, {0xED, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_Dreg|OPS_16|OPA_None, 0} },
    { CPU_386, 0, 32, 1, {0xED, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_Dreg|OPS_16|OPA_None, 0} }
};
static const x86_insn_info out_insn[] = {
    { CPU_Any, 0, 0, 1, {0xE6, 0, 0}, 0, 2,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, OPT_Areg|OPS_8|OPA_None, 0} },
    { CPU_Any, 0, 16, 1, {0xE7, 0, 0}, 0, 2,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_386, 0, 32, 1, {0xE7, 0, 0}, 0, 2,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, OPT_Areg|OPS_32|OPA_None, 0} },
    { CPU_Any, 0, 0, 1, {0xEE, 0, 0}, 0, 2,
      {OPT_Dreg|OPS_16|OPA_None, OPT_Areg|OPS_8|OPA_None, 0} },
    { CPU_Any, 0, 16, 1, {0xEF, 0, 0}, 0, 2,
      {OPT_Dreg|OPS_16|OPA_None, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_386, 0, 32, 1, {0xEF, 0, 0}, 0, 2,
      {OPT_Dreg|OPS_16|OPA_None, OPT_Areg|OPS_32|OPA_None, 0} }
};

/* Load effective address */
static const x86_insn_info lea_insn[] = {
    { CPU_Any, 0, 16, 1, {0x8D, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, 0, 32, 1, {0x8D, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x8D, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* Load segment registers from memory */
static const x86_insn_info ldes_insn[] = {
    { CPU_Not64, MOD_Op0Add, 16, 1, {0, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} },
    { CPU_386|CPU_Not64, MOD_Op0Add, 32, 1, {0, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} }
};
static const x86_insn_info lfgss_insn[] = {
    { CPU_386, MOD_Op1Add, 16, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} },
    { CPU_386, MOD_Op1Add, 32, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} }
};

/* Arithmetic - general */
static const x86_insn_info arith_insn[] = {
    { CPU_Any, MOD_Op0Add, 0, 1, {0x04, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_Op0Add, 16, 1, {0x05, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_Op0Add, 32, 1, {0x05, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_Op0Add, 64, 1, {0x05, 0, 0}, 0, 2,
      {OPT_Areg|OPS_64|OPA_None, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },

    { CPU_Any, MOD_Gap0|MOD_SpAdd, 0, 1, {0x80, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd, 0, 1, {0x80, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPA_Imm, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd, 16, 1, {0x83, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd, 16, 1, {0x81, 0x83, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA,
       OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd, 16, 1, {0x81, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_16|OPA_Imm, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd, 32, 1, {0x83, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd, 32, 1, {0x81, 0x83, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd, 32, 1, {0x81, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_Gap0|MOD_SpAdd, 64, 1, {0x83, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_Hammer|CPU_64, MOD_Gap0|MOD_SpAdd, 64, 1, {0x81, 0x83, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0} },
    { CPU_Hammer|CPU_64, MOD_Gap0|MOD_SpAdd, 64, 1, {0x81, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },

    { CPU_Any, MOD_Op0Add, 0, 1, {0x00, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_Any, MOD_Op0Add, 16, 1, {0x01, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_Op0Add, 32, 1, {0x01, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, MOD_Op0Add, 64, 1, {0x01, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },
    { CPU_Any, MOD_Op0Add, 0, 1, {0x02, 0, 0}, 0, 2,
      {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, MOD_Op0Add, 16, 1, {0x03, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_Op0Add, 32, 1, {0x03, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_Op0Add, 64, 1, {0x03, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* Arithmetic - inc/dec */
static const x86_insn_info incdec_insn[] = {
    { CPU_Any, MOD_Gap0|MOD_SpAdd, 0, 1, {0xFE, 0, 0}, 0, 1,
      {OPT_RM|OPS_8|OPA_EA, 0, 0} },
    { CPU_Not64, MOD_Op0Add, 16, 1, {0, 0, 0}, 0, 1,
      {OPT_Reg|OPS_16|OPA_Op0Add, 0, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd, 16, 1, {0xFF, 0, 0}, 0, 1,
      {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, MOD_Op0Add, 32, 1, {0, 0, 0}, 0, 1,
      {OPT_Reg|OPS_32|OPA_Op0Add, 0, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd, 32, 1, {0xFF, 0, 0}, 0, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_Gap0|MOD_SpAdd, 64, 1, {0xFF, 0, 0}, 0, 1,
      {OPT_RM|OPS_64|OPA_EA, 0, 0} },
};

/* Arithmetic - "F6" opcodes (div/idiv/mul/neg/not) */
static const x86_insn_info f6_insn[] = {
    { CPU_Any, MOD_SpAdd, 0, 1, {0xF6, 0, 0}, 0, 1,
      {OPT_RM|OPS_8|OPA_EA, 0, 0} },
    { CPU_Any, MOD_SpAdd, 16, 1, {0xF7, 0, 0}, 0, 1,
      {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386, MOD_SpAdd, 32, 1, {0xF7, 0, 0}, 0, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd, 64, 1, {0xF7, 0, 0}, 0, 1,
      {OPT_RM|OPS_64|OPA_EA, 0, 0} },
};

/* Arithmetic - test instruction */
static const x86_insn_info test_insn[] = {
    { CPU_Any, 0, 0, 1, {0xA8, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, 0, 16, 1, {0xA9, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, 0, 32, 1, {0xA9, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0xA9, 0, 0}, 0, 2,
      {OPT_Areg|OPS_64|OPA_None, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },

    { CPU_Any, 0, 0, 1, {0xF6, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, 0, 0, 1, {0xF6, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPA_Imm, 0} },
    { CPU_Any, 0, 16, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, 0, 16, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_16|OPA_Imm, 0} },
    { CPU_386, 0, 32, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, 0, 32, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },

    { CPU_Any, 0, 0, 1, {0x84, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_Any, 0, 16, 1, {0x85, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, 0, 32, 1, {0x85, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x85, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },

    { CPU_Any, 0, 0, 1, {0x84, 0, 0}, 0, 2,
      {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, 0, 16, 1, {0x85, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, 0, 32, 1, {0x85, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x85, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* Arithmetic - aad/aam */
static const x86_insn_info aadm_insn[] = {
    { CPU_Any, MOD_Op0Add, 0, 2, {0xD4, 0x0A, 0}, 0, 0, {0, 0, 0} },
    { CPU_Any, MOD_Op0Add, 0, 1, {0xD4, 0, 0}, 0, 1,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} }
};

/* Arithmetic - imul */
static const x86_insn_info imul_insn[] = {
    { CPU_Any, 0, 0, 1, {0xF6, 0, 0}, 5, 1, {OPT_RM|OPS_8|OPA_EA, 0, 0} },
    { CPU_Any, 0, 16, 1, {0xF7, 0, 0}, 5, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386, 0, 32, 1, {0xF7, 0, 0}, 5, 1, {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0xF7, 0, 0}, 5, 1,
      {OPT_RM|OPS_64|OPA_EA, 0, 0} },

    { CPU_386, 0, 16, 2, {0x0F, 0xAF, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, 0, 32, 2, {0x0F, 0xAF, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 2, {0x0F, 0xAF, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },

    { CPU_186, 0, 16, 1, {0x6B, 0, 0}, 0, 3,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPA_SImm} },
    { CPU_386, 0, 32, 1, {0x6B, 0, 0}, 0, 3,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPA_SImm} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x6B, 0, 0}, 0, 3,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPA_SImm} },

    { CPU_186, 0, 16, 1, {0x6B, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_SpareEA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_386, 0, 32, 1, {0x6B, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_SpareEA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x6B, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_SpareEA, OPT_Imm|OPS_8|OPA_SImm, 0} },

    { CPU_186, 0, 16, 1, {0x69, 0x6B, 0}, 0, 3,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_16|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail} },
    { CPU_386, 0, 32, 1, {0x69, 0x6B, 0}, 0, 3,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x69, 0x6B, 0}, 0, 3,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail} },

    { CPU_186, 0, 16, 1, {0x69, 0x6B, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_SpareEA,
       OPT_Imm|OPS_16|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail, 0} },
    { CPU_386, 0, 32, 1, {0x69, 0x6B, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_SpareEA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 1, {0x69, 0x6B, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_SpareEA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail, 0} }
};

/* Shifts - standard */
static const x86_insn_info shift_insn[] = {
    { CPU_Any, MOD_SpAdd, 0, 1, {0xD2, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    /* FIXME: imm8 is only avail on 186+, but we use imm8 to get to postponed
     * ,1 form, so it has to be marked as Any.  We need to store the active
     * CPU flags somewhere to pass that parse-time info down the line.
     */
    { CPU_Any, MOD_SpAdd, 0, 1, {0xC0, 0xD0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm|OPAP_ShiftOp,
       0} },
    { CPU_Any, MOD_SpAdd, 16, 1, {0xD3, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Any, MOD_SpAdd, 16, 1, {0xC1, 0xD1, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm|OPAP_ShiftOp,
       0} },
    { CPU_Any, MOD_SpAdd, 32, 1, {0xD3, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Any, MOD_SpAdd, 32, 1, {0xC1, 0xD1, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm|OPAP_ShiftOp,
       0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd, 64, 1, {0xD3, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd, 64, 1, {0xC1, 0xD1, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm|OPAP_ShiftOp,
       0} }
};

/* Shifts - doubleword */
static const x86_insn_info shlrd_insn[] = {
    { CPU_386, MOD_Op1Add, 16, 2, {0x0F, 0x00, 0}, 0, 3,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_386, MOD_Op1Add, 16, 2, {0x0F, 0x01, 0}, 0, 3,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare,
       OPT_Creg|OPS_8|OPA_None} },
    { CPU_386, MOD_Op1Add, 32, 2, {0x0F, 0x00, 0}, 0, 3,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_386, MOD_Op1Add, 32, 2, {0x0F, 0x01, 0}, 0, 3,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare,
       OPT_Creg|OPS_8|OPA_None} },
    { CPU_Hammer|CPU_64, MOD_Op1Add, 64, 2, {0x0F, 0x00, 0}, 0, 3,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_Hammer|CPU_64, MOD_Op1Add, 64, 2, {0x0F, 0x01, 0}, 0, 3,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare,
       OPT_Creg|OPS_8|OPA_None} }
};

/* Control transfer instructions (unconditional) */
static const x86_insn_info call_insn[] = {
    { CPU_Any, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPA_JmpRel, 0, 0} },
    { CPU_386, 0, 32, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_32|OPA_JmpRel, 0, 0} },

    { CPU_Any, 0, 16, 1, {0xE8, 0x9A, 0}, 0, 1,
      {OPT_Imm|OPS_16|OPTM_Near|OPA_JmpRel|OPAP_JmpFar, 0, 0} },
    { CPU_386, 0, 32, 1, {0xE8, 0x9A, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel|OPAP_JmpFar, 0, 0} },
    { CPU_Any, 0, 0, 1, {0xE8, 0x9A, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Near|OPA_JmpRel|OPAP_JmpFar, 0, 0} },

    { CPU_Any, 0, 16, 1, {0xFF, 0, 0}, 2, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 0, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 1, {0xFF, 0, 0}, 2, 1, {OPT_Mem|OPS_Any|OPA_EA, 0, 0} },
    { CPU_Any, 0, 16, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_RM|OPS_16|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_RM|OPS_32|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 0, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_RM|OPS_64|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_Mem|OPS_Any|OPTM_Near|OPA_EA, 0, 0} },

    { CPU_Any, 0, 16, 1, {0x9A, 0, 0}, 3, 1,
      {OPT_Imm|OPS_16|OPTM_Far|OPA_JmpRel, 0, 0} },
    { CPU_386, 0, 32, 1, {0x9A, 0, 0}, 3, 1,
      {OPT_Imm|OPS_32|OPTM_Far|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 0, 1, {0x9A, 0, 0}, 3, 1,
      {OPT_Imm|OPS_Any|OPTM_Far|OPA_JmpRel, 0, 0} },

    { CPU_Any, 0, 16, 1, {0xFF, 0, 0}, 3, 1,
      {OPT_Mem|OPS_16|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_386, 0, 32, 1, {0xFF, 0, 0}, 3, 1,
      {OPT_Mem|OPS_32|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 1, {0xFF, 0, 0}, 3, 1,
      {OPT_Mem|OPS_Any|OPTM_Far|OPA_EA, 0, 0} }
};
static const x86_insn_info jmp_insn[] = {
    { CPU_Any, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPA_JmpRel, 0, 0} },
    { CPU_386, 0, 32, 1, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_32|OPA_JmpRel, 0, 0} },

    { CPU_Any, 0, 0, 1, {0xEB, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 1, {0xE9, 0xEA, 0}, 0, 1,
      {OPT_Imm|OPS_16|OPTM_Near|OPA_JmpRel|OPAP_JmpFar, 0, 0} },
    { CPU_386, 0, 32, 1, {0xE9, 0xEA, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel|OPAP_JmpFar, 0, 0} },
    { CPU_Any, 0, 0, 1, {0xE9, 0xEA, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Near|OPA_JmpRel|OPAP_JmpFar, 0, 0} },

    { CPU_Any, 0, 16, 1, {0xFF, 0, 0}, 4, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 0, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 1, {0xFF, 0, 0}, 4, 1, {OPT_Mem|OPS_Any|OPA_EA, 0, 0} },
    { CPU_Any, 0, 16, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_RM|OPS_16|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_RM|OPS_32|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 0, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_RM|OPS_64|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_Mem|OPS_Any|OPTM_Near|OPA_EA, 0, 0} },

    { CPU_Any, 0, 16, 1, {0xEA, 0, 0}, 3, 1,
      {OPT_Imm|OPS_16|OPTM_Far|OPA_JmpRel, 0, 0} },
    { CPU_386, 0, 32, 1, {0xEA, 0, 0}, 3, 1,
      {OPT_Imm|OPS_32|OPTM_Far|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 0, 1, {0xEA, 0, 0}, 3, 1,
      {OPT_Imm|OPS_Any|OPTM_Far|OPA_JmpRel, 0, 0} },

    { CPU_Any, 0, 16, 1, {0xFF, 0, 0}, 5, 1,
      {OPT_Mem|OPS_16|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_386, 0, 32, 1, {0xFF, 0, 0}, 5, 1,
      {OPT_Mem|OPS_32|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 1, {0xFF, 0, 0}, 5, 1,
      {OPT_Mem|OPS_Any|OPTM_Far|OPA_EA, 0, 0} }
};
static const x86_insn_info retnf_insn[] = {
    { CPU_Any, MOD_Op0Add, 0, 1, {0x01, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_Any, MOD_Op0Add, 0, 1, {0x00, 0, 0}, 0, 1,
      {OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0, 0} }
};
static const x86_insn_info enter_insn[] = {
    { CPU_186, 0, 0, 1, {0xC8, 0, 0}, 0, 2,
      {OPT_Imm|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm,
       0} }
};

/* Conditional jumps */
static const x86_insn_info jcc_insn[] = {
    { CPU_Any, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPA_JmpRel, 0, 0} },
    { CPU_386, 0, 32, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_32|OPA_JmpRel, 0, 0} },
    { CPU_Any, MOD_Op0Add, 0, 1, {0x70, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, 0, 0} },
    { CPU_386, MOD_Op1Add, 16, 2, {0x0F, 0x80, 0}, 0, 1,
      {OPT_Imm|OPS_16|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_386, MOD_Op1Add, 32, 2, {0x0F, 0x80, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_386, MOD_Op1Add, 0, 2, {0x0F, 0x80, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Near|OPA_JmpRel, 0, 0} }
};
static const x86_insn_info jcxz_insn[] = {
    { CPU_Any, MOD_AdSizeR, 0, 0, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Any, MOD_AdSizeR, 0, 1, {0xE3, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, 0, 0} }
};

/* Loop instructions */
static const x86_insn_info loop_insn[] = {
    { CPU_Any, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Not64, 0, 0, 0, {0, 0, 0}, 0, 2,
      {OPT_Imm|OPS_Any|OPA_JmpRel, OPT_Creg|OPS_16|OPA_AdSizeR, 0} },
    { CPU_386, 0, 0, 0, {0, 0, 0}, 0, 2,
      {OPT_Imm|OPS_Any|OPA_JmpRel, OPT_Creg|OPS_32|OPA_AdSizeR, 0} },
    { CPU_Hammer|CPU_64, 0, 0, 0, {0, 0, 0}, 0, 2,
      {OPT_Imm|OPS_Any|OPA_JmpRel, OPT_Creg|OPS_64|OPA_AdSizeR, 0} },

    { CPU_Not64, MOD_Op0Add, 0, 1, {0xE0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, 0, 0} },
    { CPU_Any, MOD_Op0Add, 0, 1, {0xE0, 0, 0}, 0, 2,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, OPT_Creg|OPS_16|OPA_AdSizeR, 0}
    },
    { CPU_386, MOD_Op0Add, 0, 1, {0xE0, 0, 0}, 0, 2,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, OPT_Creg|OPS_32|OPA_AdSizeR, 0}
    },
    { CPU_Hammer|CPU_64, MOD_Op0Add, 0, 1, {0xE0, 0, 0}, 0, 2,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, OPT_Creg|OPS_64|OPA_AdSizeR, 0} }
};

/* Set byte on flag instructions */
static const x86_insn_info setcc_insn[] = {
    { CPU_386, MOD_Op1Add, 0, 2, {0x0F, 0x90, 0}, 2, 1,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0, 0} }
};

/* Bit manipulation - bit tests */
static const x86_insn_info bittest_insn[] = {
    { CPU_386, MOD_Op1Add, 16, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_Op1Add, 32, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add, 64, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd, 16, 2, {0x0F, 0xBA, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_8|OPA_Imm, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd, 32, 2, {0x0F, 0xBA, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_Gap0|MOD_SpAdd, 64, 2, {0x0F, 0xBA, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPA_Imm, 0} }
};

/* Bit manipulation - bit scans - also used for lar/lsl */
static const x86_insn_info bsfr_insn[] = {
    { CPU_286, MOD_Op1Add, 16, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_Op1Add, 32, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add, 64, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* Interrupts and operating system instructions */
static const x86_insn_info int_insn[] = {
    { CPU_Any, 0, 0, 1, {0xCD, 0, 0}, 0, 1,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} }
};
static const x86_insn_info bound_insn[] = {
    { CPU_186, 0, 16, 1, {0x62, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, 0, 32, 1, {0x62, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} }
};

/* Protection control */
static const x86_insn_info arpl_insn[] = {
    { CPU_286|CPU_Prot, 0, 0, 1, {0x63, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} }
};
static const x86_insn_info str_insn[] = {
    { CPU_Hammer, 0, 16, 2, {0x0F, 0x00, 0}, 1, 1,
      {OPT_Reg|OPS_16|OPA_EA, 0, 0} },
    { CPU_Hammer, 0, 32, 2, {0x0F, 0x00, 0}, 1, 1,
      {OPT_Reg|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 2, {0x0F, 0x00, 0}, 1, 1,
      {OPT_Reg|OPS_64|OPA_EA, 0, 0} },
    { CPU_286, MOD_Op1Add|MOD_SpAdd, 0, 2, {0x0F, 0x00, 0}, 0, 1,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} }
};
static const x86_insn_info prot286_insn[] = {
    { CPU_286, MOD_Op1Add|MOD_SpAdd, 0, 2, {0x0F, 0x00, 0}, 0, 1,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} }
};
static const x86_insn_info sldtmsw_insn[] = {
    { CPU_286, MOD_Op1Add|MOD_SpAdd, 0, 2, {0x0F, 0x00, 0}, 0, 1,
      {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_386, MOD_Op1Add|MOD_SpAdd, 0, 2, {0x0F, 0x00, 0}, 0, 1,
      {OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add|MOD_SpAdd, 0, 2, {0x0F, 0x00, 0}, 0, 1,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_286, MOD_Op1Add|MOD_SpAdd, 16, 2, {0x0F, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_16|OPA_EA, 0, 0} },
    { CPU_386, MOD_Op1Add|MOD_SpAdd, 32, 2, {0x0F, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add|MOD_SpAdd, 64, 2, {0x0F, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_64|OPA_EA, 0, 0} }
};

/* Floating point instructions - load/store with pop (integer and normal) */
static const x86_insn_info fldstp_insn[] = {
    { CPU_FPU, MOD_Gap0|MOD_SpAdd, 0, 1, {0xD9, 0, 0}, 0, 1,
      {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_SpAdd, 0, 1, {0xDD, 0, 0}, 0, 1,
      {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Gap1|MOD_SpAdd, 0, 1, {0xDB, 0, 0}, 0, 1,
      {OPT_Mem|OPS_80|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 2, {0xD9, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} }
};
static const x86_insn_info fildstp_insn[] = {
    { CPU_FPU, MOD_SpAdd, 0, 1, {0xDF, 0, 0}, 0, 1,
      {OPT_Mem|OPS_16|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_SpAdd, 0, 1, {0xDB, 0, 0}, 0, 1,
      {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_SpAdd, 0, 1, {0xDF, 0, 0}, 0, 1,
      {OPT_Mem|OPS_64|OPA_EA, 0, 0} }
};
static const x86_insn_info fbldstp_insn[] = {
    { CPU_FPU, MOD_SpAdd, 0, 1, {0xDF, 0, 0}, 0, 1,
      {OPT_Mem|OPS_80|OPS_Relaxed|OPA_EA, 0, 0} }
};
/* Floating point instructions - store (normal) */
static const x86_insn_info fst_insn[] = {
    { CPU_FPU, 0, 0, 1, {0xD9, 0, 0}, 2, 1, {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 1, {0xDD, 0, 0}, 2, 1, {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 2, {0xDD, 0xD0, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} }
};
/* Floating point instructions - exchange (with ST0) */
static const x86_insn_info fxch_insn[] = {
    { CPU_FPU, 0, 0, 2, {0xD9, 0xC8, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_FPU, 0, 0, 2, {0xD9, 0xC8, 0}, 0, 2,
      {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} },
    { CPU_FPU, 0, 0, 2, {0xD9, 0xC8, 0}, 0, 2,
      {OPT_Reg|OPS_80|OPA_Op1Add, OPT_ST0|OPS_80|OPA_None, 0} },
    { CPU_FPU, 0, 0, 2, {0xD9, 0xC9, 0}, 0, 0, {0, 0, 0} }
};
/* Floating point instructions - comparisons */
static const x86_insn_info fcom_insn[] = {
    { CPU_FPU, MOD_Gap0|MOD_SpAdd, 0, 1, {0xD8, 0, 0}, 0, 1,
      {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_SpAdd, 0, 1, {0xDC, 0, 0}, 0, 1,
      {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 2, {0xD8, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 2, {0xD8, 0x00, 0}, 0, 2,
      {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} }
};
/* Floating point instructions - extended comparisons */
static const x86_insn_info fcom2_insn[] = {
    { CPU_286|CPU_FPU, MOD_Op0Add|MOD_Op1Add, 0, 2, {0x00, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_286|CPU_FPU, MOD_Op0Add|MOD_Op1Add, 0, 2, {0x00, 0x00, 0}, 0, 2,
      {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} }
};
/* Floating point instructions - arithmetic */
static const x86_insn_info farith_insn[] = {
    { CPU_FPU, MOD_Gap0|MOD_Gap1|MOD_SpAdd, 0, 1, {0xD8, 0, 0}, 0, 1,
      {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Gap1|MOD_SpAdd, 0, 1, {0xDC, 0, 0}, 0, 1,
      {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Op1Add, 0, 2, {0xD8, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Op1Add, 0, 2, {0xD8, 0x00, 0}, 0, 2,
      {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 2, {0xDC, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPTM_To|OPA_Op1Add, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 2, {0xDC, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_80|OPA_Op1Add, OPT_ST0|OPS_80|OPA_None, 0} }
};
static const x86_insn_info farithp_insn[] = {
    { CPU_FPU, MOD_Op1Add, 0, 2, {0xDE, 0x01, 0}, 0, 0, {0, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 2, {0xDE, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 2, {0xDE, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_80|OPA_Op1Add, OPT_ST0|OPS_80|OPA_None, 0} }
};
/* Floating point instructions - integer arith/store wo pop/compare */
static const x86_insn_info fiarith_insn[] = {
    { CPU_FPU, MOD_Op0Add|MOD_SpAdd, 0, 1, {0x04, 0, 0}, 0, 1,
      {OPT_Mem|OPS_16|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Op0Add|MOD_SpAdd, 0, 1, {0x00, 0, 0}, 0, 1,
      {OPT_Mem|OPS_32|OPA_EA, 0, 0} }
};
/* Floating point instructions - processor control */
static const x86_insn_info fldnstcw_insn[] = {
    { CPU_FPU, MOD_SpAdd, 0, 1, {0xD9, 0, 0}, 0, 1,
      {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} }
};
static const x86_insn_info fstcw_insn[] = {
    { CPU_FPU, 0, 0, 2, {0x9B, 0xD9, 0}, 7, 1,
      {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} }
};
static const x86_insn_info fnstsw_insn[] = {
    { CPU_FPU, 0, 0, 1, {0xDD, 0, 0}, 7, 1,
      {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 2, {0xDF, 0xE0, 0}, 0, 1,
      {OPT_Areg|OPS_16|OPA_None, 0, 0} }
};
static const x86_insn_info fstsw_insn[] = {
    { CPU_FPU, 0, 0, 2, {0x9B, 0xDD, 0}, 7, 1,
      {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 3, {0x9B, 0xDF, 0xE0}, 0, 1,
      {OPT_Areg|OPS_16|OPA_None, 0, 0} }
};
static const x86_insn_info ffree_insn[] = {
    { CPU_FPU, MOD_Op0Add, 0, 2, {0x00, 0xC0, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} }
};

/* 486 extensions */
static const x86_insn_info bswap_insn[] = {
    { CPU_486, 0, 32, 2, {0x0F, 0xC8, 0}, 0, 1,
      {OPT_Reg|OPS_32|OPA_Op1Add, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 2, {0x0F, 0xC8, 0}, 0, 1,
      {OPT_Reg|OPS_64|OPA_Op1Add, 0, 0} }
};
static const x86_insn_info cmpxchgxadd_insn[] = {
    { CPU_486, MOD_Op1Add, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_486, MOD_Op1Add, 16, 2, {0x0F, 0x01, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_486, MOD_Op1Add, 32, 2, {0x0F, 0x01, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add, 64, 2, {0x0F, 0x01, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} }
};

/* Pentium extensions */
static const x86_insn_info cmpxchg8b_insn[] = {
    { CPU_586, 0, 0, 2, {0x0F, 0xC7, 0}, 1, 1,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0, 0} }
};

/* Pentium II/Pentium Pro extensions */
static const x86_insn_info cmovcc_insn[] = {
    { CPU_686, MOD_Op1Add, 16, 2, {0x0F, 0x40, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_686, MOD_Op1Add, 32, 2, {0x0F, 0x40, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add, 64, 2, {0x0F, 0x40, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};
static const x86_insn_info fcmovcc_insn[] = {
    { CPU_686|CPU_FPU, MOD_Op0Add|MOD_Op1Add, 0, 2, {0x00, 0x00, 0}, 0, 2,
      {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} }
};

/* Pentium4 extensions */
static const x86_insn_info movnti_insn[] = {
    { CPU_P4, 0, 0, 2, {0x0F, 0xC3, 0}, 0, 2,
      {OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 2, {0x0F, 0xC3, 0}, 0, 2,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} }
};
static const x86_insn_info clflush_insn[] = {
    { CPU_P3, 0, 0, 2, {0x0F, 0xAE, 0}, 7, 1,
      {OPT_Mem|OPS_8|OPS_Relaxed|OPA_EA, 0, 0} }
};

/* MMX/SSE2 instructions */
static const x86_insn_info movd_insn[] = {
    { CPU_MMX, 0, 0, 2, {0x0F, 0x6E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_MMX|CPU_Hammer|CPU_64, 0, 64, 2, {0x0F, 0x6E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_MMX, 0, 0, 2, {0x0F, 0x7E, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} },
    { CPU_MMX|CPU_Hammer|CPU_64, 0, 64, 2, {0x0F, 0x7E, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} },
    { CPU_SSE2, 0, 0, 3, {0x66, 0x0F, 0x6E}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2|CPU_Hammer|CPU_64, 0, 64, 3, {0x66, 0x0F, 0x6E}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, 0, 0, 3, {0x66, 0x0F, 0x7E}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} },
    { CPU_SSE2|CPU_Hammer|CPU_64, 0, 64, 3, {0x66, 0x0F, 0x7E}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info movq_insn[] = {
    { CPU_MMX, 0, 0, 2, {0x0F, 0x6F, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_MMX, 0, 0, 2, {0x0F, 0x7F, 0}, 0, 2,
      {OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0}
    },
    { CPU_SSE2, 0, 0, 3, {0xF3, 0x0F, 0x7E}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE2, 0, 0, 3, {0xF3, 0x0F, 0x7E}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_SSE2, 0, 0, 3, {0x66, 0x0F, 0xD6}, 0, 2,
      {OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0}
    }
};
static const x86_insn_info mmxsse2_insn[] = {
    { CPU_MMX, MOD_Op1Add, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_SSE2, MOD_Op2Add, 0, 3, {0x66, 0x0F, 0x00}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info pshift_insn[] = {
    { CPU_MMX, MOD_Op1Add, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_MMX, MOD_Gap0|MOD_Op1Add|MOD_SpAdd, 0, 2, {0x0F, 0x00, 0}, 0,
      2, {OPT_SIMDReg|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_SSE2, MOD_Op2Add, 0, 3, {0x66, 0x0F, 0x00}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_SSE2, MOD_Gap0|MOD_Op2Add|MOD_SpAdd, 0, 3, {0x66, 0x0F, 0x00}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} }
};

/* PIII (Katmai) new instructions / SIMD instructiosn */
static const x86_insn_info sseps_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info ssess_insn[] = {
    { CPU_SSE, MOD_Op0Add|MOD_Op2Add, 0, 3, {0x00, 0x0F, 0x00}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info ssecmpps_insn[] = {
    { CPU_SSE, MOD_Imm8, 0, 2, {0x0F, 0xC2, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info ssecmpss_insn[] = {
    { CPU_SSE, MOD_Op0Add|MOD_Imm8, 0, 3, {0x00, 0x0F, 0xC2}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info ssepsimm_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 2, {0x0F, 0x00, 0}, 0, 3,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};
static const x86_insn_info ssessimm_insn[] = {
    { CPU_SSE, MOD_Op0Add|MOD_Op2Add, 0, 3, {0x00, 0x0F, 0x00}, 0, 3,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};
static const x86_insn_info ldstmxcsr_insn[] = {
    { CPU_SSE, MOD_SpAdd, 0, 2, {0x0F, 0xAE, 0}, 0, 1,
      {OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0, 0} }
};
static const x86_insn_info maskmovq_insn[] = {
    { CPU_P3|CPU_MMX, 0, 0, 2, {0x0F, 0xF7, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA, 0} }
};
static const x86_insn_info movaups_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_SSE, MOD_Op1Add, 0, 2, {0x0F, 0x01, 0}, 0, 2,
      {OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0}
    }
};
static const x86_insn_info movhllhps_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};
static const x86_insn_info movhlps_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE, MOD_Op1Add, 0, 2, {0x0F, 0x01, 0}, 0, 2,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info movmskps_insn[] = {
    { CPU_SSE, 0, 0, 2, {0x0F, 0x50, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info movntps_insn[] = {
    { CPU_SSE, 0, 0, 2, {0x0F, 0x2B, 0}, 0, 2,
      {OPT_Mem|OPS_128|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info movntq_insn[] = {
    { CPU_SSE, 0, 0, 2, {0x0F, 0xE7, 0}, 0, 2,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info movss_insn[] = {
    { CPU_SSE, 0, 0, 3, {0xF3, 0x0F, 0x10}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE, 0, 0, 3, {0xF3, 0x0F, 0x10}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE, 0, 0, 3, {0xF3, 0x0F, 0x11}, 0, 2,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info pextrw_insn[] = {
    { CPU_P3|CPU_MMX, 0, 0, 2, {0x0F, 0xC5, 0}, 0, 3,
      {OPT_Reg|OPS_32|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_SSE2, 0, 0, 3, {0x66, 0x0F, 0xC5}, 0, 3,
      {OPT_Reg|OPS_32|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};
static const x86_insn_info pinsrw_insn[] = {
    { CPU_P3|CPU_MMX, 0, 0, 2, {0x0F, 0xC4, 0}, 0, 3,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_Reg|OPS_32|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_P3|CPU_MMX, 0, 0, 2, {0x0F, 0xC4, 0}, 0, 3,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_SSE2, 0, 0, 3, {0x66, 0x0F, 0xC4}, 0, 3,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Reg|OPS_32|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_SSE2, 0, 0, 3, {0x66, 0x0F, 0xC4}, 0, 3,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};
static const x86_insn_info pmovmskb_insn[] = {
    { CPU_P3|CPU_MMX, 0, 0, 2, {0x0F, 0xD7, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} },
    { CPU_SSE2, 0, 0, 3, {0x66, 0x0F, 0xD7}, 0, 2,
      {OPT_Reg|OPS_32|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info pshufw_insn[] = {
    { CPU_P3|CPU_MMX, 0, 0, 2, {0x0F, 0x70, 0}, 0, 3,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

/* SSE2 instructions */
static const x86_insn_info cmpsd_insn[] = {
    { CPU_Any, 0, 32, 1, {0xA7, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_SSE2, 0, 0, 3, {0xF2, 0x0F, 0xC2}, 0, 3,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};
static const x86_insn_info movaupd_insn[] = {
    { CPU_SSE2, MOD_Op2Add, 0, 3, {0x66, 0x0F, 0x00}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_SSE2, MOD_Op2Add, 0, 3, {0x66, 0x0F, 0x01}, 0, 2,
      {OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0}
    }
};
static const x86_insn_info movhlpd_insn[] = {
    { CPU_SSE2, MOD_Op2Add, 0, 3, {0x66, 0x0F, 0x00}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, MOD_Op2Add, 0, 3, {0x66, 0x0F, 0x01}, 0, 2,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info movmskpd_insn[] = {
    { CPU_SSE2, 0, 0, 3, {0x66, 0x0F, 0x50}, 0, 2,
      {OPT_Reg|OPS_32|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info movntpddq_insn[] = {
    { CPU_SSE2, MOD_Op2Add, 0, 3, {0x66, 0x0F, 0x00}, 0, 2,
      {OPT_Mem|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info movsd_insn[] = {
    { CPU_Any, 0, 32, 1, {0xA5, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_SSE2, 0, 0, 3, {0xF2, 0x0F, 0x10}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE2, 0, 0, 3, {0xF2, 0x0F, 0x10}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, 0, 0, 3, {0xF2, 0x0F, 0x11}, 0, 2,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info maskmovdqu_insn[] = {
    { CPU_SSE2, 0, 0, 3, {0x66, 0x0F, 0xF7}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};
static const x86_insn_info movdqau_insn[] = {
    { CPU_SSE2, MOD_Op0Add, 0, 3, {0x00, 0x0F, 0x6F}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_SSE2, MOD_Op0Add, 0, 3, {0x00, 0x0F, 0x7F}, 0, 2,
      {OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0}
    }
};
static const x86_insn_info movdq2q_insn[] = {
    { CPU_SSE2, 0, 0, 3, {0xF2, 0x0F, 0xD6}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};
static const x86_insn_info movq2dq_insn[] = {
    { CPU_SSE2, 0, 0, 3, {0xF3, 0x0F, 0xD6}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA, 0} }
};
static const x86_insn_info pslrldq_insn[] = {
    { CPU_SSE2, MOD_SpAdd, 0, 3, {0x66, 0x0F, 0x73}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} }
};

/* AMD 3DNow! instructions */
static const x86_insn_info now3d_insn[] = {
    { CPU_3DNow, MOD_Imm8, 0, 2, {0x0F, 0x0F, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* Cyrix MMX instructions */
static const x86_insn_info cyrixmmx_insn[] = {
    { CPU_Cyrix|CPU_MMX, MOD_Op1Add, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};
static const x86_insn_info pmachriw_insn[] = {
    { CPU_Cyrix|CPU_MMX, 0, 0, 2, {0x0F, 0x5E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* Cyrix extensions */
static const x86_insn_info rsdc_insn[] = {
    { CPU_486|CPU_Cyrix|CPU_SMM, 0, 0, 2, {0x0F, 0x79, 0}, 0, 2,
      {OPT_SegReg|OPS_16|OPA_Spare, OPT_Mem|OPS_80|OPS_Relaxed|OPA_EA, 0} }
};
static const x86_insn_info cyrixsmm_insn[] = {
    { CPU_486|CPU_Cyrix|CPU_SMM, MOD_Op1Add, 0, 2, {0x0F, 0x00, 0}, 0, 1,
      {OPT_Mem|OPS_80|OPS_Relaxed|OPA_EA, 0, 0} }
};
static const x86_insn_info svdc_insn[] = {
    { CPU_486|CPU_Cyrix|CPU_SMM, 0, 0, 2, {0x0F, 0x78, 0}, 0, 2,
      {OPT_Mem|OPS_80|OPS_Relaxed|OPA_EA, OPT_SegReg|OPS_16|OPA_Spare, 0} }
};

/* Obsolete/undocumented instructions */
static const x86_insn_info ibts_insn[] = {
    { CPU_386|CPU_Undoc|CPU_Obs, 0, 16, 2, {0x0F, 0xA7, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386|CPU_Undoc|CPU_Obs, 0, 32, 2, {0x0F, 0xA7, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} }
};
static const x86_insn_info umov_insn[] = {
    { CPU_386|CPU_Undoc, 0, 0, 2, {0x0F, 0x10, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_386|CPU_Undoc, 0, 16, 2, {0x0F, 0x11, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386|CPU_Undoc, 0, 32, 2, {0x0F, 0x11, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_386|CPU_Undoc, 0, 0, 2, {0x0F, 0x12, 0}, 0, 2,
      {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_Undoc, 0, 16, 2, {0x0F, 0x13, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_Undoc, 0, 32, 2, {0x0F, 0x13, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} }
};
static const x86_insn_info xbts_insn[] = {
    { CPU_386|CPU_Undoc|CPU_Obs, 0, 16, 2, {0x0F, 0xA6, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_Undoc|CPU_Obs, 0, 32, 2, {0x0F, 0xA6, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} }
};


static yasm_bytecode *
x86_new_jmprel(const unsigned long data[4], int num_operands,
	       yasm_insn_operandhead *operands, x86_insn_info *jrinfo,
	       yasm_section *cur_section, /*@null@*/ yasm_bytecode *prev_bc,
	       unsigned long lindex)
{
    x86_new_jmprel_data d;
    int num_info = (int)(data[1]&0xFF);
    x86_insn_info *info = (x86_insn_info *)data[0];
    unsigned long mod_data = data[1] >> 8;
    yasm_insn_operand *op;
    static const unsigned char size_lookup[] = {0, 8, 16, 32, 64, 80, 128, 0};

    d.lindex = lindex;

    /* We know the target is in operand 0, but sanity check for Imm. */
    op = yasm_ops_first(operands);
    if (op->type != YASM_INSN__OPERAND_IMM)
	yasm_internal_error(N_("invalid operand conversion"));

    /* Far target needs to become "seg imm:imm". */
    if ((jrinfo->operands[0] & OPTM_MASK) == OPTM_Far)
	d.target = yasm_expr_new_tree(
	    yasm_expr_new_branch(YASM_EXPR_SEG, op->data.val, lindex),
	    YASM_EXPR_SEGOFF, yasm_expr_copy(op->data.val), lindex);
    else
	d.target = op->data.val;

    /* Need to save jump origin for relative jumps. */
    d.origin = yasm_symrec_define_label("$", cur_section, prev_bc, 0, lindex);

    /* Initially assume no far opcode is available. */
    d.far_op_len = 0;

    /* See if the user explicitly specified short/near/far. */
    switch ((int)(jrinfo->operands[0] & OPTM_MASK)) {
	case OPTM_Short:
	    d.op_sel = JR_SHORT_FORCED;
	    break;
	case OPTM_Near:
	    d.op_sel = JR_NEAR_FORCED;
	    break;
	case OPTM_Far:
	    d.op_sel = JR_FAR;
	    d.far_op_len = info->opcode_len;
	    d.far_op[0] = info->opcode[0];
	    d.far_op[1] = info->opcode[1];
	    d.far_op[2] = info->opcode[2];
	    break;
	default:
	    d.op_sel = JR_NONE;
    }

    /* Set operand size */
    d.opersize = jrinfo->opersize;

    /* Check for address size setting in second operand, if present */
    if (jrinfo->num_operands > 1 &&
	(jrinfo->operands[1] & OPA_MASK) == OPA_AdSizeR)
	d.addrsize = (unsigned char)size_lookup[(jrinfo->operands[1] &
						 OPS_MASK)>>OPS_SHIFT];
    else
	d.addrsize = 0;

    /* Check for address size override */
    if (jrinfo->modifiers & MOD_AdSizeR)
	d.addrsize = (unsigned char)(mod_data & 0xFF);

    /* Scan through other infos for this insn looking for short/near versions.
     * Needs to match opersize and number of operands, also be within CPU.
     */
    d.short_op_len = 0;
    d.near_op_len = 0;
    for (; num_info>0 && (d.short_op_len == 0 || d.near_op_len == 0);
	 num_info--, info++) {
	unsigned long cpu = info->cpu | data[2];

	if ((cpu & CPU_64) && yasm_x86_LTX_mode_bits != 64)
	    continue;
	if ((cpu & CPU_Not64) && yasm_x86_LTX_mode_bits == 64)
	    continue;
	cpu &= ~(CPU_64 | CPU_Not64);

	if ((cpu_enabled & cpu) != cpu)
	    continue;

	if (info->num_operands == 0)
	    continue;

	if ((info->operands[0] & OPA_MASK) != OPA_JmpRel)
	    continue;

	if (info->opersize != d.opersize)
	    continue;

	switch ((int)(info->operands[0] & OPTM_MASK)) {
	    case OPTM_Short:
		d.short_op_len = info->opcode_len;
		d.short_op[0] = info->opcode[0];
		d.short_op[1] = info->opcode[1];
		d.short_op[2] = info->opcode[2];
		if (info->modifiers & MOD_Op0Add)
		    d.short_op[0] += (unsigned char)(mod_data & 0xFF);
		break;
	    case OPTM_Near:
		d.near_op_len = info->opcode_len;
		d.near_op[0] = info->opcode[0];
		d.near_op[1] = info->opcode[1];
		d.near_op[2] = info->opcode[2];
		if (info->modifiers & MOD_Op1Add)
		    d.near_op[1] += (unsigned char)(mod_data & 0xFF);
		if ((info->operands[0] & OPAP_MASK) == OPAP_JmpFar) {
		    d.far_op_len = 1;
		    d.far_op[0] = info->opcode[info->opcode_len];
		}
		break;
	}
    }

    return yasm_x86__bc_new_jmprel(&d);
}

yasm_bytecode *
yasm_x86__parse_insn(const unsigned long data[4], int num_operands,
		     yasm_insn_operandhead *operands,
		     yasm_section *cur_section,
		     /*@null@*/ yasm_bytecode *prev_bc, unsigned long lindex)
{
    x86_new_insn_data d;
    int num_info = (int)(data[1]&0xFF);
    x86_insn_info *info = (x86_insn_info *)data[0];
    unsigned long mod_data = data[1] >> 8;
    int found = 0;
    yasm_insn_operand *op;
    int i;
    static const unsigned int size_lookup[] = {0, 1, 2, 4, 8, 10, 16, 0};

    /* Just do a simple linear search through the info array for a match.
     * First match wins.
     */
    for (; num_info>0 && !found; num_info--, info++) {
	unsigned long cpu;
	unsigned int size;
	int mismatch = 0;

	/* Match CPU */
	cpu = info->cpu | data[2];

	if ((cpu & CPU_64) && yasm_x86_LTX_mode_bits != 64)
	    continue;
	if ((cpu & CPU_Not64) && yasm_x86_LTX_mode_bits == 64)
	    continue;
	cpu &= ~(CPU_64 | CPU_Not64);

	if ((cpu_enabled & cpu) != cpu)
	    continue;

	/* Match # of operands */
	if (num_operands != info->num_operands)
	    continue;

	if (!operands) {
	    found = 1;	    /* no operands -> must have a match here. */
	    break;
	}

	/* Match each operand type and size */
	for(i = 0, op = yasm_ops_first(operands); op && i<info->num_operands &&
	    !mismatch; op = yasm_ops_next(op), i++) {
	    /* Check operand type */
	    switch ((int)(info->operands[i] & OPT_MASK)) {
		case OPT_Imm:
		    if (op->type != YASM_INSN__OPERAND_IMM)
			mismatch = 1;
		    break;
		case OPT_RM:
		    if (op->type == YASM_INSN__OPERAND_MEMORY)
			break;
		    /*@fallthrough@*/
		case OPT_Reg:
		    if (op->type != YASM_INSN__OPERAND_REG)
			mismatch = 1;
		    else {
			switch ((x86_expritem_reg_size)(op->data.reg&~0xFUL)) {
			    case X86_REG8:
			    case X86_REG8X:
			    case X86_REG16:
			    case X86_REG32:
			    case X86_REG64:
			    case X86_FPUREG:
				break;
			    default:
				mismatch = 1;
				break;
			}
		    }
		    break;
		case OPT_Mem:
		    if (op->type != YASM_INSN__OPERAND_MEMORY)
			mismatch = 1;
		    break;
		case OPT_SIMDRM:
		    if (op->type == YASM_INSN__OPERAND_MEMORY)
			break;
		    /*@fallthrough@*/
		case OPT_SIMDReg:
		    if (op->type != YASM_INSN__OPERAND_REG)
			mismatch = 1;
		    else {
			switch ((x86_expritem_reg_size)(op->data.reg&~0xFUL)) {
			    case X86_MMXREG:
			    case X86_XMMREG:
				break;
			    default:
				mismatch = 1;
				break;
			}
		    }
		    break;
		case OPT_SegReg:
		    if (op->type != YASM_INSN__OPERAND_SEGREG)
			mismatch = 1;
		    break;
		case OPT_CRReg:
		    if (op->type != YASM_INSN__OPERAND_REG ||
			(op->data.reg & ~0xFUL) != X86_CRREG)
			mismatch = 1;
		    break;
		case OPT_DRReg:
		    if (op->type != YASM_INSN__OPERAND_REG ||
			(op->data.reg & ~0xFUL) != X86_DRREG)
			mismatch = 1;
		    break;
		case OPT_TRReg:
		    if (op->type != YASM_INSN__OPERAND_REG ||
			(op->data.reg & ~0xFUL) != X86_TRREG)
			mismatch = 1;
		    break;
		case OPT_ST0:
		    if (op->type != YASM_INSN__OPERAND_REG ||
			op->data.reg != X86_FPUREG)
			mismatch = 1;
		    break;
		case OPT_Areg:
		    if (op->type != YASM_INSN__OPERAND_REG ||
			((info->operands[i] & OPS_MASK) == OPS_8 &&
			 op->data.reg != (X86_REG8 | 0) &&
			 op->data.reg != (X86_REG8X | 0)) ||
			((info->operands[i] & OPS_MASK) == OPS_16 &&
			 op->data.reg != (X86_REG16 | 0)) ||
			((info->operands[i] & OPS_MASK) == OPS_32 &&
			 op->data.reg != (X86_REG32 | 0)) ||
			((info->operands[i] & OPS_MASK) == OPS_64 &&
			 op->data.reg != (X86_REG64 | 0)))
			mismatch = 1;
		    break;
		case OPT_Creg:
		    if (op->type != YASM_INSN__OPERAND_REG ||
			((info->operands[i] & OPS_MASK) == OPS_8 &&
			 op->data.reg != (X86_REG8 | 1) &&
			 op->data.reg != (X86_REG8X | 1)) ||
			((info->operands[i] & OPS_MASK) == OPS_16 &&
			 op->data.reg != (X86_REG16 | 1)) ||
			((info->operands[i] & OPS_MASK) == OPS_32 &&
			 op->data.reg != (X86_REG32 | 1)) ||
			((info->operands[i] & OPS_MASK) == OPS_64 &&
			 op->data.reg != (X86_REG64 | 1)))
			mismatch = 1;
		    break;
		case OPT_Dreg:
		    if (op->type != YASM_INSN__OPERAND_REG ||
			((info->operands[i] & OPS_MASK) == OPS_8 &&
			 op->data.reg != (X86_REG8 | 2) &&
			 op->data.reg != (X86_REG8X | 2)) ||
			((info->operands[i] & OPS_MASK) == OPS_16 &&
			 op->data.reg != (X86_REG16 | 2)) ||
			((info->operands[i] & OPS_MASK) == OPS_32 &&
			 op->data.reg != (X86_REG32 | 2)) ||
			((info->operands[i] & OPS_MASK) == OPS_64 &&
			 op->data.reg != (X86_REG64 | 2)))
			mismatch = 1;
		    break;
		case OPT_CS:
		    if (op->type != YASM_INSN__OPERAND_SEGREG ||
			(op->data.reg & 0xF) != 1)
			mismatch = 1;
		    break;
		case OPT_DS:
		    if (op->type != YASM_INSN__OPERAND_SEGREG ||
			(op->data.reg & 0xF) != 3)
			mismatch = 1;
		    break;
		case OPT_ES:
		    if (op->type != YASM_INSN__OPERAND_SEGREG ||
			(op->data.reg & 0xF) != 0)
			mismatch = 1;
		    break;
		case OPT_FS:
		    if (op->type != YASM_INSN__OPERAND_SEGREG ||
			(op->data.reg & 0xF) != 4)
			mismatch = 1;
		    break;
		case OPT_GS:
		    if (op->type != YASM_INSN__OPERAND_SEGREG ||
			(op->data.reg & 0xF) != 5)
			mismatch = 1;
		    break;
		case OPT_SS:
		    if (op->type != YASM_INSN__OPERAND_SEGREG ||
			(op->data.reg & 0xF) != 2)
			mismatch = 1;
		    break;
		case OPT_CR4:
		    if (op->type != YASM_INSN__OPERAND_REG ||
			op->data.reg != (X86_CRREG | 4))
			mismatch = 1;
		    break;
		case OPT_MemOffs:
		    if (op->type != YASM_INSN__OPERAND_MEMORY ||
			yasm_expr__contains(yasm_ea_get_disp(op->data.ea),
					    YASM_EXPR_REG))
			mismatch = 1;
		    break;
		default:
		    yasm_internal_error(N_("invalid operand type"));
	    }

	    if (mismatch)
		break;

	    /* Check operand size */
	    size = size_lookup[(info->operands[i] & OPS_MASK)>>OPS_SHIFT];
	    if (op->type == YASM_INSN__OPERAND_REG && op->size == 0) {
		/* Register size must exactly match */
		if (yasm_x86__get_reg_size(op->data.reg) != size)
		    mismatch = 1;
	    } else {
		if ((info->operands[i] & OPS_RMASK) == OPS_Relaxed) {
		    /* Relaxed checking */
		    if (size != 0 && op->size != size && op->size != 0)
			mismatch = 1;
		} else {
		    /* Strict checking */
		    if (op->size != size)
			mismatch = 1;
		}
	    }

	    if (mismatch)
		break;

	    /* Check target modifier */
	    switch ((int)(info->operands[i] & OPTM_MASK)) {
		case OPTM_None:
		    if (op->targetmod != 0)
			mismatch = 1;
		    break;
		case OPTM_Near:
		    if (op->targetmod != X86_NEAR)
			mismatch = 1;
		    break;
		case OPTM_Short:
		    if (op->targetmod != X86_SHORT)
			mismatch = 1;
		    break;
		case OPTM_Far:
		    if (op->targetmod != X86_FAR)
			mismatch = 1;
		    break;
		case OPTM_To:
		    if (op->targetmod != X86_TO)
			mismatch = 1;
		    break;
		default:
		    yasm_internal_error(N_("invalid target modifier type"));
	    }
	}

	if (!mismatch) {
	    found = 1;
	    break;
	}
    }

    if (!found) {
	/* Didn't find a matching one */
	yasm__error(lindex, N_("invalid combination of opcode and operands"));
	return NULL;
    }

    /* Extended error/warning handling */
    switch ((int)(info->modifiers & MOD_Ext_MASK)) {
	case MOD_ExtNone:
	    /* No extended modifier, so just continue */
	    break;
	case MOD_ExtErr:
	    switch ((int)((info->modifiers & MOD_ExtIndex_MASK)
			  >> MOD_ExtIndex_SHIFT)) {
		case 0:
		    yasm__error(lindex, N_("mismatch in operand sizes"));
		    break;
		case 1:
		    yasm__error(lindex, N_("operand size not specified"));
		    break;
		default:
		    yasm_internal_error(N_("unrecognized x86 ext mod index"));
	    }
	    return NULL;    /* It was an error */
	case MOD_ExtWarn:
	    switch ((int)((info->modifiers & MOD_ExtIndex_MASK)
			  >> MOD_ExtIndex_SHIFT)) {
		default:
		    yasm_internal_error(N_("unrecognized x86 ext mod index"));
	    }
	    break;
	default:
	    yasm_internal_error(N_("unrecognized x86 extended modifier"));
    }

    /* Shortcut to JmpRel */
    if (operands && (info->operands[0] & OPA_MASK) == OPA_JmpRel)
	return x86_new_jmprel(data, num_operands, operands, info, cur_section,
			      prev_bc, lindex);

    /* Copy what we can from info */
    d.lindex = lindex;
    d.ea = NULL;
    d.imm = NULL;
    d.opersize = info->opersize;
    d.op_len = info->opcode_len;
    d.op[0] = info->opcode[0];
    d.op[1] = info->opcode[1];
    d.op[2] = info->opcode[2];
    d.spare = info->spare;
    d.rex = (yasm_x86_LTX_mode_bits == 64 && info->opersize == 64) ? 0x48: 0;
    d.im_len = 0;
    d.im_sign = 0;
    d.shift_op = 0;
    d.signext_imm8_op = 0;

    /* Apply modifiers */
    if (info->modifiers & MOD_Op2Add) {
	d.op[2] += (unsigned char)(mod_data & 0xFF);
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_Gap0)
	mod_data >>= 8;
    if (info->modifiers & MOD_Op1Add) {
	d.op[1] += (unsigned char)(mod_data & 0xFF);
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_Gap1)
	mod_data >>= 8;
    if (info->modifiers & MOD_Op0Add) {
	d.op[0] += (unsigned char)(mod_data & 0xFF);
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_SpAdd) {
	d.spare += (unsigned char)(mod_data & 0xFF);
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_OpSizeR) {
	d.opersize = (unsigned char)(mod_data & 0xFF);
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_Imm8) {
	d.imm = yasm_expr_new_ident(yasm_expr_int(
	    yasm_intnum_new_uint(mod_data & 0xFF)), lindex);
	d.im_len = 1;
	/*mod_data >>= 8;*/
    }

    /* Go through operands and assign */
    if (operands) {
	for(i = 0, op = yasm_ops_first(operands); op && i<info->num_operands;
	    op = yasm_ops_next(op), i++) {
	    switch ((int)(info->operands[i] & OPA_MASK)) {
		case OPA_None:
		    /* Throw away the operand contents */
		    switch (op->type) {
			case YASM_INSN__OPERAND_REG:
			case YASM_INSN__OPERAND_SEGREG:
			    break;
			case YASM_INSN__OPERAND_MEMORY:
			    yasm_ea_delete(op->data.ea);
			    break;
			case YASM_INSN__OPERAND_IMM:
			    yasm_expr_delete(op->data.val);
			    break;
		    }
		    break;
		case OPA_EA:
		    switch (op->type) {
			case YASM_INSN__OPERAND_REG:
			    d.ea =
				yasm_x86__ea_new_reg(op->data.reg, &d.rex,
						     yasm_x86_LTX_mode_bits);
			    break;
			case YASM_INSN__OPERAND_SEGREG:
			    yasm_internal_error(
				N_("invalid operand conversion"));
			case YASM_INSN__OPERAND_MEMORY:
			    d.ea = op->data.ea;
			    if ((info->operands[i] & OPT_MASK) == OPT_MemOffs)
				/* Special-case for MOV MemOffs instruction */
				yasm_x86__ea_set_disponly(d.ea);
			    break;
			case YASM_INSN__OPERAND_IMM:
			    d.ea = yasm_x86__ea_new_imm(op->data.val,
				size_lookup[(info->operands[i] &
					     OPS_MASK)>>OPS_SHIFT]);
			    break;
		    }
		    break;
		case OPA_Imm:
		    if (op->type == YASM_INSN__OPERAND_IMM) {
			d.imm = op->data.val;
			d.im_len = size_lookup[(info->operands[i] &
						OPS_MASK)>>OPS_SHIFT];
		    } else
			yasm_internal_error(N_("invalid operand conversion"));
		    break;
		case OPA_SImm:
		    if (op->type == YASM_INSN__OPERAND_IMM) {
			d.imm = op->data.val;
			d.im_len = size_lookup[(info->operands[i] &
						OPS_MASK)>>OPS_SHIFT];
			d.im_sign = 1;
		    } else
			yasm_internal_error(N_("invalid operand conversion"));
		    break;
		case OPA_Spare:
		    if (op->type == YASM_INSN__OPERAND_SEGREG)
			d.spare = (unsigned char)(op->data.reg&7);
		    else if (op->type == YASM_INSN__OPERAND_REG) {
			if (yasm_x86__set_rex_from_reg(&d.rex, &d.spare,
				op->data.reg, yasm_x86_LTX_mode_bits,
				X86_REX_R)) {
			    yasm__error(lindex,
				N_("invalid combination of opcode and operands"));
			    return NULL;
			}
		    } else
			yasm_internal_error(N_("invalid operand conversion"));
		    break;
		case OPA_Op0Add:
		    if (op->type == YASM_INSN__OPERAND_REG) {
			unsigned char opadd;
			if (yasm_x86__set_rex_from_reg(&d.rex, &opadd,
				op->data.reg, yasm_x86_LTX_mode_bits,
				X86_REX_B)) {
			    yasm__error(lindex,
				N_("invalid combination of opcode and operands"));
			    return NULL;
			}
			d.op[0] += opadd;
		    } else
			yasm_internal_error(N_("invalid operand conversion"));
		    break;
		case OPA_Op1Add:
		    /* Op1Add is only used for FPU, so no need to do REX */
		    if (op->type == YASM_INSN__OPERAND_REG)
			d.op[1] += (unsigned char)(op->data.reg&7);
		    else
			yasm_internal_error(N_("invalid operand conversion"));
		    break;
		case OPA_SpareEA:
		    if (op->type == YASM_INSN__OPERAND_REG) {
			d.ea = yasm_x86__ea_new_reg(op->data.reg, &d.rex,
						    yasm_x86_LTX_mode_bits);
			if (!d.ea ||
			    yasm_x86__set_rex_from_reg(&d.rex, &d.spare,
				op->data.reg, yasm_x86_LTX_mode_bits,
				X86_REX_R)) {
			    yasm__error(lindex,
				N_("invalid combination of opcode and operands"));
			    if (d.ea)
				yasm_xfree(d.ea);
			    return NULL;
			}
		    } else
			yasm_internal_error(N_("invalid operand conversion"));
		    break;
		default:
		    yasm_internal_error(N_("unknown operand action"));
	    }

	    switch ((int)(info->operands[i] & OPAP_MASK)) {
		case OPAP_None:
		    break;
		case OPAP_ShiftOp:
		    d.shift_op = 1;
		    break;
		case OPAP_SImm8Avail:
		    d.signext_imm8_op = 1;
		    break;
		default:
		    yasm_internal_error(
			N_("unknown operand postponed action"));
	    }
	}
    }

    /* Create the bytecode and return it */
    return yasm_x86__bc_new_insn(&d);
}


#define YYCTYPE		char
#define YYCURSOR	id
#define YYLIMIT		id
#define YYMARKER	marker
#define YYFILL(n)

/*!re2c
  any = [\000-\377];
  A = [aA];
  B = [bB];
  C = [cC];
  D = [dD];
  E = [eE];
  F = [fF];
  G = [gG];
  H = [hH];
  I = [iI];
  J = [jJ];
  K = [kK];
  L = [lL];
  M = [mM];
  N = [nN];
  O = [oO];
  P = [pP];
  Q = [qQ];
  R = [rR];
  S = [sS];
  T = [tT];
  U = [uU];
  V = [vV];
  W = [wW];
  X = [xX];
  Y = [yY];
  Z = [zZ];
*/

void
yasm_x86__parse_cpu(const char *id, unsigned long lindex)
{
    /*const char *marker;*/

    /*!re2c
	/* The standard CPU names /set/ cpu_enabled. */
	"8086" {
	    cpu_enabled = CPU_Priv;
	    return;
	}
	("80" | I)? "186" {
	    cpu_enabled = CPU_186|CPU_Priv;
	    return;
	}
	("80" | I)? "286" {
	    cpu_enabled = CPU_186|CPU_286|CPU_Priv;
	    return;
	}
	("80" | I)? "386" {
	    cpu_enabled = CPU_186|CPU_286|CPU_386|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	("80" | I)? "486" {
	    cpu_enabled = CPU_186|CPU_286|CPU_386|CPU_486|CPU_FPU|CPU_SMM|
			  CPU_Prot|CPU_Priv;
	    return;
	}
	(I? "586") | (P E N T I U M) | (P "5") {
	    cpu_enabled = CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_FPU|
			  CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	(I? "686") | (P "6") | (P P R O) | (P E N T I U M P R O) {
	    cpu_enabled = CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|
			  CPU_FPU|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	(P "2") | (P E N T I U M "-"? ("2" | (I I))) {
	    cpu_enabled = CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|
			  CPU_FPU|CPU_MMX|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	(P "3") | (P E N T I U M "-"? ("3" | (I I I))) | (K A T M A I) {
	    cpu_enabled = CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|
			  CPU_P3|CPU_FPU|CPU_MMX|CPU_SSE|CPU_SMM|CPU_Prot|
			  CPU_Priv;
	    return;
	}
	(P "4") | (P E N T I U M "-"? ("4" | (I V))) | (W I L L I A M E T T E) {
	    cpu_enabled = CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|
			  CPU_P3|CPU_P4|CPU_FPU|CPU_MMX|CPU_SSE|CPU_SSE2|
			  CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	(I A "-"? "64") | (I T A N I U M) {
	    cpu_enabled = CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|
			  CPU_P3|CPU_P4|CPU_IA64|CPU_FPU|CPU_MMX|CPU_SSE|
			  CPU_SSE2|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	K "6" {
	    cpu_enabled = CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|
			  CPU_K6|CPU_FPU|CPU_MMX|CPU_3DNow|CPU_SMM|CPU_Prot|
			  CPU_Priv;
	    return;
	}
	(A T H L O N) | (K "7") {
	    cpu_enabled = CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|
			  CPU_K6|CPU_Athlon|CPU_FPU|CPU_MMX|CPU_SSE|CPU_3DNow|
			  CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	((S L E D G E)? (H A M M E R)) | (O P T E R O N) |
	(A T H L O N "-"? "64") {
	    cpu_enabled = CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|
			  CPU_K6|CPU_Athlon|CPU_Hammer|CPU_FPU|CPU_MMX|CPU_SSE|
			  CPU_3DNow|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}

	/* Features have "no" versions to disable them, and only set/reset the
	 * specific feature being changed.  All other bits are left alone.
	 */
	F P U		{ cpu_enabled |= CPU_FPU; return; }
	N O F P U	{ cpu_enabled &= ~CPU_FPU; return; }
	M M X		{ cpu_enabled |= CPU_MMX; return; }
	N O M M X	{ cpu_enabled &= ~CPU_MMX; return; }
	S S E		{ cpu_enabled |= CPU_SSE; return; }
	N O S S E	{ cpu_enabled &= ~CPU_SSE; return; }
	S S E "2"	{ cpu_enabled |= CPU_SSE2; return; }
	N O S S E "2"	{ cpu_enabled &= ~CPU_SSE2; return; }
	"3" D N O W	{ cpu_enabled |= CPU_3DNow; return; }
	N O "3" D N O W	{ cpu_enabled &= ~CPU_3DNow; return; }
	C Y R I X	{ cpu_enabled |= CPU_Cyrix; return; }
	N O C Y R I X	{ cpu_enabled &= ~CPU_Cyrix; return; }
	A M D		{ cpu_enabled |= CPU_AMD; return; }
	N O A M D	{ cpu_enabled &= ~CPU_AMD; return; }
	S M M		{ cpu_enabled |= CPU_SMM; return; }
	N O S M M	{ cpu_enabled &= ~CPU_SMM; return; }
	P R O T		{ cpu_enabled |= CPU_Prot; return; }
	N O P R O T	{ cpu_enabled &= ~CPU_Prot; return; }
	U N D O C	{ cpu_enabled |= CPU_Undoc; return; }
	N O U N D O C	{ cpu_enabled &= ~CPU_Undoc; return; }
	O B S		{ cpu_enabled |= CPU_Obs; return; }
	N O O B S	{ cpu_enabled &= ~CPU_Obs; return; }
	P R I V		{ cpu_enabled |= CPU_Priv; return; }
	N O P R I V	{ cpu_enabled &= ~CPU_Priv; return; }

	/* catchalls */
	[\001-\377]+	{
	    yasm__warning(YASM_WARN_GENERAL, lindex,
			  N_("unrecognized CPU identifier `%s'"), id);
	    return;
	}
	[\000]		{
	    yasm__warning(YASM_WARN_GENERAL, lindex,
			  N_("unrecognized CPU identifier `%s'"), id);
	    return;
	}
    */
}

yasm_arch_check_id_retval
yasm_x86__parse_check_id(unsigned long data[4], const char *id,
			 unsigned long lindex)
{
    const char *oid = id;
    /*const char *marker;*/
    /*!re2c
	/* target modifiers */
	N E A R		{
	    data[0] = X86_NEAR;
	    return YASM_ARCH_CHECK_ID_TARGETMOD;
	}
	S H O R T	{
	    data[0] = X86_SHORT;
	    return YASM_ARCH_CHECK_ID_TARGETMOD;
	}
	F A R		{
	    data[0] = X86_FAR;
	    return YASM_ARCH_CHECK_ID_TARGETMOD;
	}
	T O		{
	    data[0] = X86_TO;
	    return YASM_ARCH_CHECK_ID_TARGETMOD;
	}

	/* operand size overrides */
	O "16"	{
	    data[0] = X86_OPERSIZE;
	    data[1] = 16;
	    return YASM_ARCH_CHECK_ID_PREFIX;
	}
	O "32"	{
	    data[0] = X86_OPERSIZE;
	    data[1] = 32;
	    return YASM_ARCH_CHECK_ID_PREFIX;
	}
	O "64"	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a prefix in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_OPERSIZE;
	    data[1] = 64;
	    return YASM_ARCH_CHECK_ID_PREFIX;
	}
	/* address size overrides */
	A "16"	{
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex,
		    N_("Cannot override address size to 16 bits in 64-bit mode"));
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_ADDRSIZE;
	    data[1] = 16;
	    return YASM_ARCH_CHECK_ID_PREFIX;
	}
	A "32"	{
	    data[0] = X86_ADDRSIZE;
	    data[1] = 32;
	    return YASM_ARCH_CHECK_ID_PREFIX;
	}
	A "64"	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a prefix in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_ADDRSIZE;
	    data[1] = 64;
	    return YASM_ARCH_CHECK_ID_PREFIX;
	}

	/* instruction prefixes */
	L O C K		{
	    data[0] = X86_LOCKREP; 
	    data[1] = 0xF0;
	    return YASM_ARCH_CHECK_ID_PREFIX;
	}
	R E P N E	{
	    data[0] = X86_LOCKREP;
	    data[1] = 0xF2;
	    return YASM_ARCH_CHECK_ID_PREFIX;
	}
	R E P N Z	{
	    data[0] = X86_LOCKREP;
	    data[1] = 0xF2;
	    return YASM_ARCH_CHECK_ID_PREFIX;
	}
	R E P		{
	    data[0] = X86_LOCKREP;
	    data[1] = 0xF3;
	    return YASM_ARCH_CHECK_ID_PREFIX;
	}
	R E P E		{
	    data[0] = X86_LOCKREP;
	    data[1] = 0xF4;
	    return YASM_ARCH_CHECK_ID_PREFIX;
	}
	R E P Z		{
	    data[0] = X86_LOCKREP;
	    data[1] = 0xF4;
	    return YASM_ARCH_CHECK_ID_PREFIX;
	}

	/* control, debug, and test registers */
	C R [02-48]	{
	    if (yasm_x86_LTX_mode_bits != 64 && oid[2] == '8') {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_CRREG | (oid[2]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}
	D R [0-7]	{
	    data[0] = X86_DRREG | (oid[2]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}
	T R [0-7]	{
	    data[0] = X86_TRREG | (oid[2]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}

	/* floating point, MMX, and SSE/SSE2 registers */
	S T [0-7]	{
	    data[0] = X86_FPUREG | (oid[2]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}
	M M [0-7]	{
	    data[0] = X86_MMXREG | (oid[2]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}
	X M M [0-9]	{
	    if (yasm_x86_LTX_mode_bits != 64 &&
		(oid[3] == '8' || oid[3] == '9')) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_XMMREG | (oid[3]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}
	X M M "1" [0-5]	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG64 | (10+oid[4]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}

	/* integer registers */
	R A X	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG64 | 0;
	    return YASM_ARCH_CHECK_ID_REG;
	}
	R C X	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG64 | 1;
	    return YASM_ARCH_CHECK_ID_REG;
	}
	R D X	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG64 | 2;
	    return YASM_ARCH_CHECK_ID_REG;
	}
	R B X	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG64 | 3;
	    return YASM_ARCH_CHECK_ID_REG;
	}
	R S P	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG64 | 4;
	    return YASM_ARCH_CHECK_ID_REG;
	}
	R B P	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG64 | 5;
	    return YASM_ARCH_CHECK_ID_REG;
	}
	R S I	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG64 | 6;
	    return YASM_ARCH_CHECK_ID_REG;
	}
	R D I	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG64 | 7;
	    return YASM_ARCH_CHECK_ID_REG;
	}
	R [8-9]	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG64 | (oid[1]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}
	R "1" [0-5] {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG64 | (10+oid[2]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}

	E A X	{ data[0] = X86_REG32 | 0; return YASM_ARCH_CHECK_ID_REG; }
	E C X	{ data[0] = X86_REG32 | 1; return YASM_ARCH_CHECK_ID_REG; }
	E D X	{ data[0] = X86_REG32 | 2; return YASM_ARCH_CHECK_ID_REG; }
	E B X	{ data[0] = X86_REG32 | 3; return YASM_ARCH_CHECK_ID_REG; }
	E S P	{ data[0] = X86_REG32 | 4; return YASM_ARCH_CHECK_ID_REG; }
	E B P	{ data[0] = X86_REG32 | 5; return YASM_ARCH_CHECK_ID_REG; }
	E S I	{ data[0] = X86_REG32 | 6; return YASM_ARCH_CHECK_ID_REG; }
	E D I	{ data[0] = X86_REG32 | 7; return YASM_ARCH_CHECK_ID_REG; }
	R [8-9]	D {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG32 | (oid[1]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}
	R "1" [0-5] D {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG32 | (10+oid[2]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}

	A X	{ data[0] = X86_REG16 | 0; return YASM_ARCH_CHECK_ID_REG; }
	C X	{ data[0] = X86_REG16 | 1; return YASM_ARCH_CHECK_ID_REG; }
	D X	{ data[0] = X86_REG16 | 2; return YASM_ARCH_CHECK_ID_REG; }
	B X	{ data[0] = X86_REG16 | 3; return YASM_ARCH_CHECK_ID_REG; }
	S P	{ data[0] = X86_REG16 | 4; return YASM_ARCH_CHECK_ID_REG; }
	B P	{ data[0] = X86_REG16 | 5; return YASM_ARCH_CHECK_ID_REG; }
	S I	{ data[0] = X86_REG16 | 6; return YASM_ARCH_CHECK_ID_REG; }
	D I	{ data[0] = X86_REG16 | 7; return YASM_ARCH_CHECK_ID_REG; }
	R [8-9]	W {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG16 | (oid[1]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}
	R "1" [0-5] W {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG16 | (10+oid[2]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}

	A L	{ data[0] = X86_REG8 | 0; return YASM_ARCH_CHECK_ID_REG; }
	C L	{ data[0] = X86_REG8 | 1; return YASM_ARCH_CHECK_ID_REG; }
	D L	{ data[0] = X86_REG8 | 2; return YASM_ARCH_CHECK_ID_REG; }
	B L	{ data[0] = X86_REG8 | 3; return YASM_ARCH_CHECK_ID_REG; }
	A H	{ data[0] = X86_REG8 | 4; return YASM_ARCH_CHECK_ID_REG; }
	C H	{ data[0] = X86_REG8 | 5; return YASM_ARCH_CHECK_ID_REG; }
	D H	{ data[0] = X86_REG8 | 6; return YASM_ARCH_CHECK_ID_REG; }
	B H	{ data[0] = X86_REG8 | 7; return YASM_ARCH_CHECK_ID_REG; }
	S P L	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG8X | 4;
	    return YASM_ARCH_CHECK_ID_REG;
	}
	B P L	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG8X | 5;
	    return YASM_ARCH_CHECK_ID_REG;
	}
	S I L	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG8X | 6;
	    return YASM_ARCH_CHECK_ID_REG;
	}
	D I L	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG8X | 7;
	    return YASM_ARCH_CHECK_ID_REG;
	}
	R [8-9]	B {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG8 | (oid[1]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}
	R "1" [0-5] B {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_REG8 | (10+oid[2]-'0');
	    return YASM_ARCH_CHECK_ID_REG;
	}

	/* segment registers */
	E S	{
	    if (yasm_x86_LTX_mode_bits == 64)
		yasm__warning(YASM_WARN_GENERAL, lindex,
		    N_("`%s' segment register ignored in 64-bit mode"), oid);
	    data[0] = 0x2600;
	    return YASM_ARCH_CHECK_ID_SEGREG;
	}
	C S	{ data[0] = 0x2e01; return YASM_ARCH_CHECK_ID_SEGREG; }
	S S	{
	    if (yasm_x86_LTX_mode_bits == 64)
		yasm__warning(YASM_WARN_GENERAL, lindex,
		    N_("`%s' segment register ignored in 64-bit mode"), oid);
	    data[0] = 0x3602;
	    return YASM_ARCH_CHECK_ID_SEGREG;
	}
	D S	{
	    if (yasm_x86_LTX_mode_bits == 64)
		yasm__warning(YASM_WARN_GENERAL, lindex,
		    N_("`%s' segment register ignored in 64-bit mode"), oid);
	    data[0] = 0x3e03;
	    return YASM_ARCH_CHECK_ID_SEGREG;
	}
	F S	{ data[0] = 0x6404; return YASM_ARCH_CHECK_ID_SEGREG; }
	G S	{ data[0] = 0x6505; return YASM_ARCH_CHECK_ID_SEGREG; }

	/* RIP for 64-bit mode IP-relative offsets */
	R I P	{
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    data[0] = X86_RIP;
	    return YASM_ARCH_CHECK_ID_REG;
	}

	/* instructions */

	/* Move */
	M O V { RET_INSN(mov, 0, CPU_Any); }
	/* Move with sign/zero extend */
	M O V S X { RET_INSN(movszx, 0xBE, CPU_386); }
	M O V S X D {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is an instruction in 64-bit mode"),
			      oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    RET_INSN(movsxd, 0, CPU_Hammer|CPU_64);
	}
	M O V Z X { RET_INSN(movszx, 0xB6, CPU_386); }
	/* Push instructions */
	P U S H { RET_INSN(push, 0, CPU_Any); }
	P U S H A {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(onebyte, 0x0060, CPU_186);
	}
	P U S H A D {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(onebyte, 0x2060, CPU_386);
	}
	P U S H A W {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(onebyte, 0x1060, CPU_186);
	}
	/* Pop instructions */
	P O P { RET_INSN(pop, 0, CPU_Any); }
	P O P A {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(onebyte, 0x0061, CPU_186);
	}
	P O P A D {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(onebyte, 0x2061, CPU_386);
	}
	P O P A W {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(onebyte, 0x1061, CPU_186);
	}
	/* Exchange */
	X C H G { RET_INSN(xchg, 0, CPU_Any); }
	/* In/out from ports */
	I N { RET_INSN(in, 0, CPU_Any); }
	O U T { RET_INSN(out, 0, CPU_Any); }
	/* Load effective address */
	L E A { RET_INSN(lea, 0, CPU_Any); }
	/* Load segment registers from memory */
	L D S {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(ldes, 0xC5, CPU_Any);
	}
	L E S {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(ldes, 0xC4, CPU_Any);
	}
	L F S { RET_INSN(lfgss, 0xB4, CPU_386); }
	L G S { RET_INSN(lfgss, 0xB5, CPU_386); }
	L S S { RET_INSN(lfgss, 0xB6, CPU_386); }
	/* Flags register instructions */
	C L C { RET_INSN(onebyte, 0x00F8, CPU_Any); }
	C L D { RET_INSN(onebyte, 0x00FC, CPU_Any); }
	C L I { RET_INSN(onebyte, 0x00FA, CPU_Any); }
	C L T S { RET_INSN(twobyte, 0x0F06, CPU_286|CPU_Priv); }
	C M C { RET_INSN(onebyte, 0x00F5, CPU_Any); }
	L A H F {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
 	    RET_INSN(onebyte, 0x009F, CPU_Any);
	}
	S A H F {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(onebyte, 0x009E, CPU_Any);
	}
	P U S H F { RET_INSN(onebyte, 0x009C, CPU_Any); }
	P U S H F D { RET_INSN(onebyte, 0x209C, CPU_386); }
	P U S H F W { RET_INSN(onebyte, 0x109C, CPU_Any); }
	P U S H F Q {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is an instruction in 64-bit mode"),
			      oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    RET_INSN(onebyte, 0x409C, CPU_Hammer|CPU_64);
	}
	P O P F { RET_INSN(onebyte, 0x009D, CPU_Any); }
	P O P F D { RET_INSN(onebyte, 0x209D, CPU_386); }
	P O P F W { RET_INSN(onebyte, 0x109D, CPU_Any); }
	P O P F Q {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is an instruction in 64-bit mode"),
			      oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    RET_INSN(onebyte, 0x409D, CPU_Hammer|CPU_64);
	}
	S T C { RET_INSN(onebyte, 0x00F9, CPU_Any); }
	S T D { RET_INSN(onebyte, 0x00FD, CPU_Any); }
	S T I { RET_INSN(onebyte, 0x00FB, CPU_Any); }
	/* Arithmetic */
	A D D { RET_INSN(arith, 0x0000, CPU_Any); }
	I N C { RET_INSN(incdec, 0x0040, CPU_Any); }
	S U B { RET_INSN(arith, 0x0528, CPU_Any); }
	D E C { RET_INSN(incdec, 0x0148, CPU_Any); }
	S B B { RET_INSN(arith, 0x0318, CPU_Any); }
	C M P { RET_INSN(arith, 0x0738, CPU_Any); }
	T E S T { RET_INSN(test, 0, CPU_Any); }
	A N D { RET_INSN(arith, 0x0420, CPU_Any); }
	O R { RET_INSN(arith, 0x0108, CPU_Any); }
	X O R { RET_INSN(arith, 0x0630, CPU_Any); }
	A D C { RET_INSN(arith, 0x0210, CPU_Any); }
	N E G { RET_INSN(f6, 0x03, CPU_Any); }
	N O T { RET_INSN(f6, 0x02, CPU_Any); }
	A A A {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(onebyte, 0x0037, CPU_Any);
	}
	A A S {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(onebyte, 0x003F, CPU_Any);
	}
	D A A {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(onebyte, 0x0027, CPU_Any);
	}
	D A S {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(onebyte, 0x002F, CPU_Any);
	}
	A A D {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(aadm, 0x01, CPU_Any);
	}
	A A M {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(aadm, 0x00, CPU_Any);
	}
	/* Conversion instructions */
	C B W { RET_INSN(onebyte, 0x1098, CPU_Any); }
	C W D E { RET_INSN(onebyte, 0x2098, CPU_386); }
	C D Q E {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is an instruction in 64-bit mode"),
			      oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    RET_INSN(onebyte, 0x4098, CPU_Hammer|CPU_64);
	}
	C W D { RET_INSN(onebyte, 0x1099, CPU_Any); }
	C D Q { RET_INSN(onebyte, 0x2099, CPU_386); }
	C D O {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is an instruction in 64-bit mode"),
			      oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    RET_INSN(onebyte, 0x4099, CPU_Hammer|CPU_64);
	}
	/* Multiplication and division */
	M U L { RET_INSN(f6, 0x04, CPU_Any); }
	I M U L { RET_INSN(imul, 0, CPU_Any); }
	D I V { RET_INSN(f6, 0x06, CPU_Any); }
	I D I V { RET_INSN(f6, 0x07, CPU_Any); }
	/* Shifts */
	R O L { RET_INSN(shift, 0x00, CPU_Any); }
	R O R { RET_INSN(shift, 0x01, CPU_Any); }
	R C L { RET_INSN(shift, 0x02, CPU_Any); }
	R C R { RET_INSN(shift, 0x03, CPU_Any); }
	S A L { RET_INSN(shift, 0x04, CPU_Any); }
	S H L { RET_INSN(shift, 0x04, CPU_Any); }
	S H R { RET_INSN(shift, 0x05, CPU_Any); }
	S A R { RET_INSN(shift, 0x07, CPU_Any); }
	S H L D { RET_INSN(shlrd, 0xA4, CPU_386); }
	S H R D { RET_INSN(shlrd, 0xAC, CPU_386); }
	/* Control transfer instructions (unconditional) */
	C A L L { RET_INSN(call, 0, CPU_Any); }
	J M P { RET_INSN(jmp, 0, CPU_Any); }
	R E T { RET_INSN(retnf, 0xC2, CPU_Any); }
	R E T N { RET_INSN(retnf, 0xC2, CPU_Any); }
	R E T F { RET_INSN(retnf, 0xCA, CPU_Any); }
	E N T E R { RET_INSN(enter, 0, CPU_186); }
	L E A V E { RET_INSN(onebyte, 0x00C9, CPU_186); }
	/* Conditional jumps */
	J O { RET_INSN(jcc, 0x00, CPU_Any); }
	J N O { RET_INSN(jcc, 0x01, CPU_Any); }
	J B { RET_INSN(jcc, 0x02, CPU_Any); }
	J C { RET_INSN(jcc, 0x02, CPU_Any); }
	J N A E { RET_INSN(jcc, 0x02, CPU_Any); }
	J N B { RET_INSN(jcc, 0x03, CPU_Any); }
	J N C { RET_INSN(jcc, 0x03, CPU_Any); }
	J A E { RET_INSN(jcc, 0x03, CPU_Any); }
	J E { RET_INSN(jcc, 0x04, CPU_Any); }
	J Z { RET_INSN(jcc, 0x04, CPU_Any); }
	J N E { RET_INSN(jcc, 0x05, CPU_Any); }
	J N Z { RET_INSN(jcc, 0x05, CPU_Any); }
	J B E { RET_INSN(jcc, 0x06, CPU_Any); }
	J N A { RET_INSN(jcc, 0x06, CPU_Any); }
	J N B E { RET_INSN(jcc, 0x07, CPU_Any); }
	J A { RET_INSN(jcc, 0x07, CPU_Any); }
	J S { RET_INSN(jcc, 0x08, CPU_Any); }
	J N S { RET_INSN(jcc, 0x09, CPU_Any); }
	J P { RET_INSN(jcc, 0x0A, CPU_Any); }
	J P E { RET_INSN(jcc, 0x0A, CPU_Any); }
	J N P { RET_INSN(jcc, 0x0B, CPU_Any); }
	J P O { RET_INSN(jcc, 0x0B, CPU_Any); }
	J L { RET_INSN(jcc, 0x0C, CPU_Any); }
	J N G E { RET_INSN(jcc, 0x0C, CPU_Any); }
	J N L { RET_INSN(jcc, 0x0D, CPU_Any); }
	J G E { RET_INSN(jcc, 0x0D, CPU_Any); }
	J L E { RET_INSN(jcc, 0x0E, CPU_Any); }
	J N G { RET_INSN(jcc, 0x0E, CPU_Any); }
	J N L E { RET_INSN(jcc, 0x0F, CPU_Any); }
	J G { RET_INSN(jcc, 0x0F, CPU_Any); }
	J C X Z { RET_INSN(jcxz, 16, CPU_Any); }
	J E C X Z { RET_INSN(jcxz, 32, CPU_386); }
	J R C X Z {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is an instruction in 64-bit mode"),
			      oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    RET_INSN(jcxz, 64, CPU_Hammer|CPU_64);
	}
	/* Loop instructions */
	L O O P { RET_INSN(loop, 0x02, CPU_Any); }
	L O O P Z { RET_INSN(loop, 0x01, CPU_Any); }
	L O O P E { RET_INSN(loop, 0x01, CPU_Any); }
	L O O P N Z { RET_INSN(loop, 0x00, CPU_Any); }
	L O O P N E { RET_INSN(loop, 0x00, CPU_Any); }
	/* Set byte on flag instructions */
	S E T O { RET_INSN(setcc, 0x00, CPU_386); }
	S E T N O { RET_INSN(setcc, 0x01, CPU_386); }
	S E T B { RET_INSN(setcc, 0x02, CPU_386); }
	S E T C { RET_INSN(setcc, 0x02, CPU_386); }
	S E T N A E { RET_INSN(setcc, 0x02, CPU_386); }
	S E T N B { RET_INSN(setcc, 0x03, CPU_386); }
	S E T N C { RET_INSN(setcc, 0x03, CPU_386); }
	S E T A E { RET_INSN(setcc, 0x03, CPU_386); }
	S E T E { RET_INSN(setcc, 0x04, CPU_386); }
	S E T Z { RET_INSN(setcc, 0x04, CPU_386); }
	S E T N E { RET_INSN(setcc, 0x05, CPU_386); }
	S E T N Z { RET_INSN(setcc, 0x05, CPU_386); }
	S E T B E { RET_INSN(setcc, 0x06, CPU_386); }
	S E T N A { RET_INSN(setcc, 0x06, CPU_386); }
	S E T N B E { RET_INSN(setcc, 0x07, CPU_386); }
	S E T A { RET_INSN(setcc, 0x07, CPU_386); }
	S E T S { RET_INSN(setcc, 0x08, CPU_386); }
	S E T N S { RET_INSN(setcc, 0x09, CPU_386); }
	S E T P { RET_INSN(setcc, 0x0A, CPU_386); }
	S E T P E { RET_INSN(setcc, 0x0A, CPU_386); }
	S E T N P { RET_INSN(setcc, 0x0B, CPU_386); }
	S E T P O { RET_INSN(setcc, 0x0B, CPU_386); }
	S E T L { RET_INSN(setcc, 0x0C, CPU_386); }
	S E T N G E { RET_INSN(setcc, 0x0C, CPU_386); }
	S E T N L { RET_INSN(setcc, 0x0D, CPU_386); }
	S E T G E { RET_INSN(setcc, 0x0D, CPU_386); }
	S E T L E { RET_INSN(setcc, 0x0E, CPU_386); }
	S E T N G { RET_INSN(setcc, 0x0E, CPU_386); }
	S E T N L E { RET_INSN(setcc, 0x0F, CPU_386); }
	S E T G { RET_INSN(setcc, 0x0F, CPU_386); }
	/* String instructions. */
	C M P S B { RET_INSN(onebyte, 0x00A6, CPU_Any); }
	C M P S W { RET_INSN(onebyte, 0x10A7, CPU_Any); }
	C M P S D { RET_INSN(cmpsd, 0, CPU_Any); }
	C M P S Q {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is an instruction in 64-bit mode"),
			      oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    RET_INSN(onebyte, 0x40A7, CPU_Hammer|CPU_64);
	}
	I N S B { RET_INSN(onebyte, 0x006C, CPU_Any); }
	I N S W { RET_INSN(onebyte, 0x106D, CPU_Any); }
	I N S D { RET_INSN(onebyte, 0x206D, CPU_386); }
	O U T S B { RET_INSN(onebyte, 0x006E, CPU_Any); }
	O U T S W { RET_INSN(onebyte, 0x106F, CPU_Any); }
	O U T S D { RET_INSN(onebyte, 0x206F, CPU_386); }
	L O D S B { RET_INSN(onebyte, 0x00AC, CPU_Any); }
	L O D S W { RET_INSN(onebyte, 0x10AD, CPU_Any); }
	L O D S D { RET_INSN(onebyte, 0x20AD, CPU_386); }
	L O D S Q {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is an instruction in 64-bit mode"),
			      oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    RET_INSN(onebyte, 0x40AD, CPU_Hammer|CPU_64);
	}
	M O V S B { RET_INSN(onebyte, 0x00A4, CPU_Any); }
	M O V S W { RET_INSN(onebyte, 0x10A5, CPU_Any); }
	M O V S D { RET_INSN(movsd, 0, CPU_Any); }
	M O V S Q {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is an instruction in 64-bit mode"),
			      oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    RET_INSN(onebyte, 0x40A5, CPU_Any);
	}
	S C A S B { RET_INSN(onebyte, 0x00AE, CPU_Any); }
	S C A S W { RET_INSN(onebyte, 0x10AF, CPU_Any); }
	S C A S D { RET_INSN(onebyte, 0x20AF, CPU_386); }
	S C A S Q {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is an instruction in 64-bit mode"),
			      oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    RET_INSN(onebyte, 0x40AF, CPU_Hammer|CPU_64);
	}
	S T O S B { RET_INSN(onebyte, 0x00AA, CPU_Any); }
	S T O S W { RET_INSN(onebyte, 0x10AB, CPU_Any); }
	S T O S D { RET_INSN(onebyte, 0x20AB, CPU_386); }
	S T O S Q {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is an instruction in 64-bit mode"),
			      oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    RET_INSN(onebyte, 0x40AB, CPU_Hammer|CPU_64);
	}
	X L A T B? { RET_INSN(onebyte, 0x00D7, CPU_Any); }
	/* Bit manipulation */
	B S F { RET_INSN(bsfr, 0xBC, CPU_386); }
	B S R { RET_INSN(bsfr, 0xBD, CPU_386); }
	B T { RET_INSN(bittest, 0x04A3, CPU_386); }
	B T C { RET_INSN(bittest, 0x07BB, CPU_386); }
	B T R { RET_INSN(bittest, 0x06B3, CPU_386); }
	B T S { RET_INSN(bittest, 0x05AB, CPU_386); }
	/* Interrupts and operating system instructions */
	I N T { RET_INSN(int, 0, CPU_Any); }
	I N T "3" { RET_INSN(onebyte, 0x00CC, CPU_Any); }
	I N T "03" { RET_INSN(onebyte, 0x00CC, CPU_Any); }
	I N T O {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(onebyte, 0x00CE, CPU_Any);
	}
	I R E T { RET_INSN(onebyte, 0x00CF, CPU_Any); }
	I R E T W { RET_INSN(onebyte, 0x10CF, CPU_Any); }
	I R E T D { RET_INSN(onebyte, 0x20CF, CPU_386); }
	I R E T Q {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is an instruction in 64-bit mode"),
			      oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    RET_INSN(onebyte, 0x40CF, CPU_Hammer|CPU_64);
	}
	R S M { RET_INSN(twobyte, 0x0FAA, CPU_586|CPU_SMM); }
	B O U N D {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(bound, 0, CPU_186);
	}
	H L T { RET_INSN(onebyte, 0x00F4, CPU_Priv); }
	N O P { RET_INSN(onebyte, 0x0090, CPU_Any); }
	/* Protection control */
	A R P L {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(arpl, 0, CPU_286|CPU_Prot);
	}
	L A R { RET_INSN(bsfr, 0x02, CPU_286|CPU_Prot); }
	L G D T { RET_INSN(twobytemem, 0x020F01, CPU_286|CPU_Priv); }
	L I D T { RET_INSN(twobytemem, 0x030F01, CPU_286|CPU_Priv); }
	L L D T { RET_INSN(prot286, 0x0200, CPU_286|CPU_Prot|CPU_Priv); }
	L M S W { RET_INSN(prot286, 0x0601, CPU_286|CPU_Priv); }
	L S L { RET_INSN(bsfr, 0x03, CPU_286|CPU_Prot); }
	L T R { RET_INSN(prot286, 0x0300, CPU_286|CPU_Prot|CPU_Priv); }
	S G D T { RET_INSN(twobytemem, 0x000F01, CPU_286|CPU_Priv); }
	S I D T { RET_INSN(twobytemem, 0x010F01, CPU_286|CPU_Priv); }
	S L D T { RET_INSN(sldtmsw, 0x0000, CPU_286); }
	S M S W { RET_INSN(sldtmsw, 0x0401, CPU_286); }
	S T R { RET_INSN(str, 0, CPU_286|CPU_Prot); }
	V E R R { RET_INSN(prot286, 0x0400, CPU_286|CPU_Prot); }
	V E R W { RET_INSN(prot286, 0x0500, CPU_286|CPU_Prot); }
	/* Floating point instructions */
	F L D { RET_INSN(fldstp, 0x0500C0, CPU_FPU); }
	F I L D { RET_INSN(fildstp, 0x0500, CPU_FPU); }
	F B L D { RET_INSN(fbldstp, 0x04, CPU_FPU); }
	F S T { RET_INSN(fst, 0, CPU_FPU); }
	F I S T { RET_INSN(fiarith, 0x02DB, CPU_FPU); }
	F S T P { RET_INSN(fldstp, 0x0703D8, CPU_FPU); }
	F I S T P { RET_INSN(fildstp, 0x0703, CPU_FPU); }
	F B S T P { RET_INSN(fbldstp, 0x06, CPU_FPU); }
	F X C H { RET_INSN(fxch, 0, CPU_FPU); }
	F C O M { RET_INSN(fcom, 0x02D0, CPU_FPU); }
	F I C O M { RET_INSN(fiarith, 0x02DA, CPU_FPU); }
	F C O M P { RET_INSN(fcom, 0x03D8, CPU_FPU); }
	F I C O M P { RET_INSN(fiarith, 0x03DA, CPU_FPU); }
	F C O M P P { RET_INSN(twobyte, 0xDED9, CPU_FPU); }
	F U C O M { RET_INSN(fcom2, 0xDDE0, CPU_286|CPU_FPU); }
	F U C O M P { RET_INSN(fcom2, 0xDDE8, CPU_286|CPU_FPU); }
	F U C O M P P { RET_INSN(twobyte, 0xDAE9, CPU_286|CPU_FPU); }
	F T S T { RET_INSN(twobyte, 0xD9E4, CPU_FPU); }
	F X A M { RET_INSN(twobyte, 0xD9E5, CPU_FPU); }
	F L D "1" { RET_INSN(twobyte, 0xD9E8, CPU_FPU); }
	F L D L "2" T { RET_INSN(twobyte, 0xD9E9, CPU_FPU); }
	F L D L "2" E { RET_INSN(twobyte, 0xD9EA, CPU_FPU); }
	F L D P I { RET_INSN(twobyte, 0xD9EB, CPU_FPU); }
	F L D L G "2" { RET_INSN(twobyte, 0xD9EC, CPU_FPU); }
	F L D L N "2" { RET_INSN(twobyte, 0xD9ED, CPU_FPU); }
	F L D Z { RET_INSN(twobyte, 0xD9EE, CPU_FPU); }
	F A D D { RET_INSN(farith, 0x00C0C0, CPU_FPU); }
	F A D D P { RET_INSN(farithp, 0xC0, CPU_FPU); }
	F I A D D { RET_INSN(fiarith, 0x00DA, CPU_FPU); }
	F S U B { RET_INSN(farith, 0x04E0E8, CPU_FPU); }
	F I S U B { RET_INSN(fiarith, 0x04DA, CPU_FPU); }
	F S U B P { RET_INSN(farithp, 0xE8, CPU_FPU); }
	F S U B R { RET_INSN(farith, 0x05E8E0, CPU_FPU); }
	F I S U B R { RET_INSN(fiarith, 0x05DA, CPU_FPU); }
	F S U B R P { RET_INSN(farithp, 0xE0, CPU_FPU); }
	F M U L { RET_INSN(farith, 0x01C8C8, CPU_FPU); }
	F I M U L { RET_INSN(fiarith, 0x01DA, CPU_FPU); }
	F M U L P { RET_INSN(farithp, 0xC8, CPU_FPU); }
	F D I V { RET_INSN(farith, 0x06F0F8, CPU_FPU); }
	F I D I V { RET_INSN(fiarith, 0x06DA, CPU_FPU); }
	F D I V P { RET_INSN(farithp, 0xF8, CPU_FPU); }
	F D I V R { RET_INSN(farith, 0x07F8F0, CPU_FPU); }
	F I D I V R { RET_INSN(fiarith, 0x07DA, CPU_FPU); }
	F D I V R P { RET_INSN(farithp, 0xF0, CPU_FPU); }
	F "2" X M "1" { RET_INSN(twobyte, 0xD9F0, CPU_FPU); }
	F Y L "2" X { RET_INSN(twobyte, 0xD9F1, CPU_FPU); }
	F P T A N { RET_INSN(twobyte, 0xD9F2, CPU_FPU); }
	F P A T A N { RET_INSN(twobyte, 0xD9F3, CPU_FPU); }
	F X T R A C T { RET_INSN(twobyte, 0xD9F4, CPU_FPU); }
	F P R E M "1" { RET_INSN(twobyte, 0xD9F5, CPU_286|CPU_FPU); }
	F D E C S T P { RET_INSN(twobyte, 0xD9F6, CPU_FPU); }
	F I N C S T P { RET_INSN(twobyte, 0xD9F7, CPU_FPU); }
	F P R E M { RET_INSN(twobyte, 0xD9F8, CPU_FPU); }
	F Y L "2" X P "1" { RET_INSN(twobyte, 0xD9F9, CPU_FPU); }
	F S Q R T { RET_INSN(twobyte, 0xD9FA, CPU_FPU); }
	F S I N C O S { RET_INSN(twobyte, 0xD9FB, CPU_286|CPU_FPU); }
	F R N D I N T { RET_INSN(twobyte, 0xD9FC, CPU_FPU); }
	F S C A L E { RET_INSN(twobyte, 0xD9FD, CPU_FPU); }
	F S I N { RET_INSN(twobyte, 0xD9FE, CPU_286|CPU_FPU); }
	F C O S { RET_INSN(twobyte, 0xD9FF, CPU_286|CPU_FPU); }
	F C H S { RET_INSN(twobyte, 0xD9E0, CPU_FPU); }
	F A B S { RET_INSN(twobyte, 0xD9E1, CPU_FPU); }
	F N I N I T { RET_INSN(twobyte, 0xDBE3, CPU_FPU); }
	F I N I T { RET_INSN(threebyte, 0x98DBE3UL, CPU_FPU); }
	F L D C W { RET_INSN(fldnstcw, 0x05, CPU_FPU); }
	F N S T C W { RET_INSN(fldnstcw, 0x07, CPU_FPU); }
	F S T C W { RET_INSN(fstcw, 0, CPU_FPU); }
	F N S T S W { RET_INSN(fnstsw, 0, CPU_FPU); }
	F S T S W { RET_INSN(fstsw, 0, CPU_FPU); }
	F N C L E X { RET_INSN(twobyte, 0xDBE2, CPU_FPU); }
	F C L E X { RET_INSN(threebyte, 0x98DBE2UL, CPU_FPU); }
	F N S T E N V { RET_INSN(onebytemem, 0x06D9, CPU_FPU); }
	F S T E N V { RET_INSN(twobytemem, 0x069BD9, CPU_FPU); }
	F L D E N V { RET_INSN(onebytemem, 0x04D9, CPU_FPU); }
	F N S A V E { RET_INSN(onebytemem, 0x06DD, CPU_FPU); }
	F S A V E { RET_INSN(twobytemem, 0x069BDD, CPU_FPU); }
	F R S T O R { RET_INSN(onebytemem, 0x04DD, CPU_FPU); }
	F F R E E { RET_INSN(ffree, 0xDD, CPU_FPU); }
	F F R E E P { RET_INSN(ffree, 0xDF, CPU_686|CPU_FPU|CPU_Undoc); }
	F N O P { RET_INSN(twobyte, 0xD9D0, CPU_FPU); }
	F W A I T { RET_INSN(onebyte, 0x009B, CPU_FPU); }
	/* Prefixes (should the others be here too? should wait be a prefix? */
	W A I T { RET_INSN(onebyte, 0x009B, CPU_Any); }
	/* 486 extensions */
	B S W A P { RET_INSN(bswap, 0, CPU_486); }
	X A D D { RET_INSN(cmpxchgxadd, 0xC0, CPU_486); }
	C M P X C H G { RET_INSN(cmpxchgxadd, 0xB0, CPU_486); }
	C M P X C H G "486" { RET_INSN(cmpxchgxadd, 0xA6, CPU_486|CPU_Undoc); }
	I N V D { RET_INSN(twobyte, 0x0F08, CPU_486|CPU_Priv); }
	W B I N V D { RET_INSN(twobyte, 0x0F09, CPU_486|CPU_Priv); }
	I N V L P G { RET_INSN(twobytemem, 0x070F01, CPU_486|CPU_Priv); }
	/* 586+ and late 486 extensions */
	C P U I D { RET_INSN(twobyte, 0x0FA2, CPU_486); }
	/* Pentium extensions */
	W R M S R { RET_INSN(twobyte, 0x0F30, CPU_586|CPU_Priv); }
	R D T S C { RET_INSN(twobyte, 0x0F31, CPU_586); }
	R D M S R { RET_INSN(twobyte, 0x0F32, CPU_586|CPU_Priv); }
	C M P X C H G "8" B { RET_INSN(cmpxchg8b, 0, CPU_586); }
	/* Pentium II/Pentium Pro extensions */
	S Y S E N T E R {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(twobyte, 0x0F34, CPU_686);
	}
	S Y S E X I T {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(twobyte, 0x0F35, CPU_686|CPU_Priv);
	}
	F X S A V E { RET_INSN(twobytemem, 0x000FAE, CPU_686|CPU_FPU); }
	F X R S T O R { RET_INSN(twobytemem, 0x010FAE, CPU_686|CPU_FPU); }
	R D P M C { RET_INSN(twobyte, 0x0F33, CPU_686); }
	U D "2" { RET_INSN(twobyte, 0x0F0B, CPU_286); }
	U D "1" { RET_INSN(twobyte, 0x0FB9, CPU_286|CPU_Undoc); }
	C M O V O { RET_INSN(cmovcc, 0x00, CPU_686); }
	C M O V N O { RET_INSN(cmovcc, 0x01, CPU_686); }
	C M O V B { RET_INSN(cmovcc, 0x02, CPU_686); }
	C M O V C { RET_INSN(cmovcc, 0x02, CPU_686); }
	C M O V N A E { RET_INSN(cmovcc, 0x02, CPU_686); }
	C M O V N B { RET_INSN(cmovcc, 0x03, CPU_686); }
	C M O V N C { RET_INSN(cmovcc, 0x03, CPU_686); }
	C M O V A E { RET_INSN(cmovcc, 0x03, CPU_686); }
	C M O V E { RET_INSN(cmovcc, 0x04, CPU_686); }
	C M O V Z { RET_INSN(cmovcc, 0x04, CPU_686); }
	C M O V N E { RET_INSN(cmovcc, 0x05, CPU_686); }
	C M O V N Z { RET_INSN(cmovcc, 0x05, CPU_686); }
	C M O V B E { RET_INSN(cmovcc, 0x06, CPU_686); }
	C M O V N A { RET_INSN(cmovcc, 0x06, CPU_686); }
	C M O V N B E { RET_INSN(cmovcc, 0x07, CPU_686); }
	C M O V A { RET_INSN(cmovcc, 0x07, CPU_686); }
	C M O V S { RET_INSN(cmovcc, 0x08, CPU_686); }
	C M O V N S { RET_INSN(cmovcc, 0x09, CPU_686); }
	C M O V P { RET_INSN(cmovcc, 0x0A, CPU_686); }
	C M O V P E { RET_INSN(cmovcc, 0x0A, CPU_686); }
	C M O V N P { RET_INSN(cmovcc, 0x0B, CPU_686); }
	C M O V P O { RET_INSN(cmovcc, 0x0B, CPU_686); }
	C M O V L { RET_INSN(cmovcc, 0x0C, CPU_686); }
	C M O V N G E { RET_INSN(cmovcc, 0x0C, CPU_686); }
	C M O V N L { RET_INSN(cmovcc, 0x0D, CPU_686); }
	C M O V G E { RET_INSN(cmovcc, 0x0D, CPU_686); }
	C M O V L E { RET_INSN(cmovcc, 0x0E, CPU_686); }
	C M O V N G { RET_INSN(cmovcc, 0x0E, CPU_686); }
	C M O V N L E { RET_INSN(cmovcc, 0x0F, CPU_686); }
	C M O V G { RET_INSN(cmovcc, 0x0F, CPU_686); }
	F C M O V B { RET_INSN(fcmovcc, 0xDAC0, CPU_686|CPU_FPU); }
	F C M O V E { RET_INSN(fcmovcc, 0xDAC8, CPU_686|CPU_FPU); }
	F C M O V B E { RET_INSN(fcmovcc, 0xDAD0, CPU_686|CPU_FPU); }
	F C M O V U { RET_INSN(fcmovcc, 0xDAD8, CPU_686|CPU_FPU); }
	F C M O V N B { RET_INSN(fcmovcc, 0xDBC0, CPU_686|CPU_FPU); }
	F C M O V N E { RET_INSN(fcmovcc, 0xDBC8, CPU_686|CPU_FPU); }
	F C M O V N B E { RET_INSN(fcmovcc, 0xDBD0, CPU_686|CPU_FPU); }
	F C M O V U { RET_INSN(fcmovcc, 0xDBD8, CPU_686|CPU_FPU); }
	F C O M I { RET_INSN(fcom2, 0xDBF0, CPU_686|CPU_FPU); }
	F U C O M I { RET_INSN(fcom2, 0xDBE8, CPU_686|CPU_FPU); }
	F C O M I P { RET_INSN(fcom2, 0xDFF0, CPU_686|CPU_FPU); }
	F U C O M I P { RET_INSN(fcom2, 0xDFE8, CPU_686|CPU_FPU); }
	/* Pentium4 extensions */
	M O V N T I { RET_INSN(movnti, 0, CPU_P4); }
	C L F L U S H { RET_INSN(clflush, 0, CPU_P3); }
	L F E N C E { RET_INSN(threebyte, 0x0FAEE8, CPU_P3); }
	M F E N C E { RET_INSN(threebyte, 0x0FAEF0, CPU_P3); }
	P A U S E { RET_INSN(twobyte, 0xF390, CPU_P4); }
	/* MMX/SSE2 instructions */
	E M M S { RET_INSN(twobyte, 0x0F77, CPU_MMX); }
	M O V D { RET_INSN(movd, 0, CPU_MMX); }
	M O V Q { RET_INSN(movq, 0, CPU_MMX); }
	P A C K S S D W { RET_INSN(mmxsse2, 0x6B, CPU_MMX); }
	P A C K S S W B { RET_INSN(mmxsse2, 0x63, CPU_MMX); }
	P A C K U S W B { RET_INSN(mmxsse2, 0x67, CPU_MMX); }
	P A D D B { RET_INSN(mmxsse2, 0xFC, CPU_MMX); }
	P A D D W { RET_INSN(mmxsse2, 0xFD, CPU_MMX); }
	P A D D D { RET_INSN(mmxsse2, 0xFE, CPU_MMX); }
	P A D D Q { RET_INSN(mmxsse2, 0xD4, CPU_MMX); }
	P A D D S B { RET_INSN(mmxsse2, 0xEC, CPU_MMX); }
	P A D D S W { RET_INSN(mmxsse2, 0xED, CPU_MMX); }
	P A D D U S B { RET_INSN(mmxsse2, 0xDC, CPU_MMX); }
	P A D D U S W { RET_INSN(mmxsse2, 0xDD, CPU_MMX); }
	P A N D { RET_INSN(mmxsse2, 0xDB, CPU_MMX); }
	P A N D N { RET_INSN(mmxsse2, 0xDF, CPU_MMX); }
	P A C M P E Q B { RET_INSN(mmxsse2, 0x74, CPU_MMX); }
	P A C M P E Q W { RET_INSN(mmxsse2, 0x75, CPU_MMX); }
	P A C M P E Q D { RET_INSN(mmxsse2, 0x76, CPU_MMX); }
	P A C M P G T B { RET_INSN(mmxsse2, 0x64, CPU_MMX); }
	P A C M P G T W { RET_INSN(mmxsse2, 0x65, CPU_MMX); }
	P A C M P G T D { RET_INSN(mmxsse2, 0x66, CPU_MMX); }
	P M A D D W D { RET_INSN(mmxsse2, 0xF5, CPU_MMX); }
	P M U L H W { RET_INSN(mmxsse2, 0xE5, CPU_MMX); }
	P M U L L W { RET_INSN(mmxsse2, 0xD5, CPU_MMX); }
	P O R { RET_INSN(mmxsse2, 0xEB, CPU_MMX); }
	P S L L W { RET_INSN(pshift, 0x0671F1, CPU_MMX); }
	P S L L D { RET_INSN(pshift, 0x0672F2, CPU_MMX); }
	P S L L Q { RET_INSN(pshift, 0x0673F3, CPU_MMX); }
	P S R A W { RET_INSN(pshift, 0x0471E1, CPU_MMX); }
	P S R A D { RET_INSN(pshift, 0x0472E2, CPU_MMX); }
	P S R L W { RET_INSN(pshift, 0x0271D1, CPU_MMX); }
	P S R L D { RET_INSN(pshift, 0x0272D2, CPU_MMX); }
	P S R L Q { RET_INSN(pshift, 0x0273D3, CPU_MMX); }
	P S U B B { RET_INSN(mmxsse2, 0xF8, CPU_MMX); }
	P S U B W { RET_INSN(mmxsse2, 0xF9, CPU_MMX); }
	P S U B D { RET_INSN(mmxsse2, 0xFA, CPU_MMX); }
	P S U B Q { RET_INSN(mmxsse2, 0xFB, CPU_MMX); }
	P S U B S B { RET_INSN(mmxsse2, 0xE8, CPU_MMX); }
	P S U B S W { RET_INSN(mmxsse2, 0xE9, CPU_MMX); }
	P S U B U S B { RET_INSN(mmxsse2, 0xD8, CPU_MMX); }
	P S U B U S W { RET_INSN(mmxsse2, 0xD9, CPU_MMX); }
	P U N P C K H B W { RET_INSN(mmxsse2, 0x68, CPU_MMX); }
	P U N P C K H W D { RET_INSN(mmxsse2, 0x69, CPU_MMX); }
	P U N P C K H D Q { RET_INSN(mmxsse2, 0x6A, CPU_MMX); }
	P U N P C K L B W { RET_INSN(mmxsse2, 0x60, CPU_MMX); }
	P U N P C K L W D { RET_INSN(mmxsse2, 0x61, CPU_MMX); }
	P U N P C K L D Q { RET_INSN(mmxsse2, 0x62, CPU_MMX); }
	P X O R { RET_INSN(mmxsse2, 0xEF, CPU_MMX); }
	/* PIII (Katmai) new instructions / SIMD instructions */
	A D D P S { RET_INSN(sseps, 0x58, CPU_SSE); }
	A D D S S { RET_INSN(ssess, 0xF358, CPU_SSE); }
	A N D N P S { RET_INSN(sseps, 0x55, CPU_SSE); }
	A N D P S { RET_INSN(sseps, 0x54, CPU_SSE); }
	C M P E Q P S { RET_INSN(ssecmpps, 0x00, CPU_SSE); }
	C M P E Q S S { RET_INSN(ssecmpss, 0x00F3, CPU_SSE); }
	C M P L E P S { RET_INSN(ssecmpps, 0x02, CPU_SSE); }
	C M P L E S S { RET_INSN(ssecmpss, 0x02F3, CPU_SSE); }
	C M P L T P S { RET_INSN(ssecmpps, 0x01, CPU_SSE); }
	C M P L T S S { RET_INSN(ssecmpss, 0x01F3, CPU_SSE); }
	C M P N E Q P S { RET_INSN(ssecmpps, 0x04, CPU_SSE); }
	C M P N E Q S S { RET_INSN(ssecmpss, 0x04F3, CPU_SSE); }
	C M P N L E P S { RET_INSN(ssecmpps, 0x06, CPU_SSE); }
	C M P N L E S S { RET_INSN(ssecmpss, 0x06F3, CPU_SSE); }
	C M P N L T P S { RET_INSN(ssecmpps, 0x05, CPU_SSE); }
	C M P N L T S S { RET_INSN(ssecmpss, 0x05F3, CPU_SSE); }
	C M P O R D P S { RET_INSN(ssecmpps, 0x07, CPU_SSE); }
	C M P O R D S S { RET_INSN(ssecmpss, 0x07F3, CPU_SSE); }
	C M P U N O R D P S { RET_INSN(ssecmpps, 0x03, CPU_SSE); }
	C M P U N O R D S S { RET_INSN(ssecmpss, 0x03F3, CPU_SSE); }
	C M P P S { RET_INSN(ssepsimm, 0xC2, CPU_SSE); }
	C M P S S { RET_INSN(ssessimm, 0xF3C2, CPU_SSE); }
	C O M I S S { RET_INSN(sseps, 0x2F, CPU_SSE); }
	C V T P I "2" P S { RET_INSN(sseps, 0x2A, CPU_SSE); }
	C V T P S "2" P I { RET_INSN(sseps, 0x2D, CPU_SSE); }
	C V T S I "2" S S { RET_INSN(ssess, 0xF32A, CPU_SSE); }
	C V T S S "2" S I { RET_INSN(ssess, 0xF32D, CPU_SSE); }
	C V T T P S "2" P I { RET_INSN(sseps, 0x2C, CPU_SSE); }
	C V T T S S "2" S I { RET_INSN(ssess, 0xF32C, CPU_SSE); }
	D I V P S { RET_INSN(sseps, 0x5E, CPU_SSE); }
	D I V S S { RET_INSN(ssess, 0xF35E, CPU_SSE); }
	L D M X C S R { RET_INSN(ldstmxcsr, 0x02, CPU_SSE); }
	M A S K M O V Q { RET_INSN(maskmovq, 0, CPU_P3|CPU_MMX); }
	M A X P S { RET_INSN(sseps, 0x5F, CPU_SSE); }
	M A X S S { RET_INSN(ssess, 0xF35F, CPU_SSE); }
	M I N P S { RET_INSN(sseps, 0x5D, CPU_SSE); }
	M I N S S { RET_INSN(ssess, 0xF35D, CPU_SSE); }
	M O V A P S { RET_INSN(movaups, 0x28, CPU_SSE); }
	M O V H L P S { RET_INSN(movhllhps, 0x12, CPU_SSE); }
	M O V H P S { RET_INSN(movhlps, 0x16, CPU_SSE); }
	M O V L H P S { RET_INSN(movhllhps, 0x16, CPU_SSE); }
	M O V L P S { RET_INSN(movhlps, 0x12, CPU_SSE); }
	M O V M S K P S { RET_INSN(movmskps, 0, CPU_SSE); }
	M O V N T P S { RET_INSN(movntps, 0, CPU_SSE); }
	M O V N T Q { RET_INSN(movntq, 0, CPU_SSE); }
	M O V S S { RET_INSN(movss, 0, CPU_SSE); }
	M O V U P S { RET_INSN(movaups, 0x10, CPU_SSE); }
	M U L P S { RET_INSN(sseps, 0x59, CPU_SSE); }
	M U L S S { RET_INSN(ssess, 0xF359, CPU_SSE); }
	O R P S { RET_INSN(sseps, 0x56, CPU_SSE); }
	P A V G B { RET_INSN(mmxsse2, 0xE0, CPU_P3|CPU_MMX); }
	P A V G W { RET_INSN(mmxsse2, 0xE3, CPU_P3|CPU_MMX); }
	P E X T R W { RET_INSN(pextrw, 0, CPU_P3|CPU_MMX); }
	P I N S R W { RET_INSN(pinsrw, 0, CPU_P3|CPU_MMX); }
	P M A X S W { RET_INSN(mmxsse2, 0xEE, CPU_P3|CPU_MMX); }
	P M A X U B { RET_INSN(mmxsse2, 0xDE, CPU_P3|CPU_MMX); }
	P M I N S W { RET_INSN(mmxsse2, 0xEA, CPU_P3|CPU_MMX); }
	P M I N U B { RET_INSN(mmxsse2, 0xDA, CPU_P3|CPU_MMX); }
	P M O V M S K B { RET_INSN(pmovmskb, 0, CPU_SSE); }
	P M U L H U W { RET_INSN(mmxsse2, 0xE4, CPU_P3|CPU_MMX); }
	P R E F E T C H N T A { RET_INSN(twobytemem, 0x000F18, CPU_P3); }
	P R E F E T C H T "0" { RET_INSN(twobytemem, 0x010F18, CPU_P3); }
	P R E F E T C H T "1" { RET_INSN(twobytemem, 0x020F18, CPU_P3); }
	P R E F E T C H T "2" { RET_INSN(twobytemem, 0x030F18, CPU_P3); }
	P S A D B W { RET_INSN(mmxsse2, 0xF6, CPU_P3|CPU_MMX); }
	P S H U F W { RET_INSN(pshufw, 0, CPU_P3|CPU_MMX); }
	R C P P S { RET_INSN(sseps, 0x53, CPU_SSE); }
	R C P S S { RET_INSN(ssess, 0xF353, CPU_SSE); }
	R S Q R T P S { RET_INSN(sseps, 0x52, CPU_SSE); }
	R S Q R T S S { RET_INSN(ssess, 0xF352, CPU_SSE); }
	S F E N C E { RET_INSN(threebyte, 0x0FAEF8, CPU_P3); }
	S H U F P S { RET_INSN(ssepsimm, 0xC6, CPU_SSE); }
	S Q R T P S { RET_INSN(sseps, 0x51, CPU_SSE); }
	S Q R T S S { RET_INSN(ssess, 0xF351, CPU_SSE); }
	S T M X C S R { RET_INSN(ldstmxcsr, 0x03, CPU_SSE); }
	S U B P S { RET_INSN(sseps, 0x5C, CPU_SSE); }
	S U B S S { RET_INSN(ssess, 0xF35C, CPU_SSE); }
	U C O M I S S { RET_INSN(ssess, 0xF32E, CPU_SSE); }
	U N P C K H P S { RET_INSN(sseps, 0x15, CPU_SSE); }
	U N P C K L P S { RET_INSN(sseps, 0x14, CPU_SSE); }
	X O R P S { RET_INSN(sseps, 0x57, CPU_SSE); }
	/* SSE2 instructions */
	A D D P D { RET_INSN(ssess, 0x6658, CPU_SSE2); }
	A D D S D { RET_INSN(ssess, 0xF258, CPU_SSE2); }
	A N D N P D { RET_INSN(ssess, 0x6655, CPU_SSE2); }
	A N D P D { RET_INSN(ssess, 0x6654, CPU_SSE2); }
	C M P E Q P D { RET_INSN(ssecmpss, 0x0066, CPU_SSE2); }
	C M P E Q S D { RET_INSN(ssecmpss, 0x00F2, CPU_SSE2); }
	C M P L E P D { RET_INSN(ssecmpss, 0x0266, CPU_SSE2); }
	C M P L E S D { RET_INSN(ssecmpss, 0x02F2, CPU_SSE2); }
	C M P L T P D { RET_INSN(ssecmpss, 0x0166, CPU_SSE2); }
	C M P L T S D { RET_INSN(ssecmpss, 0x01F2, CPU_SSE2); }
	C M P N E Q P D { RET_INSN(ssecmpss, 0x0466, CPU_SSE2); }
	C M P N E Q S D { RET_INSN(ssecmpss, 0x04F2, CPU_SSE2); }
	C M P N L E P D { RET_INSN(ssecmpss, 0x0666, CPU_SSE2); }
	C M P N L E S D { RET_INSN(ssecmpss, 0x06F2, CPU_SSE2); }
	C M P N L T P D { RET_INSN(ssecmpss, 0x0566, CPU_SSE2); }
	C M P N L T S D { RET_INSN(ssecmpss, 0x05F2, CPU_SSE2); }
	C M P O R D P D { RET_INSN(ssecmpss, 0x0766, CPU_SSE2); }
	C M P O R D S D { RET_INSN(ssecmpss, 0x07F2, CPU_SSE2); }
	C M P U N O R D P D { RET_INSN(ssecmpss, 0x0366, CPU_SSE2); }
	C M P U N O R D S D { RET_INSN(ssecmpss, 0x03F2, CPU_SSE2); }
	C M P P D { RET_INSN(ssessimm, 0x66C2, CPU_SSE2); }
	/* C M P S D is in string instructions above */
	C O M I S D { RET_INSN(ssess, 0x662F, CPU_SSE2); }
	C V T P I "2" P D { RET_INSN(ssess, 0x662A, CPU_SSE2); }
	C V T S I "2" S D { RET_INSN(ssess, 0xF22A, CPU_SSE2); }
	D I V P D { RET_INSN(ssess, 0x665E, CPU_SSE2); }
	D I V S D { RET_INSN(ssess, 0xF25E, CPU_SSE2); }
	M A X P D { RET_INSN(ssess, 0x665F, CPU_SSE2); }
	M A X S D { RET_INSN(ssess, 0xF25F, CPU_SSE2); }
	M I N P D { RET_INSN(ssess, 0x665D, CPU_SSE2); }
	M I N S D { RET_INSN(ssess, 0xF25D, CPU_SSE2); }
	M O V A P D { RET_INSN(movaupd, 0x28, CPU_SSE2); }
	M O V H P D { RET_INSN(movhlpd, 0x16, CPU_SSE2); }
	M O V L P D { RET_INSN(movhlpd, 0x12, CPU_SSE2); }
	M O V M S K P D { RET_INSN(movmskpd, 0, CPU_SSE2); }
	M O V N T P D { RET_INSN(movntpddq, 0x2B, CPU_SSE2); }
	M O V N T D Q { RET_INSN(movntpddq, 0xE7, CPU_SSE2); }
	/* M O V S D is in string instructions above */
	M O V U P D { RET_INSN(movaupd, 0x10, CPU_SSE2); }
	M U L P D { RET_INSN(ssess, 0x6659, CPU_SSE2); }
	M U L S D { RET_INSN(ssess, 0xF259, CPU_SSE2); }
	O R P D { RET_INSN(ssess, 0x6656, CPU_SSE2); }
	S H U F P D { RET_INSN(ssessimm, 0x66C6, CPU_SSE2); }
	S Q R T P D { RET_INSN(ssess, 0x6651, CPU_SSE2); }
	S Q R T S D { RET_INSN(ssess, 0xF251, CPU_SSE2); }
	S U B P D { RET_INSN(ssess, 0x665C, CPU_SSE2); }
	S U B S D { RET_INSN(ssess, 0xF25C, CPU_SSE2); }
	U C O M I S D { RET_INSN(ssess, 0xF22E, CPU_SSE2); }
	U N P C K H P D { RET_INSN(ssess, 0x6615, CPU_SSE2); }
	U N P C K L P D { RET_INSN(ssess, 0x6614, CPU_SSE2); }
	X O R P D { RET_INSN(ssess, 0x6657, CPU_SSE2); }
	C V T D Q "2" P D { RET_INSN(ssess, 0xF3E6, CPU_SSE2); }
	C V T P D "2" D Q { RET_INSN(ssess, 0xF2E6, CPU_SSE2); }
	C V T D Q "2" P S { RET_INSN(sseps, 0x5B, CPU_SSE2); }
	C V T P D "2" P I { RET_INSN(ssess, 0x662D, CPU_SSE2); }
	C V T P D "2" P S { RET_INSN(ssess, 0x665A, CPU_SSE2); }
	C V T P S "2" P D { RET_INSN(sseps, 0x5A, CPU_SSE2); }
	C V T P S "2" D Q { RET_INSN(ssess, 0x665B, CPU_SSE2); }
	C V T S D "2" S I { RET_INSN(ssess, 0xF22D, CPU_SSE2); }
	C V T S D "2" S S { RET_INSN(ssess, 0xF25A, CPU_SSE2); }
	C V T S S "2" S D { RET_INSN(ssess, 0xF35A, CPU_SSE2); }
	C V T T P D "2" P I { RET_INSN(ssess, 0x662C, CPU_SSE2); }
	C V T T S D "2" S I { RET_INSN(ssess, 0xF22C, CPU_SSE2); }
	C V T T P D "2" D Q { RET_INSN(ssess, 0x66E6, CPU_SSE2); }
	C V T T P S "2" D Q { RET_INSN(ssess, 0xF35B, CPU_SSE2); }
	M A S K M O V D Q U { RET_INSN(maskmovdqu, 0, CPU_SSE2); }
	M O V D Q A { RET_INSN(movdqau, 0x66, CPU_SSE2); }
	M O V D Q U { RET_INSN(movdqau, 0xF3, CPU_SSE2); }
	M O V D Q "2" Q { RET_INSN(movdq2q, 0, CPU_SSE2); }
	M O V Q "2" D Q { RET_INSN(movq2dq, 0, CPU_SSE2); }
	P M U L U D Q { RET_INSN(mmxsse2, 0xF4, CPU_SSE2); }
	P S H U F D { RET_INSN(ssessimm, 0x6670, CPU_SSE2); }
	P S H U F H W { RET_INSN(ssessimm, 0xF370, CPU_SSE2); }
	P S H U F L W { RET_INSN(ssessimm, 0xF270, CPU_SSE2); }
	P S L L D Q { RET_INSN(pslrldq, 0x07, CPU_SSE2); }
	P S R L D Q { RET_INSN(pslrldq, 0x03, CPU_SSE2); }
	P U N P C K H Q D Q { RET_INSN(ssess, 0x666D, CPU_SSE2); }
	P U N P C K L Q D Q { RET_INSN(ssess, 0x666C, CPU_SSE2); }
	/* AMD 3DNow! instructions */
	P R E F E T C H { RET_INSN(twobytemem, 0x000F0D, CPU_3DNow); }
	P R E F E T C H W { RET_INSN(twobytemem, 0x010F0D, CPU_3DNow); }
	F E M M S { RET_INSN(twobyte, 0x0F0E, CPU_3DNow); }
	P A V G U S B { RET_INSN(now3d, 0xBF, CPU_3DNow); }
	P F "2" I D { RET_INSN(now3d, 0x1D, CPU_3DNow); }
	P F "2" I W { RET_INSN(now3d, 0x1C, CPU_Athlon|CPU_3DNow); }
	P F A C C { RET_INSN(now3d, 0xAE, CPU_3DNow); }
	P F A D D { RET_INSN(now3d, 0x9E, CPU_3DNow); }
	P F C M P E Q { RET_INSN(now3d, 0xB0, CPU_3DNow); }
	P F C M P G E { RET_INSN(now3d, 0x90, CPU_3DNow); }
	P F C M P G T { RET_INSN(now3d, 0xA0, CPU_3DNow); }
	P F M A X { RET_INSN(now3d, 0xA4, CPU_3DNow); }
	P F M I N { RET_INSN(now3d, 0x94, CPU_3DNow); }
	P F M U L { RET_INSN(now3d, 0xB4, CPU_3DNow); }
	P F N A C C { RET_INSN(now3d, 0x8A, CPU_Athlon|CPU_3DNow); }
	P F P N A C C { RET_INSN(now3d, 0x8E, CPU_Athlon|CPU_3DNow); }
	P F R C P { RET_INSN(now3d, 0x96, CPU_3DNow); }
	P F R C P I T "1" { RET_INSN(now3d, 0xA6, CPU_3DNow); }
	P F R C P I T "2" { RET_INSN(now3d, 0xB6, CPU_3DNow); }
	P F R S Q I T "1" { RET_INSN(now3d, 0xA7, CPU_3DNow); }
	P F R S Q R T { RET_INSN(now3d, 0x97, CPU_3DNow); }
	P F S U B { RET_INSN(now3d, 0x9A, CPU_3DNow); }
	P F S U B R { RET_INSN(now3d, 0xAA, CPU_3DNow); }
	P I "2" F D { RET_INSN(now3d, 0x0D, CPU_3DNow); }
	P I "2" F W { RET_INSN(now3d, 0x0C, CPU_Athlon|CPU_3DNow); }
	P M U L H R W A { RET_INSN(now3d, 0xB7, CPU_3DNow); }
	P S W A P D { RET_INSN(now3d, 0xBB, CPU_Athlon|CPU_3DNow); }
	/* AMD extensions */
	S Y S C A L L { RET_INSN(twobyte, 0x0F05, CPU_686|CPU_AMD); }
	S Y S R E T { RET_INSN(twobyte, 0x0F07, CPU_686|CPU_AMD|CPU_Priv); }
	/* AMD x86-64 extensions */
	S W A P G S {
	    if (yasm_x86_LTX_mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, lindex,
			      N_("`%s' is an instruction in 64-bit mode"),
			      oid);
		return YASM_ARCH_CHECK_ID_NONE;
	    }
	    RET_INSN(threebyte, 0x0F01F8, CPU_Hammer|CPU_64);
	}
	/* Cyrix MMX instructions */
	P A D D S I W { RET_INSN(cyrixmmx, 0x51, CPU_Cyrix|CPU_MMX); }
	P A V E B { RET_INSN(cyrixmmx, 0x50, CPU_Cyrix|CPU_MMX); }
	P D I S T I B { RET_INSN(cyrixmmx, 0x54, CPU_Cyrix|CPU_MMX); }
	P M A C H R I W { RET_INSN(pmachriw, 0, CPU_Cyrix|CPU_MMX); }
	P M A G W { RET_INSN(cyrixmmx, 0x52, CPU_Cyrix|CPU_MMX); }
	P M U L H R I W { RET_INSN(cyrixmmx, 0x5D, CPU_Cyrix|CPU_MMX); }
	P M U L H R W C { RET_INSN(cyrixmmx, 0x59, CPU_Cyrix|CPU_MMX); }
	P M V G E Z B { RET_INSN(cyrixmmx, 0x5C, CPU_Cyrix|CPU_MMX); }
	P M V L Z B { RET_INSN(cyrixmmx, 0x5B, CPU_Cyrix|CPU_MMX); }
	P M V N Z B { RET_INSN(cyrixmmx, 0x5A, CPU_Cyrix|CPU_MMX); }
	P M V Z B { RET_INSN(cyrixmmx, 0x58, CPU_Cyrix|CPU_MMX); }
	P S U B S I W { RET_INSN(cyrixmmx, 0x55, CPU_Cyrix|CPU_MMX); }
	/* Cyrix extensions */
	R D S H R { RET_INSN(twobyte, 0x0F36, CPU_686|CPU_Cyrix|CPU_SMM); }
	R S D C { RET_INSN(rsdc, 0, CPU_486|CPU_Cyrix|CPU_SMM); }
	R S L D T { RET_INSN(cyrixsmm, 0x7B, CPU_486|CPU_Cyrix|CPU_SMM); }
	R S T S { RET_INSN(cyrixsmm, 0x7D, CPU_486|CPU_Cyrix|CPU_SMM); }
	S V D C { RET_INSN(svdc, 0, CPU_486|CPU_Cyrix|CPU_SMM); }
	S V L D T { RET_INSN(cyrixsmm, 0x7A, CPU_486|CPU_Cyrix|CPU_SMM); }
	S V T S { RET_INSN(cyrixsmm, 0x7C, CPU_486|CPU_Cyrix|CPU_SMM); }
	S M I N T { RET_INSN(twobyte, 0x0F38, CPU_686|CPU_Cyrix); }
	S M I N T O L D { RET_INSN(twobyte, 0x0F7E, CPU_486|CPU_Cyrix|CPU_Obs); }
	W R S H R { RET_INSN(twobyte, 0x0F37, CPU_686|CPU_Cyrix|CPU_SMM); }
	/* Obsolete/undocumented instructions */
	F S E T P M { RET_INSN(twobyte, 0xDBE4, CPU_286|CPU_FPU|CPU_Obs); }
	I B T S { RET_INSN(ibts, 0, CPU_386|CPU_Undoc|CPU_Obs); }
	L O A D A L L { RET_INSN(twobyte, 0x0F07, CPU_386|CPU_Undoc); }
	L O A D A L L "286" { RET_INSN(twobyte, 0x0F05, CPU_286|CPU_Undoc); }
	S A L C {
	    if (yasm_x86_LTX_mode_bits == 64) {
		yasm__error(lindex, N_("`%s' invalid in 64-bit mode"), oid);
		RET_INSN(not64, 0, CPU_Not64);
	    }
	    RET_INSN(onebyte, 0x00D6, CPU_Undoc);
	}
	S M I { RET_INSN(onebyte, 0x00F1, CPU_386|CPU_Undoc); }
	U M O V { RET_INSN(umov, 0, CPU_386|CPU_Undoc); }
	X B T S { RET_INSN(xbts, 0, CPU_386|CPU_Undoc|CPU_Obs); }


	/* catchalls */
	[\001-\377]+	{
	    return YASM_ARCH_CHECK_ID_NONE;
	}
	[\000]	{
	    return YASM_ARCH_CHECK_ID_NONE;
	}
    */
}
