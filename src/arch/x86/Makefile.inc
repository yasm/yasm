# $IdPath$

YASMARCHFILES += \
	src/arch/x86/x86arch.c	\
	src/arch/x86/x86arch.h	\
	src/arch/x86/x86bc.c	\
	src/arch/x86/x86expr.c	\
	x86id.c

x86id.c: $(srcdir)/src/arch/x86/x86id.re re2c$(EXEEXT) $(srcdir)/tools/re2c/cleanup.pl
	$(top_builddir)/re2c$(EXEEXT) -b $(srcdir)/src/arch/x86/x86id.re | $(PERL) $(srcdir)/tools/re2c/cleanup.pl > $@

BUILT_SOURCES += \
	x86id.c

CLEANFILES += \
	x86id.c

EXTRA_DIST += \
	src/arch/x86/tests/Makefile.inc	\
	src/arch/x86/README	\
	src/arch/x86/x86id.re

include src/arch/x86/tests/Makefile.inc
