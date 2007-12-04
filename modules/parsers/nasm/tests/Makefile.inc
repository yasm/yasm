# $Id$

TESTS += modules/parsers/nasm/tests/nasm_test.sh

EXTRA_DIST += modules/parsers/nasm/tests/nasm_test.sh
EXTRA_DIST += modules/parsers/nasm/tests/alignnop16.asm
EXTRA_DIST += modules/parsers/nasm/tests/alignnop16.hex
EXTRA_DIST += modules/parsers/nasm/tests/alignnop32.asm
EXTRA_DIST += modules/parsers/nasm/tests/alignnop32.hex
EXTRA_DIST += modules/parsers/nasm/tests/charconstmath.asm
EXTRA_DIST += modules/parsers/nasm/tests/charconstmath.hex
EXTRA_DIST += modules/parsers/nasm/tests/endcomma.asm
EXTRA_DIST += modules/parsers/nasm/tests/endcomma.hex
EXTRA_DIST += modules/parsers/nasm/tests/equcolon.asm
EXTRA_DIST += modules/parsers/nasm/tests/equcolon.hex
EXTRA_DIST += modules/parsers/nasm/tests/equlocal.asm
EXTRA_DIST += modules/parsers/nasm/tests/equlocal.hex
EXTRA_DIST += modules/parsers/nasm/tests/hexconst.asm
EXTRA_DIST += modules/parsers/nasm/tests/hexconst.hex
EXTRA_DIST += modules/parsers/nasm/tests/long.asm
EXTRA_DIST += modules/parsers/nasm/tests/long.hex
EXTRA_DIST += modules/parsers/nasm/tests/newsect.asm
EXTRA_DIST += modules/parsers/nasm/tests/newsect.hex
EXTRA_DIST += modules/parsers/nasm/tests/orphannowarn.asm
EXTRA_DIST += modules/parsers/nasm/tests/orphannowarn.hex
EXTRA_DIST += modules/parsers/nasm/tests/prevlocalwarn.asm
EXTRA_DIST += modules/parsers/nasm/tests/prevlocalwarn.errwarn
EXTRA_DIST += modules/parsers/nasm/tests/prevlocalwarn.hex
EXTRA_DIST += modules/parsers/nasm/tests/strucalign.asm
EXTRA_DIST += modules/parsers/nasm/tests/strucalign.hex
EXTRA_DIST += modules/parsers/nasm/tests/struczero.asm
EXTRA_DIST += modules/parsers/nasm/tests/struczero.hex

EXTRA_DIST += modules/parsers/nasm/tests/worphan/Makefile.inc

include modules/parsers/nasm/tests/worphan/Makefile.inc
