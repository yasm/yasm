# $IdPath$

lib_LTLIBRARIES += yasm-coff.la

yasm_coff_la_SOURCES = \
	src/objfmts/coff/coff-objfmt.c
yasm_coff_la_LDFLAGS = -module
yasm_coff_la_LIBADD = libyasm.la
yasm_LDADD += -dlopen yasm-coff.la

EXTRA_DIST += \
	src/objfmts/coff/tests/Makefile.inc

include src/objfmts/coff/tests/Makefile.inc
