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

#include "ltdl.h"

#include "globals.h"

#include "objfmt.h"


static objfmt *dyn_objfmt = NULL;
static lt_dlhandle objfmt_module = NULL;

/* NULL-terminated list of all possibly available object format keywords.
 * Could improve this a little by generating automatically at build-time.
 */
/*@-nullassign@*/
const char *objfmts[] = {
    "dbg",
    "bin",
    NULL
};
/*@=nullassign@*/

objfmt *
find_objfmt(const char *keyword)
{
    char *modulename;

    /* Look for dynamic module.  First build full module name from keyword. */
    modulename = xmalloc(5+strlen(keyword)+1);
    strcpy(modulename, "yasm-");
    strcat(modulename, keyword);
    objfmt_module = lt_dlopenext(modulename);
    xfree(modulename);

    if (!objfmt_module)
	return NULL;

    /* Find objfmt structure */
    dyn_objfmt = (objfmt *)lt_dlsym(objfmt_module, "objfmt");

    if (!dyn_objfmt) {
	lt_dlclose(objfmt_module);
	return NULL;
    }

    /* found it */
    return dyn_objfmt;
}

void
list_objfmts(void (*printfunc) (const char *name, const char *keyword))
{
    int i;
    objfmt *of;

    /* Go through available list, and try to load each one */
    for (i = 0; objfmts[i]; i++) {
	of = find_objfmt(objfmts[i]);
	if (of) {
	    printfunc(of->name, of->keyword);
	    dyn_objfmt = NULL;
	    lt_dlclose(objfmt_module);
	}
    }
}
