# $Id$

libyasm_a_SOURCES += modules/parsers/gas/gas-parser.c
libyasm_a_SOURCES += modules/parsers/gas/gas-parser.h
libyasm_a_SOURCES += modules/parsers/gas/gas-defs.h
libyasm_a_SOURCES += modules/parsers/gas/gas-bison.y
libyasm_a_SOURCES += gas-bison.h
libyasm_a_SOURCES += gas-token.c

YASM_MODULES += parser_gas

gas-token.c: $(srcdir)/modules/parsers/gas/gas-token.re re2c$(EXEEXT) cleanup$(EXEEXT)
	$(top_builddir)/re2c$(EXEEXT) -b -o $@ $(srcdir)/modules/parsers/gas/gas-token.re
	$(top_builddir)/cleanup$(EXEEXT) $@

BUILT_SOURCES += gas-bison.c
BUILT_SOURCES += gas-bison.h
BUILT_SOURCES += gas-token.c

CLEANFILES += gas-bison.c
CLEANFILES += gas-bison.h
CLEANFILES += gas-token.c

EXTRA_DIST += modules/parsers/gas/tests/Makefile.inc
EXTRA_DIST += modules/parsers/gas/gas-token.re

include modules/parsers/gas/tests/Makefile.inc
