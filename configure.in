# $IdPath$

PERL_VERSION=5.004

AC_INIT(src/main.c)
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE(yasm, 0.0.1)

AC_ARG_ENABLE(dev,
[  --enable-dev    Enable full development build capability],
[case "${enableval}" in
  yes) dev=true ;;
  no)  dev=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-dev) ;;
esac],[dev=false])
AM_CONDITIONAL(DEV, test x$dev = xtrue)

AC_ARG_ENABLE(check,
[  --disable-check Disable building of test suite and make check],
[case "${enableval}" in
  yes) check=true ;;
  no)  check=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-check) ;;
esac],[check=true])
AM_CONDITIONAL(CHECK, test x$check = xtrue)

AM_PROG_CC_STDC

ALL_LINGUAS=""
AM_GNU_GETTEXT

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

AC_PROG_INSTALL
AC_PROG_RANLIB

AC_HEADER_STDC

AC_C_CONST
AC_TYPE_SIZE_T

AC_FUNC_VPRINTF
AC_CHECK_FUNCS(memcpy toascii abort strcasecmp stricmp strcmpi)
AC_REPLACE_FUNCS(strdup strtoul)

AC_CHECK_HEADERS(limits.h sys/queue.h sys/cdefs.h)

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
	tests/Makefile
)
