# $IdPath$

lib_LTLIBRARIES += yasm-null.la

yasm_null_la_SOURCES = \
	src/dbgfmts/null/null-dbgfmt.c
yasm_null_la_LDFLAGS = -module -avoid-version
yasm_null_la_LIBADD = libyasm.la
yasm_LDADD += -dlpreopen yasm-null.la
