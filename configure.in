# Process this file with autoconf to produce a configure script.
# $IdPath$

# Minimum required perl version for development
PERL_VERSION=5.004

#
# autoconf setup
#
AC_PREREQ(2.50)
AC_INIT([yasm], [0.0.1], [bug-yasm@tortall.net])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE(yasm, 0.0.1)

#
# autoconf command-line options
#
AC_ARG_ENABLE(dev,
[  --enable-dev        Enable full development build capability],
[case "${enableval}" in
  yes) dev=true ;;
  no)  dev=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-dev]) ;;
esac],[dev=false])
AM_CONDITIONAL(DEV, test x$dev = xtrue)

AC_ARG_ENABLE(morewarn,
[  --enable-morewarn   Enable lots of extra GCC warnings],
[case "${enableval}" in
  yes) morewarn=true ;;
  no)  morewarn=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-morewarn]) ;;
esac],[morewarn=false])

AC_ARG_ENABLE(warnerror,
[  --enable-warnerror  Treat GCC warnings as errors],
[case "${enableval}" in
  yes) warnerror=true ;;
  no)  warnerror=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-warnerror]) ;;
esac],[warnerror=false])

AC_ARG_ENABLE(profiling,
[  --enable-profiling  Enable profiling (requires GCC)],
[case "${enableval}" in
  yes) profiling=true ;;
  no)  profiling=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-profiling]) ;;
esac],[profiling=false])

AC_ARG_ENABLE(dmalloc,
[  --enable-dmalloc    Enable debug malloc (requires dmalloc library)],
[case "${enableval}" in
  yes) dmalloc=true ;;
  no)  dmalloc=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-dmalloc]) ;;
esac],[dmalloc=false])

#
# Checks for programs.
#
# lex and yacc are only required for development.
AC_PROG_YACC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AM_PROG_LEX
AC_PROG_RANLIB

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

# dmalloc library (optional)
if ${dmalloc}; then
	AC_CHECK_LIB(dmalloc, dmalloc_shutdown)
fi

#
# Checks for header files.
#
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([alloca.h limits.h sys/cdefs.h sys/ipc.h sys/msg.h sys/param.h sys/types.h sys/queue.h unistd.h])

# dmalloc library (optional)
if ${dmalloc}; then
	AC_CHECK_HEADERS([dmalloc.h])
fi

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
AC_CHECK_FUNCS([abort memcpy memmove strrchr toascii])
# Look for the case-insensitive comparison functions
AC_CHECK_FUNCS([strcasecmp strncasecmp stricmp strcmpi])
# Check for stuff wanted by the test suite.  None of this is required.
AC_CHECK_FUNCS([msgctl msgget msgrcv msgsnd strerror snprintf wait])
AC_REPLACE_FUNCS([strsep mergesort])

#
# Check for gettext() and other i18n/l10n things.
#
ALL_LINGUAS=""
AM_GNU_GETTEXT
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

# Require things for --enable-dmalloc option.
DMALLOCFLAGS=
if ${dmalloc}; then
	if test "$ac_cv_header_dmalloc_h" != yes ||
	   test "$ac_cv_lib_dmalloc_dmalloc_shutdown" != yes; then
		AC_MSG_ERROR([dmalloc required for --enable-dmalloc.])
	else
	    	AC_DEFINE([DMALLOC], 1, [Enable dmalloc library debugging.])
	fi
else
	if ${morewarn}; then
		DMALLOCFLAGS="-Wredundant-decls"
	fi
fi

# Require things for --enable-dev option.
if ${dev}; then
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

# Enable debugging if --enable-dev, otherwise optimize
if ${dev}; then
	DEVFLAGS=" -g"
else
	DEVFLAGS=" -O"
fi

# More warnings to help write clean code
if ${morewarn}; then
	MOREWARNFLAGS="-Wbad-function-cast -Wcast-align -Wcast-qual -Wchar-subscripts -Winline -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings"
fi

# Turn warnings into errors
if ${warnerror}; then
	WARNERRORFLAGS="-Werror"
fi

# Enable output of profiling information
if ${profiling}; then
	PROFILINGFLAGS="-pg"
fi

# If we're using GCC, then we can enable the above CFLAGS as well as turn on
# -ansi -pedantic -Wall.
if test "$GCC" = yes; then
	ANSI_CFLAGS="-ansi -pedantic -Wall $MOREWARNFLAGS $WARNERRORFLAGS $DEVFLAGS $PROFILINGFLAGS $DMALLOCFLAGS"
else
	ANSI_CFLAGS=""
fi
AC_SUBST(ANSI_CFLAGS)

# Check for target-specific bogus sys/queue.h
AH_TEMPLATE([HAVE_BOGUS_SYS_QUEUE_H],
	    [Workaround for bad <sys/queue.h> implementations.])
case "$host" in
*-*-sunos4*)
	AC_DEFINE([HAVE_BOGUS_SYS_QUEUE_H])
	;;
*-sni-sysv*)
	AC_DEFINE([HAVE_BOGUS_SYS_QUEUE_H])
	;;
*-*-sco3.2v4*)
	AC_DEFINE([HAVE_BOGUS_SYS_QUEUE_H])
	;;
*-*-sco3.2v5*)
	AC_DEFINE([HAVE_BOGUS_SYS_QUEUE_H])
	;;
*-*-linux*)
	AC_DEFINE([HAVE_BOGUS_SYS_QUEUE_H])
	;;
esac

AC_CONFIG_FILES([Makefile
	intl/Makefile
	po/Makefile.in
])
AC_OUTPUT
