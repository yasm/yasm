/*
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
#include "util.h"
RCSID("$IdPath$");


static /*@exits@*/ void (*fatal_func) (int type);
static int nomem_fatal_type;


void
xalloc_initialize(/*@exits@*/ void (*f) (int type), int t)
{
    fatal_func = f;
    nomem_fatal_type = t;
}

#ifndef WITH_DMALLOC

void *
xmalloc(size_t size)
{
    void *newmem;

    if (size == 0)
	size = 1;
    newmem = malloc(size);
    if (!newmem)
	fatal_func(nomem_fatal_type);

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
	fatal_func(nomem_fatal_type);

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
	fatal_func(nomem_fatal_type);

    return newmem;
}

void
xfree(void *p)
{
    if (!p)
	return;
    free(p);
}

#endif
