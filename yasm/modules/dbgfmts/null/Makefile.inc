# $IdPath$

lib_LTLIBRARIES += yasm-null.la

yasm_null_la_SOURCES = \
	modules/dbgfmts/null/null-dbgfmt.c
yasm_null_la_LDFLAGS = -module -avoid-version -no-undefined
yasm_null_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen yasm-null.la
