# $IdPath$

pkglib_LTLIBRARIES += objfmt_bin.la

objfmt_bin_la_SOURCES = modules/objfmts/bin/bin-objfmt.c
objfmt_bin_la_LDFLAGS = -module -avoid-version -no-undefined
objfmt_bin_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen objfmt_bin.la

EXTRA_DIST += modules/objfmts/bin/tests/Makefile.inc

include modules/objfmts/bin/tests/Makefile.inc
