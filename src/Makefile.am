# $IdPath$

SUBDIRS = parsers preprocs optimizers objfmts . tests

INCLUDES = -I$(top_builddir)/intl

bin_PROGRAMS = yasm

yasm_SOURCES = main.c

yasm_LDADD = \
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
	errwarn.c		\
	errwarn.h		\
	expr.c			\
	expr.h			\
	symrec.c		\
	symrec.h		\
	globals.c		\
	globals.h		\
	util.h			\
	file.c			\
	file.h			\
	section.c		\
	section.h		\
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

CFLAGS = @ANSI_CFLAGS@

EXTRA_DIST = \
	instrs.dat		\
	compat-queue.h
