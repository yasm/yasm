# $IdPath$

TESTS += \
	src/objfmts/bin/tests/bin_test.sh

EXTRA_DIST += \
	src/objfmts/bin/tests/bin_test.sh		\
	src/objfmts/bin/tests/float-err.asm		\
	src/objfmts/bin/tests/float-err.errwarn		\
	src/objfmts/bin/tests/float.asm			\
	src/objfmts/bin/tests/float.hex			\
	src/objfmts/bin/tests/float.errwarn		\
	src/objfmts/bin/tests/integer-warn.asm		\
	src/objfmts/bin/tests/integer-warn.hex		\
	src/objfmts/bin/tests/integer-warn.errwarn	\
	src/objfmts/bin/tests/integer.asm		\
	src/objfmts/bin/tests/integer.hex		\
	src/objfmts/bin/tests/integer.errwarn		\
	src/objfmts/bin/tests/reserve.asm		\
	src/objfmts/bin/tests/reserve.hex		\
	src/objfmts/bin/tests/reserve.errwarn		\
	src/objfmts/bin/tests/reserve-err.asm		\
	src/objfmts/bin/tests/reserve-err.errwarn

CLEANFILES += \
	float-err.ew \
	float \
	float.hx \
	float.ew \
	integer-warn \
	integer-warn.hx \
	integer-warn.ew \
	integer \
	integer.hx \
	integer.ew \
	reserve \
	reserve.ew \
	reserve.hx \
	reserve-err.ew
