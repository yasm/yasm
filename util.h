/**
 * \file util.h
 * \brief YASM utility functions.
 *
 * $IdPath: yasm/libyasm/util.h,v 1.50 2003/05/03 08:02:15 peter Exp $
 *
 * Includes standard headers and defines prototypes for replacement functions
 * if needed.  This is the *only* header file which should include other
 * header files!
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
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
#ifndef YASM_UTIL_H
#define YASM_UTIL_H

#ifdef YASM_LIB_INTERNAL
# define YASM_INTERNAL
# define YASM_AUTOCONF_INTERNAL
# define YASM_LIB_AC_INTERNAL
# define YASM_GETTEXT_INTERNAL
#endif

#ifdef YASM_INTERNAL

#if defined(YASM_LIB_AC_INTERNAL) && defined(HAVE_CONFIG_H)
# include <libyasm/config.h>

/* Work around glibc's non-defining of certain things when using gcc -ansi */
# if defined(HAVE_GNU_C_LIBRARY) && defined(__STRICT_ANSI__)
#  undef __STRICT_ANSI__
# endif
#endif

#endif

#include <stdio.h>

#ifdef YASM_INTERNAL

#if defined(YASM_LIB_AC_INTERNAL) && defined(HAVE_CONFIG_H)
# include <stdarg.h>
#endif

#if !defined(lint) && !defined(NDEBUG)
# define NDEBUG
#endif

#ifdef YASM_AUTOCONF_INTERNAL

# ifdef STDC_HEADERS
#  include <stddef.h>
#  include <stdlib.h>
#  include <string.h>
#  include <assert.h>
# endif

# ifdef YASM_GETTEXT_INTERNAL
#  if defined(lint)
#   define _(String)	String
#  else
#   ifdef HAVE_LOCALE_H
#    include <locale.h>
#   endif

#   ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String)	gettext(String)
#   else
#    define gettext(Msgid)			    (Msgid)
#    define dgettext(Domainname, Msgid)		    (Msgid)
#    define dcgettext(Domainname, Msgid, Category)  (Msgid)
#    define textdomain(Domainname)		    while (0) /* nothing */
#    define bindtextdomain(Domainname, Dirname)	    while (0) /* nothing */
#    define _(String)	(String)
#   endif
#  endif

#  ifdef gettext_noop
#   define N_(String)	gettext_noop(String)
#  else
#   define N_(String)	(String)
#  endif

# endif	/*YASM_GETTEXT_INTERNAL*/

#endif	/*YASM_AUTOCONF_INTERNAL*/

/** Sort an array using merge sort algorithm.
 * \internal
 * \param base	    base of array
 * \param nmemb	    number of elements in array
 * \param size	    size of each array element
 * \param compar    element comparison function
 */
int yasm__mergesort(void *base, size_t nmemb, size_t size,
		    int (*compar)(const void *, const void *));

#if defined(YASM_AUTOCONF_INTERNAL) && defined(HAVE_MERGESORT)
#define yasm__mergesort(a, b, c, d)	mergesort(a, b, c, d)
#endif

/** Separate string by delimiters.
 * \internal
 * \param stringp   string
 * \param delim	    set of 1 or more delimiters
 * \return First/next substring.
 */
/*@null@*/ char *yasm__strsep(char **stringp, const char *delim);

#if defined(YASM_AUTOCONF_INTERNAL) && defined(HAVE_STRSEP)
#define yasm__strsep(a, b)		strsep(a, b)
#endif

/** Compare two strings, ignoring case differences.
 * \internal
 * \param s1	string 1
 * \param s2	string 2
 * \return 0 if strings are equal, -1 if s1<s2, 1 if s1>s2.
 */
int yasm__strcasecmp(const char *s1, const char *s2);

/** Compare portion of two strings, ignoring case differences.
 * \internal
 * \param s1	string 1
 * \param s2	string 2
 * \param n	maximum number of characters to compare
 * \return 0 if strings are equal, -1 if s1<s2, 1 if s1>s2.
 */
int yasm__strncasecmp(const char *s1, const char *s2, size_t n);

#ifdef YASM_AUTOCONF_INTERNAL

#ifdef HAVE_STRCASECMP
# define yasm__strcasecmp(x, y)		strcasecmp(x, y)
# define yasm__strncasecmp(x, y, n)	strncasecmp(x, y, n)
#elif HAVE_STRICMP
# define yasm__strcasecmp(x, y)		stricmp(x, y)
# define yasm__strncasecmp(x, y, n)	strnicmp(x, y, n)
#elif HAVE_STRCMPI
# define yasm__strcasecmp(x, y)		strcmpi(x, y)
# define yasm__strncasecmp(x, y, n)	strncmpi(x, y, n)
#else
# define USE_OUR_OWN_STRCASECMP
#endif

#if !defined(HAVE_TOASCII) || defined(lint)
# define toascii(c) ((c) & 0x7F)
#endif

#endif	/*YASM_AUTOCONF_INTERNAL*/

#include <libyasm/compat-queue.h>

#if defined(YASM_AUTOCONF_INTERNAL) && defined(HAVE_SYS_CDEFS_H)
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

/** strdup() implementation using yasm_xmalloc().
 * \internal
 * \param str	string
 * \return Newly allocated duplicate string.
 */
/*@only@*/ char *yasm__xstrdup(const char *str);

/** strndup() implementation using yasm_xmalloc().
 * \internal
 * \param str	string
 * \param len	maximum number of characters to copy
 * \return Newly allocated duplicate string.
 */
/*@only@*/ char *yasm__xstrndup(const char *str, size_t len);

#endif	/*YASM_INTERNAL*/

/** Error-checking memory allocation.  A default implementation is provided
 * that calls yasm_fatal() on allocation errors.
 * A replacement should \em never return NULL.
 * \param size	    number of bytes to allocate
 * \return Allocated memory block.
 */
extern /*@only@*/ /*@out@*/ void * (*yasm_xmalloc) (size_t size);

/** Error-checking memory allocation (with clear-to-0).  A default
 * implementation is provided that calls yasm_fatal() on allocation errors.
 * A replacement should \em never return NULL.
 * \param size	    number of elements to allocate
 * \param elsize    size (in bytes) of each element
 * \return Allocated and cleared memory block.
 */
extern /*@only@*/ void * (*yasm_xcalloc) (size_t nelem, size_t elsize);

/** Error-checking memory reallocation.  A default implementation is provided
 * that calls yasm_fatal() on allocation errors.  A replacement should
 * \em never return NULL.
 * \param oldmem    memory block to resize
 * \param elsize    new size, in bytes
 * \return Re-allocated memory block.
 */
extern /*@only@*/ void * (*yasm_xrealloc)
    (/*@only@*/ /*@out@*/ /*@returned@*/ /*@null@*/ void *oldmem, size_t size)
    /*@modifies oldmem@*/;

/** Error-checking memory deallocation.  A default implementation is provided
 * that calls yasm_fatal() on allocation errors.
 * \param p	memory block to free
 */
extern void (*yasm_xfree) (/*@only@*/ /*@out@*/ /*@null@*/ void *p)
    /*@modifies p@*/;

#ifdef YASM_INTERNAL

#if defined(YASM_AUTOCONF_INTERNAL) && defined(WITH_DMALLOC)
# include <dmalloc.h>

#define yasm__xstrdup(str)		xstrdup(str)
#define yasm_xmalloc(size)		xmalloc(size)
#define yasm_xcalloc(count, size)	xcalloc(count, size)
#define yasm_xrealloc(ptr, size)	xrealloc(ptr, size)
#define yasm_xfree(ptr)			xfree(ptr)
#endif

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

#ifndef NELEMS
/** Get the number of elements in an array.
 * \internal
 * \param array	    array
 * \return Number of elements.
 */
#define NELEMS(array)	(sizeof(array) / sizeof(array[0]))
#endif

#endif	/*YASM_INTERNAL*/

#include <libyasm/coretype.h>

#include <libyasm/valparam.h>

#endif
