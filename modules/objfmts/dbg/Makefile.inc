# $IdPath$

pkglib_LTLIBRARIES += objfmt_dbg.la

objfmt_dbg_la_SOURCES = \
	modules/objfmts/dbg/dbg-objfmt.c
objfmt_dbg_la_LDFLAGS = -module -avoid-version -no-undefined
objfmt_dbg_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen objfmt_dbg.la
