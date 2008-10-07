# $Id: Makefile.inc 1463 2006-04-05 05:39:23Z peter $

bin_PROGRAMS += tasm

tasm_SOURCES  = frontends/tasm/tasm.c
tasm_SOURCES += frontends/tasm/tasm-options.c
tasm_SOURCES += frontends/tasm/tasm-options.h

tasm_LDADD = libyasm.a $(INTLLIBS)
