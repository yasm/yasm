/*
 * Generic functions for all object formats
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

#include "globals.h"

#include "objfmt.h"


/* Available object formats */
extern objfmt dbg_objfmt;

/* NULL-terminated list of all available object formats.
 * Someday change this if we dynamically load object formats at runtime.
 * Could improve this a little by generating automatically at build-time.
 */
/*@-nullassign@*/
objfmt *objfmts[] = {
    &dbg_objfmt,
    NULL
};
/*@=nullassign@*/

objfmt *
find_objfmt(const char *keyword)
{
    int i;

    /* We're just doing a linear search, as there aren't many object formats */
    for (i = 0; objfmts[i]; i++) {
	if (strcasecmp(objfmts[i]->keyword, keyword) == 0)
	    /*@-unqualifiedtrans@*/
	    return objfmts[i];
	    /*@=unqualifiedtrans@*/
    }

    /* no match found */
    return NULL;
}

void
list_objfmts(void (*printfunc) (const char *name, const char *keyword))
{
    int i;

    for (i = 0; objfmts[i]; i++)
	printfunc(objfmts[i]->name, objfmts[i]->keyword);
}
