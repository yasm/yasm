# $Id$

TESTS += modules/parsers/nasm/tests/nasm_test.sh

EXTRA_DIST += modules/parsers/nasm/tests/nasm_test.sh
EXTRA_DIST += modules/parsers/nasm/tests/endcomma.asm
EXTRA_DIST += modules/parsers/nasm/tests/endcomma.errwarn
EXTRA_DIST += modules/parsers/nasm/tests/endcomma.hex
EXTRA_DIST += modules/parsers/nasm/tests/equcolon.asm
EXTRA_DIST += modules/parsers/nasm/tests/equcolon.errwarn
EXTRA_DIST += modules/parsers/nasm/tests/equcolon.hex
EXTRA_DIST += modules/parsers/nasm/tests/equlocal.asm
EXTRA_DIST += modules/parsers/nasm/tests/equlocal.errwarn
EXTRA_DIST += modules/parsers/nasm/tests/equlocal.hex
EXTRA_DIST += modules/parsers/nasm/tests/long.asm
EXTRA_DIST += modules/parsers/nasm/tests/long.errwarn
EXTRA_DIST += modules/parsers/nasm/tests/long.hex
EXTRA_DIST += modules/parsers/nasm/tests/newsect.asm
EXTRA_DIST += modules/parsers/nasm/tests/newsect.errwarn
EXTRA_DIST += modules/parsers/nasm/tests/newsect.hex
EXTRA_DIST += modules/parsers/nasm/tests/prevlocalwarn.asm
EXTRA_DIST += modules/parsers/nasm/tests/prevlocalwarn.errwarn
EXTRA_DIST += modules/parsers/nasm/tests/prevlocalwarn.hex

EXTRA_DIST += modules/parsers/nasm/tests/worphan/Makefile.inc

include modules/parsers/nasm/tests/worphan/Makefile.inc
