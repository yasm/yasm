# $IdPath: yasm/modules/parsers/nasm/Makefile.inc,v 1.14 2003/05/03 06:26:15 peter Exp $

pkglib_LTLIBRARIES += parser_nasm.la

parser_nasm_la_SOURCES = \
	modules/parsers/nasm/nasm-parser.h	\
	modules/parsers/nasm/nasm-parser.c	\
	modules/parsers/nasm/nasm-defs.h	\
	modules/parsers/nasm/nasm-bison.y	\
	nasm-bison.h				\
	nasm-token.c
parser_nasm_la_LDFLAGS = -module -avoid-version -no-undefined
parser_nasm_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen parser_nasm.la

nasm-token.c: $(srcdir)/modules/parsers/nasm/nasm-token.re re2c$(EXEEXT) $(srcdir)/tools/re2c/cleanup.pl
	$(top_builddir)/re2c$(EXEEXT) -b $(srcdir)/modules/parsers/nasm/nasm-token.re | $(PERL) $(top_srcdir)/tools/re2c/cleanup.pl | sed "/^#l/ s,re2c-out\.c,$@," > $@

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
