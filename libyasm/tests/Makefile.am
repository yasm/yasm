# $IdPath$

CFLAGS = @ANSI_CFLAGS@

if CHECK
TESTS = \
	bytecode_test	\
	floatnum_test

noinst_PROGRAMS = \
	bytecode_test	\
	floatnum_test
else
TESTS =
noinst_PROGRAMS =
endif

bytecode_test_SOURCES = \
	bytecode_test.c

floatnum_test_SOURCES = \
	floatnum_test.c

INCLUDES= -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_srcdir)/check \
	-I$(top_builddir)/intl
LDADD = \
	$(top_builddir)/check/libcheck.a			\
	$(top_builddir)/src/parsers/nasm/libparser.a		\
	$(top_builddir)/src/preprocs/raw/libpreproc.a		\
	$(top_builddir)/src/optimizers/dbg/liboptimizer.a	\
	$(top_builddir)/src/objfmts/dbg/libobjfmt.a		\
	$(top_builddir)/src/libyasm.a				\
	$(INTLLIBS)

