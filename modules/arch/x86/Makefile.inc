# $Id$

libyasm_a_SOURCES += modules/arch/x86/x86arch.c
libyasm_a_SOURCES += modules/arch/x86/x86arch.h
libyasm_a_SOURCES += modules/arch/x86/x86bc.c
libyasm_a_SOURCES += modules/arch/x86/x86expr.c
libyasm_a_SOURCES += x86id.c

YASM_MODULES += arch_x86

x86id.c: $(srcdir)/modules/arch/x86/x86id.re re2c$(EXEEXT) cleanup$(EXEEXT)
	$(top_builddir)/re2c$(EXEEXT) -s $(srcdir)/modules/arch/x86/x86id.re \
	| $(top_builddir)/cleanup$(EXEEXT) \
	| sed "/^#l/ s,re2c-out\.c,$@," > $@

BUILT_SOURCES += x86id.c

CLEANFILES += x86id.c

EXTRA_DIST += modules/arch/x86/tests/Makefile.inc
EXTRA_DIST += modules/arch/x86/x86id.re

include modules/arch/x86/tests/Makefile.inc
