bin_PROGRAMS = yasm

yasm_SOURCES = \
	bison.y			\
	token.l			\
	bytecode.c		\
	errwarn.c		\
	expr.c			\
	main.c			\
	symrec.c

noinst_SCRIPTS = gen_instr.pl

AM_YFLAGS = -d

if DEV
CFLAGS = -ansi -pedantic -Wall -g
endif

token.l bison.y: instrs.dat token.l.in bison.y.in gen_instr.pl
	$(PERL) $(srcdir)/gen_instr.pl -i $(srcdir)/instrs.dat -t $(srcdir)/token.l -g $(srcdir)/bison.y

BUILT_SOURCES = token.l bison.y

EXTRA_DIST = \
	instrs.dat		\
	gen_instr.pl
