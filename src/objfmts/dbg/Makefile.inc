# $IdPath$

lib_LTLIBRARIES += yasm-dbg.la

yasm_dbg_la_SOURCES = \
	src/objfmts/dbg/dbg-objfmt.c
yasm_dbg_la_LDFLAGS = -module
yasm_dbg_la_LIBADD = libyasm.la
yasm_LDADD += -dlopen yasm-dbg.la
