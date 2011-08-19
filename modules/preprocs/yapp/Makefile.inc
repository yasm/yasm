libyasm_a_SOURCES += modules/preprocs/yapp/yapp-preproc.c
libyasm_a_SOURCES += modules/preprocs/yapp/yapp-preproc.h
libyasm_a_SOURCES += modules/preprocs/yapp/yapp-token.h
libyasm_a_SOURCES += modules/preprocs/yapp/yapp-token.l

YASM_MODULES += preproc_yapp

BUILT_SOURCES += yapp-token.c

CLEANFILES += yapp-token.c

include modules/preprocs/yapp/tests/Makefile.inc
