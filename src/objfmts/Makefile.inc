# $IdPath$

EXTRA_DIST += \
	src/objfmts/dbg/Makefile.inc \
	src/objfmts/bin/Makefile.inc \
	src/objfmts/elf/Makefile.inc

YASMOBJFMTFILES =
include src/objfmts/dbg/Makefile.inc
include src/objfmts/bin/Makefile.inc
include src/objfmts/elf/Makefile.inc
yasm_SOURCES += $(YASMOBJFMTFILES)
