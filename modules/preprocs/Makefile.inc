# $IdPath$

EXTRA_DIST += \
	src/preprocs/raw/Makefile.inc \
	src/preprocs/yapp/Makefile.inc

YASMPREPROCFILES =
include src/preprocs/raw/Makefile.inc
include src/preprocs/yapp/Makefile.inc
yasm_SOURCES += $(YASMPREPROCFILES)
