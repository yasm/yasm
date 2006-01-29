# $Id$

TESTS += bitvect_test
TESTS += floatnum_test
TESTS += leb128_test
TESTS += splitpath_test
TESTS += libyasm/tests/libyasm_test.sh

EXTRA_DIST += libyasm/tests/libyasm_test.sh
EXTRA_DIST += libyasm/tests/charconst64.asm
EXTRA_DIST += libyasm/tests/charconst64.errwarn
EXTRA_DIST += libyasm/tests/charconst64.hex
EXTRA_DIST += libyasm/tests/duplabel-err.asm
EXTRA_DIST += libyasm/tests/duplabel-err.errwarn
EXTRA_DIST += libyasm/tests/externdef.asm
EXTRA_DIST += libyasm/tests/externdef.errwarn
EXTRA_DIST += libyasm/tests/externdef.hex
EXTRA_DIST += libyasm/tests/incbin.asm
EXTRA_DIST += libyasm/tests/incbin.errwarn
EXTRA_DIST += libyasm/tests/incbin.hex
EXTRA_DIST += libyasm/tests/timesover-err.asm
EXTRA_DIST += libyasm/tests/timesover-err.errwarn
EXTRA_DIST += libyasm/tests/unary.asm
EXTRA_DIST += libyasm/tests/unary.errwarn
EXTRA_DIST += libyasm/tests/unary.hex

check_PROGRAMS += bitvect_test
check_PROGRAMS += floatnum_test
check_PROGRAMS += leb128_test
check_PROGRAMS += splitpath_test

bitvect_test_SOURCES  = libyasm/tests/bitvect_test.c
bitvect_test_LDADD = libyasm.a $(INTLLIBS)

floatnum_test_SOURCES  = libyasm/tests/floatnum_test.c
floatnum_test_LDADD = libyasm.a $(INTLLIBS)

leb128_test_SOURCES  = libyasm/tests/leb128_test.c
leb128_test_LDADD = libyasm.a $(INTLLIBS)

splitpath_test_SOURCES  = libyasm/tests/splitpath_test.c

