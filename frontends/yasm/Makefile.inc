# $Id$

bin_PROGRAMS += yasm
dist_man_MANS += yasm.1

if BUILD_MAN
yasm.1: frontends/yasm/yasm.xml
	$(XMLTO) -o $(top_builddir) man $(srcdir)/frontends/yasm/yasm.xml
endif

yasm_SOURCES  = frontends/yasm/yasm.c
yasm_SOURCES += frontends/yasm/yasm-options.c
yasm_SOURCES += frontends/yasm/yasm-options.h

yasm_LDADD = libyasm.a $(INTLLIBS)

EXTRA_DIST += frontends/yasm/yasm.xml
