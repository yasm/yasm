# $IdPath$

TESTS += \
	src/objfmts/bin/tests/objfmt_test.sh

EXTRA_DIST += \
	src/objfmts/bin/tests/objfmt_test.sh		\
	src/objfmts/bin/tests/float-err.asm		\
	src/objfmts/bin/tests/float-err.errwarn		\
	src/objfmts/bin/tests/float.asm			\
	src/objfmts/bin/tests/float.bin.hx		\
	src/objfmts/bin/tests/float.errwarn		\
	src/objfmts/bin/tests/integer-warn.asm		\
	src/objfmts/bin/tests/integer-warn.bin.hx	\
	src/objfmts/bin/tests/integer-warn.errwarn	\
	src/objfmts/bin/tests/integer.asm		\
	src/objfmts/bin/tests/integer.bin.hx		\
	src/objfmts/bin/tests/integer.errwarn		\
	src/objfmts/bin/tests/reserve.asm		\
	src/objfmts/bin/tests/reserve.bin.hx		\
	src/objfmts/bin/tests/reserve.errwarn

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
	reserve.hx
