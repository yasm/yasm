# $Id: Makefile.inc 1463 2006-04-05 05:39:23Z peter $

bin_PROGRAMS += ytasm

ytasm_SOURCES  = frontends/tasm/tasm.c
ytasm_SOURCES += frontends/tasm/tasm-options.c
ytasm_SOURCES += frontends/tasm/tasm-options.h

ytasm_LDADD = libyasm.a $(INTLLIBS)
