# $IdPath$

lib_LTLIBRARIES += yasm-yapp.la

yasm_yapp_la_SOURCES = \
	src/preprocs/yapp/yapp-preproc.h \
	src/preprocs/yapp/yapp-preproc.c \
	src/preprocs/yapp/yapp-token.h \
	src/preprocs/yapp/yapp-token.l
yasm_yapp_la_LDFLAGS = -module -avoid-version
yasm_yapp_la_LIBADD = libyasm.la
yasm_LDADD += -dlopen yasm-yapp.la

BUILT_SOURCES += \
	yapp-token.c

CLEANFILES += \
	yapp-token.c

include src/preprocs/yapp/tests/Makefile.inc
