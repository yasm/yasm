# $IdPath$

pkglib_LTLIBRARIES += dbgfmt_stabs.la

dbgfmt_stabs_la_SOURCES = modules/dbgfmts/stabs/stabs-dbgfmt.c
dbgfmt_stabs_la_LDFLAGS = -module -avoid-version -no-undefined
dbgfmt_stabs_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen dbgfmt_stabs.la
