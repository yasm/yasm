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

static /*@only@*/ /*@null@*/ intnum *
basic_optimize_resolve_label(symrec *sym)
{
    unsigned long flags;
    /*@dependent@*/ section *sect;
    /*@dependent@*/ /*@null@*/ bytecode *bc;

    if (!symrec_get_label(sym, &sect, &bc))
	return NULL;

    flags = symrec_get_opt_flags(sym);

    return NULL;
}

static int
basic_optimize_bytecode(bytecode *bc, /*@unused@*/ /*@null@*/ void *d)
{
    bc->opt_flags = BCFLAG_INPROGRESS;

    bc_calc_len(bc, basic_optimize_resolve_label);

    bc->opt_flags = BCFLAG_DONE;

    return 1;
}

static int
basic_optimize_section(section *sect, /*@unused@*/ /*@null@*/ void *d)
{
    section_set_opt_flags(sect, SECTFLAG_INPROGRESS);

    bcs_traverse(section_get_bytecodes(sect), NULL, basic_optimize_bytecode);

    section_set_opt_flags(sect, SECTFLAG_DONE);

    return 1;
}

static void
basic_optimize(sectionhead *sections)
{
    /* Optimization process: (essentially NASM's pass 1)
     *  Determine the size of all bytecodes.
     *  Check "critical" expressions (must be computable on the first pass,
     *   i.e. depend only on symbols before it).
     *  Differences from NASM:
     *   - right-hand side of EQU is /not/ a critical expr (as the entire file
     *     has already been parsed, we know all their values at this point).
     *   - not strictly top->bottom scanning; we scan through a section and
     *     hop to other sections as necessary.
     */
    sections_traverse(sections, NULL, basic_optimize_section);

    /* NASM's pass 2 is output, so we just return. */
}

/* Define optimizer structure -- see optimizer.h for details */
optimizer basic_optimizer = {
    "Only the most basic optimizations",
    "basic",
    basic_optimize
};
