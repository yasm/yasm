/* $IdPath$
 * Debugging object format (used to debug object format module interface)
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util.h"

#include <stdio.h>

#include "objfmt.h"

RCSID("$IdPath$");

static const char *
dbg_objfmt_get_default_section_name(void)
{
    fprintf(stderr, "-dbg_objfmt_get_default_section_name()\n");
    return ".text";
}

static int
dbg_objfmt_is_valid_section(const char *name)
{
    fprintf(stderr, "-dbg_objfmt_is_valid_section(\"%s\")\n", name);
    return 1;
}

/* Define objfmt structure -- see objfmt.h for details */
objfmt dbg_objfmt = {
    "Trace of all info passed to object format module",
    "dbg",
    dbg_objfmt_get_default_section_name,
    dbg_objfmt_is_valid_section
};
