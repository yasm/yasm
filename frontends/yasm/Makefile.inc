# $Id$

bin_PROGRAMS += yasm
man_MANS += frontends/yasm/yasm.1

yasm_SOURCES  = frontends/yasm/yasm.c
yasm_SOURCES += frontends/yasm/yasm-options.c
yasm_SOURCES += frontends/yasm/yasm-options.h

yasm_LDADD = libyasm.a $(INTLLIBS)

EXTRA_DIST += frontends/yasm/yasm.xml
