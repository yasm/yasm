# $IdPath$

lib_LTLIBRARIES += yasm_coff.la

yasm_coff_la_SOURCES = \
	modules/objfmts/coff/coff-objfmt.c
yasm_coff_la_LDFLAGS = -module -avoid-version -no-undefined
yasm_coff_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen yasm_coff.la

EXTRA_DIST += \
	modules/objfmts/coff/tests/Makefile.inc

include modules/objfmts/coff/tests/Makefile.inc
