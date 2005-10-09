# $Id$

libyasm_a_SOURCES += modules/parsers/nasm/nasm-parser.c
libyasm_a_SOURCES += modules/parsers/nasm/nasm-parser.h
libyasm_a_SOURCES += modules/parsers/nasm/nasm-defs.h
libyasm_a_SOURCES += modules/parsers/nasm/nasm-bison.y
libyasm_a_SOURCES += nasm-bison.h
libyasm_a_SOURCES += nasm-token.c

YASM_MODULES += parser_nasm

nasm-token.c: $(srcdir)/modules/parsers/nasm/nasm-token.re re2c$(EXEEXT)
	$(top_builddir)/re2c$(EXEEXT) -b -o $@ $(srcdir)/modules/parsers/nasm/nasm-token.re

BUILT_SOURCES += nasm-bison.c
BUILT_SOURCES += nasm-bison.h
BUILT_SOURCES += nasm-token.c

CLEANFILES += nasm-bison.c
CLEANFILES += nasm-bison.h
CLEANFILES += nasm-token.c

EXTRA_DIST += modules/parsers/nasm/tests/Makefile.inc
EXTRA_DIST += modules/parsers/nasm/nasm-token.re

include modules/parsers/nasm/tests/Makefile.inc
