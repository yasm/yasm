# $IdPath$

EXTRA_DIST += \
	src/arch/x86/Makefile.inc

YASMARCHFILES =
include src/arch/x86/Makefile.inc
yasm_SOURCES += $(YASMARCHFILES)
