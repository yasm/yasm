# $IdPath$

SUBDIRS = parsers preprocs optimizers objfmts

INCLUDES = -I../intl

bin_PROGRAMS = yasm

yasm_SOURCES = \
	bytecode.c		\
	bytecode.h		\
	errwarn.c		\
	errwarn.h		\
	expr.c			\
	expr.h			\
	main.c			\
	symrec.c		\
	symrec.h		\
	globals.h		\
	util.h			\
	section.h		\
	objfmt.h		\
	preproc.h		\
	parser.c		\
	parser.h		\
	optimizer.h		\
	strcasecmp.c

yasm_LDADD = \
	parsers/nasm/libparser.a	\
	preprocs/raw/libpreproc.a	\
	optimizers/dbg/liboptimizer.a	\
	objfmts/dbg/libobjfmt.a		\
	$(INTLLIBS)

if DEV
CFLAGS = -ansi -pedantic -Wall -g
endif

EXTRA_DIST = \
	instrs.dat		\
	compat-queue.h		\
	strdup.c		\
	strtoul.c
