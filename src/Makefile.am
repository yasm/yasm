bin_PROGRAMS = yasm

INCLUDES = -I$(top_srcdir)/include

yasm_SOURCES = \
	lex.yy.c		\
	bison.tab.c		\
	bytecode.c		\
	errwarn.c		\
	main.c			\
	symrec.c

noinst_SCRIPTS = gen_instr.pl

# yes, generate it locally.
bison.tab.c: bison.y
	$(YACC) -d -t $(srcdir)/$< -o $@
	mv bison.tab.h $(top_srcdir)/include/

# yes, generate it locally.
lex.yy.c: token.l
	$(LEX) -o$@ $(srcdir)/$<

token.l bison.y: instrs.dat token.l.in bison.y.in gen_instr.pl
	$(PERL) $(srcdir)/gen_instr.pl -i $(srcdir)/instrs.dat -t $(srcdir)/token.l -g $(srcdir)/bison.y

BUILT_SOURCES = token.l bison.y lex.yy.c bison.tab.c

EXTRA_DIST = \
	instrs.dat		\
	gen_instr.pl		\
	token.l.in		\
	lex.yy.c		\
	bison.y.in		\
	bison.tab.c
