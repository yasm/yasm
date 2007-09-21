# $Id$

libyasm_a_SOURCES += modules/arch/x86/x86arch.c
libyasm_a_SOURCES += modules/arch/x86/x86arch.h
libyasm_a_SOURCES += modules/arch/x86/x86bc.c
libyasm_a_SOURCES += modules/arch/x86/x86expr.c
libyasm_a_SOURCES += modules/arch/x86/x86id.c
libyasm_a_SOURCES += x86cpu.c
libyasm_a_SOURCES += x86regtmod.c

YASM_MODULES += arch_x86

modules/arch/x86/x86id.c: x86insn_nasm.c x86insn_gas.c

noinst_PROGRAMS += x86geninsn

x86geninsn_SOURCES =
EXTRA_DIST += modules/arch/x86/x86geninsn.c
x86geninsn_LDADD = x86geninsn.$(OBJEXT)
x86geninsn_LINK = $(CCLD_FOR_BUILD) -o $@

x86geninsn.$(OBJEXT): modules/arch/x86/x86geninsn.c
	$(CC_FOR_BUILD) $(DEFAULT_INCLUDES) $(INCLUDES) -c -o $@ `test -f modules/arch/x86/x86geninsn.c || echo '$(srcdir)/'`modules/arch/x86/x86geninsn.c

EXTRA_DIST += modules/arch/x86/x86insn.in

x86insn_nasm.gperf x86insn_gas.gperf: $(srcdir)/modules/arch/x86/x86insn.in x86geninsn$(EXEEXT)
	$(top_builddir)/x86geninsn$(EXEEXT) $(srcdir)/modules/arch/x86/x86insn.in x86insn_nasm.gperf x86insn_gas.gperf


BUILT_SOURCES += x86insn_nasm.gperf
BUILT_SOURCES += x86insn_gas.gperf
CLEANFILES += x86insn_nasm.gperf
CLEANFILES += x86insn_gas.gperf

EXTRA_DIST += modules/arch/x86/x86cpu.gperf
EXTRA_DIST += modules/arch/x86/x86regtmod.gperf

x86insn_nasm.c: x86insn_nasm.gperf genperf$(EXEEXT)
	$(top_builddir)/genperf$(EXEEXT) x86insn_nasm.gperf > $@

x86insn_gas.c: x86insn_gas.gperf genperf$(EXEEXT)
	$(top_builddir)/genperf$(EXEEXT) x86insn_gas.gperf > $@

x86cpu.c: $(srcdir)/modules/arch/x86/x86cpu.gperf genperf$(EXEEXT)
	$(top_builddir)/genperf$(EXEEXT) $(srcdir)/modules/arch/x86/x86cpu.gperf > $@

x86regtmod.c: $(srcdir)/modules/arch/x86/x86regtmod.gperf genperf$(EXEEXT)
	$(top_builddir)/genperf$(EXEEXT) $(srcdir)/modules/arch/x86/x86regtmod.gperf > $@

BUILT_SOURCES += x86insn_nasm.c
BUILT_SOURCES += x86insn_gas.c
BUILT_SOURCES += x86cpu.c
BUILT_SOURCES += x86regtmod.c
CLEANFILES += x86insn_nasm.c
CLEANFILES += x86insn_gas.c
CLEANFILES += x86cpu.c
CLEANFILES += x86cpu.c
CLEANFILES += x86regtmod.c

EXTRA_DIST += modules/arch/x86/tests/Makefile.inc

include modules/arch/x86/tests/Makefile.inc
