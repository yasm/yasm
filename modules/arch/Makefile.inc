# $Id$

EXTRA_DIST += modules/arch/x86/Makefile.inc
EXTRA_DIST += modules/arch/lc3b/Makefile.inc

include modules/arch/x86/Makefile.inc
include modules/arch/lc3b/Makefile.inc

man_MANS += modules/arch/yasm_arch.7
EXTRA_DIST += modules/arch/yasm_arch.xml
