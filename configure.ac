# $IdPath$

PERL_VERSION=5.004

AC_INIT(src/main.c)
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE(yasm, 0.0.1)

AC_ARG_ENABLE(dev,
[  --enable-dev        Enable full development build capability],
[case "${enableval}" in
  yes) dev=true ;;
  no)  dev=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-dev) ;;
esac],[dev=false])
AM_CONDITIONAL(DEV, test x$dev = xtrue)

AC_ARG_ENABLE(morewarn,
[  --enable-morewarn   Enable lots of extra GCC warnings],
[case "${enableval}" in
  yes) morewarn=true ;;
  no)  morewarn=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-morewarn) ;;
esac],[morewarn=false])

AC_ARG_ENABLE(warnerror,
[  --enable-warnerror  Treat GCC warnings as errors],
[case "${enableval}" in
  yes) warnerror=true ;;
  no)  warnerror=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-warnerror) ;;
esac],[warnerror=false])

AC_ARG_ENABLE(profiling,
[  --enable-profiling  Enable profiling (requires GCC)],
[case "${enableval}" in
  yes) profiling=true ;;
  no)  profiling=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-profiling) ;;
esac],[profiling=false])

AC_ARG_ENABLE(dmalloc,
[  --enable-dmalloc    Enable debug malloc (requires dmalloc library)],
[case "${enableval}" in
  yes) dmalloc=true ;;
  no)  dmalloc=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-dmalloc) ;;
esac],[dmalloc=false])

AC_ARG_ENABLE(check,
[  --disable-check     Disable building of test suite and make check],
[case "${enableval}" in
  yes) check=true ;;
  no)  check=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-check) ;;
esac],[check=true])
AM_CONDITIONAL(CHECK, test x$check = xtrue)

AM_PROG_CC_STDC

ALL_LINGUAS=""
AM_GNU_GETTEXT

AC_PROG_INSTALL
AC_PROG_RANLIB

AC_HEADER_STDC

AC_C_CONST
AC_TYPE_SIZE_T

AC_FUNC_VPRINTF
AC_CHECK_FUNCS(memcpy toascii abort)
AC_CHECK_FUNCS(strcasecmp stricmp strcmpi, break)
AC_REPLACE_FUNCS(strsep mergesort)

# Check for GNU C Library
AC_CACHE_CHECK([for GNU C Library], yasm_cv_header_gnulib,
	AC_EGREP_CPP(gnulib,
	[#include <features.h>
	#ifdef __GNU_LIBRARY__
	  gnulib
	#endif
	], yasm_cv_header_gnulib=yes, yasm_cv_header_gnulib=no))
if test "$yasm_cv_header_gnulib" = yes; then
	AC_DEFINE(HAVE_GNU_C_LIBRARY)
fi

AC_CHECK_HEADERS(limits.h sys/queue.h sys/cdefs.h)

# Needed tools for building lexers and parsers from original source.
# Only needed for development: generated .c's are included with distributions.
if ${dev}; then
	AM_PROG_LEX
	AC_PROG_YACC

	# Check for Perl (gen_instr.pl and the like)
	AC_PATH_PROGS(PERL, $PERL perl5 perl)
	if test -z "$PERL" || test "$PERL" = ":"; then
		AC_MSG_ERROR([perl not found in \$PATH])
	fi

	AC_MSG_CHECKING([for minimum required perl version >= $PERL_VERSION])
	_perl_version=`PERL_VERSION=$PERL_VERSION $PERL -e 'print "$]"; if ($] >= $ENV{PERL_VERSION}) { exit(0); } else { exit(1); }' 2>&5`
	_perl_res=$?
	AC_MSG_RESULT([$_perl_version])

	if test "$_perl_res" != 0; then
		AC_MSG_ERROR([Perl $PERL_VERSION or higher is required.])
	fi

	# Check for groff (for rendering manpages)
	AC_PATH_PROGS(GROFF, $GROFF groff)
	if test -z "$PERL" || test "$PERL" = ":"; then
		AC_MSG_ERROR([groff not found in \$PATH])
	fi
fi

# Check for stuff wanted by the test suite.  None of this is required.
AC_CHECK_FUNCS(fork wait msgctl msgget msgrcv msgsnd strerror snprintf)
AC_TYPE_PID_T
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(sys/types.h sys/ipc.h sys/msg.h unistd.h)

CHECK_CFLAGS=
if ${check}; then
	if test "$ac_cv_func_fork" = yes &&
	   test "$ac_cv_func_wait" = yes &&
	   test "$ac_cv_func_msgctl" = yes &&
	   test "$ac_cv_func_msgget" = yes &&
	   test "$ac_cv_func_msgrcv" = yes &&
	   test "$ac_cv_func_msgsnd" = yes; then
		AC_DEFINE(USE_FORKWAITMSG)
		CHECK_CFLAGS="-D_SVID_SOURCE -D_GNU_SOURCE"
	fi
fi
AC_SUBST(CHECK_CFLAGS)

if ${dev}; then
	DEVFLAGS=" -g"
fi

if ${morewarn}; then
	MOREWARNFLAGS="-Wbad-function-cast -Wcast-align -Wcast-qual -Wchar-subscripts -Winline -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wshadow -Wstrict-prototypes -Wwrite-strings"
fi

if ${warnerror}; then
	WARNERRORFLAGS="-Werror"
fi

if ${profiling}; then
	PROFILINGFLAGS="-pg"
fi

AC_CHECK_HEADERS(dmalloc.h)
AC_CHECK_LIB(dmalloc, main)
if ${dmalloc}; then
	if test "$ac_cv_header_dmalloc_h" != yes ||
	   test "$ac_cv_lib_dmalloc_main" != yes; then
		AC_MSG_ERROR([dmalloc required for --enable-dmalloc.])
	else
		DMALLOCFLAGS="-DDMALLOC"
	fi
else
	if ${morewarn}; then
		DMALLOCFLAGS="-Wredundant-decls"
	fi
fi

if test "$GCC" = yes; then
	ANSI_CFLAGS="-ansi -pedantic -Wall $MOREWARNFLAGS $WARNERRORFLAGS $DEVFLAGS $PROFILINGFLAGS $DMALLOCFLAGS"
else
	ANSI_CFLAGS="$DEVFLAGS"
fi
AC_SUBST(ANSI_CFLAGS)

# Check for some target-specific stuff
case "$host" in
*-*-sunos4*)
	AC_DEFINE(HAVE_BOGUS_SYS_QUEUE_H)
	;;
*-sni-sysv*)
	AC_DEFINE(HAVE_BOGUS_SYS_QUEUE_H)
	;;
*-*-sco3.2v4*)
	AC_DEFINE(HAVE_BOGUS_SYS_QUEUE_H)
	;;
*-*-sco3.2v5*)
	AC_DEFINE(HAVE_BOGUS_SYS_QUEUE_H)
	;;
*-*-linux*)
	AC_DEFINE(HAVE_BOGUS_SYS_QUEUE_H)
	;;
esac

AC_OUTPUT(Makefile
	check/Makefile
	intl/Makefile
	po/Makefile.in
	src/Makefile
	src/parsers/Makefile
	src/parsers/nasm/Makefile
	src/preprocs/Makefile
	src/preprocs/raw/Makefile
	src/optimizers/Makefile
	src/optimizers/dbg/Makefile
	src/objfmts/Makefile
	src/objfmts/dbg/Makefile
	src/tests/Makefile
	doc/Makefile
	doc/user/Makefile
	doc/programmer/Makefile
	doc/programmer/queue/Makefile
	doc/programmer/bitvect/Makefile
	tests/Makefile
)
