/*
 * Basic optimizer (equivalent to the NASM 2-pass 'no optimizer' design)
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
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "errwarn.h"
#include "intnum.h"
#include "expr.h"
#include "symrec.h"

#include "bytecode.h"
#include "section.h"

#include "bc-int.h"

#include "optimizer.h"


#define SECTFLAG_NONE		0UL
#define SECTFLAG_INPROGRESS	(1UL<<0)
#define SECTFLAG_DONE		(1UL<<1)

#define BCFLAG_NONE		0UL
#define BCFLAG_INPROGRESS	(1UL<<0)
#define BCFLAG_DONE		(1UL<<1)

static /*@dependent@*/ yasm_errwarn *cur_we;

static int basic_optimize_section_1(yasm_section *sect,
				    /*@unused@*/ /*@null@*/ void *d);

static /*@null@*/ yasm_intnum *
basic_optimize_calc_bc_dist_1(yasm_section *sect,
			      /*@null@*/ yasm_bytecode *precbc1,
			      /*@null@*/ yasm_bytecode *precbc2)
{
    unsigned int dist;
    yasm_intnum *intn;

    if (yasm_section_get_opt_flags(sect) == SECTFLAG_NONE) {
	/* Section not started.  Optimize it (recursively). */
	basic_optimize_section_1(sect, NULL);
    }

    /* If a section is done, the following will always succeed.  If it's in-
     * progress, this will fail if the bytecode comes AFTER the current one.
     */
    if (precbc2) {
	if (precbc2->opt_flags == BCFLAG_DONE) {
	    dist = precbc2->offset + precbc2->len;
	    if (precbc1) {
		if (precbc1->opt_flags == BCFLAG_DONE) {
		    if (dist < precbc1->offset + precbc1->len) {
			intn = yasm_intnum_new_uint(precbc1->offset +
						    precbc1->len - dist);
			yasm_intnum_calc(intn, YASM_EXPR_NEG, NULL);
			return intn;
		    }
		    dist -= precbc1->offset + precbc1->len;
		} else {
		    return NULL;
		}
	    }
	    return yasm_intnum_new_uint(dist);
	} else {
	    return NULL;
	}
    } else {
	if (precbc1) {
	    if (precbc1->opt_flags == BCFLAG_DONE) {
		intn = yasm_intnum_new_uint(precbc1->offset + precbc1->len);
		yasm_intnum_calc(intn, YASM_EXPR_NEG, NULL);
		return intn;
	    } else {
		return NULL;
	    }
	} else {
	    return yasm_intnum_new_uint(0);
	}
    }
}

typedef struct basic_optimize_data {
    /*@observer@*/ yasm_bytecode *precbc;
    /*@observer@*/ const yasm_section *sect;
    int saw_unknown;
} basic_optimize_data;

static int
basic_optimize_bytecode_1(/*@observer@*/ yasm_bytecode *bc, void *d)
{
    basic_optimize_data *data = (basic_optimize_data *)d;
    yasm_bc_resolve_flags bcr_retval;

    /* Don't even bother if we're in-progress or done. */
    if (bc->opt_flags == BCFLAG_INPROGRESS)
	return 1;
    if (bc->opt_flags == BCFLAG_DONE)
	return 0;

    bc->opt_flags = BCFLAG_INPROGRESS;

    if (!data->precbc)
	bc->offset = 0;
    else
	bc->offset = data->precbc->offset + data->precbc->len;
    data->precbc = bc;

    /* We're doing just a single pass, so essentially ignore whether the size
     * is minimum or not, and just check for indeterminate length (indicative
     * of circular reference).
     */
    bcr_retval = yasm_bc_resolve(bc, 0, data->sect,
				 basic_optimize_calc_bc_dist_1);
    if (bcr_retval & YASM_BC_RESOLVE_UNKNOWN_LEN) {
	if (!(bcr_retval & YASM_BC_RESOLVE_ERROR))
	    cur_we->error(bc->line, N_("circular reference detected."));
	data->saw_unknown = -1;
	return 0;
    }

    bc->opt_flags = BCFLAG_DONE;

    return 0;
}

static int
basic_optimize_section_1(yasm_section *sect, /*@null@*/ void *d)
{
    /*@null@*/ int *saw_unknown = (int *)d;
    basic_optimize_data data;
    unsigned long flags;
    int retval;

    data.precbc = NULL;
    data.sect = sect;
    data.saw_unknown = 0;

    /* Don't even bother if we're in-progress or done. */
    flags = yasm_section_get_opt_flags(sect);
    if (flags == SECTFLAG_INPROGRESS)
	return 1;
    if (flags == SECTFLAG_DONE)
	return 0;

    yasm_section_set_opt_flags(sect, SECTFLAG_INPROGRESS);

    retval = yasm_bcs_traverse(yasm_section_get_bytecodes(sect), &data,
			       basic_optimize_bytecode_1);
    if (retval != 0)
	return retval;

    if (data.saw_unknown != 0 && saw_unknown)
	*saw_unknown = data.saw_unknown;

    yasm_section_set_opt_flags(sect, SECTFLAG_DONE);

    return 0;
}

static int
basic_optimize_bytecode_2(/*@observer@*/ yasm_bytecode *bc, /*@null@*/ void *d)
{
    basic_optimize_data *data = (basic_optimize_data *)d;

    assert(data != NULL);

    if (bc->opt_flags != BCFLAG_DONE)
	cur_we->internal_error(N_("Optimizer pass 1 missed a bytecode!"));

    if (!data->precbc)
	bc->offset = 0;
    else
	bc->offset = data->precbc->offset + data->precbc->len;
    data->precbc = bc;

    if (yasm_bc_resolve(bc, 1, data->sect, yasm_common_calc_bc_dist) < 0)
	return -1;
    return 0;
}

static int
basic_optimize_section_2(yasm_section *sect, /*@unused@*/ /*@null@*/ void *d)
{
    basic_optimize_data data;

    data.precbc = NULL;
    data.sect = sect;

    if (yasm_section_get_opt_flags(sect) != SECTFLAG_DONE)
	cur_we->internal_error(N_("Optimizer pass 1 missed a section!"));

    return yasm_bcs_traverse(yasm_section_get_bytecodes(sect), &data,
			     basic_optimize_bytecode_2);
}

static void
basic_optimize(yasm_sectionhead *sections, yasm_errwarn *we)
{
    int saw_unknown = 0;

    cur_we = we;

    /* Optimization process: (essentially NASM's pass 1)
     *  Determine the size of all bytecodes.
     *  Forward references are /not/ resolved (only backward references are
     *   computed and sized).
     *  Check "critical" expressions (must be computable on the first pass,
     *   i.e. depend only on symbols before it).
     *  Differences from NASM:
     *   - right-hand side of EQU is /not/ a critical expr (as the entire file
     *     has already been parsed, we know all their values at this point).
     *   - not strictly top->bottom scanning; we scan through a section and
     *     hop to other sections as necessary.
     */
    if (yasm_sections_traverse(sections, &saw_unknown,
			       basic_optimize_section_1) < 0 ||
	saw_unknown != 0)
	return;

    /* Check completion of all sections and save bytecode changes */
    yasm_sections_traverse(sections, NULL, basic_optimize_section_2);
}

/* Define optimizer structure -- see optimizer.h for details */
yasm_optimizer yasm_basic_LTX_optimizer = {
    "Only the most basic optimizations",
    "basic",
    basic_optimize
};
