# $IdPath$

pkglib_LTLIBRARIES += arch_lc3b.la

arch_lc3b_la_SOURCES  = modules/arch/lc3b/lc3barch.c
arch_lc3b_la_SOURCES += modules/arch/lc3b/lc3barch.h
arch_lc3b_la_SOURCES += modules/arch/lc3b/lc3bbc.c
arch_lc3b_la_SOURCES += lc3bid.c

arch_lc3b_la_LDFLAGS = -module -avoid-version -no-undefined
arch_lc3b_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen arch_lc3b.la

lc3bid.c: $(srcdir)/modules/arch/lc3b/lc3bid.re re2c$(EXEEXT) cleanup$(EXEEXT)
	$(top_builddir)/re2c$(EXEEXT) -s $(srcdir)/modules/arch/lc3b/lc3bid.re \
	| $(top_builddir)/cleanup$(EXEEXT) \
	| sed "/^#l/ s,re2c-out\.c,$@," > $@

BUILT_SOURCES += lc3bid.c

CLEANFILES += lc3bid.c

EXTRA_DIST += modules/arch/lc3b/tests/Makefile.inc
EXTRA_DIST += modules/arch/lc3b/README
EXTRA_DIST += modules/arch/lc3b/lc3bid.re

include modules/arch/lc3b/tests/Makefile.inc
