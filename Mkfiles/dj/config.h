/* $Id$ */

#define yasm__splitpath(path, tail)	yasm__splitpath_win(path, tail)
#define yasm__abspath(path)		yasm__abspath_win(path)
#define yasm__combpath(from, to)	yasm__combpath_win(from, to)

/* */
/* #undef ENABLE_NLS */

/* Define if you have the `abort' function. */
#define HAVE_ABORT 1

/* */
/* #undef HAVE_CATGETS */

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
/* #undef HAVE_DCGETTEXT */

/* Define if you don't have `vprintf' but do have `_doprnt'. */
/* #undef HAVE_DOPRNT */

/* Define to 1 if you have the `getcwd' function. */
#define HAVE_GETCWD 1

/* */
/* #undef HAVE_GETTEXT */

/* Define if you have the GNU C Library */
/* #undef HAVE_GNU_C_LIBRARY */

/* Define if you have the iconv() function. */
/* #undef HAVE_ICONV */

/* Define if you have the <inttypes.h> header file. */
/* #undef HAVE_INTTYPES_H */

/* */
/* #undef HAVE_LC_MESSAGES */

/* Define to 1 if you have the <libgen.h> header file. */
/* #undef HAVE_LIBGEN_H */

/* Define if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define if you have the `memcpy' function. */
#define HAVE_MEMCPY 1

/* Define if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define if you have the `mergesort function. */
/* #undef HAVE_MERGESORT */

/* Define if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* */
#define HAVE_STPCPY 1

/* Define if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define if you have the `strcmpi' function. */
/* #undef HAVE_STRCMPI */

/* Define if you have the `stricmp' function. */
/* #undef HAVE_STRICMP */

/* Define if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define if you have the `strncasecmp' function. */
#define HAVE_STRNCASECMP 1

/* Define if you have the `strrchr' function. */
#define HAVE_STRRCHR 1

/* Define if you have the `strsep' function. */
#define HAVE_STRSEP 1

/* Define if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define if you have <sys/wait.h> that is POSIX.1 compatible. */
#define HAVE_SYS_WAIT_H 1

/* Define if you have the `toascii' function. */
#define HAVE_TOASCII 1

/* Define if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define if you have the vprintf function. */
#define HAVE_VPRINTF 1

/* Define to 1 if you have the `vsnprintf' function. */
/* #undef HAVE_VSNPRINTF */

/* Name of package */
#define PACKAGE "yasm"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "bug-yasm@tortall.net"

/* Define to build version of this package. */
#define PACKAGE_BUILD "1471"

/* Define to internal version of this package. */
#define PACKAGE_INTVER "0.5.0"

/* Define to the full name of this package. */
#define PACKAGE_NAME "yasm"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "yasm 0.5.0rc2"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "yasm"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.5.0rc2"

/* Define if the C compiler supports function prototypes. */
#define PROTOTYPES 1

/* The size of a `char', as computed by sizeof. */
/* #undef SIZEOF_CHAR */

/* The size of a `int', as computed by sizeof. */
/* #undef SIZEOF_INT */

/* The size of a `long', as computed by sizeof. */
/* #undef SIZEOF_LONG */

/* The size of a `short', as computed by sizeof. */
/* #undef SIZEOF_SHORT */

/* The size of a `void*', as computed by sizeof. */
/* #undef SIZEOF_VOIDP */

/* Define if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "0.5.x"

/* Define if using the dmalloc debugging malloc package */
/* #undef WITH_DMALLOC */

/* Define like PROTOTYPES; this can be used by system headers. */
#define __PROTOTYPES 1

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define as `__inline' if that's what the C compiler calls it, or to nothing
   if it is not supported. */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to `unsigned' if <sys/types.h> doesn't define. */
/* #undef size_t */
