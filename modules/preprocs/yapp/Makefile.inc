# $IdPath$

lib_LTLIBRARIES += yasm-yapp.la

yasm_yapp_la_SOURCES = \
	modules/preprocs/yapp/yapp-preproc.h	\
	modules/preprocs/yapp/yapp-preproc.c	\
	modules/preprocs/yapp/yapp-token.h	\
	modules/preprocs/yapp/yapp-token.l
yasm_yapp_la_LDFLAGS = -module -avoid-version -no-undefined
yasm_yapp_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen yasm-yapp.la

BUILT_SOURCES += \
	yapp-token.c

CLEANFILES += \
	yapp-token.c

include modules/preprocs/yapp/tests/Makefile.inc
