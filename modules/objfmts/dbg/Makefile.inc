# $IdPath$

lib_LTLIBRARIES += yasm-dbg.la

yasm_dbg_la_SOURCES = \
	modules/objfmts/dbg/dbg-objfmt.c
yasm_dbg_la_LDFLAGS = -module -avoid-version
yasm_dbg_la_LIBADD = libyasm.la
yasm_LDADD += -dlopen yasm-dbg.la
