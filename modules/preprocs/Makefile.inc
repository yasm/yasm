# $IdPath$

EXTRA_DIST += \
	src/preprocs/nasm/Makefile.inc \
	src/preprocs/raw/Makefile.inc \
	src/preprocs/yapp/Makefile.inc

include src/preprocs/nasm/Makefile.inc
include src/preprocs/raw/Makefile.inc
include src/preprocs/yapp/Makefile.inc
