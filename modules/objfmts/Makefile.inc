# $IdPath$

EXTRA_DIST += \
	src/objfmts/dbg/Makefile.inc

YASMOBJFMTFILES =
include src/objfmts/dbg/Makefile.inc
yasm_SOURCES += $(YASMOBJFMTFILES)
