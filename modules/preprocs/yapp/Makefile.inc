# $IdPath$

pkglib_LTLIBRARIES += preproc_yapp.la

preproc_yapp_la_SOURCES = \
	modules/preprocs/yapp/yapp-preproc.h	\
	modules/preprocs/yapp/yapp-preproc.c	\
	modules/preprocs/yapp/yapp-token.h	\
	modules/preprocs/yapp/yapp-token.l
preproc_yapp_la_LDFLAGS = -module -avoid-version -no-undefined
preproc_yapp_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen preproc_yapp.la

BUILT_SOURCES += \
	yapp-token.c

CLEANFILES += \
	yapp-token.c

include modules/preprocs/yapp/tests/Makefile.inc
