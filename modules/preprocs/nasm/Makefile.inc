# $Id$

libyasm_a_SOURCES += modules/preprocs/nasm/nasm-preproc.c
libyasm_a_SOURCES += modules/preprocs/nasm/nasm-pp.h
libyasm_a_SOURCES += modules/preprocs/nasm/nasm-pp.c
libyasm_a_SOURCES += modules/preprocs/nasm/nasm.h
libyasm_a_SOURCES += modules/preprocs/nasm/nasmlib.h
libyasm_a_SOURCES += modules/preprocs/nasm/nasmlib.c
libyasm_a_SOURCES += modules/preprocs/nasm/nasm-eval.h
libyasm_a_SOURCES += modules/preprocs/nasm/nasm-eval.c

YASM_MODULES += preproc_nasm

$(top_modulesdir)/src/preprocs/nasm/nasm-pp.c: nasm-macros.c

nasm-macros.c: $(top_srcdir)/modules/preprocs/nasm/standard.mac genmacro$(EXEEXT)
	$(top_builddir)/genmacro$(EXEEXT) $(top_srcdir)/modules/preprocs/nasm/standard.mac

BUILT_SOURCES += nasm-macros.c
CLEANFILES += nasm-macros.c

noinst_PROGRAMS += genmacro

genmacro_SOURCES =
EXTRA_DIST += modules/preprocs/nasm/genmacro.c
genmacro_LDADD = genmacro.$(OBJEXT)
genmacro_LINK = $(CCLD_FOR_BUILD) -o $@

genmacro.$(OBJEXT): modules/preprocs/nasm/genmacro.c
	$(CC_FOR_BUILD) $(DEFAULT_INCLUDES) $(INCLUDES) -c -o $@ `test -f modules/preprocs/nasm/genmacro.c || echo '$(srcdir)/'`modules/preprocs/nasm/genmacro.c

EXTRA_DIST += modules/preprocs/nasm/standard.mac
EXTRA_DIST += modules/preprocs/nasm/tests/Makefile.inc

include modules/preprocs/nasm/tests/Makefile.inc
