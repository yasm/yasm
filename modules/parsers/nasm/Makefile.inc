# $Id$

pkglib_LTLIBRARIES += parser_nasm.la

parser_nasm_la_SOURCES  = modules/parsers/nasm/nasm-parser.c
parser_nasm_la_SOURCES += modules/parsers/nasm/nasm-parser.h
parser_nasm_la_SOURCES += modules/parsers/nasm/nasm-defs.h
parser_nasm_la_SOURCES += modules/parsers/nasm/nasm-bison.y
parser_nasm_la_SOURCES += nasm-bison.h
parser_nasm_la_SOURCES += nasm-token.c

parser_nasm_la_LDFLAGS = -module -avoid-version -no-undefined
parser_nasm_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen parser_nasm.la

nasm-token.c: $(srcdir)/modules/parsers/nasm/nasm-token.re re2c$(EXEEXT) cleanup$(EXEEXT)
	$(top_builddir)/re2c$(EXEEXT) -b $(srcdir)/modules/parsers/nasm/nasm-token.re \
	| $(top_builddir)/cleanup$(EXEEXT) \
	| sed "/^#l/ s,re2c-out\.c,$@," > $@

BUILT_SOURCES += nasm-bison.c
BUILT_SOURCES += nasm-bison.h
BUILT_SOURCES += nasm-token.c

CLEANFILES += nasm-bison.c
CLEANFILES += nasm-bison.h
CLEANFILES += nasm-token.c

EXTRA_DIST += modules/parsers/nasm/tests/Makefile.inc
EXTRA_DIST += modules/parsers/nasm/nasm-token.re

include modules/parsers/nasm/tests/Makefile.inc
