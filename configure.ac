# Process this file with autoconf to produce a configure script.
# $IdPath$

# Minimum required perl version for development
PERL_VERSION=5.004

#
# autoconf setup
#
AC_PREREQ(2.50)
AC_INIT([yasm], `date "+%Y%m%d"`, [bug-yasm@tortall.net])
#AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE(yasm, `date "+%Y%m%d"`)
AM_MAINTAINER_MODE

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

#
# Checks for programs.
#
AC_PROG_YACC
# check for Bison in particular
if test "$YACC" != "bison -y"; then
    	AC_MSG_WARN([You will need bison if you want to regenerate parsers.])
else
    	AC_MSG_CHECKING([bison version])
	set `bison --version | sed -e 's/^@<:@A-Za-z() @:>@*//' -e 's/\./ /'`
	if test "$1" = "1" -a "$2" -lt "25"; then
	    	AC_MSG_WARN([You will need bison 1.25 or better if you want to regenerate parsers (found $1.$2).])
	fi
	AC_MSG_RESULT([$1.$2 (ok)])
fi
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LN_S

# Libtool
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_LIBLTDL_CONVENIENCE
AC_PROG_LIBTOOL
AC_SUBST([LIBTOOL_DEPS])

# REQUIRE a standard (ANSI/ISO) C compiler
if test "$ac_cv_prog_cc_stdc" = no; then
	AC_MSG_ERROR([A standard (ANSI/ISO C89) C compiler is required.])
fi

# Check for Perl (for gen_instr.pl and the like, needed only for development)
AC_PATH_PROGS(PERL, $PERL perl5 perl)
# Check for groff (for rendering manpages, needed only for development)
AC_PATH_PROGS(GROFF, $GROFF groff)

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
AC_CHECK_HEADERS([alloca.h limits.h sys/cdefs.h sys/ipc.h sys/msg.h sys/param.h sys/types.h sys/queue.h unistd.h])

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
# PID_T is used by the test suite (not required).
AC_TYPE_PID_T
AC_TYPE_SIZE_T

#
# Checks for library functions.
#
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([abort basename memcpy memmove strrchr toascii vsnprintf])
AC_CHECK_FUNCS([strsep mergesort])
# Look for the case-insensitive comparison functions
AC_CHECK_FUNCS([strcasecmp strncasecmp stricmp strcmpi])
# Check for stuff wanted by the test suite.  None of this is required.
AC_CHECK_FUNCS([msgctl msgget msgrcv msgsnd strerror snprintf wait])
AC_LIB_LTDL

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

# Require things for --enable-maintainer-mode option.
if test "$USE_MAINTAINER_MODE" = "yes"; then
  # Enable debugging
  if test "$debugging" != "no"; then
    debugging=yes
  fi

  # Enable more warnings
  if test "$GCC" = "yes"; then
    MORE_CFLAGS="$MORE_CFLAGS -Waggregate-return -Wbad-function-cast -Wcast-align -Wcast-qual -Wconversion -Winline -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wshadow -Wsign-compare -Wstrict-prototypes -Wwrite-strings"
  fi

  # Require Perl
  if test -z "$PERL" || test "$PERL" = ":"; then
    AC_MSG_ERROR([Perl not found in \$PATH])
  fi

  # Require Perl >= PERL_VERSION
  AC_MSG_CHECKING([for minimum required perl version >= $PERL_VERSION])
  _perl_version=`PERL_VERSION=$PERL_VERSION $PERL -e 'print "$]"; if ($] >= $ENV{PERL_VERSION}) { exit(0); } else { exit(1); }' 2>&5`
  _perl_res=$?
  AC_MSG_RESULT([$_perl_version])

  if test "$_perl_res" != 0; then
    AC_MSG_ERROR([Perl $PERL_VERSION or higher is required.])
  fi

  # Require groff
  if test -z "$GROFF" || test "$GROFF" = ":"; then
    AC_MSG_ERROR([groff not found in \$PATH])
  fi
fi

#
# Add some more CFLAGS for various options.
#

# "Check" tests can use fork/wait/msg* if ALL are available.
AH_TEMPLATE([USE_FORKWAITMSG], [Combined test for fork/wait/msg*])
if ${check}; then
  if test "$ac_cv_func_fork" = yes &&
     test "$ac_cv_func_wait" = yes &&
     test "$ac_cv_func_msgctl" = yes &&
     test "$ac_cv_func_msgget" = yes &&
     test "$ac_cv_func_msgrcv" = yes &&
     test "$ac_cv_func_msgsnd" = yes; then
    AC_DEFINE([USE_FORKWAITMSG])
    AC_DEFINE([_GNU_SOURCE], 1,
	      [Make sure we see all GNU extensions.])
    AC_DEFINE([_SVID_SOURCE], 1,
	      [Make sure we see all SVID extensions.])
  fi
fi

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

# Substitutions for libltdl
AC_SUBST(INCLTDL)
AC_SUBST(LIBLTDL)

AC_CONFIG_FILES([Makefile
	libltdl/Makefile
	m4/Makefile
	po/Makefile.in
])
AC_OUTPUT
