# $Id$

TESTS += bitvect_test
TESTS += floatnum_test
TESTS += leb128_test
TESTS += splitpath_test
TESTS += combpath_test
TESTS += libyasm/tests/libyasm_test.sh

EXTRA_DIST += libyasm/tests/libyasm_test.sh
EXTRA_DIST += libyasm/tests/absloop-err.asm
EXTRA_DIST += libyasm/tests/absloop-err.errwarn
EXTRA_DIST += libyasm/tests/charconst64.asm
EXTRA_DIST += libyasm/tests/charconst64.hex
EXTRA_DIST += libyasm/tests/duplabel-err.asm
EXTRA_DIST += libyasm/tests/duplabel-err.errwarn
EXTRA_DIST += libyasm/tests/emptydata.asm
EXTRA_DIST += libyasm/tests/emptydata.hex
EXTRA_DIST += libyasm/tests/expr-wide-ident.asm
EXTRA_DIST += libyasm/tests/expr-wide-ident.hex
EXTRA_DIST += libyasm/tests/externdef.asm
EXTRA_DIST += libyasm/tests/externdef.errwarn
EXTRA_DIST += libyasm/tests/externdef.hex
EXTRA_DIST += libyasm/tests/incbin.asm
EXTRA_DIST += libyasm/tests/incbin.hex
EXTRA_DIST += libyasm/tests/jmpsize1.asm
EXTRA_DIST += libyasm/tests/jmpsize1.hex
EXTRA_DIST += libyasm/tests/jmpsize1-err.asm
EXTRA_DIST += libyasm/tests/jmpsize1-err.errwarn
EXTRA_DIST += libyasm/tests/opt-align1.asm
EXTRA_DIST += libyasm/tests/opt-align1.hex
EXTRA_DIST += libyasm/tests/opt-align2.asm
EXTRA_DIST += libyasm/tests/opt-align2.hex
EXTRA_DIST += libyasm/tests/opt-align3.asm
EXTRA_DIST += libyasm/tests/opt-align3.hex
EXTRA_DIST += libyasm/tests/opt-circular1-err.asm
EXTRA_DIST += libyasm/tests/opt-circular1-err.errwarn
EXTRA_DIST += libyasm/tests/opt-circular2-err.asm
EXTRA_DIST += libyasm/tests/opt-circular2-err.errwarn
EXTRA_DIST += libyasm/tests/opt-circular3-err.asm
EXTRA_DIST += libyasm/tests/opt-circular3-err.errwarn
EXTRA_DIST += libyasm/tests/opt-gvmat64.asm
EXTRA_DIST += libyasm/tests/opt-gvmat64.hex
EXTRA_DIST += libyasm/tests/opt-immexpand.asm
EXTRA_DIST += libyasm/tests/opt-immexpand.hex
EXTRA_DIST += libyasm/tests/opt-immnoexpand.asm
EXTRA_DIST += libyasm/tests/opt-immnoexpand.hex
EXTRA_DIST += libyasm/tests/opt-struc.asm
EXTRA_DIST += libyasm/tests/opt-struc.hex
EXTRA_DIST += libyasm/tests/timesover-err.asm
EXTRA_DIST += libyasm/tests/timesover-err.errwarn
EXTRA_DIST += libyasm/tests/timesunder.asm
EXTRA_DIST += libyasm/tests/timesunder.hex
EXTRA_DIST += libyasm/tests/unary.asm
EXTRA_DIST += libyasm/tests/unary.hex
EXTRA_DIST += libyasm/tests/value-err.asm
EXTRA_DIST += libyasm/tests/value-err.errwarn

check_PROGRAMS += bitvect_test
check_PROGRAMS += floatnum_test
check_PROGRAMS += leb128_test
check_PROGRAMS += splitpath_test
check_PROGRAMS += combpath_test

bitvect_test_SOURCES  = libyasm/tests/bitvect_test.c
bitvect_test_LDADD = libyasm.a $(INTLLIBS)

floatnum_test_SOURCES  = libyasm/tests/floatnum_test.c
floatnum_test_LDADD = libyasm.a $(INTLLIBS)

leb128_test_SOURCES  = libyasm/tests/leb128_test.c
leb128_test_LDADD = libyasm.a $(INTLLIBS)

splitpath_test_SOURCES  = libyasm/tests/splitpath_test.c
splitpath_test_LDADD = libyasm.a $(INTLLIBS)

combpath_test_SOURCES  = libyasm/tests/combpath_test.c
combpath_test_LDADD = libyasm.a $(INTLLIBS)

