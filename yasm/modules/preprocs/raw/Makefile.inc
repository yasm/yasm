# $IdPath$

lib_LTLIBRARIES += yasm-raw.la

yasm_raw_la_SOURCES = \
	modules/preprocs/raw/raw-preproc.c
yasm_raw_la_LDFLAGS = -module -avoid-version -no-undefined
yasm_raw_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen yasm-raw.la
