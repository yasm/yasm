/*
 * Basic optimizer (equivalent to the NASM 2-pass 'no optimizer' design)
 *
 *  Copyright (C) 2001  Peter Johnson
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

#include "intnum.h"
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

static int basic_optimize_section_1(section *sect,
				    /*@unused@*/ /*@null@*/ void *d);

static /*@only@*/ /*@null@*/ intnum *
basic_optimize_resolve_label(symrec *sym)
{
    /*@dependent@*/ section *sect;
    /*@dependent@*/ /*@null@*/ bytecode *precbc;
    /*@null@*/ bytecode *bc;

    if (!symrec_get_label(sym, &sect, &precbc))
	return NULL;

    /* determine actual bc from preceding bc (how labels are stored) */
    if (!precbc)
	bc = bcs_first(section_get_bytecodes(sect));
    else
	bc = bcs_next(precbc);
    assert(bc != NULL);

    if (section_get_opt_flags(sect) == SECTFLAG_NONE) {
	/* Section not started.  Optimize it (recursively). */
	basic_optimize_section_1(sect, NULL);
    }
    /* If a section is done, the following will always succeed.  If it's in-
     * progress, this will fail if the bytecode comes AFTER the current one.
     */
    if (precbc && precbc->opt_flags == BCFLAG_DONE)
	return intnum_new_int(precbc->offset + precbc->len);
    if (bc->opt_flags == BCFLAG_DONE)
	return intnum_new_int(bc->offset);

    return NULL;
}

static int
basic_optimize_bytecode_1(bytecode *bc, void *d)
{
    bytecode **precbc = (bytecode **)d;

    /* Don't even bother if we're in-progress or done. */
    if (bc->opt_flags == BCFLAG_INPROGRESS)
	return 0;
    if (bc->opt_flags == BCFLAG_DONE)
	return 1;

    bc->opt_flags = BCFLAG_INPROGRESS;

    bc_calc_len(bc, basic_optimize_resolve_label);
    if (!*precbc)
	bc->offset = 0;
    else
	bc->offset = (*precbc)->offset + (*precbc)->len;
    *precbc = bc;

    bc->opt_flags = BCFLAG_DONE;

    return 1;
}

static int
basic_optimize_section_1(section *sect, /*@unused@*/ /*@null@*/ void *d)
{
    bytecode *precbc = NULL;
    unsigned long flags;

    /* Don't even bother if we're in-progress or done. */
    flags = section_get_opt_flags(sect);
    if (flags == SECTFLAG_INPROGRESS)
	return 0;
    if (flags == SECTFLAG_DONE)
	return 1;

    section_set_opt_flags(sect, SECTFLAG_INPROGRESS);

    if(!bcs_traverse(section_get_bytecodes(sect), &precbc,
		     basic_optimize_bytecode_1))
	return 0;

    section_set_opt_flags(sect, SECTFLAG_DONE);

    return 1;
}
#if 0
static int
basic_optimize_bytecode_2(bytecode *bc, /*@unused@*/ /*@null@*/ void *d)
{
    if (bc->opt_flags != BCFLAG_DONE)
	return 0;
    bc_resolve(bc, basic_optimize_resolve_label);
    return 1;
}

static int
basic_optimize_section_2(section *sect, /*@unused@*/ /*@null@*/ void *d)
{
    if (section_get_opt_flags(sect) != SECTFLAG_DONE)
	return 0;
    return bcs_traverse(section_get_bytecodes(sect), NULL,
			basic_optimize_bytecode_2);
}
#endif
static void
basic_optimize(sectionhead *sections)
{
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
    sections_traverse(sections, NULL, basic_optimize_section_1);

    /* Pass 2:
     *  Resolve (compute value of) forward references.
     */
    /*sections_traverse(sections, NULL, basic_optimize_section_2);*/
}

/* Define optimizer structure -- see optimizer.h for details */
optimizer basic_optimizer = {
    "Only the most basic optimizations",
    "basic",
    basic_optimize
};
