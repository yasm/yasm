# $Id$

pkglib_LTLIBRARIES += optimizer_basic.la

optimizer_basic_la_SOURCES = modules/optimizers/basic/basic-optimizer.c
optimizer_basic_la_LDFLAGS = -module -avoid-version -no-undefined
optimizer_basic_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen optimizer_basic.la
