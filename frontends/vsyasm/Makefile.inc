# $Id: Makefile.inc 1463 2006-04-05 05:39:23Z peter $

bin_PROGRAMS += vsyasm

vsyasm_SOURCES  = frontends/vsyasm/vsyasm.c
vsyasm_SOURCES += frontends/yasm/yasm-options.c
vsyasm_SOURCES += frontends/yasm/yasm-options.h

$(srcdir)/frontends/vsyasm/vsyasm.c: license.c

vsyasm_LDADD = libyasm.a $(INTLLIBS)
