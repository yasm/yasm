SUBDIRS = parsers preprocs outfmts

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
	outfmt.h		\
	preproc.h		\
	parser.h

yasm_LDADD = \
	parsers/nasm/libparser.a	\
	preprocs/raw/libpreproc.a	\
	outfmts/dbg/liboutfmt.a

if DEV
CFLAGS = -ansi -pedantic -Wall -g
endif

EXTRA_DIST = \
	instrs.dat		\
	strdup.c		\
	strtoul.c
