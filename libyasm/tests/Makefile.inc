# $Id$

TESTS += bitvect_test
TESTS += floatnum_test
TESTS += libyasm/tests/libyasm_test.sh

EXTRA_DIST += libyasm/tests/libyasm_test.sh
EXTRA_DIST += libyasm/tests/charconst64.asm
EXTRA_DIST += libyasm/tests/charconst64.errwarn
EXTRA_DIST += libyasm/tests/charconst64.hex
EXTRA_DIST += libyasm/tests/duplabel-err.asm
EXTRA_DIST += libyasm/tests/duplabel-err.errwarn
EXTRA_DIST += libyasm/tests/unary.asm
EXTRA_DIST += libyasm/tests/unary.errwarn
EXTRA_DIST += libyasm/tests/unary.hex

noinst_PROGRAMS += bitvect_test
noinst_PROGRAMS += floatnum_test

bitvect_test_CFLAGS =
bitvect_test_SOURCES  = libyasm/tests/bitvect_test.c
bitvect_test_LDFLAGS =
bitvect_test_LDADD = libyasm.la @LIBLTDL@ $(INTLLIBS) @LIBADD_DL@

floatnum_test_CFLAGS =
floatnum_test_SOURCES  = libyasm/tests/floatnum_test.c
floatnum_test_LDFLAGS =
floatnum_test_LDADD = libyasm.la @LIBLTDL@ $(INTLLIBS) @LIBADD_DL@

