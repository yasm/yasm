# $IdPath$

libyasm_la_SOURCES = \
	src/bytecode.c		\
	src/bytecode.h		\
	src/bc-int.h		\
	src/errwarn.h		\
	src/expr.c		\
	src/expr.h		\
	src/expr-int.h		\
	src/symrec.c		\
	src/symrec.h		\
	src/linemgr.h		\
	src/util.h		\
	src/coretype.h		\
	src/file.c		\
	src/file.h		\
	src/section.c		\
	src/section.h		\
	src/arch.c		\
	src/arch.h		\
	src/dbgfmt.h		\
	src/objfmt.h		\
	src/optimizer.h		\
	src/parser.h		\
	src/preproc.h		\
	src/intnum.c		\
	src/intnum.h		\
	src/floatnum.c		\
	src/floatnum.h		\
	src/hamt.c		\
	src/hamt.h		\
	src/bitvect.c		\
	src/bitvect.h		\
	src/valparam.c		\
	src/valparam.h		\
	src/errwarn.c		\
	src/linemgr.c		\
	src/xmalloc.c		\
	src/xstrdup.c		\
	src/strcasecmp.c

yasm_SOURCES += \
	src/main.c		\
	src/options.c		\
	src/options.h		\
	src/module.h		\
	src/module.c


EXTRA_DIST += \
	src/arch/Makefile.inc		\
	src/parsers/Makefile.inc	\
	src/preprocs/Makefile.inc	\
	src/optimizers/Makefile.inc	\
	src/objfmts/Makefile.inc	\
	src/tests/Makefile.inc

# Modules with more than one type of interface

lib_LTLIBRARIES += yasm-nasm.la
yasm_nasm_la_SOURCES =
yasm_nasm_la_LDFLAGS = -module -avoid-version
yasm_nasm_la_LIBADD = libyasm.la
yasm_LDADD += -dlopen yasm-nasm.la

include src/arch/Makefile.inc
include src/parsers/Makefile.inc
include src/preprocs/Makefile.inc
include src/optimizers/Makefile.inc
include src/dbgfmts/Makefile.inc
include src/objfmts/Makefile.inc
include src/tests/Makefile.inc

EXTRA_yasm_SOURCES += \
	src/compat-queue.h
