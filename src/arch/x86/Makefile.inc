# $IdPath$

lib_LTLIBRARIES += yasm-x86.la

yasm_x86_la_SOURCES = \
	src/arch/x86/x86arch.c	\
	src/arch/x86/x86arch.h	\
	src/arch/x86/x86bc.c	\
	src/arch/x86/x86expr.c	\
	x86id.c
yasm_x86_la_LDFLAGS = -module -avoid-version
yasm_x86_la_LIBADD = libyasm.la
yasm_LDADD += -dlpreopen yasm-x86.la

x86id.c: $(srcdir)/src/arch/x86/x86id.re re2c$(EXEEXT) $(srcdir)/tools/re2c/cleanup.pl
	$(top_builddir)/re2c$(EXEEXT) -b $(srcdir)/src/arch/x86/x86id.re | $(PERL) $(srcdir)/tools/re2c/cleanup.pl | sed "/^#l/ s,re2c-out\.c,$@," > $@

BUILT_SOURCES += \
	x86id.c

CLEANFILES += \
	x86id.c

EXTRA_DIST += \
	src/arch/x86/tests/Makefile.inc	\
	src/arch/x86/README	\
	src/arch/x86/x86id.re

include src/arch/x86/tests/Makefile.inc
