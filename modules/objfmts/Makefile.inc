# $IdPath$

EXTRA_DIST += \
	src/objfmts/dbg/Makefile.inc \
	src/objfmts/bin/Makefile.inc \
	src/objfmts/coff/Makefile.inc

YASMOBJFMTFILES =
include src/objfmts/dbg/Makefile.inc
include src/objfmts/bin/Makefile.inc
include src/objfmts/coff/Makefile.inc
yasm_SOURCES += $(YASMOBJFMTFILES)
