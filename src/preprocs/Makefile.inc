# $IdPath$

EXTRA_DIST += \
	src/preprocs/raw/Makefile.inc

YASMPREPROCFILES =
include src/preprocs/raw/Makefile.inc
yasm_SOURCES += $(YASMPREPROCFILES)
