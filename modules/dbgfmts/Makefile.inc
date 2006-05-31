# $Id$

EXTRA_DIST += modules/dbgfmts/codeview/Makefile.inc
EXTRA_DIST += modules/dbgfmts/dwarf2/Makefile.inc
EXTRA_DIST += modules/dbgfmts/null/Makefile.inc
EXTRA_DIST += modules/dbgfmts/stabs/Makefile.inc

include modules/dbgfmts/codeview/Makefile.inc
include modules/dbgfmts/dwarf2/Makefile.inc
include modules/dbgfmts/null/Makefile.inc
#include modules/dbgfmts/stabs/Makefile.inc
