# $IdPath$

TESTS += \
	src/arch/x86/tests/x86_test.sh

EXTRA_DIST += \
	src/arch/x86/tests/x86_test.sh		\
	src/arch/x86/tests/addbyte.asm		\
	src/arch/x86/tests/addbyte.errwarn	\
	src/arch/x86/tests/addbyte.hex		\
	src/arch/x86/tests/addrop.asm		\
	src/arch/x86/tests/addrop.errwarn	\
	src/arch/x86/tests/addrop.hex		\
	src/arch/x86/tests/addrop-err.asm	\
	src/arch/x86/tests/addrop-err.errwarn	\
	src/arch/x86/tests/cpubasic-err.asm	\
	src/arch/x86/tests/cpubasic-err.errwarn	\
	src/arch/x86/tests/div-err.asm		\
	src/arch/x86/tests/div-err.errwarn	\
	src/arch/x86/tests/effaddr.asm		\
	src/arch/x86/tests/effaddr.errwarn	\
	src/arch/x86/tests/effaddr.hex		\
	src/arch/x86/tests/lds-err.asm		\
	src/arch/x86/tests/lds-err.errwarn	\
	src/arch/x86/tests/opsize-err.asm	\
	src/arch/x86/tests/opsize-err.errwarn	\
	src/arch/x86/tests/ret.asm		\
	src/arch/x86/tests/ret.errwarn		\
	src/arch/x86/tests/ret.hex		\
	src/arch/x86/tests/shift.asm		\
	src/arch/x86/tests/shift.errwarn	\
	src/arch/x86/tests/shift.hex

CLEANFILES += \
	addbyte \
	addbyte.ew \
	addbyte.hx \
	addrop \
	addrop.ew \
	addrop.hx \
	addrop-err.ew \
	cpubasic-err.ew \
	div-err.ew \
	effaddr \
	effaddr.ew \
	effaddr.hx \
	lds-err.ew \
	opsize-err.ew \
	ret \
	ret.ew \
	ret.hx \
	shift \
	shift.ew \
	shift.hx
