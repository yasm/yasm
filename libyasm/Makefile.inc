# $IdPath$

YASMBASEFILES = \
	src/bytecode.c		\
	src/bytecode.h		\
	src/bc-int.h		\
	src/expr.c		\
	src/expr.h		\
	src/expr-int.h		\
	src/symrec.c		\
	src/symrec.h		\
	src/globals.c		\
	src/globals.h		\
	src/util.h		\
	src/coretype.h		\
	src/file.c		\
	src/file.h		\
	src/section.c		\
	src/section.h		\
	src/arch.c		\
	src/arch.h		\
	src/objfmt.c		\
	src/objfmt.h		\
	src/preproc.h		\
	src/parser.c		\
	src/parser.h		\
	src/optimizer.h		\
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
	src/xmalloc.c		\
	src/xstrdup.c		\
	src/strcasecmp.c

yasm_SOURCES += \
	src/main.c		\
	src/errwarn.c		\
	src/errwarn.h		\
	src/options.c		\
	src/options.h		\
	$(YASMBASEFILES)


EXTRA_DIST += \
	src/arch/Makefile.inc		\
	src/parsers/Makefile.inc	\
	src/preprocs/Makefile.inc	\
	src/optimizers/Makefile.inc	\
	src/objfmts/Makefile.inc	\
	src/tests/Makefile.inc

include src/arch/Makefile.inc
include src/parsers/Makefile.inc
include src/preprocs/Makefile.inc
include src/optimizers/Makefile.inc
include src/objfmts/Makefile.inc
include src/tests/Makefile.inc

EXTRA_DIST += \
	src/compat-queue.h	\
	src/lclint.sh
