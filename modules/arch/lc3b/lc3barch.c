/*
 * LC-3b architecture description
 *
 *  Copyright (C) 2003  Peter Johnson
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
/*@unused@*/ RCSID("$Id$");

#define YASM_LIB_INTERNAL
#include <libyasm.h>

#include "lc3barch.h"


yasm_arch_module yasm_lc3b_LTX_arch;


static /*@only@*/ yasm_arch *
lc3b_create(const char *machine)
{
    yasm_arch_base *arch;

    if (yasm__strcasecmp(machine, "lc3b") != 0)
	return NULL;

    arch = yasm_xmalloc(sizeof(yasm_arch_base));
    arch->module = &yasm_lc3b_LTX_arch;
    return (yasm_arch *)arch;
}

static void
lc3b_destroy(/*@only@*/ yasm_arch *arch)
{
    yasm_xfree(arch);
}

static const char *
lc3b_get_machine(/*@unused@*/ const yasm_arch *arch)
{
    return "lc3b";
}

static int
lc3b_set_var(yasm_arch *arch, const char *var, unsigned long val)
{
    return 1;
}

static int
lc3b_parse_directive(/*@unused@*/ yasm_arch *arch,
		     /*@unused@*/ const char *name,
		     /*@unused@*/ yasm_valparamhead *valparams,
		     /*@unused@*/ /*@null@*/
		     yasm_valparamhead *objext_valparams,
		     /*@unused@*/ yasm_object *object,
		     /*@unused@*/ unsigned long line)
{
    return 1;
}

static unsigned int
lc3b_get_reg_size(/*@unused@*/ yasm_arch *arch, /*@unused@*/ unsigned long reg)
{
    return 2;
}

static void
lc3b_reg_print(/*@unused@*/ yasm_arch *arch, unsigned long reg, FILE *f)
{
    fprintf(f, "r%u", (unsigned int)(reg&7));
}

static int
lc3b_floatnum_tobytes(yasm_arch *arch, const yasm_floatnum *flt,
		      unsigned char *buf, size_t destsize, size_t valsize,
		      size_t shift, int warn, unsigned long line)
{
    yasm__error(line, N_("LC-3b does not support floating point"));
    return 1;
}

static yasm_effaddr *
lc3b_ea_create_expr(yasm_arch *arch, yasm_expr *e)
{
    yasm_expr_destroy(e);
    return NULL;
}

/* Define lc3b machines -- see arch.h for details */
static yasm_arch_machine lc3b_machines[] = {
    { "LC-3b", "lc3b" },
    { NULL, NULL }
};

/* Define arch structure -- see arch.h for details */
yasm_arch_module yasm_lc3b_LTX_arch = {
    YASM_ARCH_VERSION,
    "LC-3b",
    "lc3b",
    lc3b_create,
    lc3b_destroy,
    lc3b_get_machine,
    lc3b_set_var,
    yasm_lc3b__parse_cpu,
    yasm_lc3b__parse_check_id,
    lc3b_parse_directive,
    yasm_lc3b__parse_insn,
    NULL,	/*yasm_lc3b__parse_prefix*/
    NULL,	/*yasm_lc3b__parse_seg_prefix*/
    NULL,	/*yasm_lc3b__parse_seg_override*/
    lc3b_floatnum_tobytes,
    yasm_lc3b__intnum_fixup_rel,
    yasm_lc3b__intnum_tobytes,
    lc3b_get_reg_size,
    lc3b_reg_print,
    NULL,	/*yasm_lc3b__segreg_print*/
    lc3b_ea_create_expr,
    lc3b_machines,
    "lc3b",
    2
};
