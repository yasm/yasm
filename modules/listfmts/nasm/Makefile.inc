# $Id$

pkglib_LTLIBRARIES += listfmt_nasm.la

listfmt_nasm_la_SOURCES  = modules/listfmts/nasm/nasm-listfmt.c

listfmt_nasm_la_LDFLAGS = -module -avoid-version -no-undefined
listfmt_nasm_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen listfmt_nasm.la

#EXTRA_DIST += modules/listfmts/nasm/tests/Makefile.inc

#include modules/listfmts/nasm/tests/Makefile.inc
