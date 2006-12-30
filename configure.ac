# Process this file with autoconf to produce a configure script.
# $Id$

#
# autoconf setup
#
AC_PREREQ(2.53)
AC_INIT([yasm], [HEAD], [bug-yasm@tortall.net])
#AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE(yasm, [HEAD])
AM_MAINTAINER_MODE

AC_DEFINE(PACKAGE_INTVER, ["0.5.99"],
	  [Define to internal version of this package.])
AC_DEFINE(PACKAGE_BUILD, ["HEAD"], [Define to build version of this package.])

#
# autoconf command-line options
#
AC_ARG_ENABLE(debug,
AC_HELP_STRING([--enable-debug],
	       [Turn on debugging and compile time warnings]),
[case "${enableval}" in
  yes) debugging="yes" ;;
  no)  debugging="no" ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
esac])

AC_ARG_ENABLE(warnerror,
AC_HELP_STRING([--enable-warnerror],[Treat GCC warnings as errors]),
[case "${enableval}" in
  yes) warnerror="yes" ;;
  no)  warnerror="no" ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-warnerror]) ;;
esac])

AC_ARG_ENABLE(profiling,
AC_HELP_STRING([--enable-profiling],[Enable profiling (requires GCC)]),
[case "${enableval}" in
  yes) profiling="yes" ;;
  no)  profiling="no" ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-profiling]) ;;
esac])

AC_ARG_ENABLE(gcov,
AC_HELP_STRING([--enable-gcov],[Enable gcov code coverage (requires GCC)]),
[case "${enableval}" in
  yes) gcov="yes" ;;
  no)  gcov="no" ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-gcov]) ;;
esac])

AC_ARG_ENABLE(python,
AC_HELP_STRING([--enable-python],[Build Python bindings]),
[case "${enableval}" in
  yes) enable_python="yes" ;;
  no)  enable_python="no" ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-python]) ;;
esac], enable_python="auto")

#
# Checks for programs.
#
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_RANLIB

# REQUIRE a standard (ANSI/ISO) C compiler
if test "$ac_cv_prog_cc_stdc" = no; then
	AC_MSG_ERROR([A standard (ANSI/ISO C89) C compiler is required.])
fi

# Check for xmlto (for rendering manpages, needed only for development)
AC_CHECK_PROGS([XMLTO], [$XMLTO xmlto], [:])
if test "$XMLTO" = ":"; then
  AC_MSG_WARN([xmlto not found, manpages will not be rebuilt.])
fi
AM_CONDITIONAL(BUILD_MAN, test "$XMLTO" != ":")

# Check for compiler output filename suffixes.
AC_OBJEXT
AC_EXEEXT

#
# Checks for libraries.
#
AM_WITH_DMALLOC

#
# Checks for header files.
#
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([limits.h sys/param.h libgen.h unistd.h])

# REQUIRE standard C headers
if test "$ac_cv_header_stdc" != yes; then
	AC_MSG_ERROR([Standard (ANSI/ISO C89) header files are required.])
fi

#
# Checks for typedefs, structures, and compiler characteristics.
#
AC_C_CONST
AC_C_INLINE
AC_C_PROTOTYPES
AC_TYPE_SIZE_T
AX_CREATE_STDINT_H

#
# Checks for library functions.
#
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([abort memcpy memmove strrchr toascii vsnprintf])
AC_CHECK_FUNCS([strsep mergesort getcwd])
# Look for the case-insensitive comparison functions
AC_CHECK_FUNCS([strcasecmp strncasecmp stricmp strcmpi])

#
# Check for gettext() and other i18n/l10n things.
#
ALL_LINGUAS=""
AM_GNU_GETTEXT([external])
# autoheader templates for AM_GNU_GETTEXT checks.
AH_TEMPLATE([ENABLE_NLS], [])
AH_TEMPLATE([HAVE_CATGETS], [])
AH_TEMPLATE([HAVE_GETTEXT], [])
AH_TEMPLATE([HAVE_LC_MESSAGES], [])
AH_TEMPLATE([HAVE_STPCPY], [])

# Check for GNU C Library
AH_TEMPLATE([HAVE_GNU_C_LIBRARY], [Define if you have the GNU C Library])
AC_CACHE_CHECK([for GNU C Library], yasm_cv_header_gnulib,
	AC_EGREP_CPP(gnulib,
	[#include <features.h>
	#ifdef __GNU_LIBRARY__
	  gnulib
	#endif
	], yasm_cv_header_gnulib=yes, yasm_cv_header_gnulib=no))
if test "$yasm_cv_header_gnulib" = yes; then
	AC_DEFINE([HAVE_GNU_C_LIBRARY])
fi

# Force x86 architecture only for now.
ARCH=x86
AC_SUBST([ARCH])
AC_SUBST([GCC])

# Require things for --enable-maintainer-mode option.
if test "$USE_MAINTAINER_MODE" = "yes"; then
  # Enable debugging
  if test "$debugging" != "no"; then
    debugging=yes
  fi

  # Enable more warnings
  if test "$GCC" = "yes"; then
    MORE_CFLAGS="$MORE_CFLAGS -W"
    MORE_CFLAGS="$MORE_CFLAGS -Waggregate-return"
    MORE_CFLAGS="$MORE_CFLAGS -Wbad-function-cast"
    MORE_CFLAGS="$MORE_CFLAGS -Wcast-align"
    MORE_CFLAGS="$MORE_CFLAGS -Wcast-qual"
    MORE_CFLAGS="$MORE_CFLAGS -Wchar-subscripts"
    MORE_CFLAGS="$MORE_CFLAGS -Wconversion"
#    MORE_CFLAGS="$MORE_CFLAGS -Wdeclaration-after-statement"
#    MORE_CFLAGS="$MORE_CFLAGS -Wendif-labels"
    MORE_CFLAGS="$MORE_CFLAGS -Winline"
    MORE_CFLAGS="$MORE_CFLAGS -Wmissing-declarations"
    MORE_CFLAGS="$MORE_CFLAGS -Wmissing-prototypes"
    MORE_CFLAGS="$MORE_CFLAGS -Wnested-externs"
    MORE_CFLAGS="$MORE_CFLAGS -Wpointer-arith"
    MORE_CFLAGS="$MORE_CFLAGS -Wreturn-type"
    MORE_CFLAGS="$MORE_CFLAGS -Wshadow"
    MORE_CFLAGS="$MORE_CFLAGS -Wsign-compare"
    MORE_CFLAGS="$MORE_CFLAGS -Wstrict-prototypes"
    MORE_CFLAGS="$MORE_CFLAGS -Wswitch"
    MORE_CFLAGS="$MORE_CFLAGS -Wwrite-strings"
    MORE_CFLAGS="$MORE_CFLAGS -Wno-undef"
    MORE_CFLAGS="$MORE_CFLAGS -Wno-unused"
#    MORE_CFLAGS="$MORE_CFLAGS -Wno-unused-parameter"
  fi
fi

#
# Add some more CFLAGS for various options.
#

if test "$debugging" = "no" ; then
  changequote(,)
  CFLAGS="`echo $CFLAGS' ' | sed -e 's/-g[0-9] //g' | sed -e 's/-g//g'`"
  changequote([,])
fi

# Turn warnings into errors
if test "$warnerror" = "yes"; then
  if test "$GCC" = "yes"; then
    MORE_CFLAGS="$MORE_CFLAGS -Werror"
  fi
fi

# Enable output of profiling information
if test "$profiling" = "yes"; then
  if test "$GCC" = "yes"; then
    MORE_CFLAGS="$MORE_CFLAGS -pg"
  fi
fi

# Enable output of gcov information
if test "$gcov" = "yes"; then
  if test "$GCC" = "yes"; then
    MORE_CFLAGS="$MORE_CFLAGS -fprofile-arcs -ftest-coverage"
  fi
fi

# If we're using GCC, then we can turn on -ansi -pedantic -Wall too.
if test "$GCC" = yes; then
  MORE_CFLAGS="-ansi -pedantic -Wall $MORE_CFLAGS"
fi
AC_SUBST(MORE_CFLAGS)

#
# Cross-building
#
if test "$cross_compiling" = "yes"; then
    AC_MSG_CHECKING([cc for build])
    CC_FOR_BUILD="${CC_FOR_BUILD-PATH=/usr/bin:$PATH cc}"
    AC_MSG_RESULT($CC_FOR_BUILD)
    CCLD_FOR_BUILD="$CC_FOR_BUILD"
else
    CC_FOR_BUILD="${CC_FOR_BUILD-$CC}"
    CCLD_FOR_BUILD="${CCLD_FOR_BUILD-$CC}"
fi

AC_ARG_VAR(CC_FOR_BUILD,[build system C compiler])
AC_ARG_VAR(CCLD_FOR_BUILD,[build system C linker frontend])
AC_SUBST(CC_FOR_BUILD)
AC_SUBST(CCLD_FOR_BUILD)

# libtool erroneously calls CC_FOR_BUILD HOST_CC;
# --host is the platform that the package is compiled for.
HOST_CC="$CC_FOR_BUILD"
AC_SUBST(HOST_CC)

# Detect if we can build Python bindings
# (needs Python, Python headers, and Pyrex)
if test x$enable_python = xno; then
    have_python=no
else
    AC_MSG_NOTICE([Checking to see if we can build Python bindings])
    have_python=no
    AM_PATH_PYTHON(2.4,[],[AC_MSG_WARN([Python not found])])

    if test -z "$PYTHON" || test "$PYTHON" = : ; then
        have_python=no
    else
        AC_MSG_CHECKING([for Pyrex >= 0.9.3])
        PYREX_CHECK_VERSION(0.9.3, [AC_MSG_RESULT(yes)
                                    have_pyrex=yes],
                                   [AC_MSG_RESULT(no)
                                    have_pyrex=no])

        AM_CHECK_PYTHON_HEADERS(have_python_headers=yes,have_python_headers=no)

	if test x$have_pyrex = xyes -a x$have_python_headers = xyes ; then
	    have_python=yes
        fi
    fi

    if test x$have_python = xno ; then
        if test x$enable_python = xyes ; then
            AC_MSG_ERROR([Building Python explicitly requested, but can't build Python bindings because either Pyrex, Python headers or a suitable Python version was not found])
        else
            AC_MSG_WARN([Couldn't find either Pyrex, the Python headers or a suitable version of Python, not building Python bindings])
        fi
    fi               
fi

AM_CONDITIONAL(HAVE_PYTHON, test x$have_python = xyes)

AC_CONFIG_FILES([Makefile
	po/Makefile.in
])
AC_OUTPUT
