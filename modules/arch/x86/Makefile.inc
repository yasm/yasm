# $Id$

libyasm_a_SOURCES += modules/arch/x86/x86arch.c
libyasm_a_SOURCES += modules/arch/x86/x86arch.h
libyasm_a_SOURCES += modules/arch/x86/x86bc.c
libyasm_a_SOURCES += modules/arch/x86/x86expr.c
libyasm_a_SOURCES += modules/arch/x86/x86id.c

YASM_MODULES += arch_x86

modules/arch/x86/x86id.c: x86parse.c

EXTRA_DIST += modules/arch/x86/x86parse.gap

x86parse.c: $(srcdir)/modules/arch/x86/x86parse.gap gap$(EXEEXT)
	$(top_builddir)/gap$(EXEEXT) $(srcdir)/modules/arch/x86/x86parse.gap $@

BUILT_SOURCES += x86parse.c
CLEANFILES += x86parse.c

EXTRA_DIST += modules/arch/x86/tests/Makefile.inc

include modules/arch/x86/tests/Makefile.inc
