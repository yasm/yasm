# $IdPath$

TESTS += \
	modules/parsers/nasm/tests/nasm_test.sh

EXTRA_DIST += \
	modules/parsers/nasm/tests/nasm_test.sh		\
	modules/parsers/nasm/tests/equlocal.asm		\
	modules/parsers/nasm/tests/equlocal.errwarn	\
	modules/parsers/nasm/tests/equlocal.hex		\
	modules/parsers/nasm/tests/newsect.asm		\
	modules/parsers/nasm/tests/newsect.errwarn	\
	modules/parsers/nasm/tests/newsect.hex		\
	modules/parsers/nasm/tests/prevlocalwarn.asm	\
	modules/parsers/nasm/tests/prevlocalwarn.errwarn \
	modules/parsers/nasm/tests/prevlocalwarn.hex
