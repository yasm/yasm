# $IdPath$

EXTRA_DIST += \
	modules/objfmts/dbg/Makefile.inc \
	modules/objfmts/bin/Makefile.inc \
	modules/objfmts/coff/Makefile.inc

include modules/objfmts/dbg/Makefile.inc
include modules/objfmts/bin/Makefile.inc
include modules/objfmts/coff/Makefile.inc
