# $IdPath$

#lib_LTLIBRARIES += yasm-nasm.la

yasm_nasm_la_SOURCES += \
	modules/parsers/nasm/nasm-parser.h	\
	modules/parsers/nasm/nasm-parser.c	\
	modules/parsers/nasm/nasm-defs.h	\
	modules/parsers/nasm/nasm-bison.y	\
	nasm-bison.h				\
	nasm-token.c
#yasm_nasm_la_LDFLAGS = -module -avoid-version
#yasm_nasm_la_LIBADD = libyasm.la
#YASM_MODULES += -dlopen yasm-nasm.la

nasm-token.c: $(srcdir)/modules/parsers/nasm/nasm-token.re re2c$(EXEEXT) $(srcdir)/tools/re2c/cleanup.pl
	$(top_builddir)/re2c$(EXEEXT) -b $(srcdir)/modules/parsers/nasm/nasm-token.re | $(PERL) $(srcdir)/tools/re2c/cleanup.pl | sed "/^#l/ s,re2c-out\.c,$@," > $@

BUILT_SOURCES += \
	nasm-bison.c				\
	nasm-bison.h				\
	nasm-token.c

CLEANFILES += \
	nasm-bison.c				\
	nasm-bison.h				\
	nasm-token.c

EXTRA_DIST += \
	modules/parsers/nasm/tests/Makefile.inc	\
	modules/parsers/nasm/nasm-token.re

include modules/parsers/nasm/tests/Makefile.inc
