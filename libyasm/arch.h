/* $IdPath$
 * Architecture header file
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
#ifndef YASM_ARCH_H
#define YASM_ARCH_H

struct arch {
    /* one-line description of the architecture */
    const char *name;

    /* keyword used to select architecture */
    const char *keyword;

    struct {
	/* Maximum used bytecode type value+1.  Should be set to
	 * BYTECODE_TYPE_BASE if no additional bytecode types are defined by
	 * the architecture.
	 */
	const int type_max;

	void (*bc_delete) (bytecode *bc);
	void (*bc_print) (FILE *f, const bytecode *bc);

	/* See bytecode.h comments on bc_calc_len() */
	unsigned long (*bc_calc_len) (bytecode *bc, /*@only@*/ /*@null@*/
				      intnum *(*resolve_label) (symrec *sym));
    } bc;
};

/* Available architectures */
#include "arch/x86/x86arch.h"
extern arch x86_arch;

extern arch *cur_arch;

#endif
