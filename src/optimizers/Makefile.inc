# $IdPath$

EXTRA_DIST += \
	src/optimizers/basic/Makefile.inc

YASMOPTIMIZERFILES =
include src/optimizers/basic/Makefile.inc
yasm_SOURCES += $(YASMOPTIMIZERFILES)
