# $IdPath$

if CHECK
noinst_LIBRARIES=libcheck.a
TESTS =
else
noinst_LIBRARIES=
TESTS = nocheck_error
endif

libcheck_a_SOURCES = \
	check.c		\
	check_run.c	\
	check.h		\
	check_impl.h	\
	check_msg.c	\
	check_msg.h	\
	check_log.c	\
	check_log.h	\
	check_print.c	\
	check_print.h	\
	error.c		\
	error.h		\
	list.c		\
	list.h

nocheck_error:
	@echo "Run configure with --enable-check before running make check"
	@exit 1

if DEV
CFLAGS = -ansi -pedantic -Wall -g
endif
