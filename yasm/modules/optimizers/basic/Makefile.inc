# $IdPath$

lib_LTLIBRARIES += yasm_basic.la

yasm_basic_la_SOURCES = \
	modules/optimizers/basic/basic-optimizer.c
yasm_basic_la_LDFLAGS = -module -avoid-version -no-undefined
yasm_basic_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen yasm_basic.la
