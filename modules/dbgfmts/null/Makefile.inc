# $Id$

pkglib_LTLIBRARIES += dbgfmt_null.la

dbgfmt_null_la_SOURCES = modules/dbgfmts/null/null-dbgfmt.c
dbgfmt_null_la_LDFLAGS = -module -avoid-version -no-undefined
dbgfmt_null_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen dbgfmt_null.la
