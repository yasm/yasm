# $IdPath$

lib_LTLIBRARIES += yasm-raw.la

yasm_raw_la_SOURCES = \
	src/preprocs/raw/raw-preproc.c
yasm_raw_la_LDFLAGS = -module -avoid-version
yasm_raw_la_LIBADD = libyasm.la
yasm_LDADD += -dlpreopen yasm-raw.la
