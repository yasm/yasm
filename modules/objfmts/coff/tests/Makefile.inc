# $IdPath$

TESTS += \
	src/objfmts/coff/tests/coff_test.sh

EXTRA_DIST += \
	src/objfmts/coff/tests/coff_test.sh		\
	src/objfmts/coff/tests/cofftest.c		\
	src/objfmts/coff/tests/cofftest.asm		\
	src/objfmts/coff/tests/cofftest.hex		\
	src/objfmts/coff/tests/cofftest.errwarn

CLEANFILES += \
	cofftest.o \
	cofftest.hx \
	cofftest.ew
