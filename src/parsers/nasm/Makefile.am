# $IdPath$

noinst_LIBRARIES = libparser.a

libparser_a_SOURCES = \
	parser.c		\
	bison.y			\
	token.l

noinst_SCRIPTS = gen_instr.pl

AM_YFLAGS = -d --name-prefix=nasm_parser_
AM_LFLAGS = -Pnasm_parser_ -olex.yy.c

INCLUDES = \
	-I$(top_srcdir)/src	\
	-I../../../intl

if DEV
CFLAGS = -ansi -pedantic -Wall -g
endif

token.l bison.y: $(top_srcdir)/src/instrs.dat token.l.in bison.y.in gen_instr.pl
	$(PERL) gen_instr.pl -i $(top_srcdir)/src/instrs.dat -t token.l -g bison.y

BUILT_SOURCES = token.l bison.y

EXTRA_DIST = \
	token.l.in		\
	bison.y.in		\
	bison.h			\
	gen_instr.pl
