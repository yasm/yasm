# $IdPath$

YASMPREPROCFILES += \
	src/preprocs/yapp/yapp-preproc.h \
	src/preprocs/yapp/yapp-preproc.c \
	src/preprocs/yapp/yapp-token.h \
	$(top_srcdir)/src/preprocs/yapp/yapp-token.l

BUILT_SOURCES += \
	yapp-token.c

CLEANFILES += \
	yapp-token.c

include src/preprocs/yapp/tests/Makefile.inc
