# $IdPath$

YASMPREPROCFILES += \
	src/preprocs/yapp/yapp-preproc.h \
	src/preprocs/yapp/yapp-preproc.c \
	src/preprocs/yapp/yapp-token.h \
	src/preprocs/yapp/yapp-token.l

BUILT_SOURCES += \
	yapp-token.c

CLEANFILES += \
	yapp-token.c
	src/tests/yapp_test.sh

TESTS += \
	src/preprocs/yapp/tests/yapp_test.sh

EXTRA_DIST += \
	src/preprocs/yapp/tests/define.asm \
	src/preprocs/yapp/tests/define.pre

CLEANFILES += \
	define.yp

