# $IdPath$

EXTRA_DIST += modules/objfmts/dbg/Makefile.inc
EXTRA_DIST += modules/objfmts/bin/Makefile.inc
EXTRA_DIST += modules/objfmts/coff/Makefile.inc
EXTRA_DIST += modules/objfmts/win32/Makefile.inc

include modules/objfmts/dbg/Makefile.inc
include modules/objfmts/bin/Makefile.inc
include modules/objfmts/coff/Makefile.inc
include modules/objfmts/win32/Makefile.inc
