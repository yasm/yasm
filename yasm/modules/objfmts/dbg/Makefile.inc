# $IdPath$

lib_LTLIBRARIES += yasm-dbg.la

yasm_dbg_la_SOURCES = \
	modules/objfmts/dbg/dbg-objfmt.c
yasm_dbg_la_LDFLAGS = -module -avoid-version -no-undefined
yasm_dbg_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen yasm-dbg.la
