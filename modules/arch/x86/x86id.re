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
#define MOD_Op2AddSp (1UL<<12)	/* Parameter is added as "spare" to opcode byte 2 */

/* Modifiers that aren't: these are used with the GAS parser to indicate
 * special cases.
 */
#define MOD_GasOnly	(1UL<<13)	/* Only available in GAS mode */
#define MOD_GasIllegal	(1UL<<14)	/* Illegal in GAS mode */
#define MOD_GasNoRev	(1UL<<15)	/* Don't reverse operands */
#define MOD_GasSufB	(1UL<<16)	/* GAS B suffix ok */
#define MOD_GasSufW	(1UL<<17)	/* GAS W suffix ok */
#define MOD_GasSufL	(1UL<<18)	/* GAS L suffix ok */
#define MOD_GasSufQ	(1UL<<19)	/* GAS Q suffix ok */
#define MOD_GasSufS	(1UL<<20)	/* GAS S suffix ok */
#define MOD_GasSuf_SHIFT 16
#define MOD_GasSuf_MASK	(0x1FUL<<16)

/* Modifiers that aren't actually used as modifiers.  Rather, if set, bits
 * 20-27 in the modifier are used as an index into an array.
 * Obviously, only one of these may be set at a time.
 */
#define MOD_ExtNone (0UL<<29)	/* No extended modifier */
#define MOD_ExtErr  (1UL<<29)	/* Extended error: index into error strings */
#define MOD_ExtWarn (2UL<<29)	/* Extended warning: index into warning strs */
#define MOD_Ext_MASK (0x3UL<<29)
#define MOD_ExtIndex_SHIFT	21
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
 *             5 = large imm64 that can become a sign-extended imm32.
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
#define OPAP_SImm32Avail (5UL<<17)
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
    } while (0)

/*
 * General instruction groupings
 */

/* Empty instruction */
static const x86_insn_info empty_insn[] = {
    { CPU_Any, 0, 0, 0, 0, 0, {0, 0, 0}, 0, 0, {0, 0, 0} }
};

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
    /* 64-bit forced size form */
    { CPU_Hammer|CPU_64, MOD_GasIllegal, 64, 0, 0, 1, {0xB8, 0, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Op0Add, OPT_Imm|OPS_64|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xB8, 0xC7, 0}, 0, 2,
      {OPT_Reg|OPS_64|OPA_Op0Add,
       OPT_Imm|OPS_64|OPS_Relaxed|OPA_Imm|OPAP_SImm32Avail, 0} },
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
    { CPU_Any, MOD_GasSufW, 16, 64, 0, 1, {0x58, 0, 0}, 0, 1,
      {OPT_Reg|OPS_16|OPA_Op0Add, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x58, 0, 0}, 0, 1,
      {OPT_Reg|OPS_32|OPA_Op0Add, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 0, 64, 0, 1, {0x58, 0, 0}, 0, 1,
      {OPT_Reg|OPS_64|OPA_Op0Add, 0, 0} },
    { CPU_Any, MOD_GasSufW, 16, 64, 0, 1, {0x8F, 0, 0}, 0, 1,
      {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x8F, 0, 0}, 0, 1,
      {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 0, 64, 0, 1, {0x8F, 0, 0}, 0, 1,
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
    /* Also have forced-size forms to override the optimization */
    { CPU_Any, MOD_Op0Add|MOD_GasSufB, 0, 0, 0, 1, {0x04, 0, 0}, 0, 2,
      {OPT_Areg|OPS_8|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_Op0Add|MOD_GasIllegal, 16, 0, 0, 1, {0x05, 0, 0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None, OPT_Imm|OPS_16|OPA_Imm, 0} },
    { CPU_Any, MOD_Op0Add|MOD_Op2AddSp|MOD_GasSufW, 16, 0, 0, 1,
      {0x05, 0x83, 0xC0}, 0, 2,
      {OPT_Areg|OPS_16|OPA_None,
       OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0} },
    { CPU_386, MOD_Op0Add|MOD_GasIllegal, 32, 0, 0, 1, {0x05, 0, 0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_386, MOD_Op0Add|MOD_Op2AddSp|MOD_GasSufL, 32, 0, 0, 1,
      {0x05, 0x83, 0xC0}, 0, 2,
      {OPT_Areg|OPS_32|OPA_None,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0} },
    { CPU_Hammer|CPU_64, MOD_Op0Add|MOD_GasIllegal, 64, 0, 0, 1, {0x05, 0, 0},
      0, 2, {OPT_Areg|OPS_64|OPA_None, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_Op0Add|MOD_Op2AddSp|MOD_GasSufQ, 64, 0, 0, 1,
      {0x05, 0x83, 0xC0}, 0,
      2, {OPT_Areg|OPS_64|OPA_None,
	  OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0} },

    /* Also have forced-size forms to override the optimization */
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0x80, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0x80, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPA_Imm, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0x83, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasIllegal, 16, 0, 0, 1, {0x81, 0, 0}, 0,
      2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_16|OPA_Imm, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0x81, 0x83, 0}, 0,
      2, {OPT_RM|OPS_16|OPA_EA,
	  OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0x83, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasIllegal, 32, 0, 0, 1, {0x81, 0, 0}, 0,
      2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0x81, 0x83, 0}, 0,
      2, {OPT_RM|OPS_32|OPA_EA,
	  OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0} },
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

    { CPU_186, MOD_GasSufW, 16, 0, 0, 1, {0x69, 0x6B, 0}, 0, 3,
      {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_16|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x69, 0x6B, 0}, 0, 3,
      {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x69, 0x6B, 0}, 0, 3,
      {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail} },

    { CPU_186, MOD_GasSufW, 16, 0, 0, 1, {0x69, 0x6B, 0}, 0, 2,
      {OPT_Reg|OPS_16|OPA_SpareEA,
       OPT_Imm|OPS_16|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x69, 0x6B, 0}, 0, 2,
      {OPT_Reg|OPS_32|OPA_SpareEA,
       OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail, 0} },
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x69, 0x6B, 0}, 0, 2,
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
    { CPU_Any, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xC0, 0xD0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm|OPAP_ShiftOp,
       0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xD3, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xC1, 0xD1, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm|OPAP_ShiftOp,
       0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xD3, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xC1, 0xD1, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm|OPAP_ShiftOp,
       0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xD3, 0, 0}, 0,
      2, {OPT_RM|OPS_64|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xC1, 0xD1, 0},
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
x86_finalize_common(x86_common *common, const x86_insn_info *info,
		    unsigned int mode_bits)
{
    common->addrsize = 0;
    common->opersize = info->opersize;
    common->lockrep_pre = 0;
    common->mode_bits = (unsigned char)mode_bits;
}

static void
x86_finalize_opcode(x86_opcode *opcode, const x86_insn_info *info)
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
		    unsigned long **prefixes, const x86_insn_info *info)
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

    yasm_x86__bc_apply_prefixes((x86_common *)jmpfar, NULL, num_prefixes,
				prefixes, bc->line);

    /* Transform the bytecode */
    yasm_x86__bc_transform_jmpfar(bc, jmpfar);
}

static void
x86_finalize_jmp(yasm_arch *arch, yasm_bytecode *bc, yasm_bytecode *prev_bc,
		 const unsigned long data[4], int num_operands,
		 yasm_insn_operands *operands, int num_prefixes,
		 unsigned long **prefixes, const x86_insn_info *jinfo)
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

    yasm_x86__bc_apply_prefixes((x86_common *)jmp, NULL, num_prefixes,
				prefixes, bc->line);

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
    const x86_insn_info *info = (const x86_insn_info *)data[0];
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

    if (!info) {
	num_info = 1;
	info = empty_insn;
    }

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
	mod_data >>= 8;
    }
    if (info->modifiers & MOD_Op2AddSp) {
	insn->opcode.opcode[2] += (unsigned char)(mod_data & 0xFF)<<3;
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
		case OPAP_SImm32Avail:
		    insn->postop = X86_POSTOP_SIGNEXT_IMM32;
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

    yasm_x86__bc_apply_prefixes((x86_common *)insn, &insn->rex, num_prefixes,
				prefixes, bc->line);

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
	(I? "586") | 'pentium' | 'p5' {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_FPU|CPU_SMM|
		CPU_Prot|CPU_Priv;
	    return;
	}
	(I? "686") | 'p6' | 'ppro' | 'pentiumpro' {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_FPU|
		CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	('p2') | ('pentium' "-"? ("2" | 'ii')) {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_FPU|
		CPU_MMX|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	('p3') | ('pentium' "-"? ("3" | 'iii')) | 'katmai' {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_P3|CPU_FPU|
		CPU_MMX|CPU_SSE|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	('p4') | ('pentium' "-"? ("4" | 'iv')) | 'williamette' {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_P3|CPU_P4|
		CPU_FPU|CPU_MMX|CPU_SSE|CPU_SSE2|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	('ia' "-"? "64") | 'itanium' {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_P3|CPU_P4|
		CPU_IA64|CPU_FPU|CPU_MMX|CPU_SSE|CPU_SSE2|CPU_SMM|CPU_Prot|
		CPU_Priv;
	    return;
	}
	'k6' {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_K6|CPU_FPU|
		CPU_MMX|CPU_3DNow|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	'athlon' | 'k7' {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_K6|
		CPU_Athlon|CPU_FPU|CPU_MMX|CPU_SSE|CPU_3DNow|CPU_SMM|CPU_Prot|
		CPU_Priv;
	    return;
	}
	('sledge'? 'hammer') | 'opteron' | ('athlon' "-"? "64") {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_K6|
		CPU_Athlon|CPU_Hammer|CPU_FPU|CPU_MMX|CPU_SSE|CPU_SSE2|
		CPU_3DNow|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}
	'prescott' {
	    arch_x86->cpu_enabled =
		CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|CPU_K6|
		CPU_Athlon|CPU_Hammer|CPU_FPU|CPU_MMX|CPU_SSE|CPU_SSE2|
		CPU_SSE3|CPU_3DNow|CPU_SMM|CPU_Prot|CPU_Priv;
	    return;
	}

	/* Features have "no" versions to disable them, and only set/reset the
	 * specific feature being changed.  All other bits are left alone.
	 */
	'fpu'		{ arch_x86->cpu_enabled |= CPU_FPU; return; }
	'nofpu'		{ arch_x86->cpu_enabled &= ~CPU_FPU; return; }
	'mmx'		{ arch_x86->cpu_enabled |= CPU_MMX; return; }
	'nommx'		{ arch_x86->cpu_enabled &= ~CPU_MMX; return; }
	'sse'		{ arch_x86->cpu_enabled |= CPU_SSE; return; }
	'nosse'		{ arch_x86->cpu_enabled &= ~CPU_SSE; return; }
	'sse2'		{ arch_x86->cpu_enabled |= CPU_SSE2; return; }
	'nosse2'	{ arch_x86->cpu_enabled &= ~CPU_SSE2; return; }
	'sse3'		{ arch_x86->cpu_enabled |= CPU_SSE3; return; }
	'nosse3'	{ arch_x86->cpu_enabled &= ~CPU_SSE3; return; }
	'pni'		{ arch_x86->cpu_enabled |= CPU_SSE3; return; }
	'nopni'		{ arch_x86->cpu_enabled &= ~CPU_SSE3; return; }
	'3dnow'		{ arch_x86->cpu_enabled |= CPU_3DNow; return; }
	'no3dnow'	{ arch_x86->cpu_enabled &= ~CPU_3DNow; return; }
	'cyrix'		{ arch_x86->cpu_enabled |= CPU_Cyrix; return; }
	'nocyrix'	{ arch_x86->cpu_enabled &= ~CPU_Cyrix; return; }
	'amd'		{ arch_x86->cpu_enabled |= CPU_AMD; return; }
	'noamd'		{ arch_x86->cpu_enabled &= ~CPU_AMD; return; }
	'smm'		{ arch_x86->cpu_enabled |= CPU_SMM; return; }
	'nosmm'		{ arch_x86->cpu_enabled &= ~CPU_SMM; return; }
	'prot'		{ arch_x86->cpu_enabled |= CPU_Prot; return; }
	'noprot'	{ arch_x86->cpu_enabled &= ~CPU_Prot; return; }
	'undoc'		{ arch_x86->cpu_enabled |= CPU_Undoc; return; }
	'noundoc'	{ arch_x86->cpu_enabled &= ~CPU_Undoc; return; }
	'obs'		{ arch_x86->cpu_enabled |= CPU_Obs; return; }
	'noobs'		{ arch_x86->cpu_enabled &= ~CPU_Obs; return; }
	'priv'		{ arch_x86->cpu_enabled |= CPU_Priv; return; }
	'nopriv'	{ arch_x86->cpu_enabled &= ~CPU_Priv; return; }
	'svm'		{ arch_x86->cpu_enabled |= CPU_SVM; return; }
	'nosvm'		{ arch_x86->cpu_enabled &= ~CPU_SVM; return; }
	'padlock'	{ arch_x86->cpu_enabled |= CPU_PadLock; return; }
	'nopadlock'	{ arch_x86->cpu_enabled &= ~CPU_PadLock; return; }

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
	'near'	{
	    data[0] = X86_NEAR;
	    return 1;
	}
	'short'	{
	    data[0] = X86_SHORT;
	    return 1;
	}
	'far'	{
	    data[0] = X86_FAR;
	    return 1;
	}
	'to'	{
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
	'o16' | 'data16' | 'word'	{
	    if (oid[0] != 'o' && arch_x86->parser != X86_PARSER_GAS)
		return 0;
	    data[0] = X86_OPERSIZE;
	    data[1] = 16;
	    return 1;
	}
	'o32' | 'data32' | 'dword'	{
	    if (oid[0] != 'o' && arch_x86->parser != X86_PARSER_GAS)
		return 0;
	    if (arch_x86->mode_bits == 64) {
		yasm__error(line,
		    N_("Cannot override data size to 32 bits in 64-bit mode"));
		return 0;
	    }
	    data[0] = X86_OPERSIZE;
	    data[1] = 32;
	    return 1;
	}
	'o64' | 'data64' | 'qword'	{
	    if (oid[0] != 'o' && arch_x86->parser != X86_PARSER_GAS)
		return 0;
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
	'a16' | 'addr16' | 'aword'	{
	    if (oid[1] != '1' && arch_x86->parser != X86_PARSER_GAS)
		return 0;
	    if (arch_x86->mode_bits == 64) {
		yasm__error(line,
		    N_("Cannot override address size to 16 bits in 64-bit mode"));
		return 0;
	    }
	    data[0] = X86_ADDRSIZE;
	    data[1] = 16;
	    return 1;
	}
	'a32' | 'addr32' | 'adword'	{
	    if (oid[1] != '3' && arch_x86->parser != X86_PARSER_GAS)
		return 0;
	    data[0] = X86_ADDRSIZE;
	    data[1] = 32;
	    return 1;
	}
	'a64' | 'addr64' | 'aqword'	{
	    if (oid[1] != '6' && arch_x86->parser != X86_PARSER_GAS)
		return 0;
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
	'lock'	{
	    data[0] = X86_LOCKREP; 
	    data[1] = 0xF0;
	    return 1;
	}
	'repn' ('e' | 'z')	{
	    data[0] = X86_LOCKREP;
	    data[1] = 0xF2;
	    return 1;
	}
	'rep' ('e' | 'z')?	{
	    data[0] = X86_LOCKREP;
	    data[1] = 0xF3;
	    return 1;
	}

	/* other prefixes (limited to GAS-only at the moment) */
	/* Hint taken/not taken (for jumps */
	'ht'	{
	    if (arch_x86->parser != X86_PARSER_GAS)
		return 0;
	    data[0] = X86_SEGREG;
	    data[1] = 0x3E;
	    return 1;
	}
	'hnt'	{
	    if (arch_x86->parser != X86_PARSER_GAS)
		return 0;
	    data[0] = X86_SEGREG;
	    data[1] = 0x2E;
	    return 1;
	}
	/* REX byte explicit prefixes */
	'rex'	{
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x40;
	    return 1;
	}
	'rexz'	{
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x41;
	    return 1;
	}
	'rexy'	{
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x42;
	    return 1;
	}
	'rexyz'	{
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x43;
	    return 1;
	}
	'rexx'	{
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x44;
	    return 1;
	}
	'rexxz'	{
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x45;
	    return 1;
	}
	'rexxy'	{
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x46;
	    return 1;
	}
	'rexxyz'    {
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x47;
	    return 1;
	}
	'rex64'	{
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x48;
	    return 1;
	}
	'rex64z'    {
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x49;
	    return 1;
	}
	'rex64y'    {
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x4a;
	    return 1;
	}
	'rex64yz'   {
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x4b;
	    return 1;
	}
	'rex64x'    {
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x4c;
	    return 1;
	}
	'rex64xz'   {
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x4d;
	    return 1;
	}
	'rex64xy'   {
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x4e;
	    return 1;
	}
	'rex64xyz'  {
	    if (arch_x86->parser != X86_PARSER_GAS
		|| arch_x86->mode_bits != 64)
		return 0;
	    data[0] = X86_REX;
	    data[1] = 0x4f;
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
	'cr' [02-48]	{
	    if (arch_x86->mode_bits != 64 && oid[2] == '8') {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_CRREG | (oid[2]-'0');
	    return 1;
	}
	'dr' [0-7]	{
	    data[0] = X86_DRREG | (oid[2]-'0');
	    return 1;
	}
	'tr' [0-7]	{
	    data[0] = X86_TRREG | (oid[2]-'0');
	    return 1;
	}

	/* floating point, MMX, and SSE/SSE2 registers */
	'st' [0-7]	{
	    data[0] = X86_FPUREG | (oid[2]-'0');
	    return 1;
	}
	'mm' [0-7]	{
	    data[0] = X86_MMXREG | (oid[2]-'0');
	    return 1;
	}
	'xmm' [0-9]	{
	    if (arch_x86->mode_bits != 64 &&
		(oid[3] == '8' || oid[3] == '9')) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_XMMREG | (oid[3]-'0');
	    return 1;
	}
	'xmm' "1" [0-5]	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_XMMREG | (10+oid[4]-'0');
	    return 1;
	}

	/* integer registers */
	'rax'	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 0;
	    return 1;
	}
	'rcx'	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 1;
	    return 1;
	}
	'rdx'	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 2;
	    return 1;
	}
	'rbx'	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 3;
	    return 1;
	}
	'rsp'	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 4;
	    return 1;
	}
	'rbp'	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 5;
	    return 1;
	}
	'rsi'	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 6;
	    return 1;
	}
	'rdi'	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | 7;
	    return 1;
	}
	R [8-9]   {
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | (oid[1]-'0');
	    return 1;
	}
	'r1' [0-5] {
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG64 | (10+oid[2]-'0');
	    return 1;
	}

	'eax'	{ data[0] = X86_REG32 | 0; return 1; }
	'ecx'	{ data[0] = X86_REG32 | 1; return 1; }
	'edx'	{ data[0] = X86_REG32 | 2; return 1; }
	'ebx'	{ data[0] = X86_REG32 | 3; return 1; }
	'esp'	{ data[0] = X86_REG32 | 4; return 1; }
	'ebp'	{ data[0] = X86_REG32 | 5; return 1; }
	'esi'	{ data[0] = X86_REG32 | 6; return 1; }
	'edi'	{ data[0] = X86_REG32 | 7; return 1; }
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

	'ax'	{ data[0] = X86_REG16 | 0; return 1; }
	'cx'	{ data[0] = X86_REG16 | 1; return 1; }
	'dx'	{ data[0] = X86_REG16 | 2; return 1; }
	'bx'	{ data[0] = X86_REG16 | 3; return 1; }
	'sp'	{ data[0] = X86_REG16 | 4; return 1; }
	'bp'	{ data[0] = X86_REG16 | 5; return 1; }
	'si'	{ data[0] = X86_REG16 | 6; return 1; }
	'di'	{ data[0] = X86_REG16 | 7; return 1; }
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

	'al'	{ data[0] = X86_REG8 | 0; return 1; }
	'cl'	{ data[0] = X86_REG8 | 1; return 1; }
	'dl'	{ data[0] = X86_REG8 | 2; return 1; }
	'bl'	{ data[0] = X86_REG8 | 3; return 1; }
	'ah'	{ data[0] = X86_REG8 | 4; return 1; }
	'ch'	{ data[0] = X86_REG8 | 5; return 1; }
	'dh'	{ data[0] = X86_REG8 | 6; return 1; }
	'bh'	{ data[0] = X86_REG8 | 7; return 1; }
	'spl'	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG8X | 4;
	    return 1;
	}
	'bpl'	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG8X | 5;
	    return 1;
	}
	'sil'	{
	    if (arch_x86->mode_bits != 64) {
		yasm__warning(YASM_WARN_GENERAL, line,
			      N_("`%s' is a register in 64-bit mode"), oid);
		return 0;
	    }
	    data[0] = X86_REG8X | 6;
	    return 1;
	}
	'dil'	{
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
	'rip'	{
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
    /*const char *oid = id;*/
    /*!re2c
	/* floating point, MMX, and SSE/SSE2 registers */
	'st'		{
	    data[0] = X86_FPUREG;
	    return 1;
	}
	'mm'		{
	    data[0] = X86_MMXREG;
	    return 1;
	}
	'xmm'		{
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
	'es'	{
	    if (arch_x86->mode_bits == 64)
		yasm__warning(YASM_WARN_GENERAL, line,
		    N_("`%s' segment register ignored in 64-bit mode"), oid);
	    data[0] = 0x2600;
	    return 1;
	}
	'cs'	{ data[0] = 0x2e01; return 1; }
	'ss'	{
	    if (arch_x86->mode_bits == 64)
		yasm__warning(YASM_WARN_GENERAL, line,
		    N_("`%s' segment register ignored in 64-bit mode"), oid);
	    data[0] = 0x3602;
	    return 1;
	}
	'ds'	{
	    if (arch_x86->mode_bits == 64)
		yasm__warning(YASM_WARN_GENERAL, line,
		    N_("`%s' segment register ignored in 64-bit mode"), oid);
	    data[0] = 0x3e03;
	    return 1;
	}
	'fs'	{ data[0] = 0x6404; return 1; }
	'gs'	{ data[0] = 0x6505; return 1; }

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

    data[3] = arch_x86->mode_bits;

    /*!re2c
	/* instructions */

	/* Move */
	'mov' [bBwWlL]? { RET_INSN(3, mov, 0, CPU_Any); }
	'movabs' [bBwWlLqQ]? { RET_INSN_GAS(6, movabs, 0, CPU_Hammer|CPU_64); }
	/* Move with sign/zero extend */
	'movsb' [wWlL] { suffix_ofs = -2; RET_INSN_GAS(4, movszx, 0xBE, CPU_386); }
	'movswl' { suffix_ofs = -2; RET_INSN_GAS(4, movszx, 0xBE, CPU_386); }
	'movs' [bBwW] Q {
	    suffix_ofs = -2;
	    warn64 = 1;
	    RET_INSN_GAS(4, movszx, 0xBE, CPU_Hammer|CPU_64);
	}
	'movsx' [bBwW]? { RET_INSN(5, movszx, 0xBE, CPU_386); }
	'movslq' {
	    suffix_ofs = -2;
	    warn64 = 1;
	    RET_INSN_GAS(4, movsxd, 0, CPU_Hammer|CPU_64);
	}
	'movsxd' {
	    warn64 = 1;
	    RET_INSN_NONGAS(6, movsxd, 0, CPU_Hammer|CPU_64);
	}
	'movzb' [wWlL] { suffix_ofs = -2; RET_INSN_GAS(4, movszx, 0xB6, CPU_386); }
	'movzwl' { suffix_ofs = -2; RET_INSN_GAS(4, movszx, 0xB6, CPU_386); }
	'movz' [bBwW] Q {
	    suffix_ofs = -2;
	    warn64 = 1;
	    RET_INSN_GAS(4, movszx, 0xB6, CPU_Hammer|CPU_64);
	}
	'movzx' { RET_INSN_NS(movszx, 0xB6, CPU_386); }
	/* Push instructions */
	'push' [wWlLqQ]? { RET_INSN(4, push, 0, CPU_Any); }
	'pusha' {
	    not64 = 1;
	    RET_INSN_NS(onebyte, 0x0060, CPU_186);
	}
	'pushad' {
	    not64 = 1;
	    RET_INSN_NONGAS(6, onebyte, 0x2060, CPU_386);
	}
	'pushal' {
	    not64 = 1;
	    RET_INSN_GAS(6, onebyte, 0x2060, CPU_386);
	}
	'pushaw' {
	    not64 = 1;
	    RET_INSN_NS(onebyte, 0x1060, CPU_186);
	}
	/* Pop instructions */
	'pop' [wWlLqQ]? { RET_INSN(3, pop, 0, CPU_Any); }
	'popa' {
	    not64 = 1;
	    RET_INSN_NS(onebyte, 0x0061, CPU_186);
	}
	'popad' {
	    not64 = 1;
	    RET_INSN_NONGAS(5, onebyte, 0x2061, CPU_386);
	}
	'popal' {
	    not64 = 1;
	    RET_INSN_GAS(5, onebyte, 0x2061, CPU_386);
	}
	'popaw' {
	    not64 = 1;
	    RET_INSN_NS(onebyte, 0x1061, CPU_186);
	}
	/* Exchange */
	'xchg' [bBwWlLqQ]? { RET_INSN(4, xchg, 0, CPU_Any); }
	/* In/out from ports */
	'in' [bBwWlL]? { RET_INSN(2, in, 0, CPU_Any); }
	'out' [bBwWlL]? { RET_INSN(3, out, 0, CPU_Any); }
	/* Load effective address */
	'lea' [wWlLqQ]? { RET_INSN(3, lea, 0, CPU_Any); }
	/* Load segment registers from memory */
	'lds' [wWlL]? {
	    not64 = 1;
	    RET_INSN(3, ldes, 0xC5, CPU_Any);
	}
	'les' [wWlL]? {
	    not64 = 1;
	    RET_INSN(3, ldes, 0xC4, CPU_Any);
	}
	'lfs' [wWlL]? { RET_INSN(3, lfgss, 0xB4, CPU_386); }
	'lgs' [wWlL]? { RET_INSN(3, lfgss, 0xB5, CPU_386); }
	'lss' [wWlL]? { RET_INSN(3, lfgss, 0xB2, CPU_386); }
	/* Flags register instructions */
	'clc' { RET_INSN_NS(onebyte, 0x00F8, CPU_Any); }
	'cld' { RET_INSN_NS(onebyte, 0x00FC, CPU_Any); }
	'cli' { RET_INSN_NS(onebyte, 0x00FA, CPU_Any); }
	'clts' { RET_INSN_NS(twobyte, 0x0F06, CPU_286|CPU_Priv); }
	'cmc' { RET_INSN_NS(onebyte, 0x00F5, CPU_Any); }
	'lahf' { RET_INSN_NS(onebyte, 0x009F, CPU_Any); }
	'sahf' { RET_INSN_NS(onebyte, 0x009E, CPU_Any); }
	'pushf' { RET_INSN_NS(onebyte, 0x009C, CPU_Any); }
	'pushfd' { RET_INSN_NONGAS(6, onebyte, 0x209C, CPU_386); }
	'pushfl' { RET_INSN_GAS(6, onebyte, 0x209C, CPU_386); }
	'pushfw' { RET_INSN_NS(onebyte, 0x109C, CPU_Any); }
	'pushfq' {
	    warn64 = 1;
	    RET_INSN_NS(onebyte, 0x409C, CPU_Hammer|CPU_64);
	}
	'popf' { RET_INSN_NS(onebyte, 0x40009D, CPU_Any); }
	'popfd' {
	    not64 = 1;
	    RET_INSN_NONGAS(5, onebyte, 0x00209D, CPU_386);
	}
	'popfl' {
	    not64 = 1;
	    RET_INSN_GAS(5, onebyte, 0x00209D, CPU_386);
	}
	'popfw' { RET_INSN_NS(onebyte, 0x40109D, CPU_Any); }
	'popfq' {
	    warn64 = 1;
	    RET_INSN_NS(onebyte, 0x40409D, CPU_Hammer|CPU_64);
	}
	'stc' { RET_INSN_NS(onebyte, 0x00F9, CPU_Any); }
	'std' { RET_INSN_NS(onebyte, 0x00FD, CPU_Any); }
	'sti' { RET_INSN_NS(onebyte, 0x00FB, CPU_Any); }
	/* Arithmetic */
	'add' [bBwWlLqQ]? { RET_INSN(3, arith, 0x0000, CPU_Any); }
	'inc' [bBwWlLqQ]? { RET_INSN(3, incdec, 0x0040, CPU_Any); }
	'sub' [bBwWlLqQ]? { RET_INSN(3, arith, 0x0528, CPU_Any); }
	'dec' [bBwWlLqQ]? { RET_INSN(3, incdec, 0x0148, CPU_Any); }
	'sbb' [bBwWlLqQ]? { RET_INSN(3, arith, 0x0318, CPU_Any); }
	'cmp' [bBwWlLqQ]? { RET_INSN(3, arith, 0x0738, CPU_Any); }
	'test' [bBwWlLqQ]? { RET_INSN(4, test, 0, CPU_Any); }
	'and' [bBwWlLqQ]? { RET_INSN(3, arith, 0x0420, CPU_Any); }
	'or' [bBwWlLqQ]? { RET_INSN(2, arith, 0x0108, CPU_Any); }
	'xor' [bBwWlLqQ]? { RET_INSN(3, arith, 0x0630, CPU_Any); }
	'adc' [bBwWlLqQ]? { RET_INSN(3, arith, 0x0210, CPU_Any); }
	'neg' [bBwWlLqQ]? { RET_INSN(3, f6, 0x03, CPU_Any); }
	'not' [bBwWlLqQ]? { RET_INSN(3, f6, 0x02, CPU_Any); }
	'aaa' {
	    not64 = 1;
	    RET_INSN_NS(onebyte, 0x0037, CPU_Any);
	}
	'aas' {
	    not64 = 1;
	    RET_INSN_NS(onebyte, 0x003F, CPU_Any);
	}
	'daa' {
	    not64 = 1;
	    RET_INSN_NS(onebyte, 0x0027, CPU_Any);
	}
	'das' {
	    not64 = 1;
	    RET_INSN_NS(onebyte, 0x002F, CPU_Any);
	}
	'aad' {
	    not64 = 1;
	    RET_INSN_NS(aadm, 0x01, CPU_Any);
	}
	'aam' {
	    not64 = 1;
	    RET_INSN_NS(aadm, 0x00, CPU_Any);
	}
	/* Conversion instructions */
	'cbw' { RET_INSN_NS(onebyte, 0x1098, CPU_Any); }
	'cwde' { RET_INSN_NS(onebyte, 0x2098, CPU_386); }
	'cdqe' {
	    warn64 = 1;
	    RET_INSN_NS(onebyte, 0x4098, CPU_Hammer|CPU_64);
	}
	'cwd' { RET_INSN_NS(onebyte, 0x1099, CPU_Any); }
	'cdq' { RET_INSN_NS(onebyte, 0x2099, CPU_386); }
	'cqo' {
	    warn64 = 1;
	    RET_INSN_NS(onebyte, 0x4099, CPU_Hammer|CPU_64);
	}
	/* Conversion instructions - GAS / AT&T naming */
	'cbtw' { RET_INSN_GAS(4, onebyte, 0x1098, CPU_Any); }
	'cwtl' { RET_INSN_GAS(4, onebyte, 0x2098, CPU_386); }
	'cltq' {
	    warn64 = 1;
	    RET_INSN_GAS(4, onebyte, 0x4098, CPU_Hammer|CPU_64);
	}
	'cwtd' { RET_INSN_GAS(4, onebyte, 0x1099, CPU_Any); }
	'cltd' { RET_INSN_GAS(4, onebyte, 0x2099, CPU_386); }
	'cqto' {
	    warn64 = 1;
	    RET_INSN_GAS(4, onebyte, 0x4099, CPU_Hammer|CPU_64);
	}
	/* Multiplication and division */
	'mul' [bBwWlLqQ]? { RET_INSN(3, f6, 0x04, CPU_Any); }
	'imul' [bBwWlLqQ]? { RET_INSN(4, imul, 0, CPU_Any); }
	'div' [bBwWlLqQ]? { RET_INSN(3, div, 0x06, CPU_Any); }
	'idiv' [bBwWlLqQ]? { RET_INSN(4, div, 0x07, CPU_Any); }
	/* Shifts */
	'rol' [bBwWlLqQ]? { RET_INSN(3, shift, 0x00, CPU_Any); }
	'ror' [bBwWlLqQ]? { RET_INSN(3, shift, 0x01, CPU_Any); }
	'rcl' [bBwWlLqQ]? { RET_INSN(3, shift, 0x02, CPU_Any); }
	'rcr' [bBwWlLqQ]? { RET_INSN(3, shift, 0x03, CPU_Any); }
	'sal' [bBwWlLqQ]? { RET_INSN(3, shift, 0x04, CPU_Any); }
	'shl' [bBwWlLqQ]? { RET_INSN(3, shift, 0x04, CPU_Any); }
	'shr' [bBwWlLqQ]? { RET_INSN(3, shift, 0x05, CPU_Any); }
	'sar' [bBwWlLqQ]? { RET_INSN(3, shift, 0x07, CPU_Any); }
	'shld' [wWlLqQ]? { RET_INSN(4, shlrd, 0xA4, CPU_386); }
	'shrd' [wWlLqQ]? { RET_INSN(4, shlrd, 0xAC, CPU_386); }
	/* Control transfer instructions (unconditional) */
	'call' { RET_INSN(4, call, 0, CPU_Any); }
	'jmp' { RET_INSN(3, jmp, 0, CPU_Any); }
	'ret' W? { RET_INSN(3, retnf, 0xC2, CPU_Any); }
	'retl' {
	    not64 = 1;
	    RET_INSN_GAS(3, retnf, 0xC2, CPU_Any);
	}
	'retq' {
	    warn64 = 1;
	    RET_INSN_GAS(3, retnf, 0xC2, CPU_Hammer|CPU_64);
	}
	'retn' { RET_INSN_NONGAS(4, retnf, 0xC2, CPU_Any); }
	'retf' { RET_INSN_NONGAS(4, retnf, 0xCA, CPU_Any); }
	'lretw' { RET_INSN_GAS(4, retnf, 0xCA, CPU_Any); }
	'lretl' {
	    not64 = 1;
	    RET_INSN_GAS(4, retnf, 0xCA, CPU_Any);
	}
	'lretq' {
	    warn64 = 1;
	    RET_INSN_GAS(4, retnf, 0xCA, CPU_Any);
	}
	'enter' [wWlLqQ]? { RET_INSN(5, enter, 0, CPU_186); }
	'leave' { RET_INSN_NS(onebyte, 0x4000C9, CPU_186); }
	'leave' [wW] { RET_INSN_GAS(6, onebyte, 0x0010C9, CPU_186); }
	'leave' [lLqQ] { RET_INSN_GAS(6, onebyte, 0x4000C9, CPU_186); }
	/* Conditional jumps */
	'jo' { RET_INSN_NS(jcc, 0x00, CPU_Any); }
	'jno' { RET_INSN_NS(jcc, 0x01, CPU_Any); }
	'jb' { RET_INSN_NS(jcc, 0x02, CPU_Any); }
	'jc' { RET_INSN_NS(jcc, 0x02, CPU_Any); }
	'jnae' { RET_INSN_NS(jcc, 0x02, CPU_Any); }
	'jnb' { RET_INSN_NS(jcc, 0x03, CPU_Any); }
	'jnc' { RET_INSN_NS(jcc, 0x03, CPU_Any); }
	'jae' { RET_INSN_NS(jcc, 0x03, CPU_Any); }
	'je' { RET_INSN_NS(jcc, 0x04, CPU_Any); }
	'jz' { RET_INSN_NS(jcc, 0x04, CPU_Any); }
	'jne' { RET_INSN_NS(jcc, 0x05, CPU_Any); }
	'jnz' { RET_INSN_NS(jcc, 0x05, CPU_Any); }
	'jbe' { RET_INSN_NS(jcc, 0x06, CPU_Any); }
	'jna' { RET_INSN_NS(jcc, 0x06, CPU_Any); }
	'jnbe' { RET_INSN_NS(jcc, 0x07, CPU_Any); }
	'ja' { RET_INSN_NS(jcc, 0x07, CPU_Any); }
	'js' { RET_INSN_NS(jcc, 0x08, CPU_Any); }
	'jns' { RET_INSN_NS(jcc, 0x09, CPU_Any); }
	'jp' { RET_INSN_NS(jcc, 0x0A, CPU_Any); }
	'jpe' { RET_INSN_NS(jcc, 0x0A, CPU_Any); }
	'jnp' { RET_INSN_NS(jcc, 0x0B, CPU_Any); }
	'jpo' { RET_INSN_NS(jcc, 0x0B, CPU_Any); }
	'jl' { RET_INSN_NS(jcc, 0x0C, CPU_Any); }
	'jnge' { RET_INSN_NS(jcc, 0x0C, CPU_Any); }
	'jnl' { RET_INSN_NS(jcc, 0x0D, CPU_Any); }
	'jge' { RET_INSN_NS(jcc, 0x0D, CPU_Any); }
	'jle' { RET_INSN_NS(jcc, 0x0E, CPU_Any); }
	'jng' { RET_INSN_NS(jcc, 0x0E, CPU_Any); }
	'jnle' { RET_INSN_NS(jcc, 0x0F, CPU_Any); }
	'jg' { RET_INSN_NS(jcc, 0x0F, CPU_Any); }
	'jcxz' { RET_INSN_NS(jcxz, 16, CPU_Any); }
	'jecxz' { RET_INSN_NS(jcxz, 32, CPU_386); }
	'jrcxz' {
	    warn64 = 1;
	    RET_INSN_NS(jcxz, 64, CPU_Hammer|CPU_64);
	}
	/* Loop instructions */
	'loop' { RET_INSN_NS(loop, 0x02, CPU_Any); }
	'loopz' { RET_INSN_NS(loop, 0x01, CPU_Any); }
	'loope' { RET_INSN_NS(loop, 0x01, CPU_Any); }
	'loopnz' { RET_INSN_NS(loop, 0x00, CPU_Any); }
	'loopne' { RET_INSN_NS(loop, 0x00, CPU_Any); }
	/* Set byte on flag instructions */
	'seto' B? { RET_INSN(4, setcc, 0x00, CPU_386); }
	'setno' B? { RET_INSN(5, setcc, 0x01, CPU_386); }
	'setb' B? { RET_INSN(4, setcc, 0x02, CPU_386); }
	'setc' B? { RET_INSN(4, setcc, 0x02, CPU_386); }
	'setnae' B? { RET_INSN(6, setcc, 0x02, CPU_386); }
	'setnb' B? { RET_INSN(5, setcc, 0x03, CPU_386); }
	'setnc' B? { RET_INSN(5, setcc, 0x03, CPU_386); }
	'setae' B? { RET_INSN(5, setcc, 0x03, CPU_386); }
	'sete' B? { RET_INSN(4, setcc, 0x04, CPU_386); }
	'setz' B? { RET_INSN(4, setcc, 0x04, CPU_386); }
	'setne' B? { RET_INSN(5, setcc, 0x05, CPU_386); }
	'setnz' B? { RET_INSN(5, setcc, 0x05, CPU_386); }
	'setbe' B? { RET_INSN(5, setcc, 0x06, CPU_386); }
	'setna' B? { RET_INSN(5, setcc, 0x06, CPU_386); }
	'setnbe' B? { RET_INSN(6, setcc, 0x07, CPU_386); }
	'seta' B? { RET_INSN(4, setcc, 0x07, CPU_386); }
	'sets' B? { RET_INSN(4, setcc, 0x08, CPU_386); }
	'setns' B? { RET_INSN(5, setcc, 0x09, CPU_386); }
	'setp' B? { RET_INSN(4, setcc, 0x0A, CPU_386); }
	'setpe' B? { RET_INSN(5, setcc, 0x0A, CPU_386); }
	'setnp' B? { RET_INSN(5, setcc, 0x0B, CPU_386); }
	'setpo' B? { RET_INSN(5, setcc, 0x0B, CPU_386); }
	'setl' B? { RET_INSN(4, setcc, 0x0C, CPU_386); }
	'setnge' B? { RET_INSN(6, setcc, 0x0C, CPU_386); }
	'setnl' B? { RET_INSN(5, setcc, 0x0D, CPU_386); }
	'setge' B? { RET_INSN(5, setcc, 0x0D, CPU_386); }
	'setle' B? { RET_INSN(5, setcc, 0x0E, CPU_386); }
	'setng' B? { RET_INSN(5, setcc, 0x0E, CPU_386); }
	'setnle' B? { RET_INSN(6, setcc, 0x0F, CPU_386); }
	'setg' B? { RET_INSN(4, setcc, 0x0F, CPU_386); }
	/* String instructions. */
	'cmpsb' { RET_INSN_NS(onebyte, 0x00A6, CPU_Any); }
	'cmpsw' { RET_INSN_NS(onebyte, 0x10A7, CPU_Any); }
	'cmpsd' { RET_INSN_NS(cmpsd, 0, CPU_Any); }
	'cmpsl' { RET_INSN_GAS(5, onebyte, 0x20A7, CPU_386); }
	'cmpsq' {
	    warn64 = 1;
	    RET_INSN_NS(onebyte, 0x40A7, CPU_Hammer|CPU_64);
	}
	'insb' { RET_INSN_NS(onebyte, 0x006C, CPU_Any); }
	'insw' { RET_INSN_NS(onebyte, 0x106D, CPU_Any); }
	'insd' { RET_INSN_NONGAS(4, onebyte, 0x206D, CPU_386); }
	'insl' { RET_INSN_GAS(4, onebyte, 0x206D, CPU_386); }
	'outsb' { RET_INSN_NS(onebyte, 0x006E, CPU_Any); }
	'outsw' { RET_INSN_NS(onebyte, 0x106F, CPU_Any); }
	'outsd' { RET_INSN_NONGAS(5, onebyte, 0x206F, CPU_386); }
	'outsl' { RET_INSN_GAS(5, onebyte, 0x206F, CPU_386); }
	'lodsb' { RET_INSN_NS(onebyte, 0x00AC, CPU_Any); }
	'lodsw' { RET_INSN_NS(onebyte, 0x10AD, CPU_Any); }
	'lodsd' { RET_INSN_NONGAS(5, onebyte, 0x20AD, CPU_386); }
	'lodsl' { RET_INSN_GAS(5, onebyte, 0x20AD, CPU_386); }
	'lodsq' {
	    warn64 = 1;
	    RET_INSN_NS(onebyte, 0x40AD, CPU_Hammer|CPU_64);
	}
	'movsb' { RET_INSN_NS(onebyte, 0x00A4, CPU_Any); }
	'movsw' { RET_INSN_NS(onebyte, 0x10A5, CPU_Any); }
	'movsd' { RET_INSN_NS(movsd, 0, CPU_Any); }
	'movsl' { RET_INSN_GAS(5, onebyte, 0x20A5, CPU_386); }
	'movsq' {
	    warn64 = 1;
	    RET_INSN_NS(onebyte, 0x40A5, CPU_Any);
	}
	/* smov alias for movs in GAS mode */
	'smovb' { RET_INSN_GAS(5, onebyte, 0x00A4, CPU_Any); }
	'smovw' { RET_INSN_GAS(5, onebyte, 0x10A5, CPU_Any); }
	'smovl' { RET_INSN_GAS(5, onebyte, 0x20A5, CPU_386); }
	'smovq' {
	    warn64 = 1;
	    RET_INSN_GAS(5, onebyte, 0x40A5, CPU_Any);
	}
	'scasb' { RET_INSN_NS(onebyte, 0x00AE, CPU_Any); }
	'scasw' { RET_INSN_NS(onebyte, 0x10AF, CPU_Any); }
	'scasd' { RET_INSN_NONGAS(5, onebyte, 0x20AF, CPU_386); }
	'scasl' { RET_INSN_GAS(5, onebyte, 0x20AF, CPU_386); }
	'scasq' {
	    warn64 = 1;
	    RET_INSN_NS(onebyte, 0x40AF, CPU_Hammer|CPU_64);
	}
	/* ssca alias for scas in GAS mode */
	'sscab' { RET_INSN_GAS(5, onebyte, 0x00AE, CPU_Any); }
	'sscaw' { RET_INSN_GAS(5, onebyte, 0x10AF, CPU_Any); }
	'sscal' { RET_INSN_GAS(5, onebyte, 0x20AF, CPU_386); }
	'sscaq' {
	    warn64 = 1;
	    RET_INSN_GAS(5, onebyte, 0x40AF, CPU_Hammer|CPU_64);
	}
	'stosb' { RET_INSN_NS(onebyte, 0x00AA, CPU_Any); }
	'stosw' { RET_INSN_NS(onebyte, 0x10AB, CPU_Any); }
	'stosd' { RET_INSN_NONGAS(5, onebyte, 0x20AB, CPU_386); }
	'stosl' { RET_INSN_GAS(5, onebyte, 0x20AB, CPU_386); }
	'stosq' {
	    warn64 = 1;
	    RET_INSN_NS(onebyte, 0x40AB, CPU_Hammer|CPU_64);
	}
	'xlat' B? { RET_INSN(5, onebyte, 0x00D7, CPU_Any); }
	/* Bit manipulation */
	'bsf' [wWlLqQ]? { RET_INSN(3, bsfr, 0xBC, CPU_386); }
	'bsr' [wWlLqQ]? { RET_INSN(3, bsfr, 0xBD, CPU_386); }
	'bt' [wWlLqQ]? { RET_INSN(2, bittest, 0x04A3, CPU_386); }
	'btc' [wWlLqQ]? { RET_INSN(3, bittest, 0x07BB, CPU_386); }
	'btr' [wWlLqQ]? { RET_INSN(3, bittest, 0x06B3, CPU_386); }
	'bts' [wWlLqQ]? { RET_INSN(3, bittest, 0x05AB, CPU_386); }
	/* Interrupts and operating system instructions */
	'int' { RET_INSN_NS(int, 0, CPU_Any); }
	'int3' { RET_INSN_NS(onebyte, 0x00CC, CPU_Any); }
	'int03' { RET_INSN_NONGAS(5, onebyte, 0x00CC, CPU_Any); }
	'into' {
	    not64 = 1;
	    RET_INSN_NS(onebyte, 0x00CE, CPU_Any);
	}
	'iret' { RET_INSN_NS(onebyte, 0x00CF, CPU_Any); }
	'iretw' { RET_INSN_NS(onebyte, 0x10CF, CPU_Any); }
	'iretd' { RET_INSN_NONGAS(5, onebyte, 0x20CF, CPU_386); }
	'iretl' { RET_INSN_GAS(5, onebyte, 0x20CF, CPU_386); }
	'iretq' {
	    warn64 = 1;
	    RET_INSN_NS(onebyte, 0x40CF, CPU_Hammer|CPU_64);
	}
	'rsm' { RET_INSN_NS(twobyte, 0x0FAA, CPU_586|CPU_SMM); }
	'bound' [wWlL]? {
	    not64 = 1;
	    RET_INSN(5, bound, 0, CPU_186);
	}
	'hlt' { RET_INSN_NS(onebyte, 0x00F4, CPU_Priv); }
	'nop' { RET_INSN_NS(onebyte, 0x0090, CPU_Any); }
	/* Protection control */
	'arpl' W? {
	    not64 = 1;
	    RET_INSN(4, arpl, 0, CPU_286|CPU_Prot);
	}
	'lar' [wWlLqQ]? { RET_INSN(3, bsfr, 0x02, CPU_286|CPU_Prot); }
	'lgdt' [wWlLqQ]? { RET_INSN(4, twobytemem, 0x020F01, CPU_286|CPU_Priv); }
	'lidt' [wWlLqQ]? { RET_INSN(4, twobytemem, 0x030F01, CPU_286|CPU_Priv); }
	'lldt' W? { RET_INSN(4, prot286, 0x0200, CPU_286|CPU_Prot|CPU_Priv); }
	'lmsw' W? { RET_INSN(4, prot286, 0x0601, CPU_286|CPU_Priv); }
	'lsl' [wWlLqQ]? { RET_INSN(3, bsfr, 0x03, CPU_286|CPU_Prot); }
	'ltr' W? { RET_INSN(3, prot286, 0x0300, CPU_286|CPU_Prot|CPU_Priv); }
	'sgdt' [wWlLqQ]? { RET_INSN(4, twobytemem, 0x000F01, CPU_286|CPU_Priv); }
	'sidt' [wWlLqQ]? { RET_INSN(4, twobytemem, 0x010F01, CPU_286|CPU_Priv); }
	'sldt' [wWlLqQ]? { RET_INSN(4, sldtmsw, 0x0000, CPU_286); }
	'smsw' [wWlLqQ]? { RET_INSN(4, sldtmsw, 0x0401, CPU_286); }
	'str' [wWlLqQ]? { RET_INSN(3, str, 0, CPU_286|CPU_Prot); }
	'verr' W? { RET_INSN(4, prot286, 0x0400, CPU_286|CPU_Prot); }
	'verw' W? { RET_INSN(4, prot286, 0x0500, CPU_286|CPU_Prot); }
	/* Floating point instructions */
	'fld' [lLsS]? { RET_INSN(3, fldstp, 0x0500C0, CPU_FPU); }
	'fldt' {
	    data[3] |= 0x80 << 8;
	    RET_INSN_GAS(4, fldstpt, 0x0500C0, CPU_FPU);
	}
	'fild' [lLqQsS]? { RET_INSN(4, fildstp, 0x050200, CPU_FPU); }
	'fildll' { RET_INSN_GAS(6, fbldstp, 0x05, CPU_FPU); }
	'fbld' { RET_INSN(4, fbldstp, 0x04, CPU_FPU); }
	'fst' [lLsS]? { RET_INSN(3, fst, 0, CPU_FPU); }
	'fist' [lLsS]? { RET_INSN(4, fiarith, 0x02DB, CPU_FPU); }
	'fstp' [lLsS]? { RET_INSN(4, fldstp, 0x0703D8, CPU_FPU); }
	'fstpt' {
	    data[3] |= 0x80 << 8;
	    RET_INSN_GAS(5, fldstpt, 0x0703D8, CPU_FPU);
	}
	'fistp' [lLqQsS]? { RET_INSN(5, fildstp, 0x070203, CPU_FPU); }
	'fistpll' { RET_INSN_GAS(7, fbldstp, 0x07, CPU_FPU); }
	'fbstp' { RET_INSN_NS(fbldstp, 0x06, CPU_FPU); }
	'fxch' { RET_INSN_NS(fxch, 0, CPU_FPU); }
	'fcom' [lLsS]? { RET_INSN(4, fcom, 0x02D0, CPU_FPU); }
	'ficom' [lLsS]? { RET_INSN(5, fiarith, 0x02DA, CPU_FPU); }
	'fcomp' [lLsS]? { RET_INSN(5, fcom, 0x03D8, CPU_FPU); }
	'ficomp' [lLsS]? { RET_INSN(6, fiarith, 0x03DA, CPU_FPU); }
	'fcompp' { RET_INSN_NS(twobyte, 0xDED9, CPU_FPU); }
	'fucom' { RET_INSN_NS(fcom2, 0xDDE0, CPU_286|CPU_FPU); }
	'fucomp' { RET_INSN_NS(fcom2, 0xDDE8, CPU_286|CPU_FPU); }
	'fucompp' { RET_INSN_NS(twobyte, 0xDAE9, CPU_286|CPU_FPU); }
	'ftst' { RET_INSN_NS(twobyte, 0xD9E4, CPU_FPU); }
	'fxam' { RET_INSN_NS(twobyte, 0xD9E5, CPU_FPU); }
	'fld1' { RET_INSN_NS(twobyte, 0xD9E8, CPU_FPU); }
	'fldl2t' { RET_INSN_NS(twobyte, 0xD9E9, CPU_FPU); }
	'fldl2e' { RET_INSN_NS(twobyte, 0xD9EA, CPU_FPU); }
	'fldpi' { RET_INSN_NS(twobyte, 0xD9EB, CPU_FPU); }
	'fldlg2' { RET_INSN_NS(twobyte, 0xD9EC, CPU_FPU); }
	'fldln2' { RET_INSN_NS(twobyte, 0xD9ED, CPU_FPU); }
	'fldz' { RET_INSN_NS(twobyte, 0xD9EE, CPU_FPU); }
	'fadd' [lLsS]? { RET_INSN(4, farith, 0x00C0C0, CPU_FPU); }
	'faddp' { RET_INSN_NS(farithp, 0xC0, CPU_FPU); }
	'fiadd' [lLsS]? { RET_INSN(5, fiarith, 0x00DA, CPU_FPU); }
	'fsub' [lLsS]? { RET_INSN(4, farith, 0x04E0E8, CPU_FPU); }
	'fisub' [lLsS]? { RET_INSN(5, fiarith, 0x04DA, CPU_FPU); }
	'fsubp' { RET_INSN_NS(farithp, 0xE8, CPU_FPU); }
	'fsubr' [lLsS]? { RET_INSN(5, farith, 0x05E8E0, CPU_FPU); }
	'fisubr' [lLsS]? { RET_INSN(6, fiarith, 0x05DA, CPU_FPU); }
	'fsubrp' { RET_INSN_NS(farithp, 0xE0, CPU_FPU); }
	'fmul' [lLsS]? { RET_INSN(4, farith, 0x01C8C8, CPU_FPU); }
	'fimul' [lLsS]? { RET_INSN(5, fiarith, 0x01DA, CPU_FPU); }
	'fmulp' { RET_INSN_NS(farithp, 0xC8, CPU_FPU); }
	'fdiv' [lLsS]? { RET_INSN(4, farith, 0x06F0F8, CPU_FPU); }
	'fidiv' [lLsS]? { RET_INSN(5, fiarith, 0x06DA, CPU_FPU); }
	'fdivp' { RET_INSN_NS(farithp, 0xF8, CPU_FPU); }
	'fdivr' [lLsS]? { RET_INSN(5, farith, 0x07F8F0, CPU_FPU); }
	'fidivr' [lLsS]? { RET_INSN(6, fiarith, 0x07DA, CPU_FPU); }
	'fdivrp' { RET_INSN_NS(farithp, 0xF0, CPU_FPU); }
	'f2xm1' { RET_INSN_NS(twobyte, 0xD9F0, CPU_FPU); }
	'fyl2x' { RET_INSN_NS(twobyte, 0xD9F1, CPU_FPU); }
	'fptan' { RET_INSN_NS(twobyte, 0xD9F2, CPU_FPU); }
	'fpatan' { RET_INSN_NS(twobyte, 0xD9F3, CPU_FPU); }
	'fxtract' { RET_INSN_NS(twobyte, 0xD9F4, CPU_FPU); }
	'fprem1' { RET_INSN_NS(twobyte, 0xD9F5, CPU_286|CPU_FPU); }
	'fdecstp' { RET_INSN_NS(twobyte, 0xD9F6, CPU_FPU); }
	'fincstp' { RET_INSN_NS(twobyte, 0xD9F7, CPU_FPU); }
	'fprem' { RET_INSN_NS(twobyte, 0xD9F8, CPU_FPU); }
	'fyl2xp1' { RET_INSN_NS(twobyte, 0xD9F9, CPU_FPU); }
	'fsqrt' { RET_INSN_NS(twobyte, 0xD9FA, CPU_FPU); }
	'fsincos' { RET_INSN_NS(twobyte, 0xD9FB, CPU_286|CPU_FPU); }
	'frndint' { RET_INSN_NS(twobyte, 0xD9FC, CPU_FPU); }
	'fscale' { RET_INSN_NS(twobyte, 0xD9FD, CPU_FPU); }
	'fsin' { RET_INSN_NS(twobyte, 0xD9FE, CPU_286|CPU_FPU); }
	'fcos' { RET_INSN_NS(twobyte, 0xD9FF, CPU_286|CPU_FPU); }
	'fchs' { RET_INSN_NS(twobyte, 0xD9E0, CPU_FPU); }
	'fabs' { RET_INSN_NS(twobyte, 0xD9E1, CPU_FPU); }
	'fninit' { RET_INSN_NS(twobyte, 0xDBE3, CPU_FPU); }
	'finit' { RET_INSN_NS(threebyte, 0x9BDBE3UL, CPU_FPU); }
	'fldcw' W? { RET_INSN(5, fldnstcw, 0x05, CPU_FPU); }
	'fnstcw' W? { RET_INSN(6, fldnstcw, 0x07, CPU_FPU); }
	'fstcw' W? { RET_INSN(5, fstcw, 0, CPU_FPU); }
	'fnstsw' W? { RET_INSN(6, fnstsw, 0, CPU_FPU); }
	'fstsw' W? { RET_INSN(5, fstsw, 0, CPU_FPU); }
	'fnclex' { RET_INSN_NS(twobyte, 0xDBE2, CPU_FPU); }
	'fclex' { RET_INSN_NS(threebyte, 0x9BDBE2UL, CPU_FPU); }
	'fnstenv' [lLsS]? { RET_INSN(7, onebytemem, 0x06D9, CPU_FPU); }
	'fstenv' [lLsS]? { RET_INSN(6, twobytemem, 0x069BD9, CPU_FPU); }
	'fldenv' [lLsS]? { RET_INSN(6, onebytemem, 0x04D9, CPU_FPU); }
	'fnsave' [lLsS]? { RET_INSN(6, onebytemem, 0x06DD, CPU_FPU); }
	'fsave' [lLsS]? { RET_INSN(5, twobytemem, 0x069BDD, CPU_FPU); }
	'frstor' [lLsS]? { RET_INSN(6, onebytemem, 0x04DD, CPU_FPU); }
	'ffree' { RET_INSN_NS(ffree, 0xDD, CPU_FPU); }
	'ffreep' { RET_INSN_NS(ffree, 0xDF, CPU_686|CPU_FPU|CPU_Undoc); }
	'fnop' { RET_INSN_NS(twobyte, 0xD9D0, CPU_FPU); }
	'fwait' { RET_INSN_NS(onebyte, 0x009B, CPU_FPU); }
	/* Prefixes (should the others be here too? should wait be a prefix? */
	'wait' { RET_INSN_NS(onebyte, 0x009B, CPU_Any); }
	/* 486 extensions */
	'bswap' [lLqQ]? { RET_INSN(5, bswap, 0, CPU_486); }
	'xadd' [bBwWlLqQ]? { RET_INSN(4, cmpxchgxadd, 0xC0, CPU_486); }
	'cmpxchg' [bBwWlLqQ]? { RET_INSN(7, cmpxchgxadd, 0xB0, CPU_486); }
	'cmpxchg486' { RET_INSN_NONGAS(10, cmpxchgxadd, 0xA6, CPU_486|CPU_Undoc); }
	'invd' { RET_INSN_NS(twobyte, 0x0F08, CPU_486|CPU_Priv); }
	'wbinvd' { RET_INSN_NS(twobyte, 0x0F09, CPU_486|CPU_Priv); }
	'invlpg' { RET_INSN_NS(twobytemem, 0x070F01, CPU_486|CPU_Priv); }
	/* 586+ and late 486 extensions */
	'cpuid' { RET_INSN_NS(twobyte, 0x0FA2, CPU_486); }
	/* Pentium extensions */
	'wrmsr' { RET_INSN_NS(twobyte, 0x0F30, CPU_586|CPU_Priv); }
	'rdtsc' { RET_INSN_NS(twobyte, 0x0F31, CPU_586); }
	'rdmsr' { RET_INSN_NS(twobyte, 0x0F32, CPU_586|CPU_Priv); }
	'cmpxchg8b' Q? { RET_INSN(9, cmpxchg8b, 0, CPU_586); }
	/* Pentium II/Pentium Pro extensions */
	'sysenter' {
	    not64 = 1;
	    RET_INSN_NS(twobyte, 0x0F34, CPU_686);
	}
	'sysexit' {
	    not64 = 1;
	    RET_INSN_NS(twobyte, 0x0F35, CPU_686|CPU_Priv);
	}
	'fxsave' Q? { RET_INSN(6, twobytemem, 0x000FAE, CPU_686|CPU_FPU); }
	'fxrstor' Q? { RET_INSN(7, twobytemem, 0x010FAE, CPU_686|CPU_FPU); }
	'rdpmc' { RET_INSN_NS(twobyte, 0x0F33, CPU_686); }
	'ud2' { RET_INSN_NS(twobyte, 0x0F0B, CPU_286); }
	'ud1' { RET_INSN_NS(twobyte, 0x0FB9, CPU_286|CPU_Undoc); }
	'cmovo' [wWlLqQ]? { RET_INSN(5, cmovcc, 0x00, CPU_686); }
	'cmovno' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x01, CPU_686); }
	'cmovb' [wWlLqQ]? { RET_INSN(5, cmovcc, 0x02, CPU_686); }
	'cmovc' [wWlLqQ]? { RET_INSN(5, cmovcc, 0x02, CPU_686); }
	'cmovnae' [wWlLqQ]? { RET_INSN(7, cmovcc, 0x02, CPU_686); }
	'cmovnb' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x03, CPU_686); }
	'cmovnc' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x03, CPU_686); }
	'cmovae' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x03, CPU_686); }
	'cmove' [wWlLqQ]? { RET_INSN(5, cmovcc, 0x04, CPU_686); }
	'cmovz' [wWlLqQ]? { RET_INSN(5, cmovcc, 0x04, CPU_686); }
	'cmovne' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x05, CPU_686); }
	'cmovnz' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x05, CPU_686); }
	'cmovbe' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x06, CPU_686); }
	'cmovna' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x06, CPU_686); }
	'cmovnbe' [wWlLqQ]? { RET_INSN(7, cmovcc, 0x07, CPU_686); }
	'cmova' [wWlLqQ]? { RET_INSN(5, cmovcc, 0x07, CPU_686); }
	'cmovs' [wWlLqQ]? { RET_INSN(5, cmovcc, 0x08, CPU_686); }
	'cmovns' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x09, CPU_686); }
	'cmovp' [wWlLqQ]? { RET_INSN(5, cmovcc, 0x0A, CPU_686); }
	'cmovpe' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0A, CPU_686); }
	'cmovnp' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0B, CPU_686); }
	'cmovpo' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0B, CPU_686); }
	'cmovl' [wWlLqQ]? { RET_INSN(5, cmovcc, 0x0C, CPU_686); }
	'cmovnge' [wWlLqQ]? { RET_INSN(7, cmovcc, 0x0C, CPU_686); }
	'cmovnl' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0D, CPU_686); }
	'cmovge' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0D, CPU_686); }
	'cmovle' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0E, CPU_686); }
	'cmovng' [wWlLqQ]? { RET_INSN(6, cmovcc, 0x0E, CPU_686); }
	'cmovnle' [wWlLqQ]? { RET_INSN(7, cmovcc, 0x0F, CPU_686); }
	'cmovg' [wWlLqQ]? { RET_INSN(5, cmovcc, 0x0F, CPU_686); }
	'fcmovb' { RET_INSN_NS(fcmovcc, 0xDAC0, CPU_686|CPU_FPU); }
	'fcmove' { RET_INSN_NS(fcmovcc, 0xDAC8, CPU_686|CPU_FPU); }
	'fcmovbe' { RET_INSN_NS(fcmovcc, 0xDAD0, CPU_686|CPU_FPU); }
	'fcmovu' { RET_INSN_NS(fcmovcc, 0xDAD8, CPU_686|CPU_FPU); }
	'fcmovnb' { RET_INSN_NS(fcmovcc, 0xDBC0, CPU_686|CPU_FPU); }
	'fcmovne' { RET_INSN_NS(fcmovcc, 0xDBC8, CPU_686|CPU_FPU); }
	'fcmovnbe' { RET_INSN_NS(fcmovcc, 0xDBD0, CPU_686|CPU_FPU); }
	'fcmovnu' { RET_INSN_NS(fcmovcc, 0xDBD8, CPU_686|CPU_FPU); }
	'fcomi' { RET_INSN_NS(fcom2, 0xDBF0, CPU_686|CPU_FPU); }
	'fucomi' { RET_INSN_NS(fcom2, 0xDBE8, CPU_686|CPU_FPU); }
	'fcomip' { RET_INSN_NS(fcom2, 0xDFF0, CPU_686|CPU_FPU); }
	'fucomip' { RET_INSN_NS(fcom2, 0xDFE8, CPU_686|CPU_FPU); }
	/* Pentium4 extensions */
	'movnti' [lLqQ]? { RET_INSN(6, movnti, 0, CPU_P4); }
	'clflush' { RET_INSN_NS(clflush, 0, CPU_P3); }
	'lfence' { RET_INSN_NS(threebyte, 0x0FAEE8, CPU_P3); }
	'mfence' { RET_INSN_NS(threebyte, 0x0FAEF0, CPU_P3); }
	'pause' { RET_INSN_NS(onebyte_prefix, 0xF390, CPU_P4); }
	/* MMX/SSE2 instructions */
	'emms' { RET_INSN_NS(twobyte, 0x0F77, CPU_MMX); }
	'movd' { RET_INSN_NS(movd, 0, CPU_MMX); }
	'movq' {
	    if (arch_x86->parser == X86_PARSER_GAS)
		RET_INSN(3, mov, 0, CPU_Any);
	    else
		RET_INSN_NS(movq, 0, CPU_MMX);
	}
	'packssdw' { RET_INSN_NS(mmxsse2, 0x6B, CPU_MMX); }
	'packsswb' { RET_INSN_NS(mmxsse2, 0x63, CPU_MMX); }
	'packuswb' { RET_INSN_NS(mmxsse2, 0x67, CPU_MMX); }
	'paddb' { RET_INSN_NS(mmxsse2, 0xFC, CPU_MMX); }
	'paddw' { RET_INSN_NS(mmxsse2, 0xFD, CPU_MMX); }
	'paddd' { RET_INSN_NS(mmxsse2, 0xFE, CPU_MMX); }
	'paddq' { RET_INSN_NS(mmxsse2, 0xD4, CPU_MMX); }
	'paddsb' { RET_INSN_NS(mmxsse2, 0xEC, CPU_MMX); }
	'paddsw' { RET_INSN_NS(mmxsse2, 0xED, CPU_MMX); }
	'paddusb' { RET_INSN_NS(mmxsse2, 0xDC, CPU_MMX); }
	'paddusw' { RET_INSN_NS(mmxsse2, 0xDD, CPU_MMX); }
	'pand' { RET_INSN_NS(mmxsse2, 0xDB, CPU_MMX); }
	'pandn' { RET_INSN_NS(mmxsse2, 0xDF, CPU_MMX); }
	'pcmpeqb' { RET_INSN_NS(mmxsse2, 0x74, CPU_MMX); }
	'pcmpeqw' { RET_INSN_NS(mmxsse2, 0x75, CPU_MMX); }
	'pcmpeqd' { RET_INSN_NS(mmxsse2, 0x76, CPU_MMX); }
	'pcmpgtb' { RET_INSN_NS(mmxsse2, 0x64, CPU_MMX); }
	'pcmpgtw' { RET_INSN_NS(mmxsse2, 0x65, CPU_MMX); }
	'pcmpgtd' { RET_INSN_NS(mmxsse2, 0x66, CPU_MMX); }
	'pmaddwd' { RET_INSN_NS(mmxsse2, 0xF5, CPU_MMX); }
	'pmulhw' { RET_INSN_NS(mmxsse2, 0xE5, CPU_MMX); }
	'pmullw' { RET_INSN_NS(mmxsse2, 0xD5, CPU_MMX); }
	'por' { RET_INSN_NS(mmxsse2, 0xEB, CPU_MMX); }
	'psllw' { RET_INSN_NS(pshift, 0x0671F1, CPU_MMX); }
	'pslld' { RET_INSN_NS(pshift, 0x0672F2, CPU_MMX); }
	'psllq' { RET_INSN_NS(pshift, 0x0673F3, CPU_MMX); }
	'psraw' { RET_INSN_NS(pshift, 0x0471E1, CPU_MMX); }
	'psrad' { RET_INSN_NS(pshift, 0x0472E2, CPU_MMX); }
	'psrlw' { RET_INSN_NS(pshift, 0x0271D1, CPU_MMX); }
	'psrld' { RET_INSN_NS(pshift, 0x0272D2, CPU_MMX); }
	'psrlq' { RET_INSN_NS(pshift, 0x0273D3, CPU_MMX); }
	'psubb' { RET_INSN_NS(mmxsse2, 0xF8, CPU_MMX); }
	'psubw' { RET_INSN_NS(mmxsse2, 0xF9, CPU_MMX); }
	'psubd' { RET_INSN_NS(mmxsse2, 0xFA, CPU_MMX); }
	'psubq' { RET_INSN_NS(mmxsse2, 0xFB, CPU_MMX); }
	'psubsb' { RET_INSN_NS(mmxsse2, 0xE8, CPU_MMX); }
	'psubsw' { RET_INSN_NS(mmxsse2, 0xE9, CPU_MMX); }
	'psubusb' { RET_INSN_NS(mmxsse2, 0xD8, CPU_MMX); }
	'psubusw' { RET_INSN_NS(mmxsse2, 0xD9, CPU_MMX); }
	'punpckhbw' { RET_INSN_NS(mmxsse2, 0x68, CPU_MMX); }
	'punpckhwd' { RET_INSN_NS(mmxsse2, 0x69, CPU_MMX); }
	'punpckhdq' { RET_INSN_NS(mmxsse2, 0x6A, CPU_MMX); }
	'punpcklbw' { RET_INSN_NS(mmxsse2, 0x60, CPU_MMX); }
	'punpcklwd' { RET_INSN_NS(mmxsse2, 0x61, CPU_MMX); }
	'punpckldq' { RET_INSN_NS(mmxsse2, 0x62, CPU_MMX); }
	'pxor' { RET_INSN_NS(mmxsse2, 0xEF, CPU_MMX); }
	/* PIII (Katmai) new instructions / SIMD instructions */
	'addps' { RET_INSN_NS(sseps, 0x58, CPU_SSE); }
	'addss' { RET_INSN_NS(ssess, 0xF358, CPU_SSE); }
	'andnps' { RET_INSN_NS(sseps, 0x55, CPU_SSE); }
	'andps' { RET_INSN_NS(sseps, 0x54, CPU_SSE); }
	'cmpeqps' { RET_INSN_NS(ssecmpps, 0x00, CPU_SSE); }
	'cmpeqss' { RET_INSN_NS(ssecmpss, 0x00F3, CPU_SSE); }
	'cmpleps' { RET_INSN_NS(ssecmpps, 0x02, CPU_SSE); }
	'cmpless' { RET_INSN_NS(ssecmpss, 0x02F3, CPU_SSE); }
	'cmpltps' { RET_INSN_NS(ssecmpps, 0x01, CPU_SSE); }
	'cmpltss' { RET_INSN_NS(ssecmpss, 0x01F3, CPU_SSE); }
	'cmpneqps' { RET_INSN_NS(ssecmpps, 0x04, CPU_SSE); }
	'cmpneqss' { RET_INSN_NS(ssecmpss, 0x04F3, CPU_SSE); }
	'cmpnleps' { RET_INSN_NS(ssecmpps, 0x06, CPU_SSE); }
	'cmpnless' { RET_INSN_NS(ssecmpss, 0x06F3, CPU_SSE); }
	'cmpnltps' { RET_INSN_NS(ssecmpps, 0x05, CPU_SSE); }
	'cmpnltss' { RET_INSN_NS(ssecmpss, 0x05F3, CPU_SSE); }
	'cmpordps' { RET_INSN_NS(ssecmpps, 0x07, CPU_SSE); }
	'cmpordss' { RET_INSN_NS(ssecmpss, 0x07F3, CPU_SSE); }
	'cmpunordps' { RET_INSN_NS(ssecmpps, 0x03, CPU_SSE); }
	'cmpunordss' { RET_INSN_NS(ssecmpss, 0x03F3, CPU_SSE); }
	'cmpps' { RET_INSN_NS(ssepsimm, 0xC2, CPU_SSE); }
	'cmpss' { RET_INSN_NS(ssessimm, 0xF3C2, CPU_SSE); }
	'comiss' { RET_INSN_NS(sseps, 0x2F, CPU_SSE); }
	'cvtpi2ps' { RET_INSN_NS(cvt_xmm_mm_ps, 0x2A, CPU_SSE); }
	'cvtps2pi' { RET_INSN_NS(cvt_mm_xmm64, 0x2D, CPU_SSE); }
	'cvtsi2ss' [lLqQ]? { RET_INSN(8, cvt_xmm_rmx, 0xF32A, CPU_SSE); }
	'cvtss2si' [lLqQ]? { RET_INSN(8, cvt_rx_xmm32, 0xF32D, CPU_SSE); }
	'cvttps2pi' { RET_INSN_NS(cvt_mm_xmm64, 0x2C, CPU_SSE); }
	'cvttss2si' [lLqQ]? { RET_INSN(9, cvt_rx_xmm32, 0xF32C, CPU_SSE); }
	'divps' { RET_INSN_NS(sseps, 0x5E, CPU_SSE); }
	'divss' { RET_INSN_NS(ssess, 0xF35E, CPU_SSE); }
	'ldmxcsr' { RET_INSN_NS(ldstmxcsr, 0x02, CPU_SSE); }
	'maskmovq' { RET_INSN_NS(maskmovq, 0, CPU_P3|CPU_MMX); }
	'maxps' { RET_INSN_NS(sseps, 0x5F, CPU_SSE); }
	'maxss' { RET_INSN_NS(ssess, 0xF35F, CPU_SSE); }
	'minps' { RET_INSN_NS(sseps, 0x5D, CPU_SSE); }
	'minss' { RET_INSN_NS(ssess, 0xF35D, CPU_SSE); }
	'movaps' { RET_INSN_NS(movaups, 0x28, CPU_SSE); }
	'movhlps' { RET_INSN_NS(movhllhps, 0x12, CPU_SSE); }
	'movhps' { RET_INSN_NS(movhlps, 0x16, CPU_SSE); }
	'movlhps' { RET_INSN_NS(movhllhps, 0x16, CPU_SSE); }
	'movlps' { RET_INSN_NS(movhlps, 0x12, CPU_SSE); }
	'movmskps' [lLqQ]? { RET_INSN(8, movmskps, 0, CPU_SSE); }
	'movntps' { RET_INSN_NS(movntps, 0, CPU_SSE); }
	'movntq' { RET_INSN_NS(movntq, 0, CPU_SSE); }
	'movss' { RET_INSN_NS(movss, 0, CPU_SSE); }
	'movups' { RET_INSN_NS(movaups, 0x10, CPU_SSE); }
	'mulps' { RET_INSN_NS(sseps, 0x59, CPU_SSE); }
	'mulss' { RET_INSN_NS(ssess, 0xF359, CPU_SSE); }
	'orps' { RET_INSN_NS(sseps, 0x56, CPU_SSE); }
	'pavgb' { RET_INSN_NS(mmxsse2, 0xE0, CPU_P3|CPU_MMX); }
	'pavgw' { RET_INSN_NS(mmxsse2, 0xE3, CPU_P3|CPU_MMX); }
	'pextrw' [lLqQ]? { RET_INSN(6, pextrw, 0, CPU_P3|CPU_MMX); }
	'pinsrw' [lLqQ]? { RET_INSN(6, pinsrw, 0, CPU_P3|CPU_MMX); }
	'pmaxsw' { RET_INSN_NS(mmxsse2, 0xEE, CPU_P3|CPU_MMX); }
	'pmaxub' { RET_INSN_NS(mmxsse2, 0xDE, CPU_P3|CPU_MMX); }
	'pminsw' { RET_INSN_NS(mmxsse2, 0xEA, CPU_P3|CPU_MMX); }
	'pminub' { RET_INSN_NS(mmxsse2, 0xDA, CPU_P3|CPU_MMX); }
	'pmovmskb' [lLqQ]? { RET_INSN(8, pmovmskb, 0, CPU_SSE); }
	'pmulhuw' { RET_INSN_NS(mmxsse2, 0xE4, CPU_P3|CPU_MMX); }
	'prefetchnta' { RET_INSN_NS(twobytemem, 0x000F18, CPU_P3); }
	'prefetcht0' { RET_INSN_NS(twobytemem, 0x010F18, CPU_P3); }
	'prefetcht1' { RET_INSN_NS(twobytemem, 0x020F18, CPU_P3); }
	'prefetcht2' { RET_INSN_NS(twobytemem, 0x030F18, CPU_P3); }
	'psadbw' { RET_INSN_NS(mmxsse2, 0xF6, CPU_P3|CPU_MMX); }
	'pshufw' { RET_INSN_NS(pshufw, 0, CPU_P3|CPU_MMX); }
	'rcpps' { RET_INSN_NS(sseps, 0x53, CPU_SSE); }
	'rcpss' { RET_INSN_NS(ssess, 0xF353, CPU_SSE); }
	'rsqrtps' { RET_INSN_NS(sseps, 0x52, CPU_SSE); }
	'rsqrtss' { RET_INSN_NS(ssess, 0xF352, CPU_SSE); }
	'sfence' { RET_INSN_NS(threebyte, 0x0FAEF8, CPU_P3); }
	'shufps' { RET_INSN_NS(ssepsimm, 0xC6, CPU_SSE); }
	'sqrtps' { RET_INSN_NS(sseps, 0x51, CPU_SSE); }
	'sqrtss' { RET_INSN_NS(ssess, 0xF351, CPU_SSE); }
	'stmxcsr' { RET_INSN_NS(ldstmxcsr, 0x03, CPU_SSE); }
	'subps' { RET_INSN_NS(sseps, 0x5C, CPU_SSE); }
	'subss' { RET_INSN_NS(ssess, 0xF35C, CPU_SSE); }
	'ucomiss' { RET_INSN_NS(ssess, 0x2E, CPU_SSE); }
	'unpckhps' { RET_INSN_NS(sseps, 0x15, CPU_SSE); }
	'unpcklps' { RET_INSN_NS(sseps, 0x14, CPU_SSE); }
	'xorps' { RET_INSN_NS(sseps, 0x57, CPU_SSE); }
	/* SSE2 instructions */
	'addpd' { RET_INSN_NS(ssess, 0x6658, CPU_SSE2); }
	'addsd' { RET_INSN_NS(ssess, 0xF258, CPU_SSE2); }
	'andnpd' { RET_INSN_NS(ssess, 0x6655, CPU_SSE2); }
	'andpd' { RET_INSN_NS(ssess, 0x6654, CPU_SSE2); }
	'cmpeqpd' { RET_INSN_NS(ssecmpss, 0x0066, CPU_SSE2); }
	'cmpeqsd' { RET_INSN_NS(ssecmpss, 0x00F2, CPU_SSE2); }
	'cmplepd' { RET_INSN_NS(ssecmpss, 0x0266, CPU_SSE2); }
	'cmplesd' { RET_INSN_NS(ssecmpss, 0x02F2, CPU_SSE2); }
	'cmpltpd' { RET_INSN_NS(ssecmpss, 0x0166, CPU_SSE2); }
	'cmpltsd' { RET_INSN_NS(ssecmpss, 0x01F2, CPU_SSE2); }
	'cmpneqpd' { RET_INSN_NS(ssecmpss, 0x0466, CPU_SSE2); }
	'cmpneqsd' { RET_INSN_NS(ssecmpss, 0x04F2, CPU_SSE2); }
	'cmpnlepd' { RET_INSN_NS(ssecmpss, 0x0666, CPU_SSE2); }
	'cmpnlesd' { RET_INSN_NS(ssecmpss, 0x06F2, CPU_SSE2); }
	'cmpnltpd' { RET_INSN_NS(ssecmpss, 0x0566, CPU_SSE2); }
	'cmpnltsd' { RET_INSN_NS(ssecmpss, 0x05F2, CPU_SSE2); }
	'cmpordpd' { RET_INSN_NS(ssecmpss, 0x0766, CPU_SSE2); }
	'cmpordsd' { RET_INSN_NS(ssecmpss, 0x07F2, CPU_SSE2); }
	'cmpunordpd' { RET_INSN_NS(ssecmpss, 0x0366, CPU_SSE2); }
	'cmpunordsd' { RET_INSN_NS(ssecmpss, 0x03F2, CPU_SSE2); }
	'cmppd' { RET_INSN_NS(ssessimm, 0x66C2, CPU_SSE2); }
	/* C M P S D is in string instructions above */
	'comisd' { RET_INSN_NS(ssess, 0x662F, CPU_SSE2); }
	'cvtpi2pd' { RET_INSN_NS(cvt_xmm_mm_ss, 0x662A, CPU_SSE2); }
	'cvtsi2sd' [lLqQ]? { RET_INSN(8, cvt_xmm_rmx, 0xF22A, CPU_SSE2); }
	'divpd' { RET_INSN_NS(ssess, 0x665E, CPU_SSE2); }
	'divsd' { RET_INSN_NS(ssess, 0xF25E, CPU_SSE2); }
	'maxpd' { RET_INSN_NS(ssess, 0x665F, CPU_SSE2); }
	'maxsd' { RET_INSN_NS(ssess, 0xF25F, CPU_SSE2); }
	'minpd' { RET_INSN_NS(ssess, 0x665D, CPU_SSE2); }
	'minsd' { RET_INSN_NS(ssess, 0xF25D, CPU_SSE2); }
	'movapd' { RET_INSN_NS(movaupd, 0x28, CPU_SSE2); }
	'movhpd' { RET_INSN_NS(movhlpd, 0x16, CPU_SSE2); }
	'movlpd' { RET_INSN_NS(movhlpd, 0x12, CPU_SSE2); }
	'movmskpd' [lLqQ]? { RET_INSN(8, movmskpd, 0, CPU_SSE2); }
	'movntpd' { RET_INSN_NS(movntpddq, 0x2B, CPU_SSE2); }
	'movntdq' { RET_INSN_NS(movntpddq, 0xE7, CPU_SSE2); }
	/* M O V S D is in string instructions above */
	'movupd' { RET_INSN_NS(movaupd, 0x10, CPU_SSE2); }
	'mulpd' { RET_INSN_NS(ssess, 0x6659, CPU_SSE2); }
	'mulsd' { RET_INSN_NS(ssess, 0xF259, CPU_SSE2); }
	'orpd' { RET_INSN_NS(ssess, 0x6656, CPU_SSE2); }
	'shufpd' { RET_INSN_NS(ssessimm, 0x66C6, CPU_SSE2); }
	'sqrtpd' { RET_INSN_NS(ssess, 0x6651, CPU_SSE2); }
	'sqrtsd' { RET_INSN_NS(ssess, 0xF251, CPU_SSE2); }
	'subpd' { RET_INSN_NS(ssess, 0x665C, CPU_SSE2); }
	'subsd' { RET_INSN_NS(ssess, 0xF25C, CPU_SSE2); }
	'ucomisd' { RET_INSN_NS(ssess, 0x662E, CPU_SSE2); }
	'unpckhpd' { RET_INSN_NS(ssess, 0x6615, CPU_SSE2); }
	'unpcklpd' { RET_INSN_NS(ssess, 0x6614, CPU_SSE2); }
	'xorpd' { RET_INSN_NS(ssess, 0x6657, CPU_SSE2); }
	'cvtdq2pd' { RET_INSN_NS(cvt_xmm_xmm64_ss, 0xF3E6, CPU_SSE2); }
	'cvtpd2dq' { RET_INSN_NS(ssess, 0xF2E6, CPU_SSE2); }
	'cvtdq2ps' { RET_INSN_NS(sseps, 0x5B, CPU_SSE2); }
	'cvtpd2pi' { RET_INSN_NS(cvt_mm_xmm, 0x662D, CPU_SSE2); }
	'cvtpd2ps' { RET_INSN_NS(ssess, 0x665A, CPU_SSE2); }
	'cvtps2pd' { RET_INSN_NS(cvt_xmm_xmm64_ps, 0x5A, CPU_SSE2); }
	'cvtps2dq' { RET_INSN_NS(ssess, 0x665B, CPU_SSE2); }
	'cvtsd2si' [lLqQ]? { RET_INSN(8, cvt_rx_xmm64, 0xF22D, CPU_SSE2); }
	'cvtsd2ss' { RET_INSN_NS(cvt_xmm_xmm64_ss, 0xF25A, CPU_SSE2); }
	/* P4 VMX Instructions */
	'vmcall' { RET_INSN_NS(threebyte, 0x0F01C1, CPU_P4); }
	'vmlaunch' { RET_INSN_NS(threebyte, 0x0F01C2, CPU_P4); }
	'vmresume' { RET_INSN_NS(threebyte, 0x0F01C3, CPU_P4); }
	'vmxoff' { RET_INSN_NS(threebyte, 0x0F01C4, CPU_P4); }
	'vmread' [lLqQ]? { RET_INSN(6, vmxmemrd, 0x0F78, CPU_P4); }
	'vmwrite' [lLqQ]? { RET_INSN(7, vmxmemwr, 0x0F79, CPU_P4); }
	'vmptrld' { RET_INSN_NS(vmxtwobytemem, 0x06C7, CPU_P4); }
	'vmptrst' { RET_INSN_NS(vmxtwobytemem, 0x07C7, CPU_P4); }
	'vmclear' { RET_INSN_NS(vmxthreebytemem, 0x0666C7, CPU_P4); }
	'vmxon' { RET_INSN_NS(vmxthreebytemem, 0x06F3C7, CPU_P4); }
	'cvtss2sd' { RET_INSN_NS(cvt_xmm_xmm32, 0xF35A, CPU_SSE2); }
	'cvttpd2pi' { RET_INSN_NS(cvt_mm_xmm, 0x662C, CPU_SSE2); }
	'cvttsd2si' [lLqQ]? { RET_INSN(9, cvt_rx_xmm64, 0xF22C, CPU_SSE2); }
	'cvttpd2dq' { RET_INSN_NS(ssess, 0x66E6, CPU_SSE2); }
	'cvttps2dq' { RET_INSN_NS(ssess, 0xF35B, CPU_SSE2); }
	'maskmovdqu' { RET_INSN_NS(maskmovdqu, 0, CPU_SSE2); }
	'movdqa' { RET_INSN_NS(movdqau, 0x66, CPU_SSE2); }
	'movdqu' { RET_INSN_NS(movdqau, 0xF3, CPU_SSE2); }
	'movdq2q' { RET_INSN_NS(movdq2q, 0, CPU_SSE2); }
	'movq2dq' { RET_INSN_NS(movq2dq, 0, CPU_SSE2); }
	'pmuludq' { RET_INSN_NS(mmxsse2, 0xF4, CPU_SSE2); }
	'pshufd' { RET_INSN_NS(ssessimm, 0x6670, CPU_SSE2); }
	'pshufhw' { RET_INSN_NS(ssessimm, 0xF370, CPU_SSE2); }
	'pshuflw' { RET_INSN_NS(ssessimm, 0xF270, CPU_SSE2); }
	'pslldq' { RET_INSN_NS(pslrldq, 0x07, CPU_SSE2); }
	'psrldq' { RET_INSN_NS(pslrldq, 0x03, CPU_SSE2); }
	'punpckhqdq' { RET_INSN_NS(ssess, 0x666D, CPU_SSE2); }
	'punpcklqdq' { RET_INSN_NS(ssess, 0x666C, CPU_SSE2); }
	/* SSE3 / PNI (Prescott New Instructions) instructions */
	'addsubpd' { RET_INSN_NS(ssess, 0x66D0, CPU_SSE3); }
	'addsubps' { RET_INSN_NS(ssess, 0xF2D0, CPU_SSE3); }
	'fisttp' [sSlLqQ]? { RET_INSN(6, fildstp, 0x010001, CPU_SSE3); }
	'fisttpll' {
	    suffix_over='q';
	    RET_INSN_GAS(8, fildstp, 0x07, CPU_FPU);
	}
	'haddpd' { RET_INSN_NS(ssess, 0x667C, CPU_SSE3); }
	'haddps' { RET_INSN_NS(ssess, 0xF27C, CPU_SSE3); }
	'hsubpd' { RET_INSN_NS(ssess, 0x667D, CPU_SSE3); }
	'hsubps' { RET_INSN_NS(ssess, 0xF27D, CPU_SSE3); }
	'lddqu' { RET_INSN_NS(lddqu, 0, CPU_SSE3); }
	'monitor' { RET_INSN_NS(threebyte, 0x0F01C8, CPU_SSE3); }
	'movddup' { RET_INSN_NS(cvt_xmm_xmm64_ss, 0xF212, CPU_SSE3); }
	'movshdup' { RET_INSN_NS(ssess, 0xF316, CPU_SSE3); }
	'movsldup' { RET_INSN_NS(ssess, 0xF312, CPU_SSE3); }
	'mwait' { RET_INSN_NS(threebyte, 0x0F01C9, CPU_SSE3); }
	/* AMD 3DNow! instructions */
	'prefetch' { RET_INSN_NS(twobytemem, 0x000F0D, CPU_3DNow); }
	'prefetchw' { RET_INSN_NS(twobytemem, 0x010F0D, CPU_3DNow); }
	'femms' { RET_INSN_NS(twobyte, 0x0F0E, CPU_3DNow); }
	'pavgusb' { RET_INSN_NS(now3d, 0xBF, CPU_3DNow); }
	'pf2id' { RET_INSN_NS(now3d, 0x1D, CPU_3DNow); }
	'pf2iw' { RET_INSN_NS(now3d, 0x1C, CPU_Athlon|CPU_3DNow); }
	'pfacc' { RET_INSN_NS(now3d, 0xAE, CPU_3DNow); }
	'pfadd' { RET_INSN_NS(now3d, 0x9E, CPU_3DNow); }
	'pfcmpeq' { RET_INSN_NS(now3d, 0xB0, CPU_3DNow); }
	'pfcmpge' { RET_INSN_NS(now3d, 0x90, CPU_3DNow); }
	'pfcmpgt' { RET_INSN_NS(now3d, 0xA0, CPU_3DNow); }
	'pfmax' { RET_INSN_NS(now3d, 0xA4, CPU_3DNow); }
	'pfmin' { RET_INSN_NS(now3d, 0x94, CPU_3DNow); }
	'pfmul' { RET_INSN_NS(now3d, 0xB4, CPU_3DNow); }
	'pfnacc' { RET_INSN_NS(now3d, 0x8A, CPU_Athlon|CPU_3DNow); }
	'pfpnacc' { RET_INSN_NS(now3d, 0x8E, CPU_Athlon|CPU_3DNow); }
	'pfrcp' { RET_INSN_NS(now3d, 0x96, CPU_3DNow); }
	'pfrcpit1' { RET_INSN_NS(now3d, 0xA6, CPU_3DNow); }
	'pfrcpit2' { RET_INSN_NS(now3d, 0xB6, CPU_3DNow); }
	'pfrsqit1' { RET_INSN_NS(now3d, 0xA7, CPU_3DNow); }
	'pfrsqrt' { RET_INSN_NS(now3d, 0x97, CPU_3DNow); }
	'pfsub' { RET_INSN_NS(now3d, 0x9A, CPU_3DNow); }
	'pfsubr' { RET_INSN_NS(now3d, 0xAA, CPU_3DNow); }
	'pi2fd' { RET_INSN_NS(now3d, 0x0D, CPU_3DNow); }
	'pi2fw' { RET_INSN_NS(now3d, 0x0C, CPU_Athlon|CPU_3DNow); }
	'pmulhrwa' { RET_INSN_NS(now3d, 0xB7, CPU_3DNow); }
	'pswapd' { RET_INSN_NS(now3d, 0xBB, CPU_Athlon|CPU_3DNow); }
	/* AMD extensions */
	'syscall' { RET_INSN_NS(twobyte, 0x0F05, CPU_686|CPU_AMD); }
	'sysret' [lLqQ]? { RET_INSN(6, twobyte, 0x0F07, CPU_686|CPU_AMD|CPU_Priv); }
	/* AMD x86-64 extensions */
	'swapgs' {
	    warn64 = 1;
	    RET_INSN_NS(threebyte, 0x0F01F8, CPU_Hammer|CPU_64);
	}
	'rdtscp' { RET_INSN_NS(threebyte, 0x0F01F9, CPU_686|CPU_AMD|CPU_Priv); }
	/* AMD Pacifica (SVM) instructions */
	'clgi' { RET_INSN_NS(threebyte, 0x0F01DD, CPU_Hammer|CPU_64|CPU_SVM); }
	'invlpga' { RET_INSN_NS(invlpga, 0, CPU_Hammer|CPU_64|CPU_SVM); }
	'skinit' { RET_INSN_NS(skinit, 0, CPU_Hammer|CPU_64|CPU_SVM); }
	'stgi' { RET_INSN_NS(threebyte, 0x0F01DC, CPU_Hammer|CPU_64|CPU_SVM); }
	'vmload' { RET_INSN_NS(svm_rax, 0xDA, CPU_Hammer|CPU_64|CPU_SVM); }
	'vmmcall' { RET_INSN_NS(threebyte, 0x0F01D9, CPU_Hammer|CPU_64|CPU_SVM); }
	'vmrun' { RET_INSN_NS(svm_rax, 0xD8, CPU_Hammer|CPU_64|CPU_SVM); }
	'vmsave' { RET_INSN_NS(svm_rax, 0xDB, CPU_Hammer|CPU_64|CPU_SVM); }
	/* VIA PadLock instructions */
	'xstore' ('rng')? { RET_INSN_NS(padlock, 0xC000A7, CPU_PadLock); }
	'xcryptecb' { RET_INSN_NS(padlock, 0xC8F3A7, CPU_PadLock); }
	'xcryptcbc' { RET_INSN_NS(padlock, 0xD0F3A7, CPU_PadLock); }
	'xcryptctr' { RET_INSN_NS(padlock, 0xD8F3A7, CPU_PadLock); }
	'xcryptcfb' { RET_INSN_NS(padlock, 0xE0F3A7, CPU_PadLock); }
	'xcryptofb' { RET_INSN_NS(padlock, 0xE8F3A7, CPU_PadLock); }
	'montmul' { RET_INSN_NS(padlock, 0xC0F3A6, CPU_PadLock); }
	'xsha1' { RET_INSN_NS(padlock, 0xC8F3A6, CPU_PadLock); }
	'xsha256' { RET_INSN_NS(padlock, 0xD0F3A6, CPU_PadLock); }
	/* Cyrix MMX instructions */
	'paddsiw' { RET_INSN_NS(cyrixmmx, 0x51, CPU_Cyrix|CPU_MMX); }
	'paveb' { RET_INSN_NS(cyrixmmx, 0x50, CPU_Cyrix|CPU_MMX); }
	'pdistib' { RET_INSN_NS(cyrixmmx, 0x54, CPU_Cyrix|CPU_MMX); }
	'pmachriw' { RET_INSN_NS(pmachriw, 0, CPU_Cyrix|CPU_MMX); }
	'pmagw' { RET_INSN_NS(cyrixmmx, 0x52, CPU_Cyrix|CPU_MMX); }
	'pmulhriw' { RET_INSN_NS(cyrixmmx, 0x5D, CPU_Cyrix|CPU_MMX); }
	'pmulhrwc' { RET_INSN_NS(cyrixmmx, 0x59, CPU_Cyrix|CPU_MMX); }
	'pmvgezb' { RET_INSN_NS(cyrixmmx, 0x5C, CPU_Cyrix|CPU_MMX); }
	'pmvlzb' { RET_INSN_NS(cyrixmmx, 0x5B, CPU_Cyrix|CPU_MMX); }
	'pmvnzb' { RET_INSN_NS(cyrixmmx, 0x5A, CPU_Cyrix|CPU_MMX); }
	'pmvzb' { RET_INSN_NS(cyrixmmx, 0x58, CPU_Cyrix|CPU_MMX); }
	'psubsiw' { RET_INSN_NS(cyrixmmx, 0x55, CPU_Cyrix|CPU_MMX); }
	/* Cyrix extensions */
	'rdshr' { RET_INSN_NS(twobyte, 0x0F36, CPU_686|CPU_Cyrix|CPU_SMM); }
	'rsdc' { RET_INSN_NS(rsdc, 0, CPU_486|CPU_Cyrix|CPU_SMM); }
	'rsldt' { RET_INSN_NS(cyrixsmm, 0x7B, CPU_486|CPU_Cyrix|CPU_SMM); }
	'rsts' { RET_INSN_NS(cyrixsmm, 0x7D, CPU_486|CPU_Cyrix|CPU_SMM); }
	'svdc' { RET_INSN_NS(svdc, 0, CPU_486|CPU_Cyrix|CPU_SMM); }
	'svldt' { RET_INSN_NS(cyrixsmm, 0x7A, CPU_486|CPU_Cyrix|CPU_SMM); }
	'svts' { RET_INSN_NS(cyrixsmm, 0x7C, CPU_486|CPU_Cyrix|CPU_SMM); }
	'smint' { RET_INSN_NS(twobyte, 0x0F38, CPU_686|CPU_Cyrix); }
	'smintold' { RET_INSN_NS(twobyte, 0x0F7E, CPU_486|CPU_Cyrix|CPU_Obs); }
	'wrshr' { RET_INSN_NS(twobyte, 0x0F37, CPU_686|CPU_Cyrix|CPU_SMM); }
	/* Obsolete/undocumented instructions */
	'fsetpm' { RET_INSN_NS(twobyte, 0xDBE4, CPU_286|CPU_FPU|CPU_Obs); }
	'ibts' { RET_INSN_NS(ibts, 0, CPU_386|CPU_Undoc|CPU_Obs); }
	'loadall' { RET_INSN_NS(twobyte, 0x0F07, CPU_386|CPU_Undoc); }
	'loadall286' { RET_INSN_NS(twobyte, 0x0F05, CPU_286|CPU_Undoc); }
	'salc' {
	    not64 = 1;
	    RET_INSN_NS(onebyte, 0x00D6, CPU_Undoc);
	}
	'smi' { RET_INSN_NS(onebyte, 0x00F1, CPU_386|CPU_Undoc); }
	'umov' { RET_INSN_NS(umov, 0, CPU_386|CPU_Undoc); }
	'xbts' { RET_INSN_NS(xbts, 0, CPU_386|CPU_Undoc|CPU_Obs); }


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
