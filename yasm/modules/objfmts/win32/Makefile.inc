# $IdPath$

lib_LTLIBRARIES += yasm_win32.la

yasm_win32_la_SOURCES = \
	modules/objfmts/coff/coff-objfmt.c
yasm_win32_la_LDFLAGS = -module -avoid-version -no-undefined
yasm_win32_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen yasm_win32.la

EXTRA_DIST += \
	modules/objfmts/win32/tests/Makefile.inc

include modules/objfmts/win32/tests/Makefile.inc
