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
/*@unused@*/ RCSID("$IdPath$");

#define YASM_LIB_INTERNAL
#include <libyasm.h>

#include "lc3barch.h"


static int
lc3b_initialize(/*@unused@*/ const char *machine)
{
    if (yasm__strcasecmp(machine, "lc3b") != 0)
	return 1;
    return 0;
}

static void
lc3b_cleanup(void)
{
}

int
yasm_lc3b__parse_directive(/*@unused@*/ const char *name,
			   /*@unused@*/ yasm_valparamhead *valparams,
			   /*@unused@*/ /*@null@*/
			   yasm_valparamhead *objext_valparams,
			   /*@unused@*/ yasm_sectionhead *headp,
			   /*@unused@*/ unsigned long lindex)
{
    return 1;
}

unsigned int
yasm_lc3b__get_reg_size(unsigned long reg)
{
    return 2;
}

void
yasm_lc3b__reg_print(FILE *f, unsigned long reg)
{
    fprintf(f, "r%u", (unsigned int)(reg&7));
}

static int
yasm_lc3b__floatnum_tobytes(const yasm_floatnum *flt, unsigned char *buf,
			    size_t destsize, size_t valsize, size_t shift,
			    int warn, unsigned long lindex)
{
    yasm__error(lindex, N_("LC-3b does not support floating point"));
    return 1;
}

static yasm_effaddr *
yasm_lc3b__ea_new_expr(yasm_expr *e)
{
    yasm_expr_delete(e);
    return NULL;
}

/* Define lc3b machines -- see arch.h for details */
static yasm_arch_machine lc3b_machines[] = {
    { "LC-3b", "lc3b" },
    { NULL, NULL }
};

/* Define arch structure -- see arch.h for details */
yasm_arch yasm_lc3b_LTX_arch = {
    "LC-3b",
    "lc3b",
    lc3b_initialize,
    lc3b_cleanup,
    yasm_lc3b__parse_cpu,
    yasm_lc3b__parse_check_id,
    yasm_lc3b__parse_directive,
    yasm_lc3b__parse_insn,
    NULL,	/*yasm_lc3b__parse_prefix*/
    NULL,	/*yasm_lc3b__parse_seg_prefix*/
    NULL,	/*yasm_lc3b__parse_seg_override*/
    LC3B_BYTECODE_TYPE_MAX,
    yasm_lc3b__bc_delete,
    yasm_lc3b__bc_print,
    yasm_lc3b__bc_resolve,
    yasm_lc3b__bc_tobytes,
    yasm_lc3b__floatnum_tobytes,
    yasm_lc3b__intnum_tobytes,
    yasm_lc3b__get_reg_size,
    yasm_lc3b__reg_print,
    NULL,	/*yasm_lc3b__segreg_print*/
    yasm_lc3b__ea_new_expr,
    NULL,	/*yasm_lc3b__ea_data_delete*/
    NULL,	/*yasm_lc3b__ea_data_print*/
    lc3b_machines,
    "lc3b"
};
