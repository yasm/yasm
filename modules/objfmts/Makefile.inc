# $IdPath$

EXTRA_DIST += \
	modules/objfmts/dbg/Makefile.inc \
	modules/objfmts/bin/Makefile.inc \
	modules/objfmts/coff/Makefile.inc \
	modules/objfmts/win32/Makefile.inc

include modules/objfmts/dbg/Makefile.inc
include modules/objfmts/bin/Makefile.inc
include modules/objfmts/coff/Makefile.inc
include modules/objfmts/win32/Makefile.inc
