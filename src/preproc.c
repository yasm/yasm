/*
 * Generic functions for all preprocessors
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

#include "globals.h"

#include "preproc.h"


/* NULL-terminated list of all available preprocessors.
 * Someday change this if we dynamically load preprocessors at runtime.
 * Could improve this a little by generating automatically at build-time.
 */
/*@-nullassign@*/
static preproc *preprocs[] = {
    &raw_preproc,
    NULL
};
/*@=nullassign@*/

preproc *
find_preproc(const char *keyword)
{
    int i;

    /* We're just doing a linear search, as there aren't many preprocs */
    for (i = 0; preprocs[i]; i++) {
	if (strcasecmp(preprocs[i]->keyword, keyword) == 0)
	    /*@-unqualifiedtrans@*/
	    return preprocs[i];
	    /*@=unqualifiedtrans@*/
    }

    /* no match found */
    return NULL;
}

void
list_preprocs(void (*printfunc) (const char *name, const char *keyword))
{
    int i;

    for (i = 0; preprocs[i]; i++) {
	printfunc(preprocs[i]->name, preprocs[i]->keyword);
    }
}
