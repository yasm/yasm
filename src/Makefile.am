# $IdPath$

SUBDIRS = arch parsers preprocs optimizers objfmts . tests

INCLUDES = -I$(top_builddir)/intl

bin_PROGRAMS = yasm

yasm_SOURCES = \
	main.c			\
	errwarn.c		\
	errwarn.h

yasm_LDADD = \
	arch/@ARCH@/libarch.a		\
	parsers/nasm/libparser.a	\
	preprocs/raw/libpreproc.a	\
	optimizers/dbg/liboptimizer.a	\
	objfmts/dbg/libobjfmt.a		\
	libyasm.a			\
	$(INTLLIBS)

noinst_LIBRARIES = libyasm.a

libyasm_a_SOURCES = \
	bytecode.c		\
	bytecode.h		\
	bc-int.h		\
	expr.c			\
	expr.h			\
	expr-int.h		\
	symrec.c		\
	symrec.h		\
	globals.c		\
	globals.h		\
	util.h			\
	coretype.h		\
	file.c			\
	file.h			\
	section.c		\
	section.h		\
	arch.c			\
	arch.h			\
	objfmt.h		\
	options.h		\
	options.c		\
	preproc.h		\
	parser.c		\
	parser.h		\
	optimizer.h		\
	intnum.c		\
	intnum.h		\
	floatnum.c		\
	floatnum.h		\
	ternary.c		\
	ternary.h		\
	bitvect.c		\
	bitvect.h		\
	xmalloc.c		\
	xstrdup.c		\
	strcasecmp.c

libyasm_a_LIBADD = \
	@LIBOBJS@

CFLAGS = @ANSI_CFLAGS@

EXTRA_DIST = \
	strsep.c		\
	mergesort.c		\
	compat-queue.h		\
	lclint.sh
