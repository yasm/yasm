AC_INIT(src/main.c)
AM_CONFIG_HEADER(include/config.h)

AM_INIT_AUTOMAKE(yasm, 0.0.1)

AC_PATH_PROGS(PERL, perl perl5.004 perl5.003 perl5.002 perl5.001 perl5)

AM_PROG_CC_STDC

AM_PROG_LEX
AC_PROG_YACC
AC_PROG_INSTALL

AC_HEADER_STDC

AC_C_CONST
AC_TYPE_SIZE_T

AC_FUNC_VPRINTF
AC_CHECK_FUNCS(memcpy toascii)
AC_REPLACE_FUNCS(strdup strtoul)

AC_OUTPUT(Makefile src/Makefile	include/Makefile)
