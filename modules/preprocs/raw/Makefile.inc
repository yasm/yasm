# $Id$

pkglib_LTLIBRARIES += preproc_raw.la

preproc_raw_la_SOURCES = modules/preprocs/raw/raw-preproc.c
preproc_raw_la_LDFLAGS = -module -avoid-version -no-undefined
preproc_raw_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen preproc_raw.la
