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
#include <ctype.h>
#include <util.h>
RCSID("$Id$");

#define YASM_LIB_INTERNAL
#define YASM_BC_INTERNAL
#define YASM_EXPR_INTERNAL
#include <libyasm.h>
#include <libyasm/phash.h>

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
 *             16 = immediate, value=1 (for special-case shift)
 *  - 3 bits = size (user-specified, or from register size):
 *             0 = any size acceptable/no size spec acceptable (dep. on strict)
 *             1/2/3/4 = 8/16/32/64 bits (from user or reg size)
 *             5/6 = 80/128 bits (from user)
 *             7 = current BITS setting; when this is used the size matched
 *                 gets stored into the opersize as well.
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
 * parse-time due to possibly dependent expressions.  For these, some
 * additional data (stored in the second byte of the opcode with a one-byte
 * opcode) is passed to later stages of the assembler with flags set to
 * indicate postponed actions.
 *  - 3 bits = postponed action:
 *             0 = none
 *             1 = large imm16/32 that can become a sign-extended imm8.
 *             2 = could become a short opcode mov with bits=64 and a32 prefix
 *             3 = forced 16-bit address size (override ignored, no prefix)
 *             4 = large imm64 that can become a sign-extended imm32.
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
#define OPT_Imm1	0x16
#define OPT_MASK	0x1F

#define OPS_Any		(0UL<<5)
#define OPS_8		(1UL<<5)
#define OPS_16		(2UL<<5)
#define OPS_32		(3UL<<5)
#define OPS_64		(4UL<<5)
#define OPS_80		(5UL<<5)
#define OPS_128		(6UL<<5)
#define OPS_BITS	(7UL<<5)
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
#define OPAP_SImm8Avail	(1UL<<17)
#define OPAP_ShortMov	(2UL<<17)
#define OPAP_A16	(3UL<<17)
#define OPAP_SImm32Avail (4UL<<17)
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
    { CPU_Any, MOD_Op0Add|MOD_PreAdd, 0, 0, 0x00, 1, {0x00, 0, 0}, 0, 0,
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
    { CPU_Hammer|CPU_64, MOD_GasSufQ, 64, 64, 0, 1,
      {0x68, 0x6A, 0}, 0, 1,
      {OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8Avail, 0, 0} },
    { CPU_Any|CPU_Not64, MOD_GasIllegal, 0, 0, 0, 1,
      {0x68, 0x6A, 0}, 0, 1,
      {OPT_Imm|OPS_BITS|OPS_Relaxed|OPA_Imm|OPAP_SImm8Avail, 0, 0} },
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
    { CPU_Any, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xD0, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Imm1|OPS_8|OPS_Relaxed|OPA_None, 0} },
    { CPU_186, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xC0, 0, 0}, 0, 2,
      {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xD3, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xD1, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Imm1|OPS_8|OPS_Relaxed|OPA_None, 0} },
    { CPU_186, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xC1, 0, 0}, 0, 2,
      {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xD3, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_386, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xD1, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Imm1|OPS_8|OPS_Relaxed|OPA_None, 0} },
    { CPU_386, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xC1, 0, 0}, 0, 2,
      {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xD3, 0, 0}, 0,
      2, {OPT_RM|OPS_64|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xD1, 0, 0},
      0, 2, {OPT_RM|OPS_64|OPA_EA, OPT_Imm1|OPS_8|OPS_Relaxed|OPA_None, 0} },
    { CPU_Hammer|CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xC1, 0, 0},
      0, 2, {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
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
    { CPU_EM64T|CPU_64, 0, 64, 0, 0, 1, {0xFF, 0, 0}, 3, 1,
      {OPT_Mem|OPS_64|OPTM_Far|OPA_EA, 0, 0} },
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
    { CPU_EM64T|CPU_64, 0, 64, 0, 0, 1, {0xFF, 0, 0}, 5, 1,
      {OPT_Mem|OPS_64|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 0, 0, 1, {0xFF, 0, 0}, 5, 1,
      {OPT_Mem|OPS_Any|OPTM_Far|OPA_EA, 0, 0} }
};
static const x86_insn_info retnf_insn[] = {
    { CPU_Not64, MOD_Op0Add, 0, 0, 0, 1,
      {0x01, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_Not64, MOD_Op0Add, 0, 0, 0, 1,
      {0x00, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_64, MOD_Op0Add|MOD_OpSizeR, 0, 0, 0, 1,
      {0x01, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_64, MOD_Op0Add|MOD_OpSizeR, 0, 0, 0, 1,
      {0x00, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0, 0} },
    /* GAS suffix versions */
    { CPU_Any, MOD_Op0Add|MOD_OpSizeR|MOD_GasSufW|MOD_GasSufL|MOD_GasSufQ, 0,
      0, 0, 1, {0x01, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_Any, MOD_Op0Add|MOD_OpSizeR|MOD_GasSufW|MOD_GasSufL|MOD_GasSufQ, 0,
      0, 0, 1, {0x00, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0, 0} }
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
    { CPU_286|CPU_Prot, MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x00, 0}, 1, 1,
      {OPT_Reg|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Prot, MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x00, 0}, 1, 1,
      {OPT_Reg|OPS_32|OPA_EA, 0, 0} },
    { CPU_Hammer|CPU_64|CPU_Prot, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x00, 0}, 1,
      1, {OPT_Reg|OPS_64|OPA_EA, 0, 0} },
    { CPU_286|CPU_Prot, MOD_GasSufW|MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x00, 0},
      1, 1, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} }
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
static const x86_insn_info fld_insn[] = {
    { CPU_FPU, MOD_GasSufS, 0, 0, 0, 1, {0xD9, 0, 0}, 0, 1,
      {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_GasSufL, 0, 0, 0, 1, {0xDD, 0, 0}, 0, 1,
      {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 0, 0, 1, {0xDB, 0, 0}, 5, 1,
      {OPT_Mem|OPS_80|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 0, 0, 2, {0xD9, 0xC0, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} }
};
static const x86_insn_info fstp_insn[] = {
    { CPU_FPU, MOD_GasSufS, 0, 0, 0, 1, {0xD9, 0, 0}, 3, 1,
      {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_GasSufL, 0, 0, 0, 1, {0xDD, 0, 0}, 3, 1,
      {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 0, 0, 1, {0xDB, 0, 0}, 7, 1,
      {OPT_Mem|OPS_80|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 0, 0, 2, {0xDD, 0xD8, 0}, 0, 1,
      {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} }
};
/* Long memory version of floating point load/store for GAS */
static const x86_insn_info fldstpt_insn[] = {
    { CPU_FPU, MOD_SpAdd, 0, 0, 0, 1, {0xDB, 0, 0}, 0, 1,
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
    /*@only@*/ yasm_expr *segment;

    jmpfar = yasm_xmalloc(sizeof(x86_jmpfar));
    x86_finalize_common(&jmpfar->common, info, data[3]);
    x86_finalize_opcode(&jmpfar->opcode, info);

    op = yasm_ops_first(operands);

    switch (op->targetmod) {
	case X86_FAR:
	    /* "FAR imm" target needs to become "seg imm:imm". */
	    if (yasm_value_finalize_expr(&jmpfar->offset,
					 yasm_expr_copy(op->data.val), 0)
		|| yasm_value_finalize_expr(&jmpfar->segment, op->data.val, 16))
		yasm_error_set(YASM_ERROR_TOO_COMPLEX,
			       N_("jump target expression too complex"));
	    jmpfar->segment.seg_of = 1;
	    break;
	case X86_FAR_SEGOFF:
	    /* SEG:OFF expression; split it. */
	    segment = yasm_expr_extract_segoff(&op->data.val);
	    if (!segment)
		yasm_internal_error(N_("didn't get SEG:OFF expression in jmpfar"));
	    if (yasm_value_finalize_expr(&jmpfar->segment, segment, 16))
		yasm_error_set(YASM_ERROR_TOO_COMPLEX,
			       N_("jump target segment too complex"));
	    if (yasm_value_finalize_expr(&jmpfar->offset, op->data.val, 0))
		yasm_error_set(YASM_ERROR_TOO_COMPLEX,
			       N_("jump target offset too complex"));
	    break;
	default:
	    yasm_internal_error(N_("didn't get FAR expression in jmpfar"));
    }

    yasm_x86__bc_apply_prefixes((x86_common *)jmpfar, NULL, num_prefixes,
				prefixes);

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
    if (yasm_value_finalize_expr(&jmp->target, op->data.val, 0))
	yasm_error_set(YASM_ERROR_TOO_COMPLEX,
		       N_("jump target expression too complex"));
    if (jmp->target.seg_of || jmp->target.rshift || jmp->target.curpos_rel)
	yasm_error_set(YASM_ERROR_VALUE, N_("invalid jump target"));
    jmp->target.curpos_rel = 1;

    /* Need to save jump origin for relative jumps. */
    jmp->origin_prevbc = prev_bc;

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
	yasm_error_set(YASM_ERROR_TYPE,
		       N_("no SHORT form of that jump instruction exists"));
    if ((jmp->op_sel == JMP_NEAR_FORCED) && (jmp->shortop.len == 0))
	yasm_error_set(YASM_ERROR_TYPE,
		       N_("no NEAR form of that jump instruction exists"));

    if (jmp->op_sel == JMP_NONE) {
	if (jmp->nearop.len == 0)
	    jmp->op_sel = JMP_SHORT_FORCED;
	if (jmp->shortop.len == 0)
	    jmp->op_sel = JMP_NEAR_FORCED;
    }

    yasm_x86__bc_apply_prefixes((x86_common *)jmp, NULL, num_prefixes,
				prefixes);

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
    /*@null@*/ yasm_expr *imm;
    unsigned char im_len;
    unsigned char im_sign;
    unsigned char spare;
    int i;
    unsigned int size_lookup[] = {0, 8, 16, 32, 64, 80, 128, 0};
    unsigned long do_postop = 0;

    size_lookup[7] = mode_bits;

    if (!info) {
	num_info = 1;
	info = empty_insn;
    }

    /* Build local array of operands from list, since we know we have a max
     * of 3 operands.
     */
    if (num_operands > 3) {
	yasm_error_set(YASM_ERROR_TYPE, N_("too many operands"));
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
		yasm_warn_set(YASM_WARN_GENERAL,
			      N_("indirect call without `*'"));
	    if (!op->deref && op->type == YASM_INSN__OPERAND_MEMORY
		&& !op->data.ea->strong) {
		/* Memory that is not dereferenced, and not strong, is
		 * actually an immediate for the purposes of relative jumps.
		 */
		if (op->data.ea->segreg != 0)
		    yasm_warn_set(YASM_WARN_GENERAL,
				  N_("skipping prefixes on this instruction"));
		imm = op->data.ea->disp.abs;
		op->data.ea->disp.abs = NULL;
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
		case OPT_Imm1:
		    if (op->type == YASM_INSN__OPERAND_IMM) {
			const yasm_intnum *num;
			num = yasm_expr_get_intnum(&op->data.val, NULL);
			if (!num || !yasm_intnum_is_pos1(num))
			    mismatch = 1;
		    } else
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

	    /* Check for 64-bit effective address size in NASM mode */
	    if (suffix == 0 && op->type == YASM_INSN__OPERAND_MEMORY) {
		if ((info->operands[i] & OPEAS_MASK) == OPEAS_64) {
		    if (op->data.ea->disp.size != 64)
			mismatch = 1;
		} else if (op->data.ea->disp.size == 64)
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
	yasm_error_set(YASM_ERROR_TYPE,
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
		    yasm_error_set(YASM_ERROR_TYPE,
				   N_("mismatch in operand sizes"));
		    break;
		case 1:
		    yasm_error_set(YASM_ERROR_TYPE,
				   N_("operand size not specified"));
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
    insn->x86_ea = NULL;
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
	im_len = 8;
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
			    insn->x86_ea =
				yasm_x86__ea_create_reg(op->data.reg,
							&insn->rex,
							mode_bits);
			    break;
			case YASM_INSN__OPERAND_SEGREG:
			    yasm_internal_error(
				N_("invalid operand conversion"));
			case YASM_INSN__OPERAND_MEMORY:
			    insn->x86_ea = (x86_effaddr *)op->data.ea;
			    if ((info->operands[i] & OPT_MASK) == OPT_MemOffs)
				/* Special-case for MOV MemOffs instruction */
				yasm_x86__ea_set_disponly(insn->x86_ea);
			    break;
			case YASM_INSN__OPERAND_IMM:
			    insn->x86_ea =
				yasm_x86__ea_create_imm(op->data.val,
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
			    yasm_error_set(YASM_ERROR_TYPE,
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
			    yasm_error_set(YASM_ERROR_TYPE,
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
			    yasm_error_set(YASM_ERROR_TYPE,
				N_("invalid combination of opcode and operands"));
			    return;
			}
			insn->opcode.opcode[1] += opadd;
		    } else
			yasm_internal_error(N_("invalid operand conversion"));
		    break;
		case OPA_SpareEA:
		    if (op->type == YASM_INSN__OPERAND_REG) {
			insn->x86_ea = yasm_x86__ea_create_reg(op->data.reg,
							       &insn->rex,
							       mode_bits);
			if (!insn->x86_ea ||
			    yasm_x86__set_rex_from_reg(&insn->rex, &spare,
				op->data.reg, mode_bits, X86_REX_R)) {
			    yasm_error_set(YASM_ERROR_TYPE,
				N_("invalid combination of opcode and operands"));
			    if (insn->x86_ea)
				yasm_xfree(insn->x86_ea);
			    yasm_xfree(insn);
			    return;
			}
		    } else
			yasm_internal_error(N_("invalid operand conversion"));
		    break;
		default:
		    yasm_internal_error(N_("unknown operand action"));
	    }

	    if ((info->operands[i] & OPS_MASK) == OPS_BITS)
		insn->common.opersize = (unsigned char)mode_bits;

	    switch ((int)(info->operands[i] & OPAP_MASK)) {
		case OPAP_None:
		    break;
		case OPAP_SImm8Avail:
		    insn->postop = X86_POSTOP_SIGNEXT_IMM8;
		    break;
		case OPAP_ShortMov:
		    do_postop = OPAP_ShortMov;
		    break;
		case OPAP_A16:
		    insn->postop = X86_POSTOP_ADDRESS16;
		    break;
		case OPAP_SImm32Avail:
		    do_postop = OPAP_SImm32Avail;
		    break;
		default:
		    yasm_internal_error(
			N_("unknown operand postponed action"));
	    }
	}
    }

    if (insn->x86_ea) {
	yasm_x86__ea_init(insn->x86_ea, spare);
	for (i=0; i<num_segregs; i++)
	    yasm_ea_set_segreg(&insn->x86_ea->ea, segregs[i]);
    } else if (num_segregs > 0 && insn->special_prefix == 0) {
	if (num_segregs > 1)
	    yasm_warn_set(YASM_WARN_GENERAL,
			  N_("multiple segment overrides, using leftmost"));
	insn->special_prefix = segregs[num_segregs-1]>>8;
    }
    if (imm) {
	insn->imm = yasm_imm_create_expr(imm);
	insn->imm->val.size = im_len;
	insn->imm->sign = im_sign;
    } else
	insn->imm = NULL;

    yasm_x86__bc_apply_prefixes((x86_common *)insn, &insn->rex, num_prefixes,
				prefixes);

    if (insn->postop == X86_POSTOP_ADDRESS16 && insn->common.addrsize) {
	yasm_warn_set(YASM_WARN_GENERAL, N_("address size override ignored"));
	insn->common.addrsize = 0;
    }

    /* Handle non-span-dependent post-ops here */
    switch (do_postop) {
	case OPAP_ShortMov:
	    /* Long (modrm+sib) mov instructions in amd64 can be optimized into
	     * short mov instructions if a 32-bit address override is applied in
	     * 64-bit mode to an EA of just an offset (no registers) and the
	     * target register is al/ax/eax/rax.
	     */
	    if (insn->common.mode_bits == 64 && insn->common.addrsize == 32 &&
		(!insn->x86_ea->ea.disp.abs ||
		 !yasm_expr__contains(insn->x86_ea->ea.disp.abs,
				      YASM_EXPR_REG))) {
		yasm_x86__ea_set_disponly(insn->x86_ea);
		/* Make the short form permanent. */
		insn->opcode.opcode[0] = insn->opcode.opcode[1];
	    }
	    insn->opcode.opcode[1] = 0;	/* avoid possible confusion */
	    break;
	case OPAP_SImm32Avail:
	    /* Used for 64-bit mov immediate, which can take a sign-extended
	     * imm32 as well as imm64 values.  The imm32 form is put in the
	     * second byte of the opcode and its ModRM byte is put in the third
	     * byte of the opcode.
	     */
	    if (!insn->imm->val.abs ||
		yasm_intnum_check_size(
		    yasm_expr_get_intnum(&insn->imm->val.abs, NULL),
		    32, 0, 1)) {
		/* Throwaway REX byte */
		unsigned char rex_temp = 0;

		/* Build ModRM EA - CAUTION: this depends on
		 * opcode 0 being a mov instruction!
		 */
		insn->x86_ea = yasm_x86__ea_create_reg(
		    (unsigned long)insn->opcode.opcode[0]-0xB8, &rex_temp, 64);

		/* Make the imm32s form permanent. */
		insn->opcode.opcode[0] = insn->opcode.opcode[1];
		insn->imm->val.size = 32;
	    }
	    insn->opcode.opcode[1] = 0;	/* avoid possible confusion */
	    break;
	default:
	    break;
    }

    /* Transform the bytecode */
    yasm_x86__bc_transform_insn(bc, insn);
}

/* Static parse data structure for instructions */
typedef struct insnprefix_parse_data {
    const char *name;

    /* instruction parse group - NULL if prefix */
    /*@null@*/ const x86_insn_info *group;

    /* For instruction, modifier in upper 24 bits, number of elements in group
     * in lower 8 bits.
     * For prefix, prefix type.
     */
    unsigned long data1;

    /* For instruction, cpu flags.
     * For prefix, prefix value.
     */
    unsigned long data2;

    /* suffix flags for instructions */
    enum {
	NONE = 0,
	SUF_B = (MOD_GasSufB >> MOD_GasSuf_SHIFT),
	SUF_W = (MOD_GasSufW >> MOD_GasSuf_SHIFT),
	SUF_L = (MOD_GasSufL >> MOD_GasSuf_SHIFT),
	SUF_Q = (MOD_GasSufQ >> MOD_GasSuf_SHIFT),
	SUF_S = (MOD_GasSufS >> MOD_GasSuf_SHIFT),
	WEAK = 0x80	/* Relaxed operand mode for GAS */
    } flags;
} insnprefix_parse_data;
#define INSN(name, flags, group, mod, cpu) \
    { name, group##_insn, (mod##UL<<8)|NELEMS(group##_insn), cpu, flags }
#define PREFIX(name, type, value) \
    { name, NULL, type, value, NONE }

/* Static parse data structure for CPU feature flags */
typedef struct cpu_parse_data {
    const char *name;

    unsigned long cpu;
    enum {
	CPU_MODE_VERBATIM,
	CPU_MODE_SET,
	CPU_MODE_CLEAR
    } mode;
} cpu_parse_data;

typedef struct regtmod_parse_data {
    const char *name;

    unsigned long regtmod;
} regtmod_parse_data;
#define REG(name, type, index, bits) \
    { name, (((unsigned long)YASM_ARCH_REG) << 24) | \
	    (((unsigned long)bits) << 16) | (type | index) }
#define REGGROUP(name, group) \
    { name, (((unsigned long)YASM_ARCH_REGGROUP) << 24) | (group) }
#define SEGREG(name, prefix, num, bits) \
    { name, (((unsigned long)YASM_ARCH_SEGREG) << 24) | \
	    (((unsigned long)bits) << 16) | (prefix << 8) | (num) }
#define TARGETMOD(name, mod) \
    { name, (((unsigned long)YASM_ARCH_TARGETMOD) << 24) | (mod) }

/* Pull in all parse data */
#include "x86parse.c"

yasm_arch_insnprefix
yasm_x86__parse_check_insnprefix(yasm_arch *arch, unsigned long data[4],
				 const char *id, size_t id_len)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    /*@null@*/ const insnprefix_parse_data *pdata;
    size_t i;
    static char lcaseid[16];

    if (id_len > 15)
	return YASM_ARCH_NOTINSNPREFIX;
    for (i=0; i<id_len; i++)
	lcaseid[i] = tolower(id[i]);
    lcaseid[id_len] = '\0';

    switch (arch_x86->parser) {
	case X86_PARSER_NASM:
	    pdata = insnprefix_nasm_find(lcaseid, id_len);
	    break;
	case X86_PARSER_GAS:
	    pdata = insnprefix_gas_find(lcaseid, id_len);
	    break;
	default:
	    pdata = NULL;
    }
    if (!pdata)
	return YASM_ARCH_NOTINSNPREFIX;

    if (pdata->group) {
	unsigned long cpu = pdata->data2;

	if ((cpu & CPU_64) && arch_x86->mode_bits != 64) {
	    yasm_warn_set(YASM_WARN_GENERAL,
			  N_("`%s' is an instruction in 64-bit mode"), id);
	    return YASM_ARCH_NOTINSNPREFIX;
	}
	if ((cpu & CPU_Not64) && arch_x86->mode_bits == 64) {
	    yasm_error_set(YASM_ERROR_GENERAL,
			   N_("`%s' invalid in 64-bit mode"), id);
	    data[0] = (unsigned long)not64_insn;
	    data[1] = NELEMS(not64_insn);
	    data[2] = CPU_Not64;
	    data[3] = arch_x86->mode_bits;
	    return YASM_ARCH_INSN;
	}

	data[0] = (unsigned long)pdata->group;
	data[1] = pdata->data1;
	data[2] = cpu;
	data[3] = (((unsigned long)pdata->flags)<<8) | arch_x86->mode_bits;
	return YASM_ARCH_INSN;
    } else {
	unsigned long type = pdata->data1;
	unsigned long value = pdata->data2;

	if (arch_x86->mode_bits == 64 && type == X86_OPERSIZE && value == 32) {
	    yasm_error_set(YASM_ERROR_GENERAL,
		N_("Cannot override data size to 32 bits in 64-bit mode"));
	    return YASM_ARCH_NOTINSNPREFIX;
	}

	if (arch_x86->mode_bits == 64 && type == X86_ADDRSIZE && value == 16) {
	    yasm_error_set(YASM_ERROR_GENERAL,
		N_("Cannot override address size to 16 bits in 64-bit mode"));
	    return YASM_ARCH_NOTINSNPREFIX;
	}

	if ((type == X86_REX ||
	     (value == 64 && (type == X86_OPERSIZE || type == X86_ADDRSIZE)))
	    && arch_x86->mode_bits != 64) {
	    yasm_warn_set(YASM_WARN_GENERAL,
			  N_("`%s' is a prefix in 64-bit mode"), id);
	    return YASM_ARCH_NOTINSNPREFIX;
	}
	data[0] = type;
	data[1] = value;
	return YASM_ARCH_PREFIX;
    }
}

void
yasm_x86__parse_cpu(yasm_arch *arch, const char *cpuid, size_t cpuid_len)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    /*@null@*/ const cpu_parse_data *pdata;
    size_t i;
    static char lcaseid[16];

    if (cpuid_len > 15)
	return;
    for (i=0; i<cpuid_len; i++)
	lcaseid[i] = tolower(cpuid[i]);
    lcaseid[cpuid_len] = '\0';

    pdata = cpu_find(lcaseid, cpuid_len);
    if (!pdata) {
	yasm_warn_set(YASM_WARN_GENERAL,
		      N_("unrecognized CPU identifier `%s'"), cpuid);
	return;
    }

    switch (pdata->mode) {
	case CPU_MODE_VERBATIM:
	    arch_x86->cpu_enabled = pdata->cpu;
	    break;
	case CPU_MODE_SET:
	    arch_x86->cpu_enabled |= pdata->cpu;
	    break;
	case CPU_MODE_CLEAR:
	    arch_x86->cpu_enabled &= ~pdata->cpu;
	    break;
    }
}

yasm_arch_regtmod
yasm_x86__parse_check_regtmod(yasm_arch *arch, unsigned long *data,
			      const char *id, size_t id_len)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    /*@null@*/ const regtmod_parse_data *pdata;
    size_t i;
    static char lcaseid[8];
    unsigned int bits;
    yasm_arch_regtmod type;

    if (id_len > 7)
	return YASM_ARCH_NOTREGTMOD;
    for (i=0; i<id_len; i++)
	lcaseid[i] = tolower(id[i]);
    lcaseid[id_len] = '\0';

    pdata = regtmod_find(lcaseid, id_len);
    if (!pdata)
	return YASM_ARCH_NOTREGTMOD;

    type = (yasm_arch_regtmod)(pdata->regtmod >> 24);
    bits = (pdata->regtmod >> 16) & 0xFF;

    if (type == YASM_ARCH_REG && bits != 0 && arch_x86->mode_bits != bits) {
	yasm_warn_set(YASM_WARN_GENERAL,
		      N_("`%s' is a register in %u-bit mode"), id, bits);
	return YASM_ARCH_NOTREGTMOD;
    }

    if (type == YASM_ARCH_SEGREG && bits != 0 && arch_x86->mode_bits == bits) {
	yasm_warn_set(YASM_WARN_GENERAL,
		      N_("`%s' segment register ignored in %u-bit mode"), id,
		      bits);
    }

    *data = pdata->regtmod & 0x0000FFFFUL;
    return type;
}

