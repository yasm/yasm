# $IdPath$

EXTRA_DIST += \
	modules/preprocs/nasm/Makefile.inc	\
	modules/preprocs/raw/Makefile.inc	\
	modules/preprocs/yapp/Makefile.inc

include modules/preprocs/nasm/Makefile.inc
include modules/preprocs/raw/Makefile.inc
include modules/preprocs/yapp/Makefile.inc
