# $IdPath$

CFLAGS = @ANSI_CFLAGS@

if CHECK
#TESTS = \

#noinst_PROGRAMS = \
else
#TESTS =
#noinst_PROGRAMS =
endif

INCLUDES= -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_srcdir)/check
LDADD = \
	$(top_builddir)/check/libcheck.a			\
	$(top_builddir)/src/parsers/nasm/libparser.a		\
	$(top_builddir)/src/preprocs/raw/libpreproc.a		\
	$(top_builddir)/src/optimizers/dbg/liboptimizer.a	\
	$(top_builddir)/src/objfmts/dbg/libobjfmt.a		\
	$(top_builddir)/src/libyasm.a				\
	$(INTLLIBS)

