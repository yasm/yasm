# $Id$

pkglib_LTLIBRARIES += arch_x86.la

arch_x86_la_SOURCES  = modules/arch/x86/x86arch.c
arch_x86_la_SOURCES += modules/arch/x86/x86arch.h
arch_x86_la_SOURCES += modules/arch/x86/x86bc.c
arch_x86_la_SOURCES += modules/arch/x86/x86expr.c
arch_x86_la_SOURCES += x86id.c

arch_x86_la_LDFLAGS = -module -avoid-version -no-undefined
arch_x86_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen arch_x86.la

x86id.c: $(srcdir)/modules/arch/x86/x86id.re re2c$(EXEEXT) cleanup$(EXEEXT)
	$(top_builddir)/re2c$(EXEEXT) -s $(srcdir)/modules/arch/x86/x86id.re \
	| $(top_builddir)/cleanup$(EXEEXT) \
	| sed "/^#l/ s,re2c-out\.c,$@," > $@

BUILT_SOURCES += x86id.c

CLEANFILES += x86id.c

EXTRA_DIST += modules/arch/x86/tests/Makefile.inc
EXTRA_DIST += modules/arch/x86/x86id.re

include modules/arch/x86/tests/Makefile.inc
