# $IdPath$

EXTRA_DIST += \
	modules/arch/Makefile.inc	\
	modules/parsers/Makefile.inc	\
	modules/preprocs/Makefile.inc	\
	modules/optimizers/Makefile.inc	\
	modules/objfmts/Makefile.inc

include modules/arch/Makefile.inc
include modules/parsers/Makefile.inc
include modules/preprocs/Makefile.inc
include modules/optimizers/Makefile.inc
include modules/dbgfmts/Makefile.inc
include modules/objfmts/Makefile.inc
