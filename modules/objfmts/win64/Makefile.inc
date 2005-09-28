# $Id$

# Assume objfmt_coff is included

YASM_MODULES += objfmt_win64

EXTRA_DIST += modules/objfmts/win64/tests/Makefile.inc

include modules/objfmts/win64/tests/Makefile.inc
