# $IdPath$

lib_LTLIBRARIES += yasm-bin.la

yasm_bin_la_SOURCES = \
	src/objfmts/bin/bin-objfmt.c
yasm_bin_la_LDFLAGS = -module -avoid-version
yasm_bin_la_LIBADD = libyasm.la
yasm_LDADD += -dlopen yasm-bin.la

EXTRA_DIST += \
	src/objfmts/bin/tests/Makefile.inc

include src/objfmts/bin/tests/Makefile.inc
