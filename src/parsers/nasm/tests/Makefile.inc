# $IdPath$

TESTS += \
	src/parsers/nasm/tests/nasm_test.sh

EXTRA_DIST += \
	src/parsers/nasm/tests/nasm_test.sh	\
	src/parsers/nasm/tests/equlocal.asm	\
	src/parsers/nasm/tests/equlocal.errwarn	\
	src/parsers/nasm/tests/equlocal.hex	\
	src/parsers/nasm/tests/newsect.asm	\
	src/parsers/nasm/tests/newsect.errwarn	\
	src/parsers/nasm/tests/newsect.hex

CLEANFILES += \
	equlocal \
	equlocal.ew \
	equlocal.hx \
	newsect \
	newsect.ew \
	newsect.hx
