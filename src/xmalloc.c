/*
 * Memory allocation routines with error checking.  Idea from GNU libiberty.
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "util.h"
RCSID("$IdPath$");

#include "errwarn.h"


#ifndef WITH_DMALLOC

void *
xmalloc(size_t size)
{
    void *newmem;

    if (size == 0)
	size = 1;
    newmem = malloc(size);
    if (!newmem)
	yasm_fatal(N_("out of memory"));

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
	yasm_fatal(N_("out of memory"));

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
	yasm_fatal(N_("out of memory"));

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
