/* $IdPath$
 * Memory allocation routines with error checking.  Idea from GNU libiberty.
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

#ifdef STDC_HEADERS
# include <stdlib.h>
#endif

#include <libintl.h>
#define _(String)	gettext(String)

#include "errwarn.h"

#ifndef DMALLOC

RCSID("$IdPath$");

void *
xmalloc(size_t size)
{
    void *newmem;

    if (size == 0)
	size = 1;
    newmem = malloc(size);
    if (!newmem)
	Fatal(FATAL_NOMEM);

    return newmem;
}

void *
xcalloc(size_t nelem, size_t elsize)
{
    void *newmem;

    if (nelem == 0 || elsize == 0)
	nelem = elsize = 1;

    newmem = calloc(nelem, elsize);
    if (!newmem)
	Fatal(FATAL_NOMEM);

    return newmem;
}

void *
xrealloc(void *oldmem, size_t size)
{
    void *newmem;

    if (size == 0)
	size = 1;
    if (!oldmem)
	newmem = malloc(size);
    else
	newmem = realloc(oldmem, size);
    if (!newmem)
	Fatal(FATAL_NOMEM);

    return newmem;
}

void
xfree(void *p)
{
    if (!p)
	InternalError(__LINE__, __FILE__, _("Tried to free NULL pointer"));
    free(p);
}

#endif
