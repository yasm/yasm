# $Id$

pkglib_LTLIBRARIES += objfmt_win32.la

objfmt_win32_la_SOURCES = modules/objfmts/coff/coff-objfmt.c
objfmt_win32_la_LDFLAGS = -module -avoid-version -no-undefined
objfmt_win32_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen objfmt_win32.la

EXTRA_DIST += modules/objfmts/win32/tests/Makefile.inc

include modules/objfmts/win32/tests/Makefile.inc
