# $IdPath$

lib_LTLIBRARIES += yasm_bin.la

yasm_bin_la_SOURCES = \
	modules/objfmts/bin/bin-objfmt.c
yasm_bin_la_LDFLAGS = -module -avoid-version -no-undefined
yasm_bin_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen yasm_bin.la

EXTRA_DIST += \
	modules/objfmts/bin/tests/Makefile.inc

include modules/objfmts/bin/tests/Makefile.inc
