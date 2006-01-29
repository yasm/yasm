/*
 * Split a path into directory name and file name.
 *
 *  Copyright (C) 2006  Peter Johnson
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
#include <ctype.h>

#define YASM_LIB_INTERNAL
#include "util.h"
/*@unused@*/ RCSID("$Id$");

#include "coretype.h"

size_t
yasm__splitpath_unix(const char *path, /*@out@*/ const char **tail)
{
    const char *s;
    s = strrchr(path, '/');
    if (!s) {
	/* No head */
	*tail = path;
	return 0;
    }
    *tail = s+1;
    /* Strip trailing slashes on path (except leading) */
    while (s>path && *s == '/')
	s--;
    /* Return length of head */
    return s-path+1;
}

size_t
yasm__splitpath_win(const char *path, /*@out@*/ const char **tail)
{
    const char *basepath = path;
    const char *s;

    /* split off drive letter first, if any */
    if (isalpha(path[0]) && path[1] == ':')
	basepath += 2;

    s = basepath;
    while (*s != '\0')
	s++;
    while (s >= basepath && *s != '\\' && *s != '/')
	s--;
    if (s < basepath) {
	*tail = basepath;
	if (path == basepath)
	    return 0;	/* No head */
	else
	    return 2;	/* Drive letter is head */
    }
    *tail = s+1;
    /* Strip trailing slashes on path (except leading) */
    while (s>basepath && (*s == '/' || *s == '\\'))
	s--;
    /* Return length of head */
    return s-path+1;
}

