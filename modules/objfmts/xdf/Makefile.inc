# $Id$

pkglib_LTLIBRARIES += objfmt_xdf.la

objfmt_xdf_la_SOURCES = modules/objfmts/xdf/xdf-objfmt.c
objfmt_xdf_la_LDFLAGS = -module -avoid-version -no-undefined
objfmt_xdf_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen objfmt_xdf.la

#EXTRA_DIST += modules/objfmts/xdf/tests/Makefile.inc

#include modules/objfmts/xdf/tests/Makefile.inc
