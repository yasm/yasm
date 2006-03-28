/**
 * \file libyasm/file.h
 * \brief YASM file helpers.
 *
 * \rcs
 * $Id$
 * \endrcs
 *
 * \license
 *  Copyright (C) 2001-2006  Peter Johnson
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
 * \endlicense
 */
#ifndef YASM_FILE_H
#define YASM_FILE_H

/** Split a UNIX pathname into head (directory) and tail (base filename)
 * portions.
 * \internal
 * \param path	pathname
 * \param tail	(returned) base filename
 * \return Length of head (directory).
 */
size_t yasm__splitpath_unix(const char *path, /*@out@*/ const char **tail);

/** Split a Windows pathname into head (directory) and tail (base filename)
 * portions.
 * \internal
 * \param path	pathname
 * \param tail	(returned) base filename
 * \return Length of head (directory).
 */
size_t yasm__splitpath_win(const char *path, /*@out@*/ const char **tail);

/** Split a pathname into head (directory) and tail (base filename) portions.
 * Unless otherwise defined, defaults to yasm__splitpath_unix().
 * \internal
 * \param path	pathname
 * \param tail	(returned) base filename
 * \return Length of head (directory).
 */
#ifndef yasm__splitpath
# if defined (_WIN32) || defined (WIN32) || defined (__MSDOS__) || \
 defined (__DJGPP__) || defined (__OS2__) || defined (__CYGWIN__) || \
 defined (__CYGWIN32__)
#  define yasm__splitpath(path, tail)	yasm__splitpath_win(path, tail)
# else
#  define yasm__splitpath(path, tail)	yasm__splitpath_unix(path, tail)
# endif
#endif

/** Convert a UNIX relative or absolute pathname into an absolute pathname.
 * \internal
 * \param path	pathname
 * \return Absolute version of path (newly allocated).
 */
/*@only@*/ char *yasm__abspath_unix(const char *path);

/** Convert a Windows relative or absolute pathname into an absolute pathname.
 * \internal
 * \param path	pathname
 * \return Absolute version of path (newly allocated).
 */
/*@only@*/ char *yasm__abspath_win(const char *path);

/** Convert a relative or absolute pathname into an absolute pathname.
 * Unless otherwise defined, defaults to yasm__abspath_unix().
 * \internal
 * \param path	pathname
 * \return Absolute version of path (newly allocated).
 */
#ifndef yasm__abspath
# if defined (_WIN32) || defined (WIN32) || defined (__MSDOS__) || \
 defined (__DJGPP__) || defined (__OS2__) || defined (__CYGWIN__) || \
 defined (__CYGWIN32__)
#  define yasm__abspath(path)	yasm__abspath_win(path)
# else
#  define yasm__abspath(path)	yasm__abspath_unix(path)
# endif
#endif

/** Build a UNIX pathname that is equivalent to accessing the "to" pathname
 * when you're in the directory containing "from".  Result is relative if both
 * from and to are relative.
 * \internal
 * \param from	from pathname
 * \param to	to pathname
 * \return Combined path (newly allocated).
 */
char *yasm__combpath_unix(const char *from, const char *to);

/** Build a Windows pathname that is equivalent to accessing the "to" pathname
 * when you're in the directory containing "from".  Result is relative if both
 * from and to are relative.
 * \internal
 * \param from	from pathname
 * \param to	to pathname
 * \return Combined path (newly allocated).
 */
char *yasm__combpath_win(const char *from, const char *to);

/** Build a pathname that is equivalent to accessing the "to" pathname
 * when you're in the directory containing "from".  Result is relative if both
 * from and to are relative.
 * Unless otherwise defined, defaults to yasm__combpath_unix().
 * \internal
 * \param from	from pathname
 * \param to	to pathname
 * \return Combined path (newly allocated).
 */
#ifndef yasm__combpath
# if defined (_WIN32) || defined (WIN32) || defined (__MSDOS__) || \
 defined (__DJGPP__) || defined (__OS2__) || defined (__CYGWIN__) || \
 defined (__CYGWIN32__)
#  define yasm__combpath(from, to)	yasm__combpath_win(from, to)
# else
#  define yasm__combpath(from, to)	yasm__combpath_unix(from, to)
# endif
#endif

/** Try to find and open an include file, searching through include paths.
 * First iname is looked for relative to the directory containing "from", then
 * it's looked for relative to each of the paths.
 *
 * All pathnames may be either absolute or relative; from, oname, and paths,
 * if relative, are relative from the current working directory.
 *
 * First match wins; the full pathname (newly allocated) to the opened file
 * is saved into oname, and the fopen'ed FILE * is returned.  If not found,
 * NULL is returned.
 *
 * \internal
 * \param iname	    file to include
 * \param from	    file doing the including
 * \param paths	    NULL-terminated array of paths to search (relative to from)
 *                  may be NULL if no extra paths
 * \param mode	    fopen mode string
 * \param oname	    full pathname of included file (may be relative)
 * \return fopen'ed include file, or NULL if not found.
 */
/*@null@*/ FILE *yasm__fopen_include(const char *iname, const char *from,
				     /*@null@*/ const char **paths,
				     const char *mode,
				     /*@out@*/ /*@only@*/ char **oname);

/** Write an 8-bit value to a buffer, incrementing buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	8-bit value
 */
#define YASM_WRITE_8(ptr, val)			\
	*((ptr)++) = (unsigned char)((val) & 0xFF)

/** Write a 16-bit value to a buffer in little endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_WRITE_16_L(ptr, val)		\
	do {					\
	    *((ptr)++) = (unsigned char)((val) & 0xFF);		\
	    *((ptr)++) = (unsigned char)(((val) >> 8) & 0xFF);	\
	} while (0)

/** Write a 32-bit value to a buffer in little endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_WRITE_32_L(ptr, val)		\
	do {					\
	    *((ptr)++) = (unsigned char)((val) & 0xFF);		\
	    *((ptr)++) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)++) = (unsigned char)(((val) >> 16) & 0xFF);	\
	    *((ptr)++) = (unsigned char)(((val) >> 24) & 0xFF);	\
	} while (0)

/** Write a 16-bit value to a buffer in big endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_WRITE_16_B(ptr, val)		\
	do {					\
	    *((ptr)++) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)++) = (unsigned char)((val) & 0xFF);		\
	} while (0)

/** Write a 32-bit value to a buffer in big endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_WRITE_32_B(ptr, val)		\
	do {					\
	    *((ptr)++) = (unsigned char)(((val) >> 24) & 0xFF);	\
	    *((ptr)++) = (unsigned char)(((val) >> 16) & 0xFF);	\
	    *((ptr)++) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)++) = (unsigned char)((val) & 0xFF);		\
	} while (0)


/** Write an 8-bit value to a buffer.  Does not increment buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	8-bit value
 */
#define YASM_SAVE_8(ptr, val)			\
	*(ptr) = (unsigned char)((val) & 0xFF)

/** Write a 16-bit value to a buffer in little endian.  Does not increment
 * buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_SAVE_16_L(ptr, val)		\
	do {					\
	    *(ptr) = (unsigned char)((val) & 0xFF);		\
	    *((ptr)+1) = (unsigned char)(((val) >> 8) & 0xFF);	\
	} while (0)

/** Write a 32-bit value to a buffer in little endian.  Does not increment
 * buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_SAVE_32_L(ptr, val)		\
	do {					\
	    *(ptr) = (unsigned char)((val) & 0xFF);		\
	    *((ptr)+1) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)+2) = (unsigned char)(((val) >> 16) & 0xFF);	\
	    *((ptr)+3) = (unsigned char)(((val) >> 24) & 0xFF);	\
	} while (0)

/** Write a 16-bit value to a buffer in big endian.  Does not increment buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_SAVE_16_B(ptr, val)		\
	do {					\
	    *(ptr) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)+1) = (unsigned char)((val) & 0xFF);		\
	} while (0)

/** Write a 32-bit value to a buffer in big endian.  Does not increment buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_SAVE_32_B(ptr, val)		\
	do {					\
	    *(ptr) = (unsigned char)(((val) >> 24) & 0xFF);	\
	    *((ptr)+1) = (unsigned char)(((val) >> 16) & 0xFF);	\
	    *((ptr)+2) = (unsigned char)(((val) >> 8) & 0xFF);	\
	    *((ptr)+3) = (unsigned char)((val) & 0xFF);		\
	} while (0)

/** Direct-to-file version of YASM_SAVE_16_L().
 * \note Using the macro multiple times with a single fwrite() call will
 *       probably be faster than calling this function many times.
 * \param val	16-bit value
 * \param f	file
 * \return 1 if the write was successful, 0 if not (just like fwrite()).
 */
size_t yasm_fwrite_16_l(unsigned short val, FILE *f);

/** Direct-to-file version of YASM_SAVE_32_L().
 * \note Using the macro multiple times with a single fwrite() call will
 *       probably be faster than calling this function many times.
 * \param val	32-bit value
 * \param f	file
 * \return 1 if the write was successful, 0 if not (just like fwrite()).
 */
size_t yasm_fwrite_32_l(unsigned long val, FILE *f);

/** Direct-to-file version of YASM_SAVE_16_B().
 * \note Using the macro multiple times with a single fwrite() call will
 *       probably be faster than calling this function many times.
 * \param val	16-bit value
 * \param f	file
 * \return 1 if the write was successful, 0 if not (just like fwrite()).
 */
size_t yasm_fwrite_16_b(unsigned short val, FILE *f);

/** Direct-to-file version of YASM_SAVE_32_B().
 * \note Using the macro multiple times with a single fwrite() call will
 *       probably be faster than calling this function many times.
 * \param val	32-bit value
 * \param f	file
 * \return 1 if the write was successful, 0 if not (just like fwrite()).
 */
size_t yasm_fwrite_32_b(unsigned long val, FILE *f);

/** Read an 8-bit value from a buffer, incrementing buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	8-bit value
 */
#define YASM_READ_8(val, ptr)			\
	(val) = *((ptr)++) & 0xFF

/** Read a 16-bit value from a buffer in little endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_READ_16_L(val, ptr)		\
	do {					\
	    (val) = *((ptr)++) & 0xFF;		\
	    (val) |= (*((ptr)++) & 0xFF) << 8;	\
	} while (0)

/** Read a 32-bit value from a buffer in little endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_READ_32_L(val, ptr)		\
	do {					\
	    (val) = *((ptr)++) & 0xFF;		\
	    (val) |= (*((ptr)++) & 0xFF) << 8;	\
	    (val) |= (*((ptr)++) & 0xFF) << 16;	\
	    (val) |= (*((ptr)++) & 0xFF) << 24;	\
	} while (0)

/** Read a 16-bit value from a buffer in big endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_READ_16_B(val, ptr)		\
	do {					\
	    (val) = (*((ptr)++) & 0xFF) << 8;	\
	    (val) |= *((ptr)++) & 0xFF;		\
	} while (0)

/** Read a 32-bit value from a buffer in big endian, incrementing buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_READ_32_B(val, ptr)		\
	do {					\
	    (val) = (*((ptr)++) & 0xFF) << 24;	\
	    (val) |= (*((ptr)++) & 0xFF) << 16;	\
	    (val) |= (*((ptr)++) & 0xFF) << 8;	\
	    (val) |= *((ptr)++) & 0xFF;		\
	} while (0)

/** Read an 8-bit value from a buffer.  Does not increment buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	8-bit value
 */
#define YASM_LOAD_8(val, ptr)			\
	(val) = *(ptr) & 0xFF

/** Read a 16-bit value from a buffer in little endian.  Does not increment
 * buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_LOAD_16_L(val, ptr)		\
	do {					\
	    (val) = *(ptr) & 0xFF;		\
	    (val) |= (*((ptr)+1) & 0xFF) << 8;	\
	} while (0)

/** Read a 32-bit value from a buffer in little endian.  Does not increment
 * buffer pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_LOAD_32_L(val, ptr)		\
	do {					\
	    (val) = (unsigned long)(*(ptr) & 0xFF);		    \
	    (val) |= (unsigned long)((*((ptr)+1) & 0xFF) << 8);	    \
	    (val) |= (unsigned long)((*((ptr)+2) & 0xFF) << 16);    \
	    (val) |= (unsigned long)((*((ptr)+3) & 0xFF) << 24);    \
	} while (0)

/** Read a 16-bit value from a buffer in big endian.  Does not increment buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	16-bit value
 */
#define YASM_LOAD_16_B(val, ptr)		\
	do {					\
	    (val) = (*(ptr) & 0xFF) << 8;	\
	    (val) |= *((ptr)+1) & 0xFF;		\
	} while (0)

/** Read a 32-bit value from a buffer in big endian.  Does not increment buffer
 * pointer.
 * \note Only works properly if ptr is an (unsigned char *).
 * \param ptr	buffer
 * \param val	32-bit value
 */
#define YASM_LOAD_32_B(val, ptr)		\
	do {					\
	    (val) = (unsigned long)((*(ptr) & 0xFF) << 24);	    \
	    (val) |= (unsigned long)((*((ptr)+1) & 0xFF) << 16);    \
	    (val) |= (unsigned long)((*((ptr)+2) & 0xFF) << 8);	    \
	    (val) |= (unsigned long)(*((ptr)+3) & 0xFF);	    \
	} while (0)

#endif
