# $IdPath$

bin_PROGRAMS += yasm
yasm_SOURCES = \
	frontends/yasm/yasm.c		\
	frontends/yasm/yasm-options.c	\
	frontends/yasm/yasm-options.h	\
	frontends/yasm/yasm-module.h	\
	frontends/yasm/yasm-module.c
yasm_LDADD = libyasm.la @LIBLTDL@ $(INTLLIBS) @LIBADD_DL@ $(YASM_MODULES)
