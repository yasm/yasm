/*
 * x86 architecture description
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

#include "bytecode.h"
#include "arch.h"

#include "x86-int.h"


unsigned char x86_mode_bits = 0;

/* Define arch structure -- see arch.h for details */
arch x86_arch = {
    "x86 (IA-32, x86-64)",
    "x86",
    {
	X86_BYTECODE_TYPE_MAX,
	x86_bc_delete,
	x86_bc_print,
	x86_bc_calc_len,
	x86_bc_resolve
    }
};
