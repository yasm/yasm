# $Id$

libyasm_a_SOURCES += modules/dbgfmts/dwarf2/dwarf2-dbgfmt.c

YASM_MODULES += dbgfmt_dwarf2

EXTRA_DIST += modules/dbgfmts/dwarf2/tests/Makefile.inc
include modules/dbgfmts/dwarf2/tests/Makefile.inc
