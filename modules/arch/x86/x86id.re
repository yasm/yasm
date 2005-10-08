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
RCSID("$Id$");

#define YASM_LIB_INTERNAL
#define YASM_BC_INTERNAL
#define YASM_EXPR_INTERNAL
#include <libyasm.h>

#include "modules/arch/x86/x86arch.h"


/* Opcode modifiers.  The opcode bytes are in "reverse" order because the
 * parameters are read from the arch-specific data in LSB->MSB order.
 * (only for asthetic reasons in the lexer code below, no practical reason).
 */
#define MOD_Gap0    (1UL<<0)	/* Eats a parameter */
#define MOD_Op2Add  (1UL<<1)	/* Parameter adds to opcode byte 2 */
#define MOD_Gap1    (1UL<<2)	/* Eats a parameter */
#define MOD_Op1Add  (1UL<<3)	/* Parameter adds to opcode byte 1 */
#define MOD_Gap2    (1UL<<4)	/* Eats a parameter */
#define MOD_Op0Add  (1UL<<5)	/* Parameter adds to opcode byte 0 */
#define MOD_PreAdd  (1UL<<6)	/* Parameter adds to "special" prefix */
#define MOD_SpAdd   (1UL<<7)	/* Parameter adds to "spare" value */
#define MOD_OpSizeR (1UL<<8)	/* Parameter replaces opersize */
#define MOD_Imm8    (1UL<<9)	/* Parameter is included as immediate byte */
#define MOD_AdSizeR (1UL<<10)	/* Parameter replaces addrsize (jmp only) */
#define MOD_DOpS64R (1UL<<11)	/* Parameter replaces default 64-bit opersize */

/* Modifiers that aren't: these are used with the GAS parser to indicate
 * special cases.
 */
#define MOD_GasOnly	(1UL<<12)	/* Only available in GAS mode */
#define MOD_GasIllegal	(1UL<<13)	/* Illegal in GAS mode */
#define MOD_GasNoRev	(1UL<<14)	/* Don't reverse operands */
#define MOD_GasSufB	(1UL<<15)	/* GAS B suffix ok */
#define MOD_GasSufW	(1UL<<16)	/* GAS W suffix ok */
#define MOD_GasSufL	(1UL<<17)	/* GAS L suffix ok */
#define MOD_GasSufQ	(1UL<<18)	/* GAS Q suffix ok */
#define MOD_GasSufS	(1UL<<19)	/* GAS S suffix ok */
#define MOD_GasSuf_SHIFT 15
#define MOD_GasSuf_MASK	(0x1FUL<<15)

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
 *            3 = FAR (or SEG:OFF immediate)
 *            4 = TO
 *  - 1 bit = effective address size
 *            0 = any address size allowed except for 64-bit
 *            1 = only 64-bit address size allowed
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
 *             8 = relative jump (outputs a jmp instead of normal insn)
 *             9 = operand size goes into address size (jmp only)
 *             A = far jump (outputs a farjmp instead of normal insn)
 * The below describes postponed actions: actions which can't be completed at
 * parse-time due to things like EQU and complex expressions.  For these, some
 * additional data (stored in the second byte of the opcode with a one-byte
 * opcode) is passed to later stages of the assembler with flags set to
 * indicate postponed actions.
 *  - 3 bits = postponed action:
 *             0 = none
 *             1 = shift operation with a ,1 short form (instead of imm8).
 *             2 = large imm16/32 that can become a sign-extended imm8.
 *             3 = could become a short opcode mov with bits=64 and a32 prefix
 *             4 = forced 16-bit address size (override ignored, no prefix)
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

#define OPEAS_Not64	(0UL<<9)
#define OPEAS_64	(1UL<<9)
#define OPEAS_MASK	(1UL<<9)

#define OPTM_None	(0UL<<10)
#define OPTM_Near	(1UL<<10)
#define OPTM_Short	(2UL<<10)
#define OPTM_Far	(3UL<<10)
#define OPTM_To		(4UL<<10)
#define OPTM_MASK	(7UL<<10)

#define OPA_None	(0UL<<13)
#define OPA_EA		(1UL<<13)
#define OPA_Imm		(2UL<<13)
#define OPA_SImm	(3UL<<13)
#define OPA_Spare	(4UL<<13)
#define OPA_Op0Add	(5UL<<13)
#define OPA_Op1Add	(6UL<<13)
#define OPA_SpareEA	(7UL<<13)
#define OPA_JmpRel	(8UL<<13)
#define OPA_AdSizeR	(9UL<<13)
#define OPA_JmpFar	(0xAUL<<13)
#define OPA_MASK	(0xFUL<<13)

#define OPAP_None	(0UL<<17)
#define OPAP_ShiftOp	(1UL<<17)
#define OPAP_SImm8Avail	(2UL<<17)
#define OPAP_ShortMov	(3UL<<17)
#define OPAP_A16	(4UL<<17)
#define OPAP_MASK	(7UL<<17)

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

    /* Default operand size in 64-bit mode (0 = 32-bit for readability). */
    unsigned char def_opersize_64;

    /* A special instruction prefix, used for some of the Intel SSE and SSE2
     * instructions.  Intel calls these 3-byte opcodes, but in AMD64's 64-bit
     * mode, they're treated like normal prefixes (e.g. the REX prefix needs
     * to be *after* the F2/F3/66 "prefix").
     * (0=no special prefix)
     */
    unsigned char special_prefix;

    /* The length of the basic opcode */
    unsigned char opcode_len;

    /* The basic 1-3 byte opcode (not including the special instruction
     * prefix).
     */
    unsigned char opcode[3];

    /* The 3-bit "spare" value (extended opcode) for the R/M byte field */
    unsigned char spare;

    /* The number of operands this form of the instruction takes */
    unsigned char num_operands;

    /* The types of each operand, see above */
    unsigned long operands[3];
} x86_insn_info;

/* Define lexer arch-specific data with 0-3 modifiers.
 * This assumes arch_x86 is locally defined.
 */
#define DEF_INSN_DATA(group, mod, cpu)	do { \
    data[0] = (unsigned long)group##_insn; \
    data[1] = (((unsigned long)mod)<<8) | \
    	      ((unsigned char)(sizeof(group##_insn)/sizeof(x86_insn_info))); \
    data[2] = cpu; \
    data[3] |= arch_x86->mode_bits; \
    } while (0)

/*
 * General instruction groupings
 */

/* Placeholder for instructions invalid in 64-bit mode */
static const x86_insn_info not64_insn[] = {
    { CPU_Not64, 0, 0, 0, 0, 0, {0, 0, 0}, 0, 0, {0, 0, 0} }
};

/* One byte opcode instructions with no operands */
static const x86_insn_info onebyte_insn[] = {
    { CPU_Any, MOD_Op0Add|MOD_OpSizeR|MOD_DOpS64R, 0, 0, 0, 1, {0, 0, 0}, 0, 0,
      {0, 0, 0} }
};

/* One byte opcode instructions with "special" prefix with no operands */
static const x86_insn_info onebyte_prefix_insn[] = {
    { CPU_Any, MOD_Op0Add|MOD_PreAdd, 0, 0, 0x00, 2, {0x00, 0, 0}, 0, 0,
      {0, 0, 0} }
};

/* Two byte opcode instructions with no operands */
static const x86_insn_info twobyte_insn[] = {
    { CPU_Any, MOD_Op1Add|MOD_Op0Add|MOD_GasSufL|MOD_GasSufQ, 0, 0, 0, 2,
      {0, 0, 0}, 0, 0, {0, 0, 0} }
};

/* Three byte opcode instructions with no operands */
static const x86_insn_info threebyte_insn[] = {
    { CPU_Any, MOD_Op2Add|MOD_Op1Add|MOD_Op0Add, 0, 0, 0, 3, {0, 0, 0}, 0, 0,
      {0, 0, 0} }
};

/* One byte opcode instructions with general memory operand */
static const x86_insn_info onebytemem_insn[] = {
    { CPU_Any, MOD_Op0Add|MOD_SpAdd|MOD_GasSufL|MOD_GasSufQ|MOD_GasSufS,
      0, 0, 0, 1, {0, 0, 0}, 0, 1, {OPT_Mem|OPS_Any|OPA_EA, 0, 0} }
};

/* Two byte opcode instructions with general memory operand */
static const x86_insn_info twobytemem_insn[] = {
    { CPU_Any,
      MOD_Op1Add|MOD_Op0Add|MOD_SpAdd|MOD_GasSufL|MOD_GasSufQ|MOD_GasSufS,
      0, 0, 0, 2, {0, 0, 0}, 0, 1, {OPT_Mem|OPS_Any|OPA_EA, 0, 0} }
};

/* P4 VMX Instructions */
static const x86_insn_info vmxmemrd_insn[] = {
    { CPU_Not64, MOD_Op1Add|MOD_Op0Add|MOD_GasSufL, 32, 0, 0, 2, {0, 0, 0}, 0,
      2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_64, MOD_Op1Add|MOD_Op0Add|MOD_GasSufQ, 64, 64, 0, 2, {0, 0, 0}, 0,
      2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} }
};
static const x86_insn_info vmxmemwr_insn[] = {
    { CPU_Not64, MOD_Op1Add|MOD_Op0Add|MOD_GasSufL, 32, 0, 0, 2, {0, 0, 0}, 0,
      2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64, MOD_Op1Add|MOD_Op0Add|MOD_GasSufQ, 64, 64, 0, 2, {0, 0, 0}, 0,
      2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};
static const x86_insn_info vmxtwobytemem_insn[] = {
    { CPU_Any, MOD_SpAdd|MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0, 0}, 0, 1,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0, 0} }
};
static const x86_insn_info vmxthreebytemem_insn[] = {
    { CPU_Any, MOD_SpAdd|MOD_PreAdd|MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0, 0}, 0, 1,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0, 0} }
};

/* Move instructions */
static const x86_insn_info mov_insn[] = {
    /* Absolute forms for non-64-bit mode */
    { CPU_Not64, MOD_GasSufB, 0, 0, 0, 1, {0xA0, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None, OPT_MemOffs|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Not64, MOD_GasSufW, 16, 0, 0, 1, {0xA1, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_MemOffs|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0xA1, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_MemOffs|OPS_32|OPS_Relaxed|OPA_EA, 0} },

    { CPU_Not64, MOD_GasSufB, 0, 0, 0, 1, {0xA2, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_8|OPS_Relaxed|OPA_EA, OPT_Areg|OPS_8|OPA_None, 0} },
    { CPU_Not64, MOD_GasSufW, 16, 0, 0, 1, {0xA3, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_16|OPS_Relaxed|OPA_EA, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0xA3, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_32|OPS_Relaxed|OPA_EA, OPT_Areg|OPS_32|OPA_None, 0} },

    /* 64-bit absolute forms for 64-bit mode.  Disabled for GAS, see movabs */
    { CPU_Hammer|CPU_64, 0, 0, 0, 0, 1, {0xA0, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None,
       OPT_MemOffs|OPS_8|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, 0, 16, 0, 0, 1, {0xA1, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None,
       OPT_MemOffs|OPS_16|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, 0, 32, 0, 0, 1, {0xA1, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None,
       OPT_MemOffs|OPS_32|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 0, 0, 1, {0xA1, 0, 0}, 0, 2,
      {OPT_Areg|OPS_64|OPA_None,
       OPT_MemOffs|OPS_64|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },

    { CPU_Hammer|CPU_64, 0, 0, 0, 0, 1, {0xA2, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_8|OPS_Relaxed|OPEAS_64|OPA_EA,
       OPT_Areg|OPS_8|OPA_None, 0} },
    { CPU_Hammer|CPU_64, 0, 16, 0, 0, 1, {0xA3, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_16|OPS_Relaxed|OPEAS_64|OPA_EA,
       OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_Hammer|CPU_64, 0, 32, 0, 0, 1, {0xA3, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_32|OPS_Relaxed|OPEAS_64|OPA_EA,
       OPT_Areg|OPS_32|OPA_None, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 0, 0, 1, {0xA3, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_64|OPS_Relaxed|OPEAS_64|OPA_EA,
       OPT_Areg|OPS_64|OPA_None, 0} },

    /* General 32-bit forms using Areg / short absolute option */
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x88, 0xA2, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA|OPAP_ShortMov, OPT_Areg|OPS_8|OPA_Spare,
       0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x89, 0xA3, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA|OPAP_ShortMov,
       OPT_Areg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x89, 0xA3, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA|OPAP_ShortMov,
       OPT_Areg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x89, 0xA3, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA|OPAP_ShortMov,
       OPT_Areg|OPS_64|OPA_Spare, 0} },

    /* General 32-bit forms */
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x88, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x89, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x89, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x89, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },

    /* General 32-bit forms using Areg / short absolute option */
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x8A, 0xA0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA|OPAP_ShortMov,
       0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x8B, 0xA1, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_Spare,
       OPT_RM|OPS_16|OPS_Relaxed|OPA_EA|OPAP_ShortMov, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x8B, 0xA1, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_Spare,
       OPT_RM|OPS_32|OPS_Relaxed|OPA_EA|OPAP_ShortMov, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x8B, 0xA1, 0}, 0, 2,
      {OPT_Areg|OPS_64|OPA_Spare,
       OPT_RM|OPS_64|OPS_Relaxed|OPA_EA|OPAP_ShortMov, 0} },

    /* General 32-bit forms */
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x8A, 0, 0}, 0, 2,
      {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x8B, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x8B, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x8B, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },

    /* Segment register forms */
    { CPU_Any, MOD_GasSufW, 0, 0, 0, 1, {0x8C, 0, 0}, 0, 2,
      {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA,
       OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x8C, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_EA, OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x8C, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_EA, OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x8C, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_EA, OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, 0} },

    { CPU_Any, MOD_GasSufW, 0, 0, 0, 1, {0x8E, 0, 0}, 0, 2,
      {OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare,
       OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_GasSufL, 0, 0, 0, 1, {0x8E, 0, 0}, 0, 2,
      {OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 0, 0, 0, 1, {0x8E, 0, 0}, 0, 2,
      {OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, OPT_Reg|OPS_64|OPA_EA, 0} },

    /* Immediate forms */
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xB0, 0, 0}, 0, 2,
      {OPT_Reg|OPS_8|OPA_Op0Add, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xB8, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Op0Add, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xB8, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Op0Add, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xB8, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Op0Add, OPT_Imm|OPS_64|OPS_Relaxed|OPA_Imm, 0} },
    /* Need two sets here, one for strictness on left side, one for right. */
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xC6, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xC7, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_16|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xC7, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xC7, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xC6, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xC7, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xC7, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xC7, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },

    /* CR/DR forms */
    { CPU_586|CPU_Priv|CPU_Not64, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x22, 0}, 0,
      2, {OPT_CR4|OPS_32|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, 0} },
    { CPU_386|CPU_Priv|CPU_Not64, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x22, 0}, 0,
      2, {OPT_CRReg|OPS_32|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, 0} },
    { CPU_Hammer|CPU_Priv|CPU_64, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x22, 0}, 0,
      2, {OPT_CRReg|OPS_32|OPA_Spare, OPT_Reg|OPS_64|OPA_EA, 0} },
    { CPU_586|CPU_Priv|CPU_Not64, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x20, 0}, 0,
      2, {OPT_Reg|OPS_32|OPA_EA, OPT_CR4|OPS_32|OPA_Spare, 0} },
    { CPU_386|CPU_Priv|CPU_Not64, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x20, 0}, 0,
      2, {OPT_Reg|OPS_32|OPA_EA, OPT_CRReg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_Priv|CPU_64, MOD_GasSufQ, 0, 0, 0, 2, {0x0F, 0x20, 0}, 0,
      2, {OPT_Reg|OPS_64|OPA_EA, OPT_CRReg|OPS_32|OPA_Spare, 0} },

    { CPU_386|CPU_Priv|CPU_Not64, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x23, 0}, 0,
      2, {OPT_DRReg|OPS_32|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, 0} },
    { CPU_Hammer|CPU_Priv|CPU_64, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x23, 0}, 0,
      2, {OPT_DRReg|OPS_32|OPA_Spare, OPT_Reg|OPS_64|OPA_EA, 0} },
    { CPU_386|CPU_Priv|CPU_Not64, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x21, 0}, 0,
      2, {OPT_Reg|OPS_32|OPA_EA, OPT_DRReg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_Priv|CPU_64, MOD_GasSufQ, 0, 0, 0, 2, {0x0F, 0x21, 0}, 0,
      2, {OPT_Reg|OPS_64|OPA_EA, OPT_DRReg|OPS_32|OPA_Spare, 0} },

    /* MMX/SSE2 forms for GAS parser (copied from movq_insn) */
    { CPU_MMX, MOD_GasOnly|MOD_GasSufQ, 0, 0, 0, 2, {0x0F, 0x6F, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_MMX|CPU_Hammer|CPU_64, MOD_GasOnly|MOD_GasSufQ, 64, 0, 0, 2,
      {0x0F, 0x6E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_MMX, MOD_GasOnly|MOD_GasSufQ, 0, 0, 0, 2, {0x0F, 0x7F, 0}, 0, 2,
      {OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0}
    },
    { CPU_MMX|CPU_Hammer|CPU_64, MOD_GasOnly|MOD_GasSufQ, 64, 0, 0, 2,
      {0x0F, 0x7E, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} },
    { CPU_SSE2, MOD_GasOnly|MOD_GasSufQ, 0, 0, 0xF3, 2, {0x0F, 0x7E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE2, MOD_GasOnly|MOD_GasSufQ, 0, 0, 0xF3, 2, {0x0F, 0x7E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_SSE2|CPU_Hammer|CPU_64, MOD_GasOnly|MOD_GasSufQ, 64, 0, 0x66, 2,
      {0x0F, 0x6E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, MOD_GasOnly|MOD_GasSufQ, 0, 0, 0x66, 2, {0x0F, 0xD6, 0}, 0, 2,
      {OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0}
    },
    { CPU_SSE2|CPU_Hammer|CPU_64, MOD_GasOnly|MOD_GasSufQ, 64, 0, 0x66, 2,
      {0x0F, 0x7E, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

/* 64-bit absolute move (for GAS).
 * These are disabled for GAS for normal mov above.
 */
static const x86_insn_info movabs_insn[] = {
    { CPU_Hammer|CPU_64, MOD_GasSufB, 0, 0, 0, 1, {0xA0, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None,
       OPT_MemOffs|OPS_8|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufW, 16, 0, 0, 1, {0xA1, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None,
       OPT_MemOffs|OPS_16|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufL, 32, 0, 0, 1, {0xA1, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None,
       OPT_MemOffs|OPS_32|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xA1, 0, 0}, 0, 2,
      {OPT_Areg|OPS_64|OPA_None,
       OPT_MemOffs|OPS_64|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },

    { CPU_Hammer|CPU_64, MOD_GasSufB, 0, 0, 0, 1, {0xA2, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_8|OPS_Relaxed|OPEAS_64|OPA_EA,
       OPT_Areg|OPS_8|OPA_None, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufW, 16, 0, 0, 1, {0xA3, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_16|OPS_Relaxed|OPEAS_64|OPA_EA,
       OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufL, 32, 0, 0, 1, {0xA3, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_32|OPS_Relaxed|OPEAS_64|OPA_EA,
       OPT_Areg|OPS_32|OPA_None, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xA3, 0, 0}, 0, 2,
      {OPT_MemOffs|OPS_64|OPS_Relaxed|OPEAS_64|OPA_EA,
       OPT_Areg|OPS_64|OPA_None, 0} },

    /* 64-bit immediate form */
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xB8, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Op0Add, OPT_Imm|OPS_64|OPS_Relaxed|OPA_Imm, 0} },
};

/* Move with sign/zero extend */
static const x86_insn_info movszx_insn[] = {
    { CPU_386, MOD_Op1Add|MOD_GasSufB, 16, 0, 0, 2, {0x0F, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_Op1Add|MOD_GasSufB, 32, 0, 0, 2, {0x0F, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_8|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add|MOD_GasSufB, 64, 0, 0, 2, {0x0F, 0, 0}, 0,
      2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_8|OPA_EA, 0} },
    { CPU_386, MOD_Op1Add|MOD_GasSufW, 32, 0, 0, 2, {0x0F, 1, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_16|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add|MOD_GasSufW, 64, 0, 0, 2, {0x0F, 1, 0}, 0,
      2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_16|OPA_EA, 0} }
};

/* Move with sign-extend doubleword (64-bit mode only) */
static const x86_insn_info movsxd_insn[] = {
    { CPU_Hammer|CPU_64, MOD_GasSufL, 64, 0, 0, 1, {0x63, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_32|OPA_EA, 0} }
};

/* Push instructions */
static const x86_insn_info push_insn[] = {
    { CPU_Any, MOD_GasSufW, 16, 64, 0, 1, {0x50, 0, 0}, 0, 1,
      {OPT_Reg|OPS_16|OPA_Op0Add, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x50, 0, 0}, 0, 1,
      {OPT_Reg|OPS_32|OPA_Op0Add, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 0, 64, 0, 1, {0x50, 0, 0}, 0, 1,
      {OPT_Reg|OPS_64|OPA_Op0Add, 0, 0} },
    { CPU_Any, MOD_GasSufW, 16, 64, 0, 1, {0xFF, 0, 0}, 6, 1,
      {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0xFF, 0, 0}, 6, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 0, 64, 0, 1, {0xFF, 0, 0}, 6, 1,
      {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    { CPU_Any, MOD_GasIllegal, 0, 64, 0, 1, {0x6A, 0, 0}, 0, 1,
      {OPT_Imm|OPS_8|OPA_SImm, 0, 0} },
    { CPU_Any, MOD_GasIllegal, 16, 64, 0, 1, {0x68, 0, 0}, 0, 1,
      {OPT_Imm|OPS_16|OPA_Imm, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasIllegal, 32, 0, 0, 1, {0x68, 0, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPA_Imm, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_GasIllegal, 64, 64, 0, 1, {0x68, 0, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPA_SImm, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufB, 0, 64, 0, 1, {0x6A, 0, 0}, 0, 1,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_SImm, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufW, 16, 64, 0, 1, {0x68, 0x6A, 0}, 0, 1,
      {OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 1,
      {0x68, 0x6A, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_GasOnly|MOD_GasSufQ, 64, 64, 0, 1,
      {0x68, 0x6A, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x0E, 0, 0}, 0, 1,
      {OPT_CS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, MOD_GasSufW, 16, 0, 0, 1, {0x0E, 0, 0}, 0, 1,
      {OPT_CS|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x0E, 0, 0}, 0, 1,
      {OPT_CS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x16, 0, 0}, 0, 1,
      {OPT_SS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, MOD_GasSufW, 16, 0, 0, 1, {0x16, 0, 0}, 0, 1,
      {OPT_SS|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x16, 0, 0}, 0, 1,
      {OPT_SS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x1E, 0, 0}, 0, 1,
      {OPT_DS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, MOD_GasSufW, 16, 0, 0, 1, {0x1E, 0, 0}, 0, 1,
      {OPT_DS|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x1E, 0, 0}, 0, 1,
      {OPT_DS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x06, 0, 0}, 0, 1,
      {OPT_ES|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, MOD_GasSufW, 16, 0, 0, 1, {0x06, 0, 0}, 0, 1,
      {OPT_ES|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x06, 0, 0}, 0, 1,
      {OPT_ES|OPS_32|OPA_None, 0, 0} },
    { CPU_386, 0, 0, 0, 0, 2, {0x0F, 0xA0, 0}, 0, 1,
      {OPT_FS|OPS_Any|OPA_None, 0, 0} },
    { CPU_386, MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0xA0, 0}, 0, 1,
      {OPT_FS|OPS_16|OPA_None, 0, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0xA0, 0}, 0, 1,
      {OPT_FS|OPS_32|OPA_None, 0, 0} },
    { CPU_386, 0, 0, 0, 0, 2, {0x0F, 0xA8, 0}, 0, 1,
      {OPT_GS|OPS_Any|OPA_None, 0, 0} },
    { CPU_386, MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0xA8, 0}, 0, 1,
      {OPT_GS|OPS_16|OPA_None, 0, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0xA8, 0}, 0, 1,
      {OPT_GS|OPS_32|OPA_None, 0, 0} }
};

/* Pop instructions */
static const x86_insn_info pop_insn[] = {
    { CPU_Any, 0, 16, 64, 0, 1, {0x58, 0, 0}, 0, 1,
      {OPT_Reg|OPS_16|OPA_Op0Add, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0x58, 0, 0}, 0, 1,
      {OPT_Reg|OPS_32|OPA_Op0Add, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 0, 64, 0, 1, {0x58, 0, 0}, 0, 1,
      {OPT_Reg|OPS_64|OPA_Op0Add, 0, 0} },
    { CPU_Any, 0, 16, 64, 0, 1, {0x8F, 0, 0}, 0, 1,
      {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0x8F, 0, 0}, 0, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 0, 64, 0, 1, {0x8F, 0, 0}, 0, 1,
      {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    /* POP CS is debateably valid on the 8086, if obsolete and undocumented.
     * We don't include it because it's VERY unlikely it will ever be used
     * anywhere.  If someone really wants it they can db 0x0F it.
     */
    /*{ CPU_Any|CPU_Undoc|CPU_Obs, 0, 0, 1, {0x0F, 0, 0}, 0, 1,
        {OPT_CS|OPS_Any|OPA_None, 0, 0} },*/
    { CPU_Not64, 0, 0, 0, 0, 1, {0x17, 0, 0}, 0, 1,
      {OPT_SS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, 0, 16, 0, 0, 1, {0x17, 0, 0}, 0, 1,
      {OPT_SS|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, 0, 32, 0, 0, 1, {0x17, 0, 0}, 0, 1,
      {OPT_SS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x1F, 0, 0}, 0, 1,
      {OPT_DS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, 0, 16, 0, 0, 1, {0x1F, 0, 0}, 0, 1,
      {OPT_DS|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, 0, 32, 0, 0, 1, {0x1F, 0, 0}, 0, 1,
      {OPT_DS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x07, 0, 0}, 0, 1,
      {OPT_ES|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, 0, 16, 0, 0, 1, {0x07, 0, 0}, 0, 1,
      {OPT_ES|OPS_16|OPA_None, 0, 0} },
    { CPU_Not64, 0, 32, 0, 0, 1, {0x07, 0, 0}, 0, 1,
      {OPT_ES|OPS_32|OPA_None, 0, 0} },
    { CPU_386, 0, 0, 0, 0, 2, {0x0F, 0xA1, 0}, 0, 1,
      {OPT_FS|OPS_Any|OPA_None, 0, 0} },
    { CPU_386, 0, 16, 0, 0, 2, {0x0F, 0xA1, 0}, 0, 1,
      {OPT_FS|OPS_16|OPA_None, 0, 0} },
    { CPU_386, 0, 32, 0, 0, 2, {0x0F, 0xA1, 0}, 0, 1,
      {OPT_FS|OPS_32|OPA_None, 0, 0} },
    { CPU_386, 0, 0, 0, 0, 2, {0x0F, 0xA9, 0}, 0, 1,
      {OPT_GS|OPS_Any|OPA_None, 0, 0} },
    { CPU_386, 0, 16, 0, 0, 2, {0x0F, 0xA9, 0}, 0, 1,
      {OPT_GS|OPS_16|OPA_None, 0, 0} },
    { CPU_386, 0, 32, 0, 0, 2, {0x0F, 0xA9, 0}, 0, 1,
      {OPT_GS|OPS_32|OPA_None, 0, 0} }
};

/* Exchange instructions */
static const x86_insn_info xchg_insn[] = {
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x86, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x86, 0, 0}, 0, 2,
      {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x90, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_Reg|OPS_16|OPA_Op0Add, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x90, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Op0Add, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x87, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x87, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x90, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_Reg|OPS_32|OPA_Op0Add, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x90, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Op0Add, OPT_Areg|OPS_32|OPA_None, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x87, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x87, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x90, 0, 0}, 0, 2,
      {OPT_Areg|OPS_64|OPA_None, OPT_Reg|OPS_64|OPA_Op0Add, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x90, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Op0Add, OPT_Areg|OPS_64|OPA_None, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x87, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x87, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* In/out from ports */
static const x86_insn_info in_insn[] = {
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xE4, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xE5, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xE5, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xEC, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None, OPT_Dreg|OPS_16|OPA_None, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xED, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_Dreg|OPS_16|OPA_None, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xED, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_Dreg|OPS_16|OPA_None, 0} },
    /* GAS-only variants (implict accumulator register) */
    { CPU_Any, MOD_GasOnly|MOD_GasSufB, 0, 0, 0, 1, {0xE4, 0, 0}, 0, 1,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufW, 16, 0, 0, 1, {0xE5, 0, 0}, 0, 1,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_386, MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 1, {0xE5, 0, 0}, 0, 1,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufB, 0, 0, 0, 1, {0xEC, 0, 0}, 0, 1,
      {OPT_Dreg|OPS_16|OPA_None, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufW, 16, 0, 0, 1, {0xED, 0, 0}, 0, 1,
      {OPT_Dreg|OPS_16|OPA_None, 0, 0} },
    { CPU_386, MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 1, {0xED, 0, 0}, 0, 1,
      {OPT_Dreg|OPS_16|OPA_None, 0, 0} }
};
static const x86_insn_info out_insn[] = {
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xE6, 0, 0}, 0, 2,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, OPT_Areg|OPS_8|OPA_None, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xE7, 0, 0}, 0, 2,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xE7, 0, 0}, 0, 2,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, OPT_Areg|OPS_32|OPA_None, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xEE, 0, 0}, 0, 2,
      {OPT_Dreg|OPS_16|OPA_None, OPT_Areg|OPS_8|OPA_None, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xEF, 0, 0}, 0, 2,
      {OPT_Dreg|OPS_16|OPA_None, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xEF, 0, 0}, 0, 2,
      {OPT_Dreg|OPS_16|OPA_None, OPT_Areg|OPS_32|OPA_None, 0} },
    /* GAS-only variants (implict accumulator register) */
    { CPU_Any, MOD_GasOnly|MOD_GasSufB, 0, 0, 0, 1, {0xE6, 0, 0}, 0, 1,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufW, 16, 0, 0, 1, {0xE7, 0, 0}, 0, 1,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_386, MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 1, {0xE7, 0, 0}, 0, 1,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufB, 0, 0, 0, 1, {0xEE, 0, 0}, 0, 1,
      {OPT_Dreg|OPS_16|OPA_None, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufW, 16, 0, 0, 1, {0xEF, 0, 0}, 0, 1,
      {OPT_Dreg|OPS_16|OPA_None, 0, 0} },
    { CPU_386, MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 1, {0xEF, 0, 0}, 0, 1,
      {OPT_Dreg|OPS_16|OPA_None, 0, 0} }
};

/* Load effective address */
static const x86_insn_info lea_insn[] = {
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x8D, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x8D, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x8D, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* Load segment registers from memory */
static const x86_insn_info ldes_insn[] = {
    { CPU_Not64, MOD_Op0Add|MOD_GasSufW, 16, 0, 0, 1, {0, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} },
    { CPU_386|CPU_Not64, MOD_Op0Add|MOD_GasSufL, 32, 0, 0, 1, {0, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} }
};
static const x86_insn_info lfgss_insn[] = {
    { CPU_386, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} },
    { CPU_386, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} }
};

/* Arithmetic - general */
static const x86_insn_info arith_insn[] = {
    { CPU_Any, MOD_Op0Add|MOD_GasSufB, 0, 0, 0, 1, {0x04, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_Op0Add|MOD_GasSufW, 16, 0, 0, 1, {0x05, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_Op0Add|MOD_GasSufL, 32, 0, 0, 1, {0x05, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_Op0Add|MOD_GasSufQ, 64, 0, 0, 1, {0x05, 0, 0}, 0,
      2, {OPT_Areg|OPS_64|OPA_None, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },

    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0x80, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0x80, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPA_Imm, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0x83, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0x81, 0x83, 0}, 0,
      2, {OPT_RM|OPS_16|OPA_EA,
	  OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0x81, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_16|OPA_Imm, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0x83, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0x81, 0x83, 0}, 0,
      2, {OPT_RM|OPS_32|OPA_EA,
	  OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0x81, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_Gap0|MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1,
      {0x83, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_Hammer|CPU_64, MOD_Gap0|MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1,
      {0x81, 0x83, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0} },
    { CPU_Hammer|CPU_64, MOD_Gap0|MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1,
      {0x81, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },

    { CPU_Any, MOD_Op0Add|MOD_GasSufB, 0, 0, 0, 1, {0x00, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_Any, MOD_Op0Add|MOD_GasSufW, 16, 0, 0, 1, {0x01, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_Op0Add|MOD_GasSufL, 32, 0, 0, 1, {0x01, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, MOD_Op0Add|MOD_GasSufQ, 64, 0, 0, 1, {0x01, 0, 0}, 0,
      2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },
    { CPU_Any, MOD_Op0Add|MOD_GasSufB, 0, 0, 0, 1, {0x02, 0, 0}, 0, 2,
      {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, MOD_Op0Add|MOD_GasSufW, 16, 0, 0, 1, {0x03, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_Op0Add|MOD_GasSufL, 32, 0, 0, 1, {0x03, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_Op0Add|MOD_GasSufQ, 64, 0, 0, 1, {0x03, 0, 0}, 0,
      2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* Arithmetic - inc/dec */
static const x86_insn_info incdec_insn[] = {
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xFE, 0, 0}, 0, 1,
      {OPT_RM|OPS_8|OPA_EA, 0, 0} },
    { CPU_Not64, MOD_Op0Add|MOD_GasSufW, 16, 0, 0, 1, {0, 0, 0}, 0, 1,
      {OPT_Reg|OPS_16|OPA_Op0Add, 0, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xFF, 0, 0}, 0,
      1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, MOD_Op0Add|MOD_GasSufL, 32, 0, 0, 1, {0, 0, 0}, 0, 1,
      {OPT_Reg|OPS_32|OPA_Op0Add, 0, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xFF, 0, 0}, 0, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_Gap0|MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1,
      {0xFF, 0, 0}, 0, 1, {OPT_RM|OPS_64|OPA_EA, 0, 0} },
};

/* Arithmetic - mul/neg/not F6 opcodes */
static const x86_insn_info f6_insn[] = {
    { CPU_Any, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xF6, 0, 0}, 0, 1,
      {OPT_RM|OPS_8|OPA_EA, 0, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xF7, 0, 0}, 0, 1,
      {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xF7, 0, 0}, 0, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xF7, 0, 0}, 0,
      1, {OPT_RM|OPS_64|OPA_EA, 0, 0} },
};

/* Arithmetic - div/idiv F6 opcodes
 * These allow explicit accumulator in GAS mode.
 */
static const x86_insn_info div_insn[] = {
    { CPU_Any, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xF6, 0, 0}, 0, 1,
      {OPT_RM|OPS_8|OPA_EA, 0, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xF7, 0, 0}, 0, 1,
      {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xF7, 0, 0}, 0, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xF7, 0, 0}, 0,
      1, {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    /* Versions with explicit accumulator */
    { CPU_Any, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xF6, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None, OPT_RM|OPS_8|OPA_EA, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_RM|OPS_16|OPA_EA, 0} },
    { CPU_386, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_RM|OPS_32|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xF7, 0, 0}, 0,
      2, {OPT_Areg|OPS_64|OPA_None, OPT_RM|OPS_64|OPA_EA, 0} },
};

/* Arithmetic - test instruction */
static const x86_insn_info test_insn[] = {
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xA8, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xA9, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xA9, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xA9, 0, 0}, 0, 2,
      {OPT_Areg|OPS_64|OPA_None, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },

    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xF6, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xF6, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_16|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xF7, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },

    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x84, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x85, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x85, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x85, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },

    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x84, 0, 0}, 0, 2,
      {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x85, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x85, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x85, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* Arithmetic - aad/aam */
static const x86_insn_info aadm_insn[] = {
    { CPU_Any, MOD_Op0Add, 0, 0, 0, 2, {0xD4, 0x0A, 0}, 0, 0, {0, 0, 0} },
    { CPU_Any, MOD_Op0Add, 0, 0, 0, 1, {0xD4, 0, 0}, 0, 1,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} }
};

/* Arithmetic - imul */
static const x86_insn_info imul_insn[] = {
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xF6, 0, 0}, 5, 1,
      {OPT_RM|OPS_8|OPA_EA, 0, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xF7, 0, 0}, 5, 1,
      {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xF7, 0, 0}, 5, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xF7, 0, 0}, 5, 1,
      {OPT_RM|OPS_64|OPA_EA, 0, 0} },

    { CPU_386, MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0xAF, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0xAF, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0xAF, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },

    { CPU_186, MOD_GasSufW, 16, 0, 0, 1, {0x6B, 0, 0}, 0, 3,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPA_SImm} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x6B, 0, 0}, 0, 3,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPA_SImm} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x6B, 0, 0}, 0, 3,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPA_SImm} },

    { CPU_186, MOD_GasSufW, 16, 0, 0, 1, {0x6B, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_SpareEA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x6B, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_SpareEA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x6B, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_SpareEA, OPT_Imm|OPS_8|OPA_SImm, 0} },

    { CPU_186, MOD_GasSufW, 16, 0, 0, 1, {0x6B, 0x69, 0}, 0, 3,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_16|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x6B, 0x69, 0}, 0, 3,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x6B, 0x69, 0}, 0, 3,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail} },

    { CPU_186, MOD_GasSufW, 16, 0, 0, 1, {0x6B, 0x69, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_SpareEA,
       OPT_Imm|OPS_16|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x6B, 0x69, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_SpareEA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x6B, 0x69, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_SpareEA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail, 0} }
};

/* Shifts - standard */
static const x86_insn_info shift_insn[] = {
    { CPU_Any, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xD2, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    /* FIXME: imm8 is only avail on 186+, but we use imm8 to get to postponed
     * ,1 form, so it has to be marked as Any.  We need to store the active
     * CPU flags somewhere to pass that parse-time info down the line.
     */
    { CPU_Any, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xD0, 0xC0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm|OPAP_ShiftOp,
       0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xD3, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xD1, 0xC1, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm|OPAP_ShiftOp,
       0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xD3, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xD1, 0xC1, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm|OPAP_ShiftOp,
       0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd, 64, 0, 0, 1, {0xD3, 0, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd, 64, 0, 0, 1, {0xD1, 0xC1, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm|OPAP_ShiftOp,
       0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xD3, 0, 0}, 0,
      2, {OPT_RM|OPS_64|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xD1, 0xC1, 0},
      0, 2, {OPT_RM|OPS_64|OPA_EA,
	     OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm|OPAP_ShiftOp, 0} },
    /* In GAS mode, single operands are equivalent to shifting by 1 forms */
    { CPU_Any, MOD_SpAdd|MOD_GasOnly|MOD_GasSufB, 0, 0, 0, 1, {0xD0, 0, 0},
      0, 1, {OPT_RM|OPS_8|OPA_EA, 0, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasOnly|MOD_GasSufW, 16, 0, 0, 1, {0xD1, 0, 0},
      0, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 1, {0xD1, 0, 0},
      0, 1, {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd|MOD_GasOnly|MOD_GasSufQ, 64, 0, 0, 1,
      {0xD1, 0, 0}, 0, 1, {OPT_RM|OPS_64|OPA_EA, 0, 0} }
};

/* Shifts - doubleword */
static const x86_insn_info shlrd_insn[] = {
    { CPU_386, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x00, 0}, 0, 3,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_386, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x01, 0}, 0, 3,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare,
       OPT_Creg|OPS_8|OPA_None} },
    { CPU_386, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x00, 0}, 0, 3,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_386, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x01, 0}, 0, 3,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare,
       OPT_Creg|OPS_8|OPA_None} },
    { CPU_Hammer|CPU_64, MOD_Op1Add|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x00, 0},
      0, 3, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare,
	     OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_Hammer|CPU_64, MOD_Op1Add|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x01, 0},
      0, 3, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare,
	     OPT_Creg|OPS_8|OPA_None} },
    /* GAS parser supports two-operand form for shift with CL count */
    { CPU_386, MOD_Op1Add|MOD_GasOnly|MOD_GasSufW, 16, 0, 0, 2,
      {0x0F, 0x01, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_Op1Add|MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 2,
      {0x0F, 0x01, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add|MOD_GasOnly|MOD_GasSufQ, 64, 0, 0, 2,
      {0x0F, 0x01, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} }
};

/* Control transfer instructions (unconditional) */
static const x86_insn_info call_insn[] = {
    { CPU_Any, 0, 0, 0, 0, 0, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 0, 0, 0, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_16|OPA_JmpRel, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 0, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPA_JmpRel, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 0, 0, 0, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPA_JmpRel, 0, 0} },

    { CPU_Any, 0, 16, 64, 0, 1, {0xE8, 0, 0}, 0, 1,
      {OPT_Imm|OPS_16|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xE8, 0, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 64, 0, 1, {0xE8, 0, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 0, 64, 0, 1, {0xE8, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Near|OPA_JmpRel, 0, 0} },

    { CPU_Any, 0, 16, 0, 0, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 64, 0, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 64, 0, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_Mem|OPS_Any|OPA_EA, 0, 0} },
    { CPU_Any, 0, 16, 64, 0, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_RM|OPS_16|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_RM|OPS_32|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 64, 0, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_RM|OPS_64|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 64, 0, 1, {0xFF, 0, 0}, 2, 1,
      {OPT_Mem|OPS_Any|OPTM_Near|OPA_EA, 0, 0} },

    { CPU_Not64, 0, 16, 0, 0, 1, {0x9A, 0, 0}, 3, 1,
      {OPT_Imm|OPS_16|OPTM_Far|OPA_JmpFar, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0x9A, 0, 0}, 3, 1,
      {OPT_Imm|OPS_32|OPTM_Far|OPA_JmpFar, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x9A, 0, 0}, 3, 1,
      {OPT_Imm|OPS_Any|OPTM_Far|OPA_JmpFar, 0, 0} },

    { CPU_Any, 0, 16, 0, 0, 1, {0xFF, 0, 0}, 3, 1,
      {OPT_Mem|OPS_16|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_386, 0, 32, 0, 0, 1, {0xFF, 0, 0}, 3, 1,
      {OPT_Mem|OPS_32|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 0, 0, 1, {0xFF, 0, 0}, 3, 1,
      {OPT_Mem|OPS_Any|OPTM_Far|OPA_EA, 0, 0} }
};
static const x86_insn_info jmp_insn[] = {
    { CPU_Any, 0, 0, 0, 0, 0, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 0, 0, 0, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_16|OPA_JmpRel, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPA_JmpRel, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 0, 0, 1, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPA_JmpRel, 0, 0} },

    { CPU_Any, 0, 0, 64, 0, 1, {0xEB, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 64, 0, 1, {0xE9, 0, 0}, 0, 1,
      {OPT_Imm|OPS_16|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xE9, 0, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 64, 0, 1, {0xE9, 0, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 0, 64, 0, 1, {0xE9, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Near|OPA_JmpRel, 0, 0} },

    { CPU_Any, 0, 16, 64, 0, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 64, 0, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 64, 0, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_Mem|OPS_Any|OPA_EA, 0, 0} },
    { CPU_Any, 0, 16, 64, 0, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_RM|OPS_16|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_RM|OPS_32|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 64, 0, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_RM|OPS_64|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 64, 0, 1, {0xFF, 0, 0}, 4, 1,
      {OPT_Mem|OPS_Any|OPTM_Near|OPA_EA, 0, 0} },

    { CPU_Not64, 0, 16, 0, 0, 1, {0xEA, 0, 0}, 3, 1,
      {OPT_Imm|OPS_16|OPTM_Far|OPA_JmpFar, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xEA, 0, 0}, 3, 1,
      {OPT_Imm|OPS_32|OPTM_Far|OPA_JmpFar, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0xEA, 0, 0}, 3, 1,
      {OPT_Imm|OPS_Any|OPTM_Far|OPA_JmpFar, 0, 0} },

    { CPU_Any, 0, 16, 0, 0, 1, {0xFF, 0, 0}, 5, 1,
      {OPT_Mem|OPS_16|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_386, 0, 32, 0, 0, 1, {0xFF, 0, 0}, 5, 1,
      {OPT_Mem|OPS_32|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 0, 0, 1, {0xFF, 0, 0}, 5, 1,
      {OPT_Mem|OPS_Any|OPTM_Far|OPA_EA, 0, 0} }
};
static const x86_insn_info retnf_insn[] = {
    { CPU_Any, MOD_Op0Add, 0, 0, 0, 1,
      {0x01, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_Any, MOD_Op0Add, 0, 0, 0, 1,
      {0x00, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0, 0} },
    /* GAS suffix versions */
    { CPU_Any, MOD_Op0Add|MOD_GasSufW, 16, 0, 0, 1,
      {0x01, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_Any, MOD_Op0Add|MOD_GasSufW, 16, 0, 0, 1,
      {0x00, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_Any, MOD_Op0Add|MOD_GasSufL|MOD_GasSufQ, 0, 0, 0, 1,
      {0x01, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_Any, MOD_Op0Add|MOD_GasSufL|MOD_GasSufQ, 0, 0, 0, 1,
      {0x00, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0, 0} }
};
static const x86_insn_info enter_insn[] = {
    { CPU_186|CPU_Not64, MOD_GasNoRev|MOD_GasSufL, 0, 0, 0, 1, {0xC8, 0, 0}, 0,
      2, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_EA|OPAP_A16,
	  OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_GasNoRev|MOD_GasSufQ, 64, 64, 0, 1, {0xC8, 0, 0},
      0, 2, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_EA|OPAP_A16,
	     OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    /* GAS suffix version */
    { CPU_186, MOD_GasOnly|MOD_GasNoRev|MOD_GasSufW, 16, 0, 0, 1,
      {0xC8, 0, 0}, 0, 2, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_EA|OPAP_A16,
			   OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
};

/* Conditional jumps */
static const x86_insn_info jcc_insn[] = {
    { CPU_Any, 0, 0, 0, 0, 0, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 0, 0, 0, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_16|OPA_JmpRel, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 0, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPA_JmpRel, 0, 0} },
    { CPU_Hammer|CPU_64, 0, 64, 0, 0, 0, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPA_JmpRel, 0, 0} },

    { CPU_Any, MOD_Op0Add, 0, 64, 0, 1, {0x70, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, 0, 0} },
    { CPU_386, MOD_Op1Add, 16, 64, 0, 2, {0x0F, 0x80, 0}, 0, 1,
      {OPT_Imm|OPS_16|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_386|CPU_Not64, MOD_Op1Add, 32, 0, 0, 2, {0x0F, 0x80, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add, 64, 64, 0, 2, {0x0F, 0x80, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_386, MOD_Op1Add, 0, 64, 0, 2, {0x0F, 0x80, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Near|OPA_JmpRel, 0, 0} }
};
static const x86_insn_info jcxz_insn[] = {
    { CPU_Any, MOD_AdSizeR, 0, 0, 0, 0, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Any, MOD_AdSizeR, 0, 64, 0, 1, {0xE3, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, 0, 0} }
};

/* Loop instructions */
static const x86_insn_info loop_insn[] = {
    { CPU_Any, 0, 0, 0, 0, 0, {0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 0, {0, 0, 0}, 0, 2,
      {OPT_Imm|OPS_Any|OPA_JmpRel, OPT_Creg|OPS_16|OPA_AdSizeR, 0} },
    { CPU_386, 0, 0, 64, 0, 0, {0, 0, 0}, 0, 2,
      {OPT_Imm|OPS_Any|OPA_JmpRel, OPT_Creg|OPS_32|OPA_AdSizeR, 0} },
    { CPU_Hammer|CPU_64, 0, 0, 64, 0, 0, {0, 0, 0}, 0, 2,
      {OPT_Imm|OPS_Any|OPA_JmpRel, OPT_Creg|OPS_64|OPA_AdSizeR, 0} },

    { CPU_Not64, MOD_Op0Add, 0, 0, 0, 1, {0xE0, 0, 0}, 0, 1,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, 0, 0} },
    { CPU_Any, MOD_Op0Add, 0, 64, 0, 1, {0xE0, 0, 0}, 0, 2,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, OPT_Creg|OPS_16|OPA_AdSizeR, 0}
    },
    { CPU_386, MOD_Op0Add, 0, 64, 0, 1, {0xE0, 0, 0}, 0, 2,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, OPT_Creg|OPS_32|OPA_AdSizeR, 0}
    },
    { CPU_Hammer|CPU_64, MOD_Op0Add, 0, 64, 0, 1, {0xE0, 0, 0}, 0, 2,
      {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, OPT_Creg|OPS_64|OPA_AdSizeR, 0} }
};

/* Set byte on flag instructions */
static const x86_insn_info setcc_insn[] = {
    { CPU_386, MOD_Op1Add|MOD_GasSufB, 0, 0, 0, 2, {0x0F, 0x90, 0}, 2, 1,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0, 0} }
};

/* Bit manipulation - bit tests */
static const x86_insn_info bittest_insn[] = {
    { CPU_386, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x00, 0},
      0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0xBA, 0},
      0, 2, {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0xBA, 0},
      0, 2, {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_Gap0|MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 2,
      {0x0F, 0xBA, 0}, 0, 2,
      {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} }
};

/* Bit manipulation - bit scans - also used for lar/lsl */
static const x86_insn_info bsfr_insn[] = {
    { CPU_286, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x00, 0},
      0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* Interrupts and operating system instructions */
static const x86_insn_info int_insn[] = {
    { CPU_Any, 0, 0, 0, 0, 1, {0xCD, 0, 0}, 0, 1,
      {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} }
};
static const x86_insn_info bound_insn[] = {
    { CPU_186, MOD_GasSufW, 16, 0, 0, 1, {0x62, 0, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x62, 0, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} }
};

/* Protection control */
static const x86_insn_info arpl_insn[] = {
    { CPU_286|CPU_Prot, MOD_GasSufW, 0, 0, 0, 1, {0x63, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} }
};
static const x86_insn_info str_insn[] = {
    { CPU_Hammer, MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x00, 0}, 1, 1,
      {OPT_Reg|OPS_16|OPA_EA, 0, 0} },
    { CPU_Hammer, MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x00, 0}, 1, 1,
      {OPT_Reg|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x00, 0}, 1, 1,
      {OPT_Reg|OPS_64|OPA_EA, 0, 0} },
    { CPU_286, MOD_Op1Add|MOD_SpAdd|MOD_GasSufW, 0, 0, 0, 2, {0x0F, 0x00, 0},
      0, 1, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} }
};
static const x86_insn_info prot286_insn[] = {
    { CPU_286, MOD_Op1Add|MOD_SpAdd|MOD_GasSufW, 0, 0, 0, 2, {0x0F, 0x00, 0},
      0, 1, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} }
};
static const x86_insn_info sldtmsw_insn[] = {
    { CPU_286, MOD_Op1Add|MOD_SpAdd|MOD_GasSufW, 0, 0, 0, 2, {0x0F, 0x00, 0},
      0, 1, {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_386, MOD_Op1Add|MOD_SpAdd|MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x00, 0},
      0, 1, {OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add|MOD_SpAdd|MOD_GasSufQ, 0, 0, 0, 2,
      {0x0F, 0x00, 0}, 0, 1, {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_286, MOD_Op1Add|MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x00, 0},
      0, 1, {OPT_Reg|OPS_16|OPA_EA, 0, 0} },
    { CPU_386, MOD_Op1Add|MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x00, 0},
      0, 1, {OPT_Reg|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add|MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 2,
      {0x0F, 0x00, 0}, 0, 1, {OPT_Reg|OPS_64|OPA_EA, 0, 0} }
};

/* Floating point instructions - load/store with pop (integer and normal) */
static const x86_insn_info fldstp_insn[] = {
    { CPU_FPU, MOD_Gap0|MOD_SpAdd|MOD_GasSufS, 0, 0, 0, 1, {0xD9, 0, 0}, 0, 1,
      {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 0, 0, 0, 1, {0xDD, 0, 0}, 0, 1,
      {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Gap1|MOD_SpAdd, 0, 0, 0, 1, {0xDB, 0, 0}, 0, 1,
      {OPT_Mem|OPS_80|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 0, 0, 2, {0xD9, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} }
};
/* Long memory version of floating point load/store for GAS */
static const x86_insn_info fldstpt_insn[] = {
    { CPU_FPU, MOD_Gap0|MOD_Gap1|MOD_SpAdd, 0, 0, 0, 1, {0xDB, 0, 0}, 0, 1,
      {OPT_Mem|OPS_80|OPA_EA, 0, 0} }
};
static const x86_insn_info fildstp_insn[] = {
    { CPU_FPU, MOD_SpAdd|MOD_GasSufS, 0, 0, 0, 1, {0xDF, 0, 0}, 0, 1,
      {OPT_Mem|OPS_16|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_SpAdd|MOD_GasSufL, 0, 0, 0, 1, {0xDB, 0, 0}, 0, 1,
      {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Op0Add|MOD_SpAdd|MOD_GasSufQ, 0, 0, 0, 1,
      {0xDD, 0, 0}, 0, 1, {OPT_Mem|OPS_64|OPA_EA, 0, 0} }
};
static const x86_insn_info fbldstp_insn[] = {
    { CPU_FPU, MOD_SpAdd, 0, 0, 0, 1, {0xDF, 0, 0}, 0, 1,
      {OPT_Mem|OPS_80|OPS_Relaxed|OPA_EA, 0, 0} }
};
/* Floating point instructions - store (normal) */
static const x86_insn_info fst_insn[] = {
    { CPU_FPU, MOD_GasSufS, 0, 0, 0, 1, {0xD9, 0, 0}, 2, 1,
      {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_GasSufL, 0, 0, 0, 1, {0xDD, 0, 0}, 2, 1,
      {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 0, 0, 2, {0xDD, 0xD0, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} }
};
/* Floating point instructions - exchange (with ST0) */
static const x86_insn_info fxch_insn[] = {
    { CPU_FPU, 0, 0, 0, 0, 2, {0xD9, 0xC8, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_FPU, 0, 0, 0, 0, 2, {0xD9, 0xC8, 0}, 0, 2,
      {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} },
    { CPU_FPU, 0, 0, 0, 0, 2, {0xD9, 0xC8, 0}, 0, 2,
      {OPT_Reg|OPS_80|OPA_Op1Add, OPT_ST0|OPS_80|OPA_None, 0} },
    { CPU_FPU, 0, 0, 0, 0, 2, {0xD9, 0xC9, 0}, 0, 0, {0, 0, 0} }
};
/* Floating point instructions - comparisons */
static const x86_insn_info fcom_insn[] = {
    { CPU_FPU, MOD_Gap0|MOD_SpAdd|MOD_GasSufS, 0, 0, 0, 1, {0xD8, 0, 0}, 0, 1,
      {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 0, 0, 0, 1, {0xDC, 0, 0}, 0, 1,
      {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 0, 0, 2, {0xD8, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    /* Alias for fcom %st(1) for GAS compat */
    { CPU_FPU, MOD_Op1Add|MOD_GasOnly, 0, 0, 0, 2, {0xD8, 0x01, 0}, 0, 0,
      {0, 0, 0} },
    { CPU_FPU, MOD_Op1Add|MOD_GasIllegal, 0, 0, 0, 2, {0xD8, 0x00, 0}, 0, 2,
      {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} }
};
/* Floating point instructions - extended comparisons */
static const x86_insn_info fcom2_insn[] = {
    { CPU_286|CPU_FPU, MOD_Op0Add|MOD_Op1Add, 0, 0, 0, 2, {0x00, 0x00, 0},
      0, 1, {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_286|CPU_FPU, MOD_Op0Add|MOD_Op1Add, 0, 0, 0, 2, {0x00, 0x00, 0},
      0, 2, {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} }
};
/* Floating point instructions - arithmetic */
static const x86_insn_info farith_insn[] = {
    { CPU_FPU, MOD_Gap0|MOD_Gap1|MOD_SpAdd|MOD_GasSufS, 0, 0, 0, 1,
      {0xD8, 0, 0}, 0, 1, {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Gap1|MOD_SpAdd|MOD_GasSufL, 0, 0, 0, 1,
      {0xDC, 0, 0}, 0, 1, {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Op1Add, 0, 0, 0, 2, {0xD8, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Op1Add, 0, 0, 0, 2, {0xD8, 0x00, 0}, 0, 2,
      {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 0, 0, 2, {0xDC, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPTM_To|OPA_Op1Add, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 0, 0, 2, {0xDC, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_80|OPA_Op1Add, OPT_ST0|OPS_80|OPA_None, 0} }
};
static const x86_insn_info farithp_insn[] = {
    { CPU_FPU, MOD_Op1Add, 0, 0, 0, 2, {0xDE, 0x01, 0}, 0, 0, {0, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 0, 0, 2, {0xDE, 0x00, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 0, 0, 2, {0xDE, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_80|OPA_Op1Add, OPT_ST0|OPS_80|OPA_None, 0} }
};
/* Floating point instructions - integer arith/store wo pop/compare */
static const x86_insn_info fiarith_insn[] = {
    { CPU_FPU, MOD_Op0Add|MOD_SpAdd|MOD_GasSufS, 0, 0, 0, 1, {0x04, 0, 0}, 0,
      1, {OPT_Mem|OPS_16|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Op0Add|MOD_SpAdd|MOD_GasSufL, 0, 0, 0, 1, {0x00, 0, 0}, 0,
      1, {OPT_Mem|OPS_32|OPA_EA, 0, 0} }
};
/* Floating point instructions - processor control */
static const x86_insn_info fldnstcw_insn[] = {
    { CPU_FPU, MOD_SpAdd|MOD_GasSufW, 0, 0, 0, 1, {0xD9, 0, 0}, 0, 1,
      {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} }
};
static const x86_insn_info fstcw_insn[] = {
    { CPU_FPU, MOD_GasSufW, 0, 0, 0, 2, {0x9B, 0xD9, 0}, 7, 1,
      {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} }
};
static const x86_insn_info fnstsw_insn[] = {
    { CPU_FPU, MOD_GasSufW, 0, 0, 0, 1, {0xDD, 0, 0}, 7, 1,
      {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_GasSufW, 0, 0, 0, 2, {0xDF, 0xE0, 0}, 0, 1,
      {OPT_Areg|OPS_16|OPA_None, 0, 0} }
};
static const x86_insn_info fstsw_insn[] = {
    { CPU_FPU, MOD_GasSufW, 0, 0, 0, 2, {0x9B, 0xDD, 0}, 7, 1,
      {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_GasSufW, 0, 0, 0, 3, {0x9B, 0xDF, 0xE0}, 0, 1,
      {OPT_Areg|OPS_16|OPA_None, 0, 0} }
};
static const x86_insn_info ffree_insn[] = {
    { CPU_FPU, MOD_Op0Add, 0, 0, 0, 2, {0x00, 0xC0, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} }
};

/* 486 extensions */
static const x86_insn_info bswap_insn[] = {
    { CPU_486, MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0xC8, 0}, 0, 1,
      {OPT_Reg|OPS_32|OPA_Op1Add, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0xC8, 0}, 0, 1,
      {OPT_Reg|OPS_64|OPA_Op1Add, 0, 0} }
};
static const x86_insn_info cmpxchgxadd_insn[] = {
    { CPU_486, MOD_Op1Add|MOD_GasSufB, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_486, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_486, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x01, 0},
      0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} }
};

/* Pentium extensions */
static const x86_insn_info cmpxchg8b_insn[] = {
    { CPU_586, MOD_GasSufQ, 0, 0, 0, 2, {0x0F, 0xC7, 0}, 1, 1,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0, 0} }
};

/* Pentium II/Pentium Pro extensions */
static const x86_insn_info cmovcc_insn[] = {
    { CPU_686, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x40, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_686, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x40, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_Op1Add|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x40, 0},
      0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};
static const x86_insn_info fcmovcc_insn[] = {
    { CPU_686|CPU_FPU, MOD_Op0Add|MOD_Op1Add, 0, 0, 0, 2, {0x00, 0x00, 0},
      0, 2, {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} }
};

/* Pentium4 extensions */
static const x86_insn_info movnti_insn[] = {
    { CPU_P4, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0xC3, 0}, 0, 2,
      {OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0xC3, 0}, 0, 2,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} }
};
static const x86_insn_info clflush_insn[] = {
    { CPU_P3, 0, 0, 0, 0, 2, {0x0F, 0xAE, 0}, 7, 1,
      {OPT_Mem|OPS_8|OPS_Relaxed|OPA_EA, 0, 0} }
};

/* MMX/SSE2 instructions */
static const x86_insn_info movd_insn[] = {
    { CPU_MMX, 0, 0, 0, 0, 2, {0x0F, 0x6E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_MMX|CPU_Hammer|CPU_64, 0, 64, 0, 0, 2, {0x0F, 0x6E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_MMX, 0, 0, 0, 0, 2, {0x0F, 0x7E, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} },
    { CPU_MMX|CPU_Hammer|CPU_64, 0, 64, 0, 0, 2, {0x0F, 0x7E, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} },
    { CPU_SSE2, 0, 0, 0, 0x66, 2, {0x0F, 0x6E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2|CPU_Hammer|CPU_64, 0, 64, 0, 0x66, 2, {0x0F, 0x6E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, 0, 0, 0, 0x66, 2, {0x0F, 0x7E, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} },
    { CPU_SSE2|CPU_Hammer|CPU_64, 0, 64, 0, 0x66, 2, {0x0F, 0x7E, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info movq_insn[] = {
    { CPU_MMX, 0, 0, 0, 0, 2, {0x0F, 0x6F, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_MMX|CPU_Hammer|CPU_64, 0, 64, 0, 0, 2, {0x0F, 0x6E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_MMX, 0, 0, 0, 0, 2, {0x0F, 0x7F, 0}, 0, 2,
      {OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0}
    },
    { CPU_MMX|CPU_Hammer|CPU_64, 0, 64, 0, 0, 2, {0x0F, 0x7E, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} },
    { CPU_SSE2, 0, 0, 0, 0xF3, 2, {0x0F, 0x7E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE2, 0, 0, 0, 0xF3, 2, {0x0F, 0x7E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_SSE2|CPU_Hammer|CPU_64, 0, 64, 0, 0x66, 2, {0x0F, 0x6E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, 0, 0, 0, 0x66, 2, {0x0F, 0xD6, 0}, 0, 2,
      {OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0}
    },
    { CPU_SSE2|CPU_Hammer|CPU_64, 0, 64, 0, 0x66, 2, {0x0F, 0x7E, 0}, 0, 2,
      {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info mmxsse2_insn[] = {
    { CPU_MMX, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info pshift_insn[] = {
    { CPU_MMX, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_MMX, MOD_Gap0|MOD_Op1Add|MOD_SpAdd, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0,
      2, {OPT_SIMDReg|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_SSE2, MOD_Gap0|MOD_Op1Add|MOD_SpAdd, 0, 0, 0x66, 2, {0x0F, 0x00, 0},
      0, 2,
      {OPT_SIMDReg|OPS_128|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} }
};

/* PIII (Katmai) new instructions / SIMD instructiosn */
static const x86_insn_info sseps_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info cvt_xmm_xmm64_ss_insn[] = {
    { CPU_SSE, MOD_PreAdd|MOD_Op1Add, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0}
    },
    { CPU_SSE, MOD_PreAdd|MOD_Op1Add, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info cvt_xmm_xmm64_ps_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0x00}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0}
    },
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0x00}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info cvt_xmm_xmm32_insn[] = {
    { CPU_SSE, MOD_PreAdd|MOD_Op1Add, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0}
    },
    { CPU_SSE, MOD_PreAdd|MOD_Op1Add, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info cvt_rx_xmm64_insn[] = {
    { CPU_SSE, MOD_PreAdd|MOD_Op1Add|MOD_GasSufL, 0, 0, 0x00, 2,
      {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0}
    },
    { CPU_SSE, MOD_PreAdd|MOD_Op1Add|MOD_GasSufL, 0, 0, 0x00, 2,
      {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0}
    },
    /* REX */
    { CPU_SSE|CPU_Hammer|CPU_64, MOD_PreAdd|MOD_Op1Add|MOD_GasSufQ, 64, 0,
      0x00, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0}
    },
    { CPU_SSE|CPU_Hammer|CPU_64, MOD_PreAdd|MOD_Op1Add|MOD_GasSufQ, 64, 0,
      0x00, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info cvt_rx_xmm32_insn[] = {
    { CPU_SSE, MOD_PreAdd|MOD_Op1Add|MOD_GasSufL, 0, 0, 0x00, 2,
      {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0}
    },
    { CPU_SSE, MOD_PreAdd|MOD_Op1Add|MOD_GasSufL, 0, 0, 0x00, 2,
      {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0}
    },
    /* REX */
    { CPU_SSE|CPU_Hammer|CPU_64, MOD_PreAdd|MOD_Op1Add|MOD_GasSufQ, 64, 0,
      0x00, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0}
    },
    { CPU_SSE|CPU_Hammer|CPU_64, MOD_PreAdd|MOD_Op1Add|MOD_GasSufQ, 64, 0,
      0x00, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info cvt_mm_xmm64_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0x00}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0}
    },
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0x00}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info cvt_mm_xmm_insn[] = {
    { CPU_SSE, MOD_PreAdd|MOD_Op1Add, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info cvt_xmm_mm_ss_insn[] = {
    { CPU_SSE, MOD_PreAdd|MOD_Op1Add, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info cvt_xmm_mm_ps_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0x00}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info cvt_xmm_rmx_insn[] = {
    { CPU_SSE, MOD_PreAdd|MOD_Op1Add|MOD_GasSufL, 0, 0, 0x00, 2,
      {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0}
    },
    /* REX */
    { CPU_Hammer|CPU_64, MOD_PreAdd|MOD_Op1Add|MOD_GasSufQ, 64, 0, 0x00, 2,
      {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info ssess_insn[] = {
    { CPU_SSE, MOD_PreAdd|MOD_Op1Add, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info ssecmpps_insn[] = {
    { CPU_SSE, MOD_Imm8, 0, 0, 0, 2, {0x0F, 0xC2, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info ssecmpss_insn[] = {
    { CPU_SSE, MOD_PreAdd|MOD_Imm8, 0, 0, 0x00, 2, {0x0F, 0xC2, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    }
};
static const x86_insn_info ssepsimm_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 3,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};
static const x86_insn_info ssessimm_insn[] = {
    { CPU_SSE, MOD_PreAdd|MOD_Op1Add, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 3,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};
static const x86_insn_info ldstmxcsr_insn[] = {
    { CPU_SSE, MOD_SpAdd, 0, 0, 0, 2, {0x0F, 0xAE, 0}, 0, 1,
      {OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0, 0} }
};
static const x86_insn_info maskmovq_insn[] = {
    { CPU_P3|CPU_MMX, 0, 0, 0, 0, 2, {0x0F, 0xF7, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA, 0} }
};
static const x86_insn_info movaups_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2,
      {OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0}
    }
};
static const x86_insn_info movhllhps_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};
static const x86_insn_info movhlps_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info movmskps_insn[] = {
    { CPU_SSE, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x50, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x50, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};
static const x86_insn_info movntps_insn[] = {
    { CPU_SSE, 0, 0, 0, 0, 2, {0x0F, 0x2B, 0}, 0, 2,
      {OPT_Mem|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info movntq_insn[] = {
    { CPU_SSE, 0, 0, 0, 0, 2, {0x0F, 0xE7, 0}, 0, 2,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} }
};
static const x86_insn_info movss_insn[] = {
    { CPU_SSE, 0, 0, 0, 0xF3, 2, {0x0F, 0x10, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE, 0, 0, 0, 0xF3, 2, {0x0F, 0x10, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE, 0, 0, 0, 0xF3, 2, {0x0F, 0x11, 0}, 0, 2,
      {OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info pextrw_insn[] = {
    { CPU_P3|CPU_MMX, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0xC5, 0}, 0, 3,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_SSE2, MOD_GasSufL, 0, 0, 0x66, 2, {0x0F, 0xC5, 0}, 0, 3,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0xC5, 0}, 0, 3,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0x66, 2, {0x0F, 0xC5, 0}, 0, 3,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};
static const x86_insn_info pinsrw_insn[] = {
    { CPU_P3|CPU_MMX, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0xC4, 0}, 0, 3,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_Reg|OPS_32|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0xC4, 0}, 0, 3,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_Reg|OPS_64|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_P3|CPU_MMX, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0xC4, 0}, 0, 3,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_SSE2, MOD_GasSufL, 0, 0, 0x66, 2, {0x0F, 0xC4, 0}, 0, 3,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Reg|OPS_32|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0x66, 2, {0x0F, 0xC4, 0}, 0, 3,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Reg|OPS_64|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_SSE2, MOD_GasSufL, 0, 0, 0x66, 2, {0x0F, 0xC4, 0}, 0, 3,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};
static const x86_insn_info pmovmskb_insn[] = {
    { CPU_P3|CPU_MMX, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0xD7, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA, 0} },
    { CPU_SSE2, MOD_GasSufL, 0, 0, 0x66, 2, {0x0F, 0xD7, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0xD7, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0x66, 2, {0x0F, 0xD7, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};
static const x86_insn_info pshufw_insn[] = {
    { CPU_P3|CPU_MMX, 0, 0, 0, 0, 2, {0x0F, 0x70, 0}, 0, 3,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

/* SSE2 instructions */
static const x86_insn_info cmpsd_insn[] = {
    { CPU_Any, MOD_GasIllegal, 32, 0, 0, 1, {0xA7, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_SSE2, 0, 0, 0, 0xF2, 2, {0x0F, 0xC2, 0}, 0, 3,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};
static const x86_insn_info movaupd_insn[] = {
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x01, 0}, 0, 2,
      {OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0}
    }
};
static const x86_insn_info movhlpd_insn[] = {
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x01, 0}, 0, 2,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info movmskpd_insn[] = {
    { CPU_SSE2, MOD_GasSufL, 0, 0, 0x66, 2, {0x0F, 0x50, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};
static const x86_insn_info movntpddq_insn[] = {
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_Mem|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info movsd_insn[] = {
    { CPU_Any, MOD_GasIllegal, 32, 0, 0, 1, {0xA5, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_SSE2, 0, 0, 0, 0xF2, 2, {0x0F, 0x10, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE2, 0, 0, 0, 0xF2, 2, {0x0F, 0x10, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, 0, 0, 0, 0xF2, 2, {0x0F, 0x11, 0}, 0, 2,
      {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};
static const x86_insn_info maskmovdqu_insn[] = {
    { CPU_SSE2, 0, 0, 0, 0x66, 2, {0x0F, 0xF7, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};
static const x86_insn_info movdqau_insn[] = {
    { CPU_SSE2, MOD_PreAdd, 0, 0, 0x00, 2, {0x0F, 0x6F, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0}
    },
    { CPU_SSE2, MOD_PreAdd, 0, 0, 0x00, 2, {0x0F, 0x7F, 0}, 0, 2,
      {OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0}
    }
};
static const x86_insn_info movdq2q_insn[] = {
    { CPU_SSE2, 0, 0, 0, 0xF2, 2, {0x0F, 0xD6, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};
static const x86_insn_info movq2dq_insn[] = {
    { CPU_SSE2, 0, 0, 0, 0xF3, 2, {0x0F, 0xD6, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA, 0} }
};
static const x86_insn_info pslrldq_insn[] = {
    { CPU_SSE2, MOD_SpAdd, 0, 0, 0x66, 2, {0x0F, 0x73, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} }
};

/* SSE3 instructions */
static const x86_insn_info lddqu_insn[] = {
    { CPU_SSE3, 0, 0, 0, 0xF2, 2, {0x0F, 0xF0, 0}, 0, 2,
      {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} }
};

/* AMD 3DNow! instructions */
static const x86_insn_info now3d_insn[] = {
    { CPU_3DNow, MOD_Imm8, 0, 0, 0, 2, {0x0F, 0x0F, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* AMD Pacifica (SVM) instructions */
static const x86_insn_info invlpga_insn[] = {
    { CPU_SVM, 0, 0, 0, 0, 3, {0x0F, 0x01, 0xDF}, 0, 0, {0, 0, 0} },
    { CPU_SVM, 0, 0, 0, 0, 3, {0x0F, 0x01, 0xDF}, 0, 2,
      {OPT_Areg|OPS_64|OPA_None, OPT_Creg|OPS_32|OPA_None, 0} }
};
static const x86_insn_info skinit_insn[] = {
    { CPU_SVM, 0, 0, 0, 0, 3, {0x0F, 0x01, 0xDE}, 0, 0, {0, 0, 0} },
    { CPU_SVM, 0, 0, 0, 0, 3, {0x0F, 0x01, 0xDE}, 0, 1,
      {OPT_Areg|OPS_32|OPA_None, 0, 0} }
};
static const x86_insn_info svm_rax_insn[] = {
    { CPU_SVM, MOD_Op2Add, 0, 0, 0, 3, {0x0F, 0x01, 0x00}, 0, 0, {0, 0, 0} },
    { CPU_SVM, MOD_Op2Add, 0, 0, 0, 3, {0x0F, 0x01, 0x00}, 0, 1,
      {OPT_Areg|OPS_64|OPA_None, 0, 0} }
};
/* VIA PadLock instructions */
static const x86_insn_info padlock_insn[] = {
    { CPU_Any, MOD_Imm8|MOD_PreAdd|MOD_Op1Add, 0, 0, 0x00, 2, {0x0F, 0x00, 0},
      0, 0, {0, 0, 0} }
};

/* Cyrix MMX instructions */
static const x86_insn_info cyrixmmx_insn[] = {
    { CPU_Cyrix|CPU_MMX, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};
static const x86_insn_info pmachriw_insn[] = {
    { CPU_Cyrix|CPU_MMX, 0, 0, 0, 0, 2, {0x0F, 0x5E, 0}, 0, 2,
      {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

/* Cyrix extensions */
static const x86_insn_info rsdc_insn[] = {
    { CPU_486|CPU_Cyrix|CPU_SMM, 0, 0, 0, 0, 2, {0x0F, 0x79, 0}, 0, 2,
      {OPT_SegReg|OPS_16|OPA_Spare, OPT_Mem|OPS_80|OPS_Relaxed|OPA_EA, 0} }
};
static const x86_insn_info cyrixsmm_insn[] = {
    { CPU_486|CPU_Cyrix|CPU_SMM, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 1,
      {OPT_Mem|OPS_80|OPS_Relaxed|OPA_EA, 0, 0} }
};
static const x86_insn_info svdc_insn[] = {
    { CPU_486|CPU_Cyrix|CPU_SMM, 0, 0, 0, 0, 2, {0x0F, 0x78, 0}, 0, 2,
      {OPT_Mem|OPS_80|OPS_Relaxed|OPA_EA, OPT_SegReg|OPS_16|OPA_Spare, 0} }
};

/* Obsolete/undocumented instructions */
static const x86_insn_info ibts_insn[] = {
    { CPU_386|CPU_Undoc|CPU_Obs, 0, 16, 0, 0, 2, {0x0F, 0xA7, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386|CPU_Undoc|CPU_Obs, 0, 32, 0, 0, 2, {0x0F, 0xA7, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} }
};
static const x86_insn_info umov_insn[] = {
    { CPU_386|CPU_Undoc, 0, 0, 0, 0, 2, {0x0F, 0x10, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_386|CPU_Undoc, 0, 16, 0, 0, 2, {0x0F, 0x11, 0}, 0, 2,
      {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386|CPU_Undoc, 0, 32, 0, 0, 2, {0x0F, 0x11, 0}, 0, 2,
      {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_386|CPU_Undoc, 0, 0, 0, 0, 2, {0x0F, 0x12, 0}, 0, 2,
      {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_Undoc, 0, 16, 0, 0, 2, {0x0F, 0x13, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_Undoc, 0, 32, 0, 0, 2, {0x0F, 0x13, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} }
};
static const x86_insn_info xbts_insn[] = {
    { CPU_386|CPU_Undoc|CPU_Obs, 0, 16, 0, 0, 2, {0x0F, 0xA6, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_Undoc|CPU_Obs, 0, 32, 0, 0, 2, {0x0F, 0xA6, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} }
};


static void
x86_finalize_common(x86_common *common, x86_insn_info *info,
		    unsigned int mode_bits)
{
    common->addrsize = 0;
    common->opersize = info->opersize;
    common->lockrep_pre = 0;
    common->mode_bits = (unsigned char)mode_bits;
}

static void
x86_finalize_opcode(x86_opcode *opcode, x86_insn_info *info)
{
    opcode->len = info->opcode_len;
    opcode->opcode[0] = info->opcode[0];
    opcode->opcode[1] = info->opcode[1];
    opcode->opcode[2] = info->opcode[2];
}

static void
x86_finalize_jmpfar(yasm_arch *arch, yasm_bytecode *bc,
		    const unsigned long data[4], int num_operands,
		    yasm_insn_operands *operands, int num_prefixes,
		    unsigned long **prefixes, x86_insn_info *info)
{
    x86_jmpfar *jmpfar;
    yasm_insn_operand *op;

    jmpfar = yasm_xmalloc(sizeof(x86_jmpfar));
    x86_finalize_common(&jmpfar->common, info, data[3]);
    x86_finalize_opcode(&jmpfar->opcode, info);

    op = yasm_ops_first(operands);

    switch (op->targetmod) {
	case X86_FAR:
	    /* "FAR imm" target needs to become "seg imm:imm". */
	    jmpfar->offset = yasm_expr_copy(op->data.val);
	    jmpfar->segment = yasm_expr_create_branch(YASM_EXPR_SEG,
						      op->data.val, bc->line);
	    break;
	case X86_FAR_SEGOFF:
	    /* SEG:OFF expression; split it. */
	    jmpfar->offset = op->data.val;
	    jmpfar->segment = yasm_expr_extract_segoff(&jmpfar->offset);
	    if (!jmpfar->segment)
		yasm_internal_error(N_("didn't get SEG:OFF expression in jmpfar"));
	    break;
	default:
	    yasm_internal_error(N_("didn't get FAR expression in jmpfar"));
    }

    yasm_x86__bc_apply_prefixes((x86_common *)jmpfar, num_prefixes, prefixes,
				bc->line);

    /* Transform the bytecode */
    yasm_x86__bc_transform_jmpfar(bc, jmpfar);
}

static void
x86_finalize_jmp(yasm_arch *arch, yasm_bytecode *bc, yasm_bytecode *prev_bc,
		 const unsigned long data[4], int num_operands,
		 yasm_insn_operands *operands, int num_prefixes,
		 unsigned long **prefixes, x86_insn_info *jinfo)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    x86_jmp *jmp;
    int num_info = (int)(data[1]&0xFF);
    x86_insn_info *info = (x86_insn_info *)data[0];
    unsigned long mod_data = data[1] >> 8;
    unsigned char mode_bits = (unsigned char)(data[3] & 0xFF);
    /*unsigned char suffix = (unsigned char)((data[3]>>8) & 0xFF);*/
    yasm_insn_operand *op;
    static const unsigned char size_lookup[] = {0, 8, 16, 32, 64, 80, 128, 0};

    /* We know the target is in operand 0, but sanity check for Imm. */
    op = yasm_ops_first(operands);
    if (op->type != YASM_INSN__OPERAND_IMM)
	yasm_internal_error(N_("invalid operand conversion"));

    jmp = yasm_xmalloc(sizeof(x86_jmp));
    x86_finalize_common(&jmp->common, jinfo, mode_bits);
    jmp->target = op->data.val;

    /* Need to save jump origin for relative jumps. */
    jmp->origin = yasm_symtab_define_label2("$", prev_bc, 0, bc->line);

    /* See if the user explicitly specified short/near/far. */
    switch ((int)(jinfo->operands[0] & OPTM_MASK)) {
	case OPTM_Short:
	    jmp->op_sel = JMP_SHORT_FORCED;
	    break;
	case OPTM_Near:
	    jmp->op_sel = JMP_NEAR_FORCED;
	    break;
	default:
	    jmp->op_sel = JMP_NONE;
    }

    /* Check for address size setting in second operand, if present */
    if (jinfo->num_operands > 1 &&
	(jinfo->operands[1] & OPA_MASK) == OPA_AdSizeR)
	jmp->common.addrsize = (unsigned char)
	    size_lookup[(jinfo->operands[1] & OPS_MASK)>>OPS_SHIFT];

    /* Check for address size override */
    if (jinfo->modifiers & MOD_AdSizeR)
	jmp->common.addrsize = (unsigned char)(mod_data & 0xFF);

    /* Scan through other infos for this insn looking for short/near versions.
     * Needs to match opersize and number of operands, also be within CPU.
     */
    jmp->shortop.len = 0;
    jmp->nearop.len = 0;
    for (; num_info>0 && (jmp->shortop.len == 0 || jmp->nearop.len == 0);
	 num_info--, info++) {
	unsigned long cpu = info->cpu | data[2];

	if ((cpu & CPU_64) && mode_bits != 64)
	    continue;
	if ((cpu & CPU_Not64) && mode_bits == 64)
	    continue;
	cpu &= ~(CPU_64 | CPU_Not64);

	if ((arch_x86->cpu_enabled & cpu) != cpu)
	    continue;

	if (info->num_operands == 0)
	    continue;

	if ((info->operands[0] & OPA_MASK) != OPA_JmpRel)
	    continue;

	if (info->opersize != jmp->common.opersize)
	    continue;

	switch ((int)(info->operands[0] & OPTM_MASK)) {
	    case OPTM_Short:
		x86_finalize_opcode(&jmp->shortop, info);
		if (info->modifiers & MOD_Op0Add)
		    jmp->shortop.opcode[0] += (unsigned char)(mod_data & 0xFF);
		break;
	    case OPTM_Near:
		x86_finalize_opcode(&jmp->nearop, info);
		if (info->modifiers & MOD_Op1Add)
		    jmp->nearop.opcode[1] += (unsigned char)(mod_data & 0xFF);
		break;
	}
    }

    if ((jmp->op_sel == JMP_SHORT_FORCED) && (jmp->nearop.len == 0))
	yasm__error(bc->line,
		    N_("no SHORT form of that jump instruction exists"));
    if ((jmp->op_sel == JMP_NEAR_FORCED) && (jmp->shortop.len == 0))
	yasm__error(bc->line,
		    N_("no NEAR form of that jump instruction exists"));

    yasm_x86__bc_apply_prefixes((x86_common *)jmp, num_prefixes, prefixes,
				bc->line);

    /* Transform the bytecode */
    yasm_x86__bc_transform_jmp(bc, jmp);
}

void
yasm_x86__finalize_insn(yasm_arch *arch, yasm_bytecode *bc,
			yasm_bytecode *prev_bc, const unsigned long data[4],
			int num_operands,
			/*@null@*/ yasm_insn_operands *operands,
			int num_prefixes, unsigned long **prefixes,
			int num_segregs, const unsigned long *segregs)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    x86_insn *insn;
    int num_info = (int)(data[1]&0xFF);
    x86_insn_info *info = (x86_insn_info *)data[0];
    unsigned long mod_data = data[1] >> 8;
    unsigned char mode_bits = (unsigned char)(data[3] & 0xFF);
    unsigned long suffix = (data[3]>>8) & 0xFF;
    int found = 0;
    yasm_insn_operand *op, *ops[4], *rev_ops[4], **use_ops;
    /*@null@*/ yasm_symrec *origin;
    /*@null@*/ yasm_expr *imm;
    unsigned char im_len;
    unsigned char im_sign;
    unsigned char spare;
    int i;
    static const unsigned int size_lookup[] = {0, 1, 2, 4, 8, 10, 16, 0};

    /* Build local array of operands from list, since we know we have a max
     * of 3 operands.
     */
    if (num_operands > 3) {
	yasm__error(bc->line, N_("too many operands"));
	return;
    }
    ops[0] = ops[1] = ops[2] = ops[3] = NULL;
    for (i = 0, op = yasm_ops_first(operands); op && i < num_operands;
	 op = yasm_operand_next(op), i++)
	ops[i] = op;
    use_ops = ops;

    /* If we're running in GAS mode, build a reverse array of the operands
     * as most GAS instructions have reversed operands from Intel style.
     */
    if (arch_x86->parser == X86_PARSER_GAS) {
	rev_ops[0] = rev_ops[1] = rev_ops[2] = rev_ops[3] = NULL;
	for (i = num_operands-1, op = yasm_ops_first(operands); op && i >= 0;
	     op = yasm_operand_next(op), i--)
	    rev_ops[i] = op;
    }

    /* If we're running in GAS mode, look at the first insn_info to see
     * if this is a relative jump (OPA_JmpRel).  If so, run through the
     * operands and adjust for dereferences / lack thereof.
     */
    if (arch_x86->parser == X86_PARSER_GAS
	&& (info->operands[0] & OPA_MASK) == OPA_JmpRel) {
	for (i = 0, op = ops[0]; op; op = ops[++i]) {
	    if (!op->deref && (op->type == YASM_INSN__OPERAND_REG
			       || (op->type == YASM_INSN__OPERAND_MEMORY
				   && op->data.ea->strong)))
		yasm__warning(YASM_WARN_GENERAL, bc->line,
			      N_("indirect call without `*'"));
	    if (!op->deref && op->type == YASM_INSN__OPERAND_MEMORY
		&& !op->data.ea->strong) {
		/* Memory that is not dereferenced, and not strong, is
		 * actually an immediate for the purposes of relative jumps.
		 */
		if (op->data.ea->segreg != 0)
		    yasm__warning(YASM_WARN_GENERAL, bc->line,
				  N_("skipping prefixes on this instruction"));
		imm = yasm_expr_copy(op->data.ea->disp);
		yasm_ea_destroy(op->data.ea);
		op->type = YASM_INSN__OPERAND_IMM;
		op->data.val = imm;
	    }
	}
    }

    /* Look for SEG:OFF operands and apply X86_FAR_SEGOFF targetmod. */
    for (i = 0, op = ops[0]; op; op = ops[++i]) {
	if (op->type == YASM_INSN__OPERAND_IMM && op->targetmod == 0 &&
	    yasm_expr_is_op(op->data.val, YASM_EXPR_SEGOFF))
	    op->targetmod = X86_FAR_SEGOFF;
    }

    /* Just do a simple linear search through the info array for a match.
     * First match wins.
     */
    for (; num_info>0 && !found; num_info--, info++) {
	unsigned long cpu;
	unsigned int size;
	int mismatch = 0;

	/* Match CPU */
	cpu = info->cpu | data[2];

	if ((cpu & CPU_64) && mode_bits != 64)
	    continue;
	if ((cpu & CPU_Not64) && mode_bits == 64)
	    continue;
	cpu &= ~(CPU_64 | CPU_Not64);

	if ((arch_x86->cpu_enabled & cpu) != cpu)
	    continue;

	/* Match # of operands */
	if (num_operands != info->num_operands)
	    continue;

	/* Match parser mode */
	if ((info->modifiers & MOD_GasOnly)
	    && arch_x86->parser != X86_PARSER_GAS)
	    continue;
	if ((info->modifiers & MOD_GasIllegal)
	    && arch_x86->parser == X86_PARSER_GAS)
	    continue;

	/* Match suffix (if required) */
	if (suffix != 0 && suffix != 0x80
	    && ((suffix<<MOD_GasSuf_SHIFT) & info->modifiers) == 0)
	    continue;

	if (!operands) {
	    found = 1;	    /* no operands -> must have a match here. */
	    break;
	}

	/* Use reversed operands in GAS mode if not otherwise specified */
	if (arch_x86->parser == X86_PARSER_GAS) {
	    if (info->modifiers & MOD_GasNoRev)
		use_ops = ops;
	    else
		use_ops = rev_ops;
	}

	/* Match each operand type and size */
	for (i = 0, op = use_ops[0]; op && i<info->num_operands && !mismatch;
	     op = use_ops[++i]) {
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
	    if (suffix != 0) {
		/* Require relaxed operands for GAS mode (don't allow
		 * per-operand sizing).
		 */
		if (op->type == YASM_INSN__OPERAND_REG && op->size == 0) {
		    /* Register size must exactly match */
		    if (yasm_x86__get_reg_size(arch, op->data.reg) != size)
			mismatch = 1;
		} else if ((info->operands[i] & OPT_MASK) == OPT_Imm
		    && (info->operands[i] & OPS_RMASK) != OPS_Relaxed
		    && (info->operands[i] & OPA_MASK) != OPA_JmpRel)
		    mismatch = 1;
	    } else {
		if (op->type == YASM_INSN__OPERAND_REG && op->size == 0) {
		    /* Register size must exactly match */
		    if (yasm_x86__get_reg_size(arch, op->data.reg) != size)
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
	    }

	    if (mismatch)
		break;

	    /* Check for 64-bit effective address size */
	    if (op->type == YASM_INSN__OPERAND_MEMORY) {
		if ((info->operands[i] & OPEAS_MASK) == OPEAS_64) {
		    if (op->data.ea->len != 8)
			mismatch = 1;
		} else if (op->data.ea->len == 8)
		    mismatch = 1;
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
		    if (op->targetmod != X86_FAR &&
			op->targetmod != X86_FAR_SEGOFF)
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
	yasm__error(bc->line,
		    N_("invalid combination of opcode and operands"));
	return;
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
		    yasm__error(bc->line, N_("mismatch in operand sizes"));
		    break;
		case 1:
		    yasm__error(bc->line, N_("operand size not specified"));
		    break;
		default:
		    yasm_internal_error(N_("unrecognized x86 ext mod index"));
	    }
	    return;	/* It was an error */
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

    if (operands) {
	switch (info->operands[0] & OPA_MASK) {
	    case OPA_JmpRel:
		/* Shortcut to JmpRel */
		x86_finalize_jmp(arch, bc, prev_bc, data, num_operands,
				 operands, num_prefixes, prefixes, info);
		return;
	    case OPA_JmpFar:
		/* Shortcut to JmpFar */
		x86_finalize_jmpfar(arch, bc, data, num_operands, operands,
				    num_prefixes, prefixes, info);
		return;
	}
    }

    /* Copy what we can from info */
    insn = yasm_xmalloc(sizeof(x86_insn));
    x86_finalize_common(&insn->common, info, mode_bits);
    x86_finalize_opcode(&insn->opcode, info);
    insn->ea = NULL;
    origin = NULL;
    imm = NULL;
    insn->def_opersize_64 = info->def_opersize_64;
    insn->special_prefix = info->special_prefix;
    spare = info->spare;
    im_len = 0;
    im_sign = 0;
    insn->postop = X86_POSTOP_NONE;
    insn->rex = 0;

    /* Apply modifiers */
    if (info->modifiers & MOD_Gap0)
	mod_data >>= 8;
    if (info->modifiers & MOD_Op2Add) {
	insn->opcode.opcode[2] += (unsigned char)(mod_data & 0xFF);
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_Gap1)
	mod_data >>= 8;
    if (info->modifiers & MOD_Op1Add) {
	insn->opcode.opcode[1] += (unsigned char)(mod_data & 0xFF);
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_Gap2)
	mod_data >>= 8;
    if (info->modifiers & MOD_Op0Add) {
	insn->opcode.opcode[0] += (unsigned char)(mod_data & 0xFF);
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_PreAdd) {
	insn->special_prefix += (unsigned char)(mod_data & 0xFF);
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_SpAdd) {
	spare += (unsigned char)(mod_data & 0xFF);
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_OpSizeR) {
	insn->common.opersize = (unsigned char)(mod_data & 0xFF);
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_Imm8) {
	imm = yasm_expr_create_ident(yasm_expr_int(
	    yasm_intnum_create_uint(mod_data & 0xFF)), bc->line);
	im_len = 1;
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_DOpS64R) {
	insn->def_opersize_64 = (unsigned char)(mod_data & 0xFF);
	/*mod_data >>= 8;*/
    }

    /* Go through operands and assign */
    if (operands) {
	for (i = 0, op = use_ops[0]; op && i<info->num_operands;
	     op = use_ops[++i]) {
	    switch ((int)(info->operands[i] & OPA_MASK)) {
		case OPA_None:
		    /* Throw away the operand contents */
		    switch (op->type) {
			case YASM_INSN__OPERAND_REG:
			case YASM_INSN__OPERAND_SEGREG:
			    break;
			case YASM_INSN__OPERAND_MEMORY:
			    yasm_ea_destroy(op->data.ea);
			    break;
			case YASM_INSN__OPERAND_IMM:
			    yasm_expr_destroy(op->data.val);
			    break;
		    }
		    break;
		case OPA_EA:
		    switch (op->type) {
			case YASM_INSN__OPERAND_REG:
			    insn->ea =
				yasm_x86__ea_create_reg(op->data.reg,
							&insn->rex,
							mode_bits);
			    break;
			case YASM_INSN__OPERAND_SEGREG:
			    yasm_internal_error(
				N_("invalid operand conversion"));
			case YASM_INSN__OPERAND_MEMORY:
			    insn->ea = op->data.ea;
			    if ((info->operands[i] & OPT_MASK) == OPT_MemOffs)
				/* Special-case for MOV MemOffs instruction */
				yasm_x86__ea_set_disponly(insn->ea);
			    else if (mode_bits == 64)
				/* Save origin for possible RIP-relative */
				origin =
				    yasm_symtab_define_label2("$", prev_bc, 0,
							      bc->line);
			    break;
			case YASM_INSN__OPERAND_IMM:
			    insn->ea = yasm_x86__ea_create_imm(op->data.val,
				size_lookup[(info->operands[i] &
					     OPS_MASK)>>OPS_SHIFT]);
			    break;
		    }
		    break;
		case OPA_Imm:
		    if (op->type == YASM_INSN__OPERAND_IMM) {
			imm = op->data.val;
			im_len = size_lookup[(info->operands[i] &
					      OPS_MASK)>>OPS_SHIFT];
		    } else
			yasm_internal_error(N_("invalid operand conversion"));
		    break;
		case OPA_SImm:
		    if (op->type == YASM_INSN__OPERAND_IMM) {
			imm = op->data.val;
			im_len = size_lookup[(info->operands[i] &
					      OPS_MASK)>>OPS_SHIFT];
			im_sign = 1;
		    } else
			yasm_internal_error(N_("invalid operand conversion"));
		    break;
		case OPA_Spare:
		    if (op->type == YASM_INSN__OPERAND_SEGREG)
			spare = (unsigned char)(op->data.reg&7);
		    else if (op->type == YASM_INSN__OPERAND_REG) {
			if (yasm_x86__set_rex_from_reg(&insn->rex, &spare,
				op->data.reg, mode_bits, X86_REX_R)) {
			    yasm__error(bc->line,
				N_("invalid combination of opcode and operands"));
			    return;
			}
		    } else
			yasm_internal_error(N_("invalid operand conversion"));
		    break;
		case OPA_Op0Add:
		    if (op->type == YASM_INSN__OPERAND_REG) {
			unsigned char opadd;
			if (yasm_x86__set_rex_from_reg(&insn->rex, &opadd,
				op->data.reg, mode_bits, X86_REX_B)) {
			    yasm__error(bc->line,
				N_("invalid combination of opcode and operands"));
			    return;
			}
			insn->opcode.opcode[0] += opadd;
		    } else
			yasm_internal_error(N_("invalid operand conversion"));
		    break;
		case OPA_Op1Add:
		    if (op->type == YASM_INSN__OPERAND_REG) {
			unsigned char opadd;
			if (yasm_x86__set_rex_from_reg(&insn->rex, &opadd,
				op->data.reg, mode_bits, X86_REX_B)) {
			    yasm__error(bc->line,
				N_("invalid combination of opcode and operands"));
			    return;
			}
			insn->opcode.opcode[1] += opadd;
		    } else
			yasm_internal_error(N_("invalid operand conversion"));
		    break;
		case OPA_SpareEA:
		    if (op->type == YASM_INSN__OPERAND_REG) {
			insn->ea = yasm_x86__ea_create_reg(op->data.reg,
							   &insn->rex,
							   mode_bits);
			if (!insn->ea ||
			    yasm_x86__set_rex_from_reg(&insn->rex, &spare,
				op->data.reg, mode_bits, X86_REX_R)) {
			    yasm__error(bc->line,
				N_("invalid combination of opcode and operands"));
			    if (insn->ea)
				yasm_xfree(insn->ea);
			    yasm_xfree(insn);
			    return;
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
		    insn->postop = X86_POSTOP_SHIFT;
		    break;
		case OPAP_SImm8Avail:
		    insn->postop = X86_POSTOP_SIGNEXT_IMM8;
		    break;
		case OPAP_ShortMov:
		    insn->postop = X86_POSTOP_SHORTMOV;
		    break;
		case OPAP_A16:
		    insn->postop = X86_POSTOP_ADDRESS16;
		    break;
		default:
		    yasm_internal_error(
			N_("unknown operand postponed action"));
	    }
	}
    }

    if (insn->ea) {
	yasm_x86__ea_init(insn->ea, spare, origin);
	for (i=0; i<num_segregs; i++)
	    yasm_ea_set_segreg(insn->ea, segregs[i], bc->line);
    } else if (num_segregs > 0 && insn->special_prefix == 0) {
	if (num_segregs > 1)
	    yasm__warning(YASM_WARN_GENERAL, bc->line,
			  N_("multiple segment overrides, using leftmost"));
	insn->special_prefix = segregs[num_segregs-1]>>8;
    }
    if (imm) {
	insn->imm = yasm_imm_create_expr(imm);
	insn->imm->len = im_len;
	insn->imm->sign = im_sign;
    } else
	insn->imm = NULL;

    yasm_x86__bc_apply_prefixes((x86_common *)insn, num_prefixes, prefixes,
				bc->line);

    if (insn->postop == X86_POSTOP_ADDRESS16 && insn->common.addrsize) {
	yasm__warning(YASM_WARN_GENERAL, bc->line,
		      N_("address size override ignored"));
	insn->common.addrsize = 0;
    }

    /* Transform the bytecode */
    yasm_x86__bc_transform_insn(bc, insn);
}


#define YYCTYPE		char
#define YYCURSOR	id
#define YYLIMIT		id
#define YYMARKER	marker
#define YYFILL(n)	(void)(n)

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
yasm_x86__parse_cpu(yasm_arch *arch, const char *id, unsigned long line)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    /*const char *marker;*/

    /*!re2c
	/* The standard CPU names /set/ cpu_enabled. */
	"8086" {
	    arch_x86->cpu_enabled = CPU_Priv;
	    return;
	}
	("80" | I)? "186" {
	    arch_x86->cpu_enabled = CPU_186|CPU_Priv;
	    return;
	}
	("80" | I)? "286" {
	    arch_x86->cpu_enabled = CPU_186|CPU_286|CPU_Priv;
	    return;
	}
	("80" | I)? "386" {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	("80" | I)? "486" {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_FPU|CPU_SMM|CPU_Prot|
		CPU_Priv;
	    return;
	}
	(I? "586") | (P E N T I U M) | (P "5") {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_FPU|CPU_SMM|
		CPU_Prot|CPU_Priv;
	    return;
	}
	(I? "686") | (P "6") | (P P R O) | (P E N T I U M P R O) {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_FPU|
		CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	(P "2") | (P E N T I U M "-"? ("2" | (I I))) {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_FPU|
		CPU_MMX|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	(P "3") | (P E N T I U M "-"? ("3" | (I I I))) | (K A T M A I) {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_P3|CPU_FPU|
		CPU_MMX|CPU_SSE|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	(P "4") | (P E N T I U M "-"? ("4" | (I V))) | (W I L L I A M E T T E) {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_P3|CPU_P4|
		CPU_FPU|CPU_MMX|CPU_SSE|CPU_SSE2|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	(I A "-"? "64") | (I T A N I U M) {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_P3|CPU_P4|
		CPU_IA64|CPU_FPU|CPU_MMX|CPU_SSE|CPU_SSE2|CPU_SMM|CPU_Prot|
		CPU_Priv;
	    return;
	}
	K "6" {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_K6|CPU_FPU|
		CPU_MMX|CPU_3DNow|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	(A T H L O N) | (K "7") {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_K6|
		CPU_Athlon|CPU_FPU|CPU_MMX|CPU_SSE|CPU_3DNow|CPU_SMM|CPU_Prot|
		CPU_Priv;
	    return;
	}
	((S L E D G E)? (H A M M E R)) | (O P T E R O N) |
	(A T H L O N "-"? "64") {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_K6|
		CPU_Athlon|CPU_Hammer|CPU_FPU|CPU_MMX|CPU_SSE|CPU_SSE2|
		CPU_3DNow|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	P R E S C O T T {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_K6|
		CPU_Athlon|CPU_Hammer|CPU_FPU|CPU_MMX|CPU_SSE|CPU_SSE2|
		CPU_SSE3|CPU_3DNow|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}

	/* Features have "no" versions to disable them, and only set/reset the
	 * specific feature being changed.  All other bits are left alone.
	 */
	F P U		{ arch_x86->cpu_enabled |= CPU_FPU; return; }
	N O F P U	{ arch_x86->cpu_enabled &= ~CPU_FPU; return; }
	M M X		{ arch_x86->cpu_enabled |= CPU_MMX; return; }
	N O M M X	{ arch_x86->cpu_enabled &= ~CPU_MMX; return; }
	S S E		{ arch_x86->cpu_enabled |= CPU_SSE; return; }
	N O S S E	{ arch_x86->cpu_enabled &= ~CPU_SSE; return; }
	S S E "2"	{ arch_x86->cpu_enabled |= CPU_SSE2; return; }
	N O S S E "2"	{ arch_x86->cpu_enabled &= ~CPU_SSE2; return; }
	S S E "3"	{ arch_x86->cpu_enabled |= CPU_SSE3; return; }
	N O S S E "3"	{ arch_x86->cpu_enabled &= ~CPU_SSE3; return; }
	P N I		{ arch_x86->cpu_enabled |= CPU_SSE3; return; }
	N O P N I	{ arch_x86->cpu_enabled &= ~CPU_SSE3; return; }
	"3" D N O W	{ arch_x86->cpu_enabled |= CPU_3DNow; return; }
	N O "3" D N O W	{ arch_x86->cpu_enabled &= ~CPU_3DNow; return; }
	C Y R I X	{ arch_x86->cpu_enabled |= CPU_Cyrix; return; }
	N O C Y R I X	{ arch_x86->cpu_enabled &= ~CPU_Cyrix; return; }
	A M D		{ arch_x86->cpu_enabled |= CPU_AMD; return; }
	N O A M D	{ arch_x86->cpu_enabled &= ~CPU_AMD; return; }
	S M M		{ arch_x86->cpu_enabled |= CPU_SMM; return; }
	N O S M M	{ arch_x86->cpu_enabled &= ~CPU_SMM; return; }
	P R O T		{ arch_x86->cpu_enabled |= CPU_Prot; return; }
	N O P R O T	{ arch_x86->cpu_enabled &= ~CPU_Prot; return; }
	U N D O C	{ arch_x86->cpu_enabled |= CPU_Undoc; return; }
	N O U N D O C	{ arch_x86->cpu_enabled &= ~CPU_Undoc; return; }
	O B S		{ arch_x86->cpu_enabled |= CPU_Obs; return; }
	N O O B S	{ arch_x86->cpu_enabled &= ~CPU_Obs; return; }
	P R I V		{ arch_x86->cpu_enabled |= CPU_Priv; return; }
	N O P R I V	{ arch_x86->cpu_enabled &= ~CPU_Priv; return; }
	S V M		{ arch_x86->cpu_enabled |= CPU_SVM; return; }
	N O S V M	{ arch_x86->cpu_enabled &= ~CPU_SVM; return; }
	P A D L O C K	{ arch_x86->cpu_enabled |= CPU_PadLock; return; }
	N O P A D L O C K   { arch_x86->cpu_enabled &= ~CPU_PadLock; return; }

	/* catchalls */
	[\001-\377]+	{
	    yasm__warning(YASM_WARN_GENERAL, line,
			  N_("unrecognized CPU identifier `%s'"), id);
	    return;
	}
	[\000]		{
	    yasm__warning(YASM_WARN_GENERAL, line,
			  N_("unrecognized CPU identifier `%s'"), id);
	    return;
	}
    */
}

int
yasm_x86__parse_check_targetmod(yasm_arch *arch, unsigned long data[1],
				const char *id, unsigned long line)
{
    /*!re2c
	/* target modifiers */
	N E A R		{
	    data[0] = X86_NEAR;
	    return 1;
	}
	S H O R T	{
	    data[0] = X86_SHORT;
	    return 1;
	}
	F A R		{
	    data[0] = X86_FAR;
	    return 1;
	}
	T O		{
	    data[0] = X86_TO;
	    return 1;
	}

	/* catchalls */
	[\001-\377]+	{
	    return 0;
	}
	[\000]	{
	    return 0;
	}
    */
    return 0;
}

int
yasm_x86__parse_check_prefix(yasm_arch *arch, unsigned long data[4],
			     const char *id, unsigned long line)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    const char *oid = id;
    /*!re2c
	/* operand size overrides */
	O "16"	{
	    data[0] = X86_OPERSIZE;
	    data[1] = 16;
	    return 1;
	}
	O "32"	{
	    data[0] = X86_OPERSIZE;
	    data[1] = 32;
	    return 1;
	}
	O "64"	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a prefix in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_OPERSIZE;
	    data[1] = 64;
	    return 1;
	}
	/* address size overrides */
	A "16"	{
	    if (arch_x86->mode_bits == 64) {
		yasm__error(line,
		    N_("Cannot override address size to 16 bits in 64-bit mode"));
		return 0;
	    }
	    data[0] = X86_ADDRSIZE;
	    data[1] = 16;
	    return 1;
	}
	A "32"	{
	    data[0] = X86_ADDRSIZE;
	    data[1] = 32;
	    return 1;
	}
	A "64"	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a prefix in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_ADDRSIZE;
	    data[1] = 64;
	    return 1;
	}

	/* instruction prefixes */
	L O C K		{
	    data[0] = X86_LOCKREP; 
	    data[1] = 0xF0;
	    return 1;
	}
	R E P N E	{
	    data[0] = X86_LOCKREP;
	    data[1] = 0xF2;
	    return 1;
	}
	R E P N Z	{
	    data[0] = X86_LOCKREP;
	    data[1] = 0xF2;
	    return 1;
	}
	R E P		{
	    data[0] = X86_LOCKREP;
	    data[1] = 0xF3;
	    return 1;
	}
	R E P E		{
	    data[0] = X86_LOCKREP;
	    data[1] = 0xF3;
	    return 1;
	}
	R E P Z		{
	    data[0] = X86_LOCKREP;
	    data[1] = 0xF3;
	    return 1;
	}

	/* catchalls */
	[\001-\377]+	{
	    return 0;
	}
	[\000]	{
	    return 0;
	}
    */
    return 0;
}

int
yasm_x86__parse_check_reg(yasm_arch *arch, unsigned long data[1],
			  const char *id, unsigned long line)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    const char *oid = id;
    /*!re2c
	/* control, debug, and test registers */
	C R [02-48]	{
	    if (arch_x86->mode_bits != 64 && oid[2] == '8') {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_CRREG | (oid[2]-'0');
	    return 1;
	}
	D R [0-7]	{
	    data[0] = X86_DRREG | (oid[2]-'0');
	    return 1;
	}
	T R [0-7]	{
	    data[0] = X86_TRREG | (oid[2]-'0');
	    return 1;
	}

	/* floating point, MMX, and SSE/SSE2 registers */
	S T [0-7]	{
	    data[0] = X86_FPUREG | (oid[2]-'0');
	    return 1;
	}
	M M [0-7]	{
	    data[0] = X86_MMXREG | (oid[2]-'0');
	    return 1;
	}
	X M M [0-9]	{
	    if (arch_x86->mode_bits != 64 &&
		(oid[3] == '8' || oid[3] == '9')) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_XMMREG | (oid[3]-'0');
	    return 1;
	}
	X M M "1" [0-5]	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_XMMREG | (10+oid[4]-'0');
	    return 1;
	}

	/* integer registers */
	R A X	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 0;
	    return 1;
	}
	R C X	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 1;
	    return 1;
	}
	R D X	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 2;
	    return 1;
	}
	R B X	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 3;
	    return 1;
	}
	R S P	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 4;
	    return 1;
	}
	R B P	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 5;
	    return 1;
	}
	R S I	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 6;
	    return 1;
	}
	R D I	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 7;
	    return 1;
	}
	R [8-9]	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | (oid[1]-'0');
	    return 1;
	}
	R "1" [0-5] {
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | (10+oid[2]-'0');
	    return 1;
	}

	E A X	{ data[0] = X86_REG32 | 0; return 1; }
	E C X	{ data[0] = X86_REG32 | 1; return 1; }
	E D X	{ data[0] = X86_REG32 | 2; return 1; }
	E B X	{ data[0] = X86_REG32 | 3; return 1; }
	E S P	{ data[0] = X86_REG32 | 4; return 1; }
	E B P	{ data[0] = X86_REG32 | 5; return 1; }
	E S I	{ data[0] = X86_REG32 | 6; return 1; }
	E D I	{ data[0] = X86_REG32 | 7; return 1; }
	R [8-9]	D {
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG32 | (oid[1]-'0');
	    return 1;
	}
	R "1" [0-5] D {
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG32 | (10+oid[2]-'0');
	    return 1;
	}

	A X	{ data[0] = X86_REG16 | 0; return 1; }
	C X	{ data[0] = X86_REG16 | 1; return 1; }
	D X	{ data[0] = X86_REG16 | 2; return 1; }
	B X	{ data[0] = X86_REG16 | 3; return 1; }
	S P	{ data[0] = X86_REG16 | 4; return 1; }
	B P	{ data[0] = X86_REG16 | 5; return 1; }
	S I	{ data[0] = X86_REG16 | 6; return 1; }
	D I	{ data[0] = X86_REG16 | 7; return 1; }
	R [8-9]	W {
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG16 | (oid[1]-'0');
	    return 1;
	}
	R "1" [0-5] W {
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG16 | (10+oid[2]-'0');
	    return 1;
	}

	A L	{ data[0] = X86_REG8 | 0; return 1; }
	C L	{ data[0] = X86_REG8 | 1; return 1; }
	D L	{ data[0] = X86_REG8 | 2; return 1; }
	B L	{ data[0] = X86_REG8 | 3; return 1; }
	A H	{ data[0] = X86_REG8 | 4; return 1; }
	C H	{ data[0] = X86_REG8 | 5; return 1; }
	D H	{ data[0] = X86_REG8 | 6; return 1; }
	B H	{ data[0] = X86_REG8 | 7; return 1; }
	S P L	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG8X | 4;
	    return 1;
	}
	B P L	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG8X | 5;
	    return 1;
	}
	S I L	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG8X | 6;
	    return 1;
	}
	D I L	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG8X | 7;
	    return 1;
	}
	R [8-9]	B {
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG8 | (oid[1]-'0');
	    return 1;
	}
	R "1" [0-5] B {
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG8 | (10+oid[2]-'0');
	    return 1;
	}

	/* RIP for 64-bit mode IP-relative offsets */
	R I P	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_RIP;
	    return 1;
	}

	/* catchalls */
	[\001-\377]+	{
	    return 0;
	}
	[\000]	{
	    return 0;
	}
    */
    return 0;
}

int
yasm_x86__parse_check_reggroup(yasm_arch *arch, unsigned long data[1],
			       const char *id, unsigned long line)
{
    const char *oid = id;
    /*!re2c
	/* floating point, MMX, and SSE/SSE2 registers */
	S T		{
	    data[0] = X86_FPUREG;
	    return 1;
	}
	M M		{
	    data[0] = X86_MMXREG;
	    return 1;
	}
	X M M		{
	    data[0] = X86_XMMREG;
	    return 1;
	}

	/* catchalls */
	[\001-\377]+	{
	    return 0;
	}
	[\000]	{
	    return 0;
	}
    */
    return 0;
}

int
yasm_x86__parse_check_segreg(yasm_arch *arch, unsigned long data[1],
			     const char *id, unsigned long line)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    const char *oid = id;
    /*!re2c
	/* segment registers */
	E S	{
	    if (arch_x86->mode_bits == 64)
		yasm__warning(YASM_WARN_GENERAL, line,
		    N_("`%s' segment register ignored in 64-bit mode"), oid);
	    data[0] = 0x2600;
	    return 1;
	}
	C S	{ data[0] = 0x2e01; return 1; }
	S S	{
	    if (arch_x86->mode_bits == 64)
		yasm__warning(YASM_WARN_GENERAL, line,
		    N_("`%s' segment register ignored in 64-bit mode"), oid);
	    data[0] = 0x3602;
	    return 1;
	}
	D S	{
	    if (arch_x86->mode_bits == 64)
		yasm__warning(YASM_WARN_GENERAL, line,
		    N_("`%s' segment register ignored in 64-bit mode"), oid);
	    data[0] = 0x3e03;
	    return 1;
	}
	F S	{ data[0] = 0x6404; return 1; }
	G S	{ data[0] = 0x6505; return 1; }

	/* catchalls */
	[\001-\377]+	{
	    return 0;
	}
	[\000]	{
	    return 0;
	}
    */
    return 0;
}

#define RET_INSN(nosuffixsize, group, mod, cpu)	do { \
    suffix = (id-oid) > nosuffixsize; \
    DEF_INSN_DATA(group, mod, cpu); \
    goto done; \
    } while (0)

/* No suffix version of RET_INSN */
#define RET_INSN_NS(group, mod, cpu)	do { \
    DEF_INSN_DATA(group, mod, cpu); \
    goto done; \
    } while (0)

#define RET_INSN_GAS(nosuffixsize, group, mod, cpu) do { \
    if (arch_x86->parser != X86_PARSER_GAS) \
	return 0; \
    RET_INSN(nosuffixsize, group, mod, cpu); \
    } while (0)

#define RET_INSN_NONGAS(nosuffixsize, group, mod, cpu) do { \
    if (arch_x86->parser == X86_PARSER_GAS) \
	return 0; \
    RET_INSN(nosuffixsize, group, mod, cpu); \
    } while (0)

int
yasm_x86__parse_check_insn(yasm_arch *arch, unsigned long data[4],
			   const char *id, unsigned long line)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    const char *oid = id;
    /*const char *marker;*/
    int suffix = 0;
    int not64 = 0;
    int warn64 = 0;
    int suffix_ofs = -1;
    char suffix_over = '\0';

    data[3] = 0;

    /*!re2c
	/* instructions */

	/* Move */
	M O V [bBwWlL]? { RET_INSN(3, mov, 0, CPU_Any); }
	M O V A B S [bBwWlLqQ]? { RET_INSN_GAS(6, movabs, 0, CPU_Hammer|CPU_64); }
	/* Move with sign/zero extend */
	M O V S B [wWlL] { suffix_ofs = -2; RET_INSN_GAS(4, movszx, 0xBE, CPU_386); }
	M O V S W L { suffix_ofs = -2; RET_INSN_GAS(4, movszx, 0xBE, CPU_386); }
	M O V S [bBwW] Q {
	    suffix_ofs = -2;
	    warn64 = 1;
	    RET_INSN_GAS(4, movszx, 0xBE, CPU_Hammer|CPU_64);
	}
	M O V S X [bBwW]? { RET_INSN(5, movszx, 0xBE, CPU_386); }
	M O V S L Q {
	    suffix_ofs = -2;
	    warn64 = 1;
	    RET_INSN_GAS(4, movsxd, 0, CPU_Hammer|CPU_64);
	}
	M O V S X D {
	    warn64 = 1;
	    RET_INSN_NONGAS(6, movsxd, 0, CPU_Hammer|CPU_64);
	}
	M O V Z B [wWlL] { suffix_ofs = -2; RET_INSN_GAS(4, movszx, 0xB6, CPU_386); }
	M O V Z W L { suffix_ofs = -2; RET_INSN_GAS(4, movszx, 0xB6, CPU_386); }
	M O V Z [bBwW] Q {
	    suffix_ofs = -2;
	    warn64 = 1;
	    RET_INSN_GAS(4, movszx, 0xB6, CPU_Hammer|CPU_64);
	}
	M O V Z X { RET_INSN(5, movszx, 0xB6, CPU_386); }
	/* Push instructions */
	P U S H [wWlLqQ]? { RET_INSN(4, push, 0, CPU_Any); }
	P U S H A {
	    not64 = 1;
	    RET_INSN(5, onebyte, 0x0060, CPU_186);
	}
	P U S H A D {
	    not64 = 1;
	    RET_INSN_NONGAS(6, onebyte, 0x2060, CPU_386);
	}
	P U S H A L {
	    not64 = 1;
	    RET_INSN_GAS(6, onebyte, 0x2060, CPU_386);
	}
	P U S H A W {
	    not64 = 1;
	    RET_INSN(6, onebyte, 0x1060, CPU_186);
	}
	/* Pop instructions */
	P O P [wWlLqQ]? { RET_INSN(3, pop, 0, CPU_Any); }
	P O P A {
	    not64 = 1;
	    RET_INSN(4, onebyte, 0x0061, CPU_186);
	}
	P O P A D {
	    not64 = 1;
	    RET_INSN_NONGAS(5, onebyte, 0x2061, CPU_386);
	}
	P O P A L {
	    not64 = 1;
	    RET_INSN_GAS(5, onebyte, 0x2061, CPU_386);
	}
	P O P A W {
	    not64 = 1;
	    RET_INSN(5, onebyte, 0x1061, CPU_186);
	}
	/* Exchange */
	X C H G [bBwWlLqQ]? { RET_INSN(4, xchg, 0, CPU_Any); }
	/* In/out from ports */
	I N [bBwWlL]? { RET_INSN(2, in, 0, CPU_Any); }
	O U T [bBwWlL]? { RET_INSN(3, out, 0, CPU_Any); }
	/* Load effective address */
	L E A [wWlLqQ]? { RET_INSN(3, lea, 0, CPU_Any); }
	/* Load segment registers from memory */
	L D S [wWlL]? {
	    not64 = 1;
	    RET_INSN(3, ldes, 0xC5, CPU_Any);
	}
	L E S [wWlL]? {
	    not64 = 1;
	    RET_INSN(3, ldes, 0xC4, CPU_Any);
	}
	L F S [wWlL]? { RET_INSN(3, lfgss, 0xB4, CPU_386); }
	L G S [wWlL]? { RET_INSN(3, lfgss, 0xB5, CPU_386); }
	L S S [wWlL]? { RET_INSN(3, lfgss, 0xB2, CPU_386); }
	/* Flags register instructions */
	C L C { RET_INSN(3, onebyte, 0x00F8, CPU_Any); }
	C L D { RET_INSN(3, onebyte, 0x00FC, CPU_Any); }
	C L I { RET_INSN(3, onebyte, 0x00FA, CPU_Any); }
	C L T S { RET_INSN(4, twobyte, 0x0F06, CPU_286|CPU_Priv); }
	C M C { RET_INSN(3, onebyte, 0x00F5, CPU_Any); }
	L A H F { RET_INSN(4, onebyte, 0x009F, CPU_Any); }
	S A H F { RET_INSN(4, onebyte, 0x009E, CPU_Any); }
	P U S H F { RET_INSN(5, onebyte, 0x009C, CPU_Any); }
	P U S H F D { RET_INSN_NONGAS(6, onebyte, 0x209C, CPU_386); }
	P U S H F L { RET_INSN_GAS(6, onebyte, 0x209C, CPU_386); }
	P U S H F W { RET_INSN(6, onebyte, 0x109C, CPU_Any); }
	P U S H F Q {
	    warn64 = 1;
	    RET_INSN(6, onebyte, 0x409C, CPU_Hammer|CPU_64);
	}
	P O P F { RET_INSN(4, onebyte, 0x40009D, CPU_Any); }
	P O P F D {
	    not64 = 1;
	    RET_INSN_NONGAS(5, onebyte, 0x00209D, CPU_386);
	}
	P O P F L {
	    not64 = 1;
	    RET_INSN_GAS(5, onebyte, 0x00209D, CPU_386);
	}
	P O P F W { RET_INSN(5, onebyte, 0x40109D, CPU_Any); }
	P O P F Q {
	    warn64 = 1;
	    RET_INSN(5, onebyte, 0x40409D, CPU_Hammer|CPU_64);
	}
	S T C { RET_INSN(3, onebyte, 0x00F9, CPU_Any); }
	S T D { RET_INSN(3, onebyte, 0x00FD, CPU_Any); }
	S T I { RET_INSN(3, onebyte, 0x00FB, CPU_Any); }
	/* Arithmetic */
	A D D [bBwWlLqQ]? { RET_INSN(3, arith, 0x0000, CPU_Any); }
	I N C [bBwWlLqQ]? { RET_INSN(3, incdec, 0x0040, CPU_Any); }
	S U B [bBwWlLqQ]? { RET_INSN(3, arith, 0x0528, CPU_Any); }
	D E C [bBwWlLqQ]? { RET_INSN(3, incdec, 0x0148, CPU_Any); }
	S B B [bBwWlLqQ]? { RET_INSN(3, arith, 0x0318, CPU_Any); }
	C M P [bBwWlLqQ]? { RET_INSN(3, arith, 0x0738, CPU_Any); }
	T E S T [bBwWlLqQ]? { RET_INSN(4, test, 0, CPU_Any); }
	A N D [bBwWlLqQ]? { RET_INSN(3, arith, 0x0420, CPU_Any); }
	O R [bBwWlLqQ]? { RET_INSN(2, arith, 0x0108, CPU_Any); }
	X O R [bBwWlLqQ]? { RET_INSN(3, arith, 0x0630, CPU_Any); }
	A D C [bBwWlLqQ]? { RET_INSN(3, arith, 0x0210, CPU_Any); }
	N E G [bBwWlLqQ]? { RET_INSN(3, f6, 0x03, CPU_Any); }
	N O T [bBwWlLqQ]? { RET_INSN(3, f6, 0x02, CPU_Any); }
	A A A {
	    not64 = 1;
	    RET_INSN(3, onebyte, 0x0037, CPU_Any);
	}
	A A S {
	    not64 = 1;
	    RET_INSN(3, onebyte, 0x003F, CPU_Any);
	}
	D A A {
	    not64 = 1;
	    RET_INSN(3, onebyte, 0x0027, CPU_Any);
	}
	D A S {
	    not64 = 1;
	    RET_INSN(3, onebyte, 0x002F, CPU_Any);
	}
	A A D {
	    not64 = 1;
	    RET_INSN(3, aadm, 0x01, CPU_Any);
	}
	A A M {
	    not64 = 1;
	    RET_INSN(3, aadm, 0x00, CPU_Any);
	}
	/* Conversion instructions */
	C B W { RET_INSN(3, onebyte, 0x1098, CPU_Any); }
	C W D E { RET_INSN(4, onebyte, 0x2098, CPU_386); }
	C D Q E {
	    warn64 = 1;
	    RET_INSN(4, onebyte, 0x4098, CPU_Hammer|CPU_64);
	}
	C W D { RET_INSN(3, onebyte, 0x1099, CPU_Any); }
	C D Q { RET_INSN(3, onebyte, 0x2099, CPU_386); }
	C Q O {
	    warn64 = 1;
	    RET_INSN(3, onebyte, 0x4099, CPU_Hammer|CPU_64);
	}
	/* Conversion instructions - GAS / AT&T naming */
	C B T W { RET_INSN_GAS(4, onebyte, 0x1098, CPU_Any); }
	C W T L { RET_INSN_GAS(4, onebyte, 0x2098, CPU_386); }
	C L T Q {
	    warn64 = 1;
	    RET_INSN_GAS(4, onebyte, 0x4098, CPU_Hammer|CPU_64);
	}
	C W T D { RET_INSN_GAS(4, onebyte, 0x1099, CPU_Any); }
	C L T D { RET_INSN_GAS(4, onebyte, 0x2099, CPU_386); }
	C Q T O {
	    warn64 = 1;
	    RET_INSN_GAS(4, onebyte, 0x4099, CPU_Hammer|CPU_64);
	}
	/* Multiplication and division */
	M U L [bBwWlLqQ]? { RET_INSN(3, f6, 0x04, CPU_Any); }
	I M U L [bBwWlLqQ]? { RET_INSN(4, imul, 0, CPU_Any); }
	D I V [bBwWlLqQ]? { RET_INSN(3, div, 0x06, CPU_Any); }
	I D I V [bBwWlLqQ]? { RET_INSN(4, div, 0x07, CPU_Any); }
	/* Shifts */
	R O L [bBwWlLqQ]? { RET_INSN(3, shift, 0x00, CPU_Any); }
	R O R [bBwWlLqQ]? { RET_INSN(3, shift, 0x01, CPU_Any); }
	R C L [bBwWlLqQ]? { RET_INSN(3, shift, 0x02, CPU_Any); }
	R C R [bBwWlLqQ]? { RET_INSN(3, shift, 0x03, CPU_Any); }
	S A L [bBwWlLqQ]? { RET_INSN(3, shift, 0x04, CPU_Any); }
	S H L [bBwWlLqQ]? { RET_INSN(3, shift, 0x04, CPU_Any); }
	S H R [bBwWlLqQ]? { RET_INSN(3, shift, 0x05, CPU_Any); }
	S A R [bBwWlLqQ]? { RET_INSN(3, shift, 0x07, CPU_Any); }
	S H L D [wWlLqQ]? { RET_INSN(4, shlrd, 0xA4, CPU_386); }
	S H R D [wWlLqQ]? { RET_INSN(4, shlrd, 0xAC, CPU_386); }
	/* Control transfer instructions (unconditional) */
	C A L L { RET_INSN(4, call, 0, CPU_Any); }
	J M P { RET_INSN(3, jmp, 0, CPU_Any); }
	R E T W? { RET_INSN(3, retnf, 0xC2, CPU_Any); }
	R E T L {
	    not64 = 1;
	    RET_INSN_GAS(3, retnf, 0xC2, CPU_Any);
	}
	R E T Q {
	    warn64 = 1;
	    RET_INSN_GAS(3, retnf, 0xC2, CPU_Hammer|CPU_64);
	}
	R E T N { RET_INSN_NONGAS(4, retnf, 0xC2, CPU_Any); }
	R E T F { RET_INSN_NONGAS(4, retnf, 0xCA, CPU_Any); }
	L R E T W { RET_INSN_GAS(4, retnf, 0xCA, CPU_Any); }
	L R E T L {
	    not64 = 1;
	    RET_INSN_GAS(4, retnf, 0xCA, CPU_Any);
	}
	L R E T Q {
	    warn64 = 1;
	    RET_INSN_GAS(4, retnf, 0xCA, CPU_Any);
	}
	E N T E R [wWlLqQ]? { RET_INSN(5, enter, 0, CPU_186); }
	L E A V E { RET_INSN(5, onebyte, 0x4000C9, CPU_186); }
	L E A V E [wW] { RET_INSN_GAS(6, onebyte, 0x0010C9, CPU_186); }
	L E A V E [lLqQ] { RET_INSN_GAS(6, onebyte, 0x4000C9, CPU_186); }
	/* Conditional jumps */
	J O { RET_INSN(2, jcc, 0x00, CPU_Any); }
	J N O { RET_INSN(3, jcc, 0x01, CPU_Any); }
	J B { RET_INSN(2, jcc, 0x02, CPU_Any); }
	J C { RET_INSN(2, jcc, 0x02, CPU_Any); }
	J N A E { RET_INSN(4, jcc, 0x02, CPU_Any); }
	J N B { RET_INSN(3, jcc, 0x03, CPU_Any); }
	J N C { RET_INSN(3, jcc, 0x03, CPU_Any); }
	J A E { RET_INSN(3, jcc, 0x03, CPU_Any); }
	J E { RET_INSN(2, jcc, 0x04, CPU_Any); }
	J Z { RET_INSN(2, jcc, 0x04, CPU_Any); }
	J N E { RET_INSN(3, jcc, 0x05, CPU_Any); }
	J N Z { RET_INSN(3, jcc, 0x05, CPU_Any); }
	J B E { RET_INSN(3, jcc, 0x06, CPU_Any); }
	J N A { RET_INSN(3, jcc, 0x06, CPU_Any); }
	J N B E { RET_INSN(4, jcc, 0x07, CPU_Any); }
	J A { RET_INSN(2, jcc, 0x07, CPU_Any); }
	J S { RET_INSN(2, jcc, 0x08, CPU_Any); }
	J N S { RET_INSN(3, jcc, 0x09, CPU_Any); }
	J P { RET_INSN(2, jcc, 0x0A, CPU_Any); }
	J P E { RET_INSN(3, jcc, 0x0A, CPU_Any); }
	J N P { RET_INSN(3, jcc, 0x0B, CPU_Any); }
	J P O { RET_INSN(3, jcc, 0x0B, CPU_Any); }
	J L { RET_INSN(2, jcc, 0x0C, CPU_Any); }
	J N G E { RET_INSN(4, jcc, 0x0C, CPU_Any); }
	J N L { RET_INSN(3, jcc, 0x0D, CPU_Any); }
	J G E { RET_INSN(3, jcc, 0x0D, CPU_Any); }
	J L E { RET_INSN(3, jcc, 0x0E, CPU_Any); }
	J N G { RET_INSN(3, jcc, 0x0E, CPU_Any); }
	J N L E { RET_INSN(3, jcc, 0x0F, CPU_Any); }
	J G { RET_INSN(2, jcc, 0x0F, CPU_Any); }
	J C X Z { RET_INSN(4, jcxz, 16, CPU_Any); }
	J E C X Z { RET_INSN(5, jcxz, 32, CPU_386); }
	J R C X Z {
	    warn64 = 1;
	    RET_INSN(5, jcxz, 64, CPU_Hammer|CPU_64);
	}
	/* Loop instructions */
	L O O P { RET_INSN(4, loop, 0x02, CPU_Any); }
	L O O P Z { RET_INSN(5, loop, 0x01, CPU_Any); }
	L O O P E { RET_INSN(5, loop, 0x01, CPU_Any); }
	L O O P N Z { RET_INSN(6, loop, 0x00, CPU_Any); }
	L O O P N E { RET_INSN(6, loop, 0x00, CPU_Any); }
	/* Set byte on flag instructions */
	S E T O B? { RET_INSN(4, setcc, 0x00, CPU_386); }
	S E T N O B? { RET_INSN(5, setcc, 0x01, CPU_386); }
	S E T B B? { RET_INSN(4, setcc, 0x02, CPU_386); }
	S E T C B? { RET_INSN(4, setcc, 0x02, CPU_386); }
	S E T N A E B? { RET_INSN(6, setcc, 0x02, CPU_386); }
	S E T N B B? { RET_INSN(5, setcc, 0x03, CPU_386); }
	S E T N C B? { RET_INSN(5, setcc, 0x03, CPU_386); }
	S E T A E B? { RET_INSN(5, setcc, 0x03, CPU_386); }
	S E T E B? { RET_INSN(4, setcc, 0x04, CPU_386); }
	S E T Z B? { RET_INSN(4, setcc, 0x04, CPU_386); }
	S E T N E B? { RET_INSN(5, setcc, 0x05, CPU_386); }
	S E T N Z B? { RET_INSN(5, setcc, 0x05, CPU_386); }
	S E T B E B? { RET_INSN(5, setcc, 0x06, CPU_386); }
	S E T N A B? { RET_INSN(5, setcc, 0x06, CPU_386); }
	S E T N B E B? { RET_INSN(6, setcc, 0x07, CPU_386); }
	S E T A B? { RET_INSN(4, setcc, 0x07, CPU_386); }
	S E T S B? { RET_INSN(4, setcc, 0x08, CPU_386); }
	S E T N S B? { RET_INSN(5, setcc, 0x09, CPU_386); }
	S E T P B? { RET_INSN(4, setcc, 0x0A, CPU_386); }
	S E T P E B? { RET_INSN(5, setcc, 0x0A, CPU_386); }
	S E T N P B? { RET_INSN(5, setcc, 0x0B, CPU_386); }
	S E T P O B? { RET_INSN(5, setcc, 0x0B, CPU_386); }
	S E T L B? { RET_INSN(4, setcc, 0x0C, CPU_386); }
	S E T N G E B? { RET_INSN(6, setcc, 0x0C, CPU_386); }
	S E T N L B? { RET_INSN(5, setcc, 0x0D, CPU_386); }
	S E T G E B? { RET_INSN(5, setcc, 0x0D, CPU_386); }
	S E T L E B? { RET_INSN(5, setcc, 0x0E, CPU_386); }
	S E T N G B? { RET_INSN(5, setcc, 0x0E, CPU_386); }
	S E T N L E B? { RET_INSN(6, setcc, 0x0F, CPU_386); }
	S E T G B? { RET_INSN(4, setcc, 0x0F, CPU_386); }
	/* String instructions. */
	C M P S B { RET_INSN(5, onebyte, 0x00A6, CPU_Any); }
	C M P S W { RET_INSN(5, onebyte, 0x10A7, CPU_Any); }
	C M P S D { RET_INSN(5, cmpsd, 0, CPU_Any); }
	C M P S L { RET_INSN_GAS(5, onebyte, 0x20A7, CPU_386); }
	C M P S Q {
	    warn64 = 1;
	    RET_INSN(5, onebyte, 0x40A7, CPU_Hammer|CPU_64);
	}
	I N S B { RET_INSN(4, onebyte, 0x006C, CPU_Any); }
	I N S W { RET_INSN(4, onebyte, 0x106D, CPU_Any); }
	I N S D { RET_INSN_NONGAS(4, onebyte, 0x206D, CPU_386); }
	I N S L { RET_INSN_GAS(4, onebyte, 0x206D, CPU_386); }
	O U T S B { RET_INSN(5, onebyte, 0x006E, CPU_Any); }
	O U T S W { RET_INSN(5, onebyte, 0x106F, CPU_Any); }
	O U T S D { RET_INSN_NONGAS(5, onebyte, 0x206F, CPU_386); }
	O U T S L { RET_INSN_GAS(5, onebyte, 0x206F, CPU_386); }
	L O D S B { RET_INSN(5, onebyte, 0x00AC, CPU_Any); }
	L O D S W { RET_INSN(5, onebyte, 0x10AD, CPU_Any); }
	L O D S D { RET_INSN_NONGAS(5, onebyte, 0x20AD, CPU_386); }
	L O D S L { RET_INSN_GAS(5, onebyte, 0x20AD, CPU_386); }
	L O D S Q {
	    warn64 = 1;
	    RET_INSN(5, onebyte, 0x40AD, CPU_Hammer|CPU_64);
	}
	M O V S B { RET_INSN(5, onebyte, 0x00A4, CPU_Any); }
	M O V S W { RET_INSN(5, onebyte, 0x10A5, CPU_Any); }
	M O V S D { RET_INSN(5, movsd, 0, CPU_Any); }
	M O V S L { RET_INSN_GAS(5, onebyte, 0x20A5, CPU_386); }
	M O V S Q {
	    warn64 = 1;
	    RET_INSN(5, onebyte, 0x40A5, CPU_Any);
	}
	/* smov alias for movs in GAS mode */
	S M O V B { RET_INSN_GAS(5, onebyte, 0x00A4, CPU_Any); }
	S M O V W { RET_INSN_GAS(5, onebyte, 0x10A5, CPU_Any); }
	S M O V L { RET_INSN_GAS(5, onebyte, 0x20A5, CPU_386); }
	S M O V Q {
	    warn64 = 1;
	    RET_INSN_GAS(5, onebyte, 0x40A5, CPU_Any);
	}
	S C A S B { RET_INSN(5, onebyte, 0x00AE, CPU_Any); }
	S C A S W { RET_INSN(5, onebyte, 0x10AF, CPU_Any); }
	S C A S D { RET_INSN_NONGAS(5, onebyte, 0x20AF, CPU_386); }
	S C A S L { RET_INSN_GAS(5, onebyte, 0x20AF, CPU_386); }
	S C A S Q {
	    warn64 = 1;
	    RET_INSN(5, onebyte, 0x40AF, CPU_Hammer|CPU_64);
	}
	/* ssca alias for scas in GAS mode */
	S S C A B { RET_INSN_GAS(5, onebyte, 0x00AE, CPU_Any); }
	S S C A W { RET_INSN_GAS(5, onebyte, 0x10AF, CPU_Any); }
	S S C A L { RET_INSN_GAS(5, onebyte, 0x20AF, CPU_386); }
	S S C A Q {
	    warn64 = 1;
	    RET_INSN_GAS(5, onebyte, 0x40AF, CPU_Hammer|CPU_64);
	}
	S T O S B { RET_INSN(5, onebyte, 0x00AA, CPU_Any); }
	S T O S W { RET_INSN(5, onebyte, 0x10AB, CPU_Any); }
	S T O S D { RET_INSN_NONGAS(5, onebyte, 0x20AB, CPU_386); }
	S T O S L { RET_INSN_GAS(5, onebyte, 0x20AB, CPU_386); }
	S T O S Q {
	    warn64 = 1;
	    RET_INSN(5, onebyte, 0x40AB, CPU_Hammer|CPU_64);
	}
	X L A T B? { RET_INSN(5, onebyte, 0x00D7, CPU_Any); }
	/* Bit manipulation */
	B S F [wWlLqQ]? { RET_INSN(3, bsfr, 0xBC, CPU_386); }
	B S R [wWlLqQ]? { RET_INSN(3, bsfr, 0xBD, CPU_386); }
	B T [wWlLqQ]? { RET_INSN(2, bittest, 0x04A3, CPU_386); }
	B T C [wWlLqQ]? { RET_INSN(3, bittest, 0x07BB, CPU_386); }
	B T R [wWlLqQ]? { RET_INSN(3, bittest, 0x06B3, CPU_386); }
	B T S [wWlLqQ]? { RET_INSN(3, bittest, 0x05AB, CPU_386); }
	/* Interrupts and operating system instructions */
	I N T { RET_INSN(3, int, 0, CPU_Any); }
	I N T "3" { RET_INSN(4, onebyte, 0x00CC, CPU_Any); }
	I N T "03" { RET_INSN_NONGAS(5, onebyte, 0x00CC, CPU_Any); }
	I N T O {
	    not64 = 1;
	    RET_INSN(4, onebyte, 0x00CE, CPU_Any);
	}
	I R E T { RET_INSN(4, onebyte, 0x00CF, CPU_Any); }
	I R E T W { RET_INSN(5, onebyte, 0x10CF, CPU_Any); }
	I R E T D { RET_INSN_NONGAS(5, onebyte, 0x20CF, CPU_386); }
	I R E T L { RET_INSN_GAS(5, onebyte, 0x20CF, CPU_386); }
	I R E T Q {
	    warn64 = 1;
	    RET_INSN(5, onebyte, 0x40CF, CPU_Hammer|CPU_64);
	}
	R S M { RET_INSN(3, twobyte, 0x0FAA, CPU_586|CPU_SMM); }
	B O U N D [wWlL]? {
	    not64 = 1;
	    RET_INSN(5, bound, 0, CPU_186);
	}
	H L T { RET_INSN(3, onebyte, 0x00F4, CPU_Priv); }
	N O P { RET_INSN(3, onebyte, 0x0090, CPU_Any); }
	/* Protection control */
	A R P L W? {
	    not64 = 1;
	    RET_INSN(4, arpl, 0, CPU_286|CPU_Prot);
	}
	L A R [wWlLqQ]? { RET_INSN(3, bsfr, 0x02, CPU_286|CPU_Prot); }
	L G D T [wWlLqQ]? { RET_INSN(4, twobytemem, 0x020F01, CPU_286|CPU_Priv); }
	L I D T [wWlLqQ]? { RET_INSN(4, twobytemem, 0x030F01, CPU_286|CPU_Priv); }
	L L D T W? { RET_INSN(4, prot286, 0x0200, CPU_286|CPU_Prot|CPU_Priv); }
	L M S W W? { RET_INSN(4, prot286, 0x0601, CPU_286|CPU_Priv); }
	L S L [wWlLqQ]? { RET_INSN(3, bsfr, 0x03, CPU_286|CPU_Prot); }
	L T R W? { RET_INSN(3, prot286, 0x0300, CPU_286|CPU_Prot|CPU_Priv); }
	S G D T [wWlLqQ]? { RET_INSN(4, twobytemem, 0x000F01, CPU_286|CPU_Priv); }
	S I D T [wWlLqQ]? { RET_INSN(4, twobytemem, 0x010F01, CPU_286|CPU_Priv); }
	S L D T [wWlLqQ]? { RET_INSN(4, sldtmsw, 0x0000, CPU_286); }
	S M S W [wWlLqQ]? { RET_INSN(4, sldtmsw, 0x0401, CPU_286); }
	S T R [wWlLqQ]? { RET_INSN(3, str, 0, CPU_286|CPU_Prot); }
	V E R R W? { RET_INSN(4, prot286, 0x0400, CPU_286|CPU_Prot); }
	V E R W W? { RET_INSN(4, prot286, 0x0500, CPU_286|CPU_Prot); }
	/* Floating point instructions */
	F L D [lLsS]? { RET_INSN(3, fldstp, 0x0500C0, CPU_FPU); }
	F L D T {
	    data[3] |= 0x80 << 8;
	    RET_INSN_GAS(4, fldstpt, 0x0500C0, CPU_FPU);
	}
	F I L D [lLqQsS]? { RET_INSN(4, fildstp, 0x050200, CPU_FPU); }
	F I L D L L { RET_INSN_GAS(6, fbldstp, 0x05, CPU_FPU); }
	F B L D { RET_INSN(4, fbldstp, 0x04, CPU_FPU); }
	F S T [lLsS]? { RET_INSN(3, fst, 0, CPU_FPU); }
	F I S T [lLsS]? { RET_INSN(4, fiarith, 0x02DB, CPU_FPU); }
	F S T P [lLsS]? { RET_INSN(4, fldstp, 0x0703D8, CPU_FPU); }
	F S T P T {
	    data[3] |= 0x80 << 8;
	    RET_INSN_GAS(5, fldstpt, 0x0703D8, CPU_FPU);
	}
	F I S T P [lLqQsS]? { RET_INSN(5, fildstp, 0x070203, CPU_FPU); }
	F I S T P L L { RET_INSN_GAS(7, fbldstp, 0x07, CPU_FPU); }
	F B S T P { RET_INSN(5, fbldstp, 0x06, CPU_FPU); }
	F X C H { RET_INSN(4, fxch, 0, CPU_FPU); }
	F C O M [lLsS]? { RET_INSN(4, fcom, 0x02D0, CPU_FPU); }
	F I C O M [lLsS]? { RET_INSN(5, fiarith, 0x02DA, CPU_FPU); }
	F C O M P [lLsS]? { RET_INSN(5, fcom, 0x03D8, CPU_FPU); }
	F I C O M P [lLsS]? { RET_INSN(6, fiarith, 0x03DA, CPU_FPU); }
	F C O M P P { RET_INSN(6, twobyte, 0xDED9, CPU_FPU); }
	F U C O M { RET_INSN(5, fcom2, 0xDDE0, CPU_286|CPU_FPU); }
	F U C O M P { RET_INSN(6, fcom2, 0xDDE8, CPU_286|CPU_FPU); }
	F U C O M P P { RET_INSN(7, twobyte, 0xDAE9, CPU_286|CPU_FPU); }
	F T S T { RET_INSN(4, twobyte, 0xD9E4, CPU_FPU); }
	F X A M { RET_INSN(4, twobyte, 0xD9E5, CPU_FPU); }
	F L D "1" { RET_INSN(4, twobyte, 0xD9E8, CPU_FPU); }
	F L D L "2" T { RET_INSN(6, twobyte, 0xD9E9, CPU_FPU); }
	F L D L "2" E { RET_INSN(6, twobyte, 0xD9EA, CPU_FPU); }
	F L D P I { RET_INSN(5, twobyte, 0xD9EB, CPU_FPU); }
	F L D L G "2" { RET_INSN(6, twobyte, 0xD9EC, CPU_FPU); }
	F L D L N "2" { RET_INSN(6, twobyte, 0xD9ED, CPU_FPU); }
	F L D Z { RET_INSN(4, twobyte, 0xD9EE, CPU_FPU); }
	F A D D [lLsS]? { RET_INSN(4, farith, 0x00C0C0, CPU_FPU); }
	F A D D P { RET_INSN(5, farithp, 0xC0, CPU_FPU); }
	F I A D D [lLsS]? { RET_INSN(5, fiarith, 0x00DA, CPU_FPU); }
	F S U B [lLsS]? { RET_INSN(4, farith, 0x04E0E8, CPU_FPU); }
	F I S U B [lLsS]? { RET_INSN(5, fiarith, 0x04DA, CPU_FPU); }
	F S U B P { RET_INSN(5, farithp, 0xE8, CPU_FPU); }
	F S U B R [lLsS]? { RET_INSN(5, farith, 0x05E8E0, CPU_FPU); }
	F I S U B R [lLsS]? { RET_INSN(6, fiarith, 0x05DA, CPU_FPU); }
	F S U B R P { RET_INSN(6, farithp, 0xE0, CPU_FPU); }
	F M U L [lLsS]? { RET_INSN(4, farith, 0x01C8C8, CPU_FPU); }
	F I M U L [lLsS]? { RET_INSN(5, fiarith, 0x01DA, CPU_FPU); }
	F M U L P { RET_INSN(5, farithp, 0xC8, CPU_FPU); }
	F D I V [lLsS]? { RET_INSN(4, farith, 0x06F0F8, CPU_FPU); }
	F I D I V [lLsS]? { RET_INSN(5, fiarith, 0x06DA, CPU_FPU); }
	F D I V P { RET_INSN(5, farithp, 0xF8, CPU_FPU); }
	F D I V R [lLsS]? { RET_INSN(5, farith, 0x07F8F0, CPU_FPU); }
	F I D I V R [lLsS]? { RET_INSN(6, fiarith, 0x07DA, CPU_FPU); }
	F D I V R P { RET_INSN(6, farithp, 0xF0, CPU_FPU); }
	F "2" X M "1" { RET_INSN(5, twobyte, 0xD9F0, CPU_FPU); }
	F Y L "2" X { RET_INSN(5, twobyte, 0xD9F1, CPU_FPU); }
	F P T A N { RET_INSN(5, twobyte, 0xD9F2, CPU_FPU); }
	F P A T A N { RET_INSN(6, twobyte, 0xD9F3, CPU_FPU); }
	F X T R A C T { RET_INSN(7, twobyte, 0xD9F4, CPU_FPU); }
	F P R E M "1" { RET_INSN(6, twobyte, 0xD9F5, CPU_286|CPU_FPU); }
	F D E C S T P { RET_INSN(7, twobyte, 0xD9F6, CPU_FPU); }
	F I N C S T P { RET_INSN(7, twobyte, 0xD9F7, CPU_FPU); }
	F P R E M { RET_INSN(5, twobyte, 0xD9F8, CPU_FPU); }
	F Y L "2" X P "1" { RET_INSN(7, twobyte, 0xD9F9, CPU_FPU); }
	F S Q R T { RET_INSN(5, twobyte, 0xD9FA, CPU_FPU); }
	F S I N C O S { RET_INSN(7, twobyte, 0xD9FB, CPU_286|CPU_FPU); }
	F R N D I N T { RET_INSN(7, twobyte, 0xD9FC, CPU_FPU); }
	F S C A L E { RET_INSN(6, twobyte, 0xD9FD, CPU_FPU); }
	F S I N { RET_INSN(4, twobyte, 0xD9FE, CPU_286|CPU_FPU); }
	F C O S { RET_INSN(4, twobyte, 0xD9FF, CPU_286|CPU_FPU); }
	F C H S { RET_INSN(4, twobyte, 0xD9E0, CPU_FPU); }
	F A B S { RET_INSN(4, twobyte, 0xD9E1, CPU_FPU); }
	F N I N I T { RET_INSN(6, twobyte, 0xDBE3, CPU_FPU); }
	F I N I T { RET_INSN(5, threebyte, 0x9BDBE3UL, CPU_FPU); }
	F L D C W W? { RET_INSN(5, fldnstcw, 0x05, CPU_FPU); }
	F N S T C W W? { RET_INSN(6, fldnstcw, 0x07, CPU_FPU); }
	F S T C W W? { RET_INSN(5, fstcw, 0, CPU_FPU); }
	F N S T S W W? { RET_INSN(6, fnstsw, 0, CPU_FPU); }
	F S T S W W? { RET_INSN(5, fstsw, 0, CPU_FPU); }
	F N C L E X { RET_INSN(6, twobyte, 0xDBE2, CPU_FPU); }
	F C L E X { RET_INSN(5, threebyte, 0x9BDBE2UL, CPU_FPU); }
	F N S T E N V [lLsS]? { RET_INSN(7, onebytemem, 0x06D9, CPU_FPU); }
	F S T E N V [lLsS]? { RET_INSN(6, twobytemem, 0x069BD9, CPU_FPU); }
	F L D E N V [lLsS]? { RET_INSN(6, onebytemem, 0x04D9, CPU_FPU); }
	F N S A V E [lLsS]? { RET_INSN(6, onebytemem, 0x06DD, CPU_FPU); }
	F S A V E [lLsS]? { RET_INSN(5, twobytemem, 0x069BDD, CPU_FPU); }
	F R S T O R [lLsS]? { RET_INSN(6, onebytemem, 0x04DD, CPU_FPU); }
	F F R E E { RET_INSN(5, ffree, 0xDD, CPU_FPU); }
	F F R E E P { RET_INSN(6, ffree, 0xDF, CPU_686|CPU_FPU|CPU_Undoc); }
	F N O P { RET_INSN(4, twobyte, 0xD9D0, CPU_FPU); }
	F W A I T { RET_INSN(5, onebyte, 0x009B, CPU_FPU); }
	/* Prefixes (should the others be here too? should wait be a prefix? */
	W A I T { RET_INSN(4, onebyte, 0x009B, CPU_Any); }
	/* 486 extensions */
	B S W A P [lLqQ]? { RET_INSN(5, bswap, 0, CPU_486); }
	X A D D [bBwWlLqQ]? { RET_INSN(4, cmpxchgxadd, 0xC0, CPU_486); }
	C M P X C H G [bBwWlLqQ]? { RET_INSN(7, cmpxchgxadd, 0xB0, CPU_486); }
	C M P X C H G "486" { RET_INSN_NONGAS(10, cmpxchgxadd, 0xA6, CPU_486|CPU_Undoc); }
	I N V D { RET_INSN(4, twobyte, 0x0F08, CPU_486|CPU_Priv); }
	W B I N V D { RET_INSN(6, twobyte, 0x0F09, CPU_486|CPU_Priv); }
	I N V L P G { RET_INSN(6, twobytemem, 0x070F01, CPU_486|CPU_Priv); }
	/* 586+ and late 486 extensions */
	C P U I D { RET_INSN(5, twobyte, 0x0FA2, CPU_486); }
	/* Pentium extensions */
	W R M S R { RET_INSN(5, twobyte, 0x0F30, CPU_586|CPU_Priv); }
	R D T S C { RET_INSN(5, twobyte, 0x0F31, CPU_586); }
	R D M S R { RET_INSN(5, twobyte, 0x0F32, CPU_586|CPU_Priv); }
	C M P X C H G "8" B Q? { RET_INSN(9, cmpxchg8b, 0, CPU_586); }
	/* Pentium II/Pentium Pro extensions */
	S Y S E N T E R {
	    not64 = 1;
	    RET_INSN(8, twobyte, 0x0F34, CPU_686);
	}
	S Y S E X I T {
	    not64 = 1;
	    RET_INSN(7, twobyte, 0x0F35, CPU_686|CPU_Priv);
	}
	F X S A V E Q? { RET_INSN(6, twobytemem, 0x000FAE, CPU_686|CPU_FPU); }
	F X R S T O R Q? { RET_INSN(7, twobytemem, 0x010FAE, CPU_686|CPU_FPU); }
	R D P M C { RET_INSN(5, twobyte, 0x0F33, CPU_686); }
	U D "2" { RET_INSN(3, twobyte, 0x0F0B, CPU_286); }
	U D "1" { RET_INSN(3, twobyte, 0x0FB9, CPU_286|CPU_Undoc); }
	C M O V O [wWlLqQ]? { RET_INSN(5, cmovcc, 0x00, CPU_686); }
	C M O V N O [wWlLqQ]? { RET_INSN(6, cmovcc, 0x01, CPU_686); }
	C M O V B [wWlLqQ]? { RET_INSN(5, cmovcc, 0x02, CPU_686); }
	C M O V C [wWlLqQ]? { RET_INSN(5, cmovcc, 0x02, CPU_686); }
	C M O V N A E [wWlLqQ]? { RET_INSN(7, cmovcc, 0x02, CPU_686); }
	C M O V N B [wWlLqQ]? { RET_INSN(6, cmovcc, 0x03, CPU_686); }
	C M O V N C [wWlLqQ]? { RET_INSN(6, cmovcc, 0x03, CPU_686); }
	C M O V A E [wWlLqQ]? { RET_INSN(6, cmovcc, 0x03, CPU_686); }
	C M O V E [wWlLqQ]? { RET_INSN(5, cmovcc, 0x04, CPU_686); }
	C M O V Z [wWlLqQ]? { RET_INSN(5, cmovcc, 0x04, CPU_686); }
	C M O V N E [wWlLqQ]? { RET_INSN(6, cmovcc, 0x05, CPU_686); }
	C M O V N Z [wWlLqQ]? { RET_INSN(6, cmovcc, 0x05, CPU_686); }
	C M O V B E [wWlLqQ]? { RET_INSN(6, cmovcc, 0x06, CPU_686); }
	C M O V N A [wWlLqQ]? { RET_INSN(6, cmovcc, 0x06, CPU_686); }
	C M O V N B E [wWlLqQ]? { RET_INSN(7, cmovcc, 0x07, CPU_686); }
	C M O V A [wWlLqQ]? { RET_INSN(5, cmovcc, 0x07, CPU_686); }
	C M O V S [wWlLqQ]? { RET_INSN(5, cmovcc, 0x08, CPU_686); }
	C M O V N S [wWlLqQ]? { RET_INSN(6, cmovcc, 0x09, CPU_686); }
	C M O V P [wWlLqQ]? { RET_INSN(5, cmovcc, 0x0A, CPU_686); }
	C M O V P E [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0A, CPU_686); }
	C M O V N P [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0B, CPU_686); }
	C M O V P O [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0B, CPU_686); }
	C M O V L [wWlLqQ]? { RET_INSN(5, cmovcc, 0x0C, CPU_686); }
	C M O V N G E [wWlLqQ]? { RET_INSN(7, cmovcc, 0x0C, CPU_686); }
	C M O V N L [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0D, CPU_686); }
	C M O V G E [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0D, CPU_686); }
	C M O V L E [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0E, CPU_686); }
	C M O V N G [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0E, CPU_686); }
	C M O V N L E [wWlLqQ]? { RET_INSN(7, cmovcc, 0x0F, CPU_686); }
	C M O V G [wWlLqQ]? { RET_INSN(5, cmovcc, 0x0F, CPU_686); }
	F C M O V B { RET_INSN(6, fcmovcc, 0xDAC0, CPU_686|CPU_FPU); }
	F C M O V E { RET_INSN(6, fcmovcc, 0xDAC8, CPU_686|CPU_FPU); }
	F C M O V B E { RET_INSN(7, fcmovcc, 0xDAD0, CPU_686|CPU_FPU); }
	F C M O V U { RET_INSN(6, fcmovcc, 0xDAD8, CPU_686|CPU_FPU); }
	F C M O V N B { RET_INSN(7, fcmovcc, 0xDBC0, CPU_686|CPU_FPU); }
	F C M O V N E { RET_INSN(7, fcmovcc, 0xDBC8, CPU_686|CPU_FPU); }
	F C M O V N B E { RET_INSN(8, fcmovcc, 0xDBD0, CPU_686|CPU_FPU); }
	F C M O V U { RET_INSN(6, fcmovcc, 0xDBD8, CPU_686|CPU_FPU); }
	F C O M I { RET_INSN(5, fcom2, 0xDBF0, CPU_686|CPU_FPU); }
	F U C O M I { RET_INSN(6, fcom2, 0xDBE8, CPU_686|CPU_FPU); }
	F C O M I P { RET_INSN(6, fcom2, 0xDFF0, CPU_686|CPU_FPU); }
	F U C O M I P { RET_INSN(7, fcom2, 0xDFE8, CPU_686|CPU_FPU); }
	/* Pentium4 extensions */
	M O V N T I [lLqQ]? { RET_INSN(6, movnti, 0, CPU_P4); }
	C L F L U S H { RET_INSN(7, clflush, 0, CPU_P3); }
	L F E N C E { RET_INSN(6, threebyte, 0x0FAEE8, CPU_P3); }
	M F E N C E { RET_INSN(6, threebyte, 0x0FAEF0, CPU_P3); }
	P A U S E { RET_INSN(5, onebyte_prefix, 0xF390, CPU_P4); }
	/* MMX/SSE2 instructions */
	E M M S { RET_INSN(4, twobyte, 0x0F77, CPU_MMX); }
	M O V D { RET_INSN(4, movd, 0, CPU_MMX); }
	M O V Q {
	    if (arch_x86->parser == X86_PARSER_GAS)
		RET_INSN(3, mov, 0, CPU_Any);
	    else
		RET_INSN(4, movq, 0, CPU_MMX);
	}
	P A C K S S D W { RET_INSN(8, mmxsse2, 0x6B, CPU_MMX); }
	P A C K S S W B { RET_INSN(8, mmxsse2, 0x63, CPU_MMX); }
	P A C K U S W B { RET_INSN(8, mmxsse2, 0x67, CPU_MMX); }
	P A D D B { RET_INSN(5, mmxsse2, 0xFC, CPU_MMX); }
	P A D D W { RET_INSN(5, mmxsse2, 0xFD, CPU_MMX); }
	P A D D D { RET_INSN(5, mmxsse2, 0xFE, CPU_MMX); }
	P A D D Q { RET_INSN(5, mmxsse2, 0xD4, CPU_MMX); }
	P A D D S B { RET_INSN(6, mmxsse2, 0xEC, CPU_MMX); }
	P A D D S W { RET_INSN(6, mmxsse2, 0xED, CPU_MMX); }
	P A D D U S B { RET_INSN(7, mmxsse2, 0xDC, CPU_MMX); }
	P A D D U S W { RET_INSN(7, mmxsse2, 0xDD, CPU_MMX); }
	P A N D { RET_INSN(4, mmxsse2, 0xDB, CPU_MMX); }
	P A N D N { RET_INSN(5, mmxsse2, 0xDF, CPU_MMX); }
	P C M P E Q B { RET_INSN(7, mmxsse2, 0x74, CPU_MMX); }
	P C M P E Q W { RET_INSN(7, mmxsse2, 0x75, CPU_MMX); }
	P C M P E Q D { RET_INSN(7, mmxsse2, 0x76, CPU_MMX); }
	P C M P G T B { RET_INSN(7, mmxsse2, 0x64, CPU_MMX); }
	P C M P G T W { RET_INSN(7, mmxsse2, 0x65, CPU_MMX); }
	P C M P G T D { RET_INSN(7, mmxsse2, 0x66, CPU_MMX); }
	P M A D D W D { RET_INSN(7, mmxsse2, 0xF5, CPU_MMX); }
	P M U L H W { RET_INSN(6, mmxsse2, 0xE5, CPU_MMX); }
	P M U L L W { RET_INSN(6, mmxsse2, 0xD5, CPU_MMX); }
	P O R { RET_INSN(3, mmxsse2, 0xEB, CPU_MMX); }
	P S L L W { RET_INSN(5, pshift, 0x0671F1, CPU_MMX); }
	P S L L D { RET_INSN(5, pshift, 0x0672F2, CPU_MMX); }
	P S L L Q { RET_INSN(5, pshift, 0x0673F3, CPU_MMX); }
	P S R A W { RET_INSN(5, pshift, 0x0471E1, CPU_MMX); }
	P S R A D { RET_INSN(5, pshift, 0x0472E2, CPU_MMX); }
	P S R L W { RET_INSN(5, pshift, 0x0271D1, CPU_MMX); }
	P S R L D { RET_INSN(5, pshift, 0x0272D2, CPU_MMX); }
	P S R L Q { RET_INSN(5, pshift, 0x0273D3, CPU_MMX); }
	P S U B B { RET_INSN(5, mmxsse2, 0xF8, CPU_MMX); }
	P S U B W { RET_INSN(5, mmxsse2, 0xF9, CPU_MMX); }
	P S U B D { RET_INSN(5, mmxsse2, 0xFA, CPU_MMX); }
	P S U B Q { RET_INSN(5, mmxsse2, 0xFB, CPU_MMX); }
	P S U B S B { RET_INSN(6, mmxsse2, 0xE8, CPU_MMX); }
	P S U B S W { RET_INSN(6, mmxsse2, 0xE9, CPU_MMX); }
	P S U B U S B { RET_INSN(7, mmxsse2, 0xD8, CPU_MMX); }
	P S U B U S W { RET_INSN(7, mmxsse2, 0xD9, CPU_MMX); }
	P U N P C K H B W { RET_INSN(9, mmxsse2, 0x68, CPU_MMX); }
	P U N P C K H W D { RET_INSN(9, mmxsse2, 0x69, CPU_MMX); }
	P U N P C K H D Q { RET_INSN(9, mmxsse2, 0x6A, CPU_MMX); }
	P U N P C K L B W { RET_INSN(9, mmxsse2, 0x60, CPU_MMX); }
	P U N P C K L W D { RET_INSN(9, mmxsse2, 0x61, CPU_MMX); }
	P U N P C K L D Q { RET_INSN(9, mmxsse2, 0x62, CPU_MMX); }
	P X O R { RET_INSN(4, mmxsse2, 0xEF, CPU_MMX); }
	/* PIII (Katmai) new instructions / SIMD instructions */
	A D D P S { RET_INSN(5, sseps, 0x58, CPU_SSE); }
	A D D S S { RET_INSN(5, ssess, 0xF358, CPU_SSE); }
	A N D N P S { RET_INSN(6, sseps, 0x55, CPU_SSE); }
	A N D P S { RET_INSN(5, sseps, 0x54, CPU_SSE); }
	C M P E Q P S { RET_INSN(7, ssecmpps, 0x00, CPU_SSE); }
	C M P E Q S S { RET_INSN(7, ssecmpss, 0x00F3, CPU_SSE); }
	C M P L E P S { RET_INSN(7, ssecmpps, 0x02, CPU_SSE); }
	C M P L E S S { RET_INSN(7, ssecmpss, 0x02F3, CPU_SSE); }
	C M P L T P S { RET_INSN(7, ssecmpps, 0x01, CPU_SSE); }
	C M P L T S S { RET_INSN(7, ssecmpss, 0x01F3, CPU_SSE); }
	C M P N E Q P S { RET_INSN(8, ssecmpps, 0x04, CPU_SSE); }
	C M P N E Q S S { RET_INSN(8, ssecmpss, 0x04F3, CPU_SSE); }
	C M P N L E P S { RET_INSN(8, ssecmpps, 0x06, CPU_SSE); }
	C M P N L E S S { RET_INSN(8, ssecmpss, 0x06F3, CPU_SSE); }
	C M P N L T P S { RET_INSN(8, ssecmpps, 0x05, CPU_SSE); }
	C M P N L T S S { RET_INSN(8, ssecmpss, 0x05F3, CPU_SSE); }
	C M P O R D P S { RET_INSN(8, ssecmpps, 0x07, CPU_SSE); }
	C M P O R D S S { RET_INSN(8, ssecmpss, 0x07F3, CPU_SSE); }
	C M P U N O R D P S { RET_INSN(10, ssecmpps, 0x03, CPU_SSE); }
	C M P U N O R D S S { RET_INSN(10, ssecmpss, 0x03F3, CPU_SSE); }
	C M P P S { RET_INSN(5, ssepsimm, 0xC2, CPU_SSE); }
	C M P S S { RET_INSN(5, ssessimm, 0xF3C2, CPU_SSE); }
	C O M I S S { RET_INSN(6, sseps, 0x2F, CPU_SSE); }
	C V T P I "2" P S { RET_INSN(8, cvt_xmm_mm_ps, 0x2A, CPU_SSE); }
	C V T P S "2" P I { RET_INSN(8, cvt_mm_xmm64, 0x2D, CPU_SSE); }
	C V T S I "2" S S [lLqQ]? { RET_INSN(8, cvt_xmm_rmx, 0xF32A, CPU_SSE); }
	C V T S S "2" S I [lLqQ]? { RET_INSN(8, cvt_rx_xmm32, 0xF32D, CPU_SSE); }
	C V T T P S "2" P I { RET_INSN(9, cvt_mm_xmm64, 0x2C, CPU_SSE); }
	C V T T S S "2" S I [lLqQ]? { RET_INSN(9, cvt_rx_xmm32, 0xF32C, CPU_SSE); }
	D I V P S { RET_INSN(5, sseps, 0x5E, CPU_SSE); }
	D I V S S { RET_INSN(5, ssess, 0xF35E, CPU_SSE); }
	L D M X C S R { RET_INSN(7, ldstmxcsr, 0x02, CPU_SSE); }
	M A S K M O V Q { RET_INSN(8, maskmovq, 0, CPU_P3|CPU_MMX); }
	M A X P S { RET_INSN(5, sseps, 0x5F, CPU_SSE); }
	M A X S S { RET_INSN_NS(ssess, 0xF35F, CPU_SSE); }
	M I N P S { RET_INSN_NS(sseps, 0x5D, CPU_SSE); }
	M I N S S { RET_INSN_NS(ssess, 0xF35D, CPU_SSE); }
	M O V A P S { RET_INSN_NS(movaups, 0x28, CPU_SSE); }
	M O V H L P S { RET_INSN_NS(movhllhps, 0x12, CPU_SSE); }
	M O V H P S { RET_INSN_NS(movhlps, 0x16, CPU_SSE); }
	M O V L H P S { RET_INSN_NS(movhllhps, 0x16, CPU_SSE); }
	M O V L P S { RET_INSN_NS(movhlps, 0x12, CPU_SSE); }
	M O V M S K P S [lLqQ]? { RET_INSN(8, movmskps, 0, CPU_SSE); }
	M O V N T P S { RET_INSN_NS(movntps, 0, CPU_SSE); }
	M O V N T Q { RET_INSN_NS(movntq, 0, CPU_SSE); }
	M O V S S { RET_INSN_NS(movss, 0, CPU_SSE); }
	M O V U P S { RET_INSN_NS(movaups, 0x10, CPU_SSE); }
	M U L P S { RET_INSN_NS(sseps, 0x59, CPU_SSE); }
	M U L S S { RET_INSN_NS(ssess, 0xF359, CPU_SSE); }
	O R P S { RET_INSN_NS(sseps, 0x56, CPU_SSE); }
	P A V G B { RET_INSN_NS(mmxsse2, 0xE0, CPU_P3|CPU_MMX); }
	P A V G W { RET_INSN_NS(mmxsse2, 0xE3, CPU_P3|CPU_MMX); }
	P E X T R W [lLqQ]? { RET_INSN(6, pextrw, 0, CPU_P3|CPU_MMX); }
	P I N S R W [lLqQ]? { RET_INSN(6, pinsrw, 0, CPU_P3|CPU_MMX); }
	P M A X S W { RET_INSN_NS(mmxsse2, 0xEE, CPU_P3|CPU_MMX); }
	P M A X U B { RET_INSN_NS(mmxsse2, 0xDE, CPU_P3|CPU_MMX); }
	P M I N S W { RET_INSN_NS(mmxsse2, 0xEA, CPU_P3|CPU_MMX); }
	P M I N U B { RET_INSN_NS(mmxsse2, 0xDA, CPU_P3|CPU_MMX); }
	P M O V M S K B [lLqQ]? { RET_INSN(8, pmovmskb, 0, CPU_SSE); }
	P M U L H U W { RET_INSN_NS(mmxsse2, 0xE4, CPU_P3|CPU_MMX); }
	P R E F E T C H N T A { RET_INSN_NS(twobytemem, 0x000F18, CPU_P3); }
	P R E F E T C H T "0" { RET_INSN_NS(twobytemem, 0x010F18, CPU_P3); }
	P R E F E T C H T "1" { RET_INSN_NS(twobytemem, 0x020F18, CPU_P3); }
	P R E F E T C H T "2" { RET_INSN_NS(twobytemem, 0x030F18, CPU_P3); }
	P S A D B W { RET_INSN_NS(mmxsse2, 0xF6, CPU_P3|CPU_MMX); }
	P S H U F W { RET_INSN_NS(pshufw, 0, CPU_P3|CPU_MMX); }
	R C P P S { RET_INSN_NS(sseps, 0x53, CPU_SSE); }
	R C P S S { RET_INSN_NS(ssess, 0xF353, CPU_SSE); }
	R S Q R T P S { RET_INSN_NS(sseps, 0x52, CPU_SSE); }
	R S Q R T S S { RET_INSN_NS(ssess, 0xF352, CPU_SSE); }
	S F E N C E { RET_INSN_NS(threebyte, 0x0FAEF8, CPU_P3); }
	S H U F P S { RET_INSN_NS(ssepsimm, 0xC6, CPU_SSE); }
	S Q R T P S { RET_INSN_NS(sseps, 0x51, CPU_SSE); }
	S Q R T S S { RET_INSN_NS(ssess, 0xF351, CPU_SSE); }
	S T M X C S R { RET_INSN_NS(ldstmxcsr, 0x03, CPU_SSE); }
	S U B P S { RET_INSN_NS(sseps, 0x5C, CPU_SSE); }
	S U B S S { RET_INSN_NS(ssess, 0xF35C, CPU_SSE); }
	U C O M I S S { RET_INSN_NS(ssess, 0x2E, CPU_SSE); }
	U N P C K H P S { RET_INSN_NS(sseps, 0x15, CPU_SSE); }
	U N P C K L P S { RET_INSN_NS(sseps, 0x14, CPU_SSE); }
	X O R P S { RET_INSN_NS(sseps, 0x57, CPU_SSE); }
	/* SSE2 instructions */
	A D D P D { RET_INSN_NS(ssess, 0x6658, CPU_SSE2); }
	A D D S D { RET_INSN_NS(ssess, 0xF258, CPU_SSE2); }
	A N D N P D { RET_INSN_NS(ssess, 0x6655, CPU_SSE2); }
	A N D P D { RET_INSN_NS(ssess, 0x6654, CPU_SSE2); }
	C M P E Q P D { RET_INSN_NS(ssecmpss, 0x0066, CPU_SSE2); }
	C M P E Q S D { RET_INSN_NS(ssecmpss, 0x00F2, CPU_SSE2); }
	C M P L E P D { RET_INSN_NS(ssecmpss, 0x0266, CPU_SSE2); }
	C M P L E S D { RET_INSN_NS(ssecmpss, 0x02F2, CPU_SSE2); }
	C M P L T P D { RET_INSN_NS(ssecmpss, 0x0166, CPU_SSE2); }
	C M P L T S D { RET_INSN_NS(ssecmpss, 0x01F2, CPU_SSE2); }
	C M P N E Q P D { RET_INSN_NS(ssecmpss, 0x0466, CPU_SSE2); }
	C M P N E Q S D { RET_INSN_NS(ssecmpss, 0x04F2, CPU_SSE2); }
	C M P N L E P D { RET_INSN_NS(ssecmpss, 0x0666, CPU_SSE2); }
	C M P N L E S D { RET_INSN_NS(ssecmpss, 0x06F2, CPU_SSE2); }
	C M P N L T P D { RET_INSN_NS(ssecmpss, 0x0566, CPU_SSE2); }
	C M P N L T S D { RET_INSN_NS(ssecmpss, 0x05F2, CPU_SSE2); }
	C M P O R D P D { RET_INSN_NS(ssecmpss, 0x0766, CPU_SSE2); }
	C M P O R D S D { RET_INSN_NS(ssecmpss, 0x07F2, CPU_SSE2); }
	C M P U N O R D P D { RET_INSN_NS(ssecmpss, 0x0366, CPU_SSE2); }
	C M P U N O R D S D { RET_INSN_NS(ssecmpss, 0x03F2, CPU_SSE2); }
	C M P P D { RET_INSN_NS(ssessimm, 0x66C2, CPU_SSE2); }
	/* C M P S D is in string instructions above */
	C O M I S D { RET_INSN_NS(ssess, 0x662F, CPU_SSE2); }
	C V T P I "2" P D { RET_INSN_NS(cvt_xmm_mm_ss, 0x662A, CPU_SSE2); }
	C V T S I "2" S D [lLqQ]? { RET_INSN(8, cvt_xmm_rmx, 0xF22A, CPU_SSE2); }
	D I V P D { RET_INSN_NS(ssess, 0x665E, CPU_SSE2); }
	D I V S D { RET_INSN_NS(ssess, 0xF25E, CPU_SSE2); }
	M A X P D { RET_INSN_NS(ssess, 0x665F, CPU_SSE2); }
	M A X S D { RET_INSN_NS(ssess, 0xF25F, CPU_SSE2); }
	M I N P D { RET_INSN_NS(ssess, 0x665D, CPU_SSE2); }
	M I N S D { RET_INSN_NS(ssess, 0xF25D, CPU_SSE2); }
	M O V A P D { RET_INSN_NS(movaupd, 0x28, CPU_SSE2); }
	M O V H P D { RET_INSN_NS(movhlpd, 0x16, CPU_SSE2); }
	M O V L P D { RET_INSN_NS(movhlpd, 0x12, CPU_SSE2); }
	M O V M S K P D [lLqQ]? { RET_INSN(8, movmskpd, 0, CPU_SSE2); }
	M O V N T P D { RET_INSN_NS(movntpddq, 0x2B, CPU_SSE2); }
	M O V N T D Q { RET_INSN_NS(movntpddq, 0xE7, CPU_SSE2); }
	/* M O V S D is in string instructions above */
	M O V U P D { RET_INSN_NS(movaupd, 0x10, CPU_SSE2); }
	M U L P D { RET_INSN_NS(ssess, 0x6659, CPU_SSE2); }
	M U L S D { RET_INSN_NS(ssess, 0xF259, CPU_SSE2); }
	O R P D { RET_INSN_NS(ssess, 0x6656, CPU_SSE2); }
	S H U F P D { RET_INSN_NS(ssessimm, 0x66C6, CPU_SSE2); }
	S Q R T P D { RET_INSN_NS(ssess, 0x6651, CPU_SSE2); }
	S Q R T S D { RET_INSN_NS(ssess, 0xF251, CPU_SSE2); }
	S U B P D { RET_INSN_NS(ssess, 0x665C, CPU_SSE2); }
	S U B S D { RET_INSN_NS(ssess, 0xF25C, CPU_SSE2); }
	U C O M I S D { RET_INSN_NS(ssess, 0x662E, CPU_SSE2); }
	U N P C K H P D { RET_INSN_NS(ssess, 0x6615, CPU_SSE2); }
	U N P C K L P D { RET_INSN_NS(ssess, 0x6614, CPU_SSE2); }
	X O R P D { RET_INSN_NS(ssess, 0x6657, CPU_SSE2); }
	C V T D Q "2" P D { RET_INSN_NS(cvt_xmm_xmm64_ss, 0xF3E6, CPU_SSE2); }
	C V T P D "2" D Q { RET_INSN_NS(ssess, 0xF2E6, CPU_SSE2); }
	C V T D Q "2" P S { RET_INSN_NS(sseps, 0x5B, CPU_SSE2); }
	C V T P D "2" P I { RET_INSN_NS(cvt_mm_xmm, 0x662D, CPU_SSE2); }
	C V T P D "2" P S { RET_INSN_NS(ssess, 0x665A, CPU_SSE2); }
	C V T P S "2" P D { RET_INSN_NS(cvt_xmm_xmm64_ps, 0x5A, CPU_SSE2); }
	C V T P S "2" D Q { RET_INSN_NS(ssess, 0x665B, CPU_SSE2); }
	C V T S D "2" S I [lLqQ]? { RET_INSN(8, cvt_rx_xmm64, 0xF22D, CPU_SSE2); }
	C V T S D "2" S S { RET_INSN_NS(cvt_xmm_xmm64_ss, 0xF25A, CPU_SSE2); }
	/* P4 VMX Instructions */
	V M C A L L     { RET_INSN_NS(threebyte, 0x0F01C1, CPU_P4); }
	V M L A U N C H { RET_INSN_NS(threebyte, 0x0F01C2, CPU_P4); }
	V M R E S U M E { RET_INSN_NS(threebyte, 0x0F01C3, CPU_P4); }
	V M X O F F     { RET_INSN_NS(threebyte, 0x0F01C4, CPU_P4); }
	V M R E A D [lLqQ]? { RET_INSN(6, vmxmemrd, 0x0F78, CPU_P4); }
	V M W R I T E [lLqQ]? { RET_INSN(7, vmxmemwr, 0x0F79, CPU_P4); }
	V M P T R L D { RET_INSN_NS(vmxtwobytemem, 0x06C7, CPU_P4); }
	V M P T R S T { RET_INSN_NS(vmxtwobytemem, 0x07C7, CPU_P4); }
	V M C L E A R { RET_INSN_NS(vmxthreebytemem, 0x0666C7, CPU_P4); }
	V M X O N     { RET_INSN_NS(vmxthreebytemem, 0x06F3C7, CPU_P4); }
	C V T S S "2" S D { RET_INSN_NS(cvt_xmm_xmm32, 0xF35A, CPU_SSE2); }
	C V T T P D "2" P I { RET_INSN_NS(cvt_mm_xmm, 0x662C, CPU_SSE2); }
	C V T T S D "2" S I [lLqQ]? { RET_INSN(9, cvt_rx_xmm64, 0xF22C, CPU_SSE2); }
	C V T T P D "2" D Q { RET_INSN_NS(ssess, 0x66E6, CPU_SSE2); }
	C V T T P S "2" D Q { RET_INSN_NS(ssess, 0xF35B, CPU_SSE2); }
	M A S K M O V D Q U { RET_INSN_NS(maskmovdqu, 0, CPU_SSE2); }
	M O V D Q A { RET_INSN_NS(movdqau, 0x66, CPU_SSE2); }
	M O V D Q U { RET_INSN_NS(movdqau, 0xF3, CPU_SSE2); }
	M O V D Q "2" Q { RET_INSN_NS(movdq2q, 0, CPU_SSE2); }
	M O V Q "2" D Q { RET_INSN_NS(movq2dq, 0, CPU_SSE2); }
	P M U L U D Q { RET_INSN_NS(mmxsse2, 0xF4, CPU_SSE2); }
	P S H U F D { RET_INSN_NS(ssessimm, 0x6670, CPU_SSE2); }
	P S H U F H W { RET_INSN_NS(ssessimm, 0xF370, CPU_SSE2); }
	P S H U F L W { RET_INSN_NS(ssessimm, 0xF270, CPU_SSE2); }
	P S L L D Q { RET_INSN_NS(pslrldq, 0x07, CPU_SSE2); }
	P S R L D Q { RET_INSN_NS(pslrldq, 0x03, CPU_SSE2); }
	P U N P C K H Q D Q { RET_INSN_NS(ssess, 0x666D, CPU_SSE2); }
	P U N P C K L Q D Q { RET_INSN_NS(ssess, 0x666C, CPU_SSE2); }
	/* SSE3 / PNI (Prescott New Instructions) instructions */
	A D D S U B P D { RET_INSN_NS(ssess, 0x66D0, CPU_SSE3); }
	A D D S U B P S { RET_INSN_NS(ssess, 0xF2D0, CPU_SSE3); }
	F I S T T P [sSlLqQ]? { RET_INSN(6, fildstp, 0x010001, CPU_SSE3); }
	F I S T T P L L {
	    suffix_over='q';
	    RET_INSN_GAS(8, fildstp, 0x07, CPU_FPU);
	}
	H A D D P D { RET_INSN_NS(ssess, 0x667C, CPU_SSE3); }
	H A D D P S { RET_INSN_NS(ssess, 0xF27C, CPU_SSE3); }
	H S U B P D { RET_INSN_NS(ssess, 0x667D, CPU_SSE3); }
	H S U B P S { RET_INSN_NS(ssess, 0xF27D, CPU_SSE3); }
	L D D Q U { RET_INSN_NS(lddqu, 0, CPU_SSE3); }
	M O N I T O R { RET_INSN_NS(threebyte, 0x0F01C8, CPU_SSE3); }
	M O V D D U P { RET_INSN_NS(cvt_xmm_xmm64_ss, 0xF212, CPU_SSE3); }
	M O V S H D U P { RET_INSN_NS(ssess, 0xF316, CPU_SSE3); }
	M O V S L D U P { RET_INSN_NS(ssess, 0xF312, CPU_SSE3); }
	M W A I T { RET_INSN_NS(threebyte, 0x0F01C9, CPU_SSE3); }
	/* AMD 3DNow! instructions */
	P R E F E T C H { RET_INSN_NS(twobytemem, 0x000F0D, CPU_3DNow); }
	P R E F E T C H W { RET_INSN_NS(twobytemem, 0x010F0D, CPU_3DNow); }
	F E M M S { RET_INSN_NS(twobyte, 0x0F0E, CPU_3DNow); }
	P A V G U S B { RET_INSN_NS(now3d, 0xBF, CPU_3DNow); }
	P F "2" I D { RET_INSN_NS(now3d, 0x1D, CPU_3DNow); }
	P F "2" I W { RET_INSN_NS(now3d, 0x1C, CPU_Athlon|CPU_3DNow); }
	P F A C C { RET_INSN_NS(now3d, 0xAE, CPU_3DNow); }
	P F A D D { RET_INSN_NS(now3d, 0x9E, CPU_3DNow); }
	P F C M P E Q { RET_INSN_NS(now3d, 0xB0, CPU_3DNow); }
	P F C M P G E { RET_INSN_NS(now3d, 0x90, CPU_3DNow); }
	P F C M P G T { RET_INSN_NS(now3d, 0xA0, CPU_3DNow); }
	P F M A X { RET_INSN_NS(now3d, 0xA4, CPU_3DNow); }
	P F M I N { RET_INSN_NS(now3d, 0x94, CPU_3DNow); }
	P F M U L { RET_INSN_NS(now3d, 0xB4, CPU_3DNow); }
	P F N A C C { RET_INSN_NS(now3d, 0x8A, CPU_Athlon|CPU_3DNow); }
	P F P N A C C { RET_INSN_NS(now3d, 0x8E, CPU_Athlon|CPU_3DNow); }
	P F R C P { RET_INSN_NS(now3d, 0x96, CPU_3DNow); }
	P F R C P I T "1" { RET_INSN_NS(now3d, 0xA6, CPU_3DNow); }
	P F R C P I T "2" { RET_INSN_NS(now3d, 0xB6, CPU_3DNow); }
	P F R S Q I T "1" { RET_INSN_NS(now3d, 0xA7, CPU_3DNow); }
	P F R S Q R T { RET_INSN_NS(now3d, 0x97, CPU_3DNow); }
	P F S U B { RET_INSN(5, now3d, 0x9A, CPU_3DNow); }
	P F S U B R { RET_INSN(6, now3d, 0xAA, CPU_3DNow); }
	P I "2" F D { RET_INSN(5, now3d, 0x0D, CPU_3DNow); }
	P I "2" F W { RET_INSN(5, now3d, 0x0C, CPU_Athlon|CPU_3DNow); }
	P M U L H R W A { RET_INSN(8, now3d, 0xB7, CPU_3DNow); }
	P S W A P D { RET_INSN(6, now3d, 0xBB, CPU_Athlon|CPU_3DNow); }
	/* AMD extensions */
	S Y S C A L L { RET_INSN(7, twobyte, 0x0F05, CPU_686|CPU_AMD); }
	S Y S R E T [lLqQ]? { RET_INSN(6, twobyte, 0x0F07, CPU_686|CPU_AMD|CPU_Priv); }
	/* AMD x86-64 extensions */
	S W A P G S {
	    warn64 = 1;
	    RET_INSN(6, threebyte, 0x0F01F8, CPU_Hammer|CPU_64);
	}
	R D T S C P { RET_INSN(6, threebyte, 0x0F01F9, CPU_686|CPU_AMD|CPU_Priv); }
	/* AMD Pacifica (SVM) instructions */
	C L G I { RET_INSN_NS(threebyte, 0x0F01DD, CPU_Hammer|CPU_64|CPU_SVM); }
	I N V L P G A { RET_INSN_NS(invlpga, 0, CPU_Hammer|CPU_64|CPU_SVM); }
	S K I N I T { RET_INSN_NS(skinit, 0, CPU_Hammer|CPU_64|CPU_SVM); }
	S T G I { RET_INSN_NS(threebyte, 0x0F01DC, CPU_Hammer|CPU_64|CPU_SVM); }
	V M L O A D { RET_INSN_NS(svm_rax, 0xDA, CPU_Hammer|CPU_64|CPU_SVM); }
	V M M C A L L { RET_INSN_NS(threebyte, 0x0F01D9, CPU_Hammer|CPU_64|CPU_SVM); }
	V M R U N { RET_INSN_NS(svm_rax, 0xD8, CPU_Hammer|CPU_64|CPU_SVM); }
	V M S A V E { RET_INSN_NS(svm_rax, 0xDB, CPU_Hammer|CPU_64|CPU_SVM); }
	/* VIA PadLock instructions */
	X S T O R E (R N G)? { RET_INSN_NS(padlock, 0xC000A7, CPU_PadLock); }
	X C R Y P T E C B { RET_INSN_NS(padlock, 0xC8F3A7, CPU_PadLock); }
	X C R Y P T C B C { RET_INSN_NS(padlock, 0xD0F3A7, CPU_PadLock); }
	X C R Y P T C T R { RET_INSN_NS(padlock, 0xD8F3A7, CPU_PadLock); }
	X C R Y P T C F B { RET_INSN_NS(padlock, 0xE0F3A7, CPU_PadLock); }
	X C R Y P T O F B { RET_INSN_NS(padlock, 0xE8F3A7, CPU_PadLock); }
	M O N T M U L { RET_INSN_NS(padlock, 0xC0F3A6, CPU_PadLock); }
	X S H A "1" { RET_INSN_NS(padlock, 0xC8F3A6, CPU_PadLock); }
	X S H A "256" { RET_INSN_NS(padlock, 0xD0F3A6, CPU_PadLock); }
	/* Cyrix MMX instructions */
	P A D D S I W { RET_INSN(7, cyrixmmx, 0x51, CPU_Cyrix|CPU_MMX); }
	P A V E B { RET_INSN(5, cyrixmmx, 0x50, CPU_Cyrix|CPU_MMX); }
	P D I S T I B { RET_INSN(7, cyrixmmx, 0x54, CPU_Cyrix|CPU_MMX); }
	P M A C H R I W { RET_INSN(8, pmachriw, 0, CPU_Cyrix|CPU_MMX); }
	P M A G W { RET_INSN(5, cyrixmmx, 0x52, CPU_Cyrix|CPU_MMX); }
	P M U L H R I W { RET_INSN(8, cyrixmmx, 0x5D, CPU_Cyrix|CPU_MMX); }
	P M U L H R W C { RET_INSN(8, cyrixmmx, 0x59, CPU_Cyrix|CPU_MMX); }
	P M V G E Z B { RET_INSN(7, cyrixmmx, 0x5C, CPU_Cyrix|CPU_MMX); }
	P M V L Z B { RET_INSN(6, cyrixmmx, 0x5B, CPU_Cyrix|CPU_MMX); }
	P M V N Z B { RET_INSN(6, cyrixmmx, 0x5A, CPU_Cyrix|CPU_MMX); }
	P M V Z B { RET_INSN(5, cyrixmmx, 0x58, CPU_Cyrix|CPU_MMX); }
	P S U B S I W { RET_INSN(7, cyrixmmx, 0x55, CPU_Cyrix|CPU_MMX); }
	/* Cyrix extensions */
	R D S H R { RET_INSN(5, twobyte, 0x0F36, CPU_686|CPU_Cyrix|CPU_SMM); }
	R S D C { RET_INSN(4, rsdc, 0, CPU_486|CPU_Cyrix|CPU_SMM); }
	R S L D T { RET_INSN(5, cyrixsmm, 0x7B, CPU_486|CPU_Cyrix|CPU_SMM); }
	R S T S { RET_INSN(4, cyrixsmm, 0x7D, CPU_486|CPU_Cyrix|CPU_SMM); }
	S V D C { RET_INSN(4, svdc, 0, CPU_486|CPU_Cyrix|CPU_SMM); }
	S V L D T { RET_INSN(5, cyrixsmm, 0x7A, CPU_486|CPU_Cyrix|CPU_SMM); }
	S V T S { RET_INSN(4, cyrixsmm, 0x7C, CPU_486|CPU_Cyrix|CPU_SMM); }
	S M I N T { RET_INSN(5, twobyte, 0x0F38, CPU_686|CPU_Cyrix); }
	S M I N T O L D { RET_INSN(8, twobyte, 0x0F7E, CPU_486|CPU_Cyrix|CPU_Obs); }
	W R S H R { RET_INSN(5, twobyte, 0x0F37, CPU_686|CPU_Cyrix|CPU_SMM); }
	/* Obsolete/undocumented instructions */
	F S E T P M { RET_INSN(6, twobyte, 0xDBE4, CPU_286|CPU_FPU|CPU_Obs); }
	I B T S { RET_INSN(4, ibts, 0, CPU_386|CPU_Undoc|CPU_Obs); }
	L O A D A L L { RET_INSN(7, twobyte, 0x0F07, CPU_386|CPU_Undoc); }
	L O A D A L L "286" { RET_INSN(10, twobyte, 0x0F05, CPU_286|CPU_Undoc); }
	S A L C {
	    not64 = 1;
	    RET_INSN(4, onebyte, 0x00D6, CPU_Undoc);
	}
	S M I { RET_INSN(3, onebyte, 0x00F1, CPU_386|CPU_Undoc); }
	U M O V { RET_INSN(4, umov, 0, CPU_386|CPU_Undoc); }
	X B T S { RET_INSN(4, xbts, 0, CPU_386|CPU_Undoc|CPU_Obs); }


	/* catchalls */
	[\001-\377]+	{
	    return 0;
	}
	[\000]	{
	    return 0;
	}
    */
done:
    if (suffix) {
	/* If not using the GAS parser, no instructions have suffixes. */
	if (arch_x86->parser != X86_PARSER_GAS)
	    return 0;

	if (suffix_over == '\0')
	    suffix_over = id[suffix_ofs];
	/* Match suffixes */
	switch (suffix_over) {
	    case 'b':
	    case 'B':
		data[3] |= (MOD_GasSufB >> MOD_GasSuf_SHIFT) << 8;
		break;
	    case 'w':
	    case 'W':
		data[3] |= (MOD_GasSufW >> MOD_GasSuf_SHIFT) << 8;
		break;
	    case 'l':
	    case 'L':
		data[3] |= (MOD_GasSufL >> MOD_GasSuf_SHIFT) << 8;
		break;
	    case 'q':
	    case 'Q':
		data[3] |= (MOD_GasSufQ >> MOD_GasSuf_SHIFT) << 8;
		break;
	    case 's':
	    case 'S':
		data[3] |= (MOD_GasSufS >> MOD_GasSuf_SHIFT) << 8;
		break;
	    default:
		yasm_internal_error(N_("unrecognized suffix"));
	}
    }
    if (warn64 && arch_x86->mode_bits != 64) {
	yasm__warning(YASM_WARN_GENERAL, line,
		      N_("`%s' is an instruction in 64-bit mode"),
		      oid);
	return 0;
    }
    if (not64 && arch_x86->mode_bits == 64) {
	yasm__error(line, N_("`%s' invalid in 64-bit mode"), oid);
	DEF_INSN_DATA(not64, 0, CPU_Not64);
	return 1;
    }
    return 1;
}
