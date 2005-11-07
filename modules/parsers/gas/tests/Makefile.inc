# $Id$

TESTS += modules/parsers/gas/tests/gas_test.sh

EXTRA_DIST += modules/parsers/gas/tests/gas_test.sh
EXTRA_DIST += modules/parsers/gas/tests/dataref-imm.asm
EXTRA_DIST += modules/parsers/gas/tests/dataref-imm.errwarn
EXTRA_DIST += modules/parsers/gas/tests/dataref-imm.hex
EXTRA_DIST += modules/parsers/gas/tests/datavis.asm
EXTRA_DIST += modules/parsers/gas/tests/datavis.errwarn
EXTRA_DIST += modules/parsers/gas/tests/datavis.hex
EXTRA_DIST += modules/parsers/gas/tests/datavis2.asm
EXTRA_DIST += modules/parsers/gas/tests/datavis2.errwarn
EXTRA_DIST += modules/parsers/gas/tests/datavis2.hex
EXTRA_DIST += modules/parsers/gas/tests/gas-instlabel.asm
EXTRA_DIST += modules/parsers/gas/tests/gas-instlabel.errwarn
EXTRA_DIST += modules/parsers/gas/tests/gas-instlabel.hex
EXTRA_DIST += modules/parsers/gas/tests/gas-semi.asm
EXTRA_DIST += modules/parsers/gas/tests/gas-semi.errwarn
EXTRA_DIST += modules/parsers/gas/tests/gas-semi.hex
EXTRA_DIST += modules/parsers/gas/tests/gassectalign.asm
EXTRA_DIST += modules/parsers/gas/tests/gassectalign.errwarn
EXTRA_DIST += modules/parsers/gas/tests/gassectalign.hex
EXTRA_DIST += modules/parsers/gas/tests/jmpcall.asm
EXTRA_DIST += modules/parsers/gas/tests/jmpcall.errwarn
EXTRA_DIST += modules/parsers/gas/tests/jmpcall.hex
EXTRA_DIST += modules/parsers/gas/tests/leb128.asm
EXTRA_DIST += modules/parsers/gas/tests/leb128.errwarn
EXTRA_DIST += modules/parsers/gas/tests/leb128.hex
EXTRA_DIST += modules/parsers/gas/tests/reggroup-err.asm
EXTRA_DIST += modules/parsers/gas/tests/reggroup-err.errwarn
EXTRA_DIST += modules/parsers/gas/tests/reggroup.asm
EXTRA_DIST += modules/parsers/gas/tests/reggroup.errwarn
EXTRA_DIST += modules/parsers/gas/tests/reggroup.hex
EXTRA_DIST += modules/parsers/gas/tests/strzero.asm
EXTRA_DIST += modules/parsers/gas/tests/strzero.errwarn
EXTRA_DIST += modules/parsers/gas/tests/strzero.hex
EXTRA_DIST += modules/parsers/gas/tests/varinsn.asm
EXTRA_DIST += modules/parsers/gas/tests/varinsn.errwarn
EXTRA_DIST += modules/parsers/gas/tests/varinsn.hex

EXTRA_DIST += modules/parsers/gas/tests/bin/Makefile.inc
include modules/parsers/gas/tests/bin/Makefile.inc
