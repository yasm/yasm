# $IdPath$

lib_LTLIBRARIES += yasm-basic.la

yasm_basic_la_SOURCES = \
	modules/optimizers/basic/basic-optimizer.c
yasm_basic_la_LDFLAGS = -module -avoid-version
yasm_basic_la_LIBADD = libyasm.la
yasm_LDADD += -dlpreopen yasm-basic.la
