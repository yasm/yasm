# $Id$

libyasm_a_SOURCES += modules/objfmts/coff/coff-objfmt.c

YASM_MODULES += objfmt_coff

EXTRA_DIST += modules/objfmts/coff/tests/Makefile.inc

include modules/objfmts/coff/tests/Makefile.inc
