/* $IdPath$
 * Includes standard headers and defines prototypes for replacement functions
 * if needed.  This is the *only* header file which should include other
 * header files!
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
 * 3. Neither the name of the author nor the names of other contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#ifndef YASM_UTIL_H
#define YASM_UTIL_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>

#if !defined(lint)
# define NDEBUG
#endif

#ifdef STDC_HEADERS
# include <stddef.h>
# include <stdlib.h>
# include <string.h>
# include <assert.h>
#endif

#if defined(lint)
#define _(String)	String
#else
# ifdef HAVE_LOCALE_H
#  include <locale.h>
# endif

# ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String)	gettext(String)
# else
#  define gettext(Msgid)    (Msgid)
#  define dgettext(Domainname, Msgid)	(Msgid)
#  define dcgettext(Domainname, Msgid, Category)    (Msgid)
#  define textdomain(Domainname)		while (0) /* nothing */
#  define bindtextdomain(Domainname, Dirname)	while (0) /* nothing */
#  define _(String)	(String)
# endif
#endif

#ifdef gettext_noop
# define N_(String)	gettext_noop(String)
#else
# define N_(String)	(String)
#endif

#if !defined(HAVE_MERGESORT) || defined(lint)
int mergesort(void *base, size_t nmemb, size_t size,
	      int (*compar)(const void *, const void *));
#endif

#if !defined(HAVE_STRSEP) || defined(lint)
/*@null@*/ char *strsep(char **stringp, const char *delim);
#endif

#ifndef HAVE_STRCASECMP
# ifdef HAVE_STRICMP
#  define strcasecmp(x, y)	stricmp(x, y)
#  define strncasecmp(x, y)	strnicmp(x, y)
# elif HAVE_STRCMPI
#  define strcasecmp(x, y)	strcmpi(x, y)
#  define strncasecmp(x, y)	strncmpi(x, y)
# else
#  define USE_OUR_OWN_STRCASECMP
# endif
#endif

#if defined(USE_OUR_OWN_STRCASECMP) || defined(lint)
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
#endif

#if !defined(HAVE_TOASCII) || defined(lint)
# define toascii(c) ((c) & 0x7F)
#endif

#include "compat-queue.h"

#ifdef HAVE_SYS_CDEFS_H
# include <sys/cdefs.h>
#endif

#ifdef __RCSID
# define RCSID(s)	__RCSID(s)
#else
# ifdef __GNUC__
#  ifdef __ELF__
#   define RCSID(s)	__asm__(".ident\t\"" s "\"")
#  else
#   define RCSID(s)	static const char rcsid[] = s
#  endif
# else
#  define RCSID(s)	static const char rcsid[] = s
# endif
#endif

/* strdup() implementation with error checking (using xmalloc). */
/*@only@*/ char *xstrdup(const char *str);

/* Error-checking memory allocation routines in xmalloc.c. */
/*@only@*/ /*@out@*/ void *xmalloc(size_t size);
/*@only@*/ void *xcalloc(size_t nelem, size_t elsize);
/*@only@*/ void *xrealloc(/*@only@*/ /*@out@*/ /*@returned@*/ /*@null@*/
			  void *oldmem, size_t size) /*@modifies oldmem@*/;
void xfree(/*@only@*/ /*@out@*/ /*@null@*/ void *p) /*@modifies p@*/;

#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif

/*@only@*/ char *xstrndup(const char *str, size_t len);

/* Bit-counting: used primarily by HAMT but also in a few other places. */
#define SK5	0x55555555
#define SK3	0x33333333
#define SKF0	0x0F0F0F0F
#define BitCount(d, s)		do {		\
	d = s;					\
	d -= (d>>1) & SK5;			\
	d = (d & SK3) + ((d>>2) & SK3);		\
	d = (d & SKF0) + ((d>>4) & SKF0);	\
	d += d>>16;				\
	d += d>>8;				\
    } while (0)

/* Get the number of elements in an array. */
#ifndef NELEMS
#define NELEMS(array)	(sizeof(array) / sizeof(array[0]))
#endif

#include "coretype.h"

#include "valparam.h"

#endif
