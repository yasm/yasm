# $Id$

bin_PROGRAMS += yasm

yasm_SOURCES  = frontends/yasm/yasm.c
yasm_SOURCES += frontends/yasm/yasm-options.c
yasm_SOURCES += frontends/yasm/yasm-options.h
yasm_SOURCES += frontends/yasm/yasm-module.h
yasm_SOURCES += frontends/yasm/yasm-module.c

yasm_LDADD = libyasm.la @LIBLTDL@ $(INTLLIBS) @LIBADD_DL@ $(YASM_MODULES)
yasm_LDADD += @LIBOBJS@
