# $IdPath$

EXTRA_DIST += \
	src/parsers/nasm/Makefile.inc

YASMPARSERFILES =
include src/parsers/nasm/Makefile.inc
yasm_SOURCES += $(YASMPARSERFILES)
