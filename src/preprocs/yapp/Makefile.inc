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
	src/preprocs/yapp/tests/raw.asm \
	src/preprocs/yapp/tests/raw.pre \
	src/preprocs/yapp/tests/comment.asm \
	src/preprocs/yapp/tests/comment.pre \
	src/preprocs/yapp/tests/define.asm \
	src/preprocs/yapp/tests/define.pre \
	src/preprocs/yapp/tests/ifdef.asm \
	src/preprocs/yapp/tests/ifdef.pre \
	src/preprocs/yapp/tests/include.asm \
	src/preprocs/yapp/tests/include.pre \
	src/preprocs/yapp/tests/rinclude.asm \
	src/preprocs/yapp/tests/rinclude.pre

CLEANFILES += \
	raw.yp \
	comment.yp \
	define.yp \
	ifdef.yp \
	include.yp \
	rinclude.yp

