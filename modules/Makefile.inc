# $IdPath$

EXTRA_DIST += \
	modules/arch/Makefile.inc	\
	modules/parsers/Makefile.inc	\
	modules/preprocs/Makefile.inc	\
	modules/optimizers/Makefile.inc	\
	modules/objfmts/Makefile.inc

# Modules with more than one type of interface

lib_LTLIBRARIES += yasm-nasm.la
yasm_nasm_la_SOURCES =
yasm_nasm_la_LDFLAGS = -module -avoid-version
yasm_nasm_la_LIBADD = libyasm.la
yasm_LDADD += -dlopen yasm-nasm.la

include modules/arch/Makefile.inc
include modules/parsers/Makefile.inc
include modules/preprocs/Makefile.inc
include modules/optimizers/Makefile.inc
include modules/dbgfmts/Makefile.inc
include modules/objfmts/Makefile.inc
