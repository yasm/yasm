# $IdPath$

CFLAGS = @ANSI_CFLAGS@

if CHECK
TESTS = \
	bitvect_test	\
	bytecode_test	\
	floatnum_test	\
	memexpr_test

noinst_PROGRAMS = \
	bitvect_test	\
	bytecode_test	\
	floatnum_test	\
	memexpr_test

else
TESTS =
noinst_PROGRAMS =
endif

LDADD = \
	$(top_builddir)/src/libyasm.a				\
	$(top_builddir)/check/libcheck.a			\
	$(top_builddir)/src/parsers/nasm/libparser.a		\
	$(top_builddir)/src/preprocs/raw/libpreproc.a		\
	$(top_builddir)/src/optimizers/dbg/liboptimizer.a	\
	$(top_builddir)/src/objfmts/dbg/libobjfmt.a		\
	$(top_builddir)/src/arch/@ARCH@/libarch.a		\
	$(top_builddir)/src/libyasm.a				\
	$(INTLLIBS)

bitvect_test_SOURCES = \
	bitvect_test.c

bitvect_test_LDADD = \
	$(top_builddir)/src/errwarn.o	\
	$(LDADD)

bytecode_test_SOURCES = \
	bytecode_test.c

bytecode_test_LDADD = \
	$(top_builddir)/src/errwarn.o	\
	$(LDADD)

floatnum_test_SOURCES = \
	floatnum_test.c

floatnum_test_LDADD = \
	$(top_builddir)/src/errwarn.o	\
	$(LDADD)

memexpr_test_SOURCES = \
	memexpr_test.c

INCLUDES= -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_srcdir)/check \
	-I$(top_srcdir)/src/arch/@ARCH@ -I$(top_builddir)/intl

