/*
 * x86 architecture description
 *
 *  Copyright (C) 2002  Peter Johnson
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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "file.h"

#include "errwarn.h"
#include "intnum.h"
#include "floatnum.h"
#include "expr.h"

#include "bytecode.h"

#include "arch.h"

#include "x86arch.h"


unsigned char yasm_x86_LTX_mode_bits = 0;
/*@dependent@*/ errwarn *yasm_x86_errwarn;


static void
x86_initialize(errwarn *we)
{
    yasm_x86_errwarn = we;
}

static void
x86_cleanup(void)
{
}

int
x86_directive(const char *name, valparamhead *valparams,
	      /*@unused@*/ /*@null@*/ valparamhead *objext_valparams,
	      /*@unused@*/ sectionhead *headp, unsigned long lindex)
{
    valparam *vp;
    const intnum *intn;
    long lval;

    if (strcasecmp(name, "bits") == 0) {
	if ((vp = vps_first(valparams)) && !vp->val && vp->param != NULL &&
	    (intn = expr_get_intnum(&vp->param, NULL)) != NULL &&
	    (lval = intnum_get_int(intn)) && (lval == 16 || lval == 32))
	    yasm_x86_LTX_mode_bits = (unsigned char)lval;
	else
	    cur_we->error(lindex, N_("invalid argument to [%s]"), "BITS");
	return 0;
    } else
	return 1;
}

unsigned int
x86_get_reg_size(unsigned long reg)
{
    switch ((x86_expritem_reg_size)(reg & ~7)) {
	case X86_REG8:
	    return 1;
	case X86_REG16:
	    return 2;
	case X86_REG32:
	case X86_CRREG:
	case X86_DRREG:
	case X86_TRREG:
	    return 4;
	case X86_MMXREG:
	    return 8;
	case X86_XMMREG:
	    return 16;
	case X86_FPUREG:
	    return 10;
	default:
	    cur_we->internal_error(N_("unknown register size"));
    }
    return 0;
}

void
x86_reg_print(FILE *f, unsigned long reg)
{
    static const char *name8[] = {"al","cl","dl","bl","ah","ch","dh","bh"};
    static const char *name1632[] = {"ax","cx","dx","bx","sp","bp","si","di"};

    switch ((x86_expritem_reg_size)(reg&~7)) {
	case X86_REG8:
	    fprintf(f, "%s", name8[reg&7]);
	    break;
	case X86_REG16:
	    fprintf(f, "%s", name1632[reg&7]);
	    break;
	case X86_REG32:
	    fprintf(f, "e%s", name1632[reg&7]);
	    break;
	case X86_MMXREG:
	    fprintf(f, "mm%d", (int)(reg&7));
	    break;
	case X86_XMMREG:
	    fprintf(f, "xmm%d", (int)(reg&7));
	    break;
	case X86_CRREG:
	    fprintf(f, "cr%d", (int)(reg&7));
	    break;
	case X86_DRREG:
	    fprintf(f, "dr%d", (int)(reg&7));
	    break;
	case X86_TRREG:
	    fprintf(f, "tr%d", (int)(reg&7));
	    break;
	case X86_FPUREG:
	    fprintf(f, "st%d", (int)(reg&7));
	    break;
	default:
	    cur_we->internal_error(N_("unknown register size"));
    }
}

void
x86_segreg_print(FILE *f, unsigned long segreg)
{
    static const char *name[] = {"es","cs","ss","ds","fs","gs"};
    fprintf(f, "%s", name[segreg&7]);
}

void
x86_handle_prefix(bytecode *bc, const unsigned long data[4],
		  unsigned long lindex)
{
    switch((x86_parse_insn_prefix)data[0]) {
	case X86_LOCKREP:
	    x86_bc_insn_set_lockrep_prefix(bc, (unsigned char)data[1], lindex);
	    break;
	case X86_ADDRSIZE:
	    x86_bc_insn_addrsize_override(bc, (unsigned char)data[1]);
	    break;
	case X86_OPERSIZE:
	    x86_bc_insn_opersize_override(bc, (unsigned char)data[1]);
	    break;
    }
}

void
x86_handle_seg_prefix(bytecode *bc, unsigned long segreg, unsigned long lindex)
{
    x86_ea_set_segment(x86_bc_insn_get_ea(bc), (unsigned char)(segreg>>8),
		       lindex);
}

void
x86_handle_seg_override(effaddr *ea, unsigned long segreg,
			unsigned long lindex)
{
    x86_ea_set_segment(ea, (unsigned char)(segreg>>8), lindex);
}

/* Define arch structure -- see arch.h for details */
arch yasm_x86_LTX_arch = {
    "x86 (IA-32, x86-64)",
    "x86",
    x86_initialize,
    x86_cleanup,
    {
	x86_switch_cpu,
	x86_check_identifier,
	x86_directive,
	x86_new_insn,
	x86_handle_prefix,
	x86_handle_seg_prefix,
	x86_handle_seg_override,
	x86_ea_new_expr
    },
    {
	X86_BYTECODE_TYPE_MAX,
	x86_bc_delete,
	x86_bc_print,
	x86_bc_resolve,
	x86_bc_tobytes
    },
    x86_floatnum_tobytes,
    x86_intnum_tobytes,
    x86_get_reg_size,
    x86_reg_print,
    x86_segreg_print,
    NULL,	/* x86_ea_data_delete */
    x86_ea_data_print
};
