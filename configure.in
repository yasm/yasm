AC_INIT(src/main.c)
AM_CONFIG_HEADER(include/config.h)

AM_INIT_AUTOMAKE(yasm, 0.0.1)

AC_PATH_PROGS(PERL, perl perl5.004 perl5.003 perl5.002 perl5.001 perl5)

AC_PROG_CC
AM_PROG_LEX
AC_PROG_YACC
AC_PROG_INSTALL

AC_HEADER_STDC
AC_CHECK_FUNCS(memcpy)
AC_REPLACE_FUNCS(strdup)

AC_OUTPUT(Makefile src/Makefile	include/Makefile)
