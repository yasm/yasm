# $IdPath$

pkglib_LTLIBRARIES += objfmt_coff.la

objfmt_coff_la_SOURCES = modules/objfmts/coff/coff-objfmt.c
objfmt_coff_la_LDFLAGS = -module -avoid-version -no-undefined
objfmt_coff_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen objfmt_coff.la

EXTRA_DIST += modules/objfmts/coff/tests/Makefile.inc

include modules/objfmts/coff/tests/Makefile.inc
