/*
 * x86 architecture description
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
/*@unused@*/ RCSID("$IdPath$");

#define YASM_LIB_INTERNAL
#include <libyasm.h>

#include "x86arch.h"


yasm_arch_module yasm_x86_LTX_arch;


static /*@only@*/ yasm_arch *
x86_create(const char *machine)
{
    yasm_arch_x86 *arch_x86;
    unsigned int amd64_machine;

    if (yasm__strcasecmp(machine, "x86") == 0)
	amd64_machine = 0;
    else if (yasm__strcasecmp(machine, "amd64") == 0)
	amd64_machine = 1;
    else
	return NULL;

    arch_x86 = yasm_xmalloc(sizeof(yasm_arch_x86));

    arch_x86->arch.module = &yasm_x86_LTX_arch;

    arch_x86->cpu_enabled = ~CPU_Any;
    arch_x86->amd64_machine = amd64_machine;
    arch_x86->mode_bits = 0;

    return (yasm_arch *)arch_x86;
}

static void
x86_destroy(/*@only@*/ yasm_arch *arch)
{
    yasm_xfree(arch);
}

static const char *
x86_get_machine(const yasm_arch *arch)
{
    const yasm_arch_x86 *arch_x86 = (const yasm_arch_x86 *)arch;
    if (arch_x86->amd64_machine)
	return "amd64";
    else
	return "x86";
}

static int
x86_set_var(yasm_arch *arch, const char *var, unsigned long val)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    if (yasm__strcasecmp(var, "mode_bits") == 0)
	arch_x86->mode_bits = val;
    else
	return 1;
    return 0;
}

static int
x86_parse_directive(yasm_arch *arch, const char *name,
		    yasm_valparamhead *valparams,
		    /*@unused@*/ /*@null@*/
		    yasm_valparamhead *objext_valparams,
		    /*@unused@*/ yasm_object *object, unsigned long line)
{
    yasm_arch_x86 *arch_x86 = (yasm_arch_x86 *)arch;
    yasm_valparam *vp;
    const yasm_intnum *intn;
    long lval;

    if (yasm__strcasecmp(name, "bits") == 0) {
	if ((vp = yasm_vps_first(valparams)) && !vp->val &&
	    vp->param != NULL &&
	    (intn = yasm_expr_get_intnum(&vp->param, NULL)) != NULL &&
	    (lval = yasm_intnum_get_int(intn)) &&
	    (lval == 16 || lval == 32 || lval == 64))
	    arch_x86->mode_bits = (unsigned char)lval;
	else
	    yasm__error(line, N_("invalid argument to [%s]"), "BITS");
	return 0;
    } else
	return 1;
}

unsigned int
yasm_x86__get_reg_size(yasm_arch *arch, unsigned long reg)
{
    switch ((x86_expritem_reg_size)(reg & ~0xFUL)) {
	case X86_REG8:
	case X86_REG8X:
	    return 1;
	case X86_REG16:
	    return 2;
	case X86_REG32:
	case X86_CRREG:
	case X86_DRREG:
	case X86_TRREG:
	    return 4;
	case X86_REG64:
	case X86_MMXREG:
	    return 8;
	case X86_XMMREG:
	    return 16;
	case X86_FPUREG:
	    return 10;
	default:
	    yasm_internal_error(N_("unknown register size"));
    }
    return 0;
}

static void
x86_reg_print(yasm_arch *arch, unsigned long reg, FILE *f)
{
    static const char *name8[] = {"al","cl","dl","bl","ah","ch","dh","bh"};
    static const char *name8x[] = {
	"al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
	"r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"
    };
    static const char *name16[] = {
	"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
	"r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"
    };
    static const char *name32[] = {
	"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"
	"r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
    };
    static const char *name64[] = {
	"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"
	"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };

    switch ((x86_expritem_reg_size)(reg & ~0xFUL)) {
	case X86_REG8:
	    fprintf(f, "%s", name8[reg&0xF]);
	    break;
	case X86_REG8X:
	    fprintf(f, "%s", name8x[reg&0xF]);
	    break;
	case X86_REG16:
	    fprintf(f, "%s", name16[reg&0xF]);
	    break;
	case X86_REG32:
	    fprintf(f, "%s", name32[reg&0xF]);
	    break;
	case X86_REG64:
	    fprintf(f, "%s", name64[reg&0xF]);
	    break;
	case X86_MMXREG:
	    fprintf(f, "mm%d", (int)(reg&0xF));
	    break;
	case X86_XMMREG:
	    fprintf(f, "xmm%d", (int)(reg&0xF));
	    break;
	case X86_CRREG:
	    fprintf(f, "cr%d", (int)(reg&0xF));
	    break;
	case X86_DRREG:
	    fprintf(f, "dr%d", (int)(reg&0xF));
	    break;
	case X86_TRREG:
	    fprintf(f, "tr%d", (int)(reg&0xF));
	    break;
	case X86_FPUREG:
	    fprintf(f, "st%d", (int)(reg&0xF));
	    break;
	default:
	    yasm_internal_error(N_("unknown register size"));
    }
}

static void
x86_segreg_print(yasm_arch *arch, unsigned long segreg, FILE *f)
{
    static const char *name[] = {"es","cs","ss","ds","fs","gs"};
    fprintf(f, "%s", name[segreg&7]);
}

static void
x86_parse_prefix(yasm_arch *arch, yasm_bytecode *bc,
		 const unsigned long data[4], unsigned long line)
{
    switch((x86_parse_insn_prefix)data[0]) {
	case X86_LOCKREP:
	    yasm_x86__bc_insn_set_lockrep_prefix(bc, data[1] & 0xff, line);
	    break;
	case X86_ADDRSIZE:
	    yasm_x86__bc_insn_addrsize_override(bc, data[1]);
	    break;
	case X86_OPERSIZE:
	    yasm_x86__bc_insn_opersize_override(bc, data[1]);
	    break;
    }
}

static void
x86_parse_seg_prefix(yasm_arch *arch, yasm_bytecode *bc, unsigned long segreg,
		     unsigned long line)
{
    yasm_x86__ea_set_segment(yasm_x86__bc_insn_get_ea(bc),
			     (unsigned char)(segreg>>8), line);
}

static void
x86_parse_seg_override(yasm_arch *arch, yasm_effaddr *ea,
		       unsigned long segreg, unsigned long line)
{
    yasm_x86__ea_set_segment(ea, (unsigned char)(segreg>>8), line);
}

/* Define x86 machines -- see arch.h for details */
static yasm_arch_machine x86_machines[] = {
    { "IA-32 and derivatives", "x86" },
    { "AMD64", "amd64" },
    { NULL, NULL }
};

/* Define arch structure -- see arch.h for details */
yasm_arch_module yasm_x86_LTX_arch = {
    YASM_ARCH_VERSION,
    "x86 (IA-32 and derivatives), AMD64",
    "x86",
    x86_create,
    x86_destroy,
    x86_get_machine,
    x86_set_var,
    yasm_x86__parse_cpu,
    yasm_x86__parse_check_id,
    x86_parse_directive,
    yasm_x86__parse_insn,
    x86_parse_prefix,
    x86_parse_seg_prefix,
    x86_parse_seg_override,
    yasm_x86__floatnum_tobytes,
    yasm_x86__intnum_tobytes,
    yasm_x86__get_reg_size,
    x86_reg_print,
    x86_segreg_print,
    yasm_x86__ea_create_expr,
    x86_machines,
    "x86",
    2
};
