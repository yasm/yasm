# $IdPath$

pkglib_LTLIBRARIES += preproc_nasm.la

preproc_nasm_la_SOURCES = \
	modules/preprocs/nasm/nasm-preproc.c \
	modules/preprocs/nasm/nasm-pp.h \
	modules/preprocs/nasm/nasm-pp.c \
	modules/preprocs/nasm/nasm.h \
	modules/preprocs/nasm/nasmlib.h \
	modules/preprocs/nasm/nasmlib.c \
	modules/preprocs/nasm/nasm-eval.h \
	modules/preprocs/nasm/nasm-eval.c
preproc_nasm_la_LDFLAGS = -module -avoid-version -no-undefined
preproc_nasm_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen preproc_nasm.la

$(top_modulesdir)/src/preprocs/nasm/nasm-pp.c: nasm-macros.c

nasm-macros.c: $(top_srcdir)/modules/preprocs/nasm/macros.pl $(top_srcdir)/modules/preprocs/nasm/standard.mac
	$(PERL) $(top_srcdir)/modules/preprocs/nasm/macros.pl $(top_srcdir)/modules/preprocs/nasm/standard.mac

BUILT_SOURCES += nasm-macros.c
CLEANFILES += nasm-macros.c

EXTRA_DIST += modules/preprocs/nasm/macros.pl \
	modules/preprocs/nasm/standard.mac \
	modules/preprocs/nasm/tests/Makefile.inc

include modules/preprocs/nasm/tests/Makefile.inc
