AC_INIT(src/main.c)
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE(yasm, 0.0.1)

AC_ARG_ENABLE(dev,
[  --enable-dev    Use standard development FLAGS settings],
[case "${enableval}" in
  yes) dev=true ;;
  no)  dev=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-dev) ;;
esac],[dev=false])
AM_CONDITIONAL(DEV, test x$dev = xtrue)

AC_PATH_PROGS(PERL, perl perl5.004 perl5.003 perl5.002 perl5.001 perl5)

AM_PROG_CC_STDC

AM_PROG_LEX
AC_PROG_YACC
AC_PROG_INSTALL
AC_PROG_RANLIB

AC_HEADER_STDC

AC_C_CONST
AC_TYPE_SIZE_T

AC_FUNC_VPRINTF
AC_CHECK_FUNCS(memcpy toascii)
AC_REPLACE_FUNCS(strdup strtoul)

AC_CHECK_HEADERS(sys/queue.h)

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
esac

AC_OUTPUT(Makefile src/Makefile src/parsers/Makefile src/parsers/nasm/Makefile src/preprocs/Makefile src/preprocs/raw/Makefile src/outfmts/Makefile src/outfmts/dbg/Makefile)
