# $IdPath$

noinst_LIBRARIES = libparser.a

if DEV
libparser_a_SOURCES = \
	parser.c		\
	bison.y			\
	token.l
else
libparser_a_SOURCES = \
	parser.c		\
	bison.c			\
	token.c

bison.c: bison.y
	@echo "Run configure with --enable-dev before making changes here."
	@exit 1

token.c: token.l
	@echo "Run configure with --enable-dev before making changes here."
	@exit 1
endif

noinst_SCRIPTS = gen_instr.pl

AM_YFLAGS = -d --name-prefix=nasm_parser_
AM_LFLAGS = -Pnasm_parser_ -olex.yy.c

INCLUDES = \
	-I$(top_srcdir)/src	\
	-I$(top_builddir)/intl

CFLAGS = @ANSI_CFLAGS@

token.l bison.y: $(top_srcdir)/src/arch/@ARCH@/instrs.dat token.l.in bison.y.in gen_instr.pl
if DEV
	$(PERL) gen_instr.pl -i $(top_srcdir)/src/instrs.dat -t token.l -g bison.y
else
	@echo "Run configure with --enable-dev before making changes here."
	@exit 1
endif

BUILT_SOURCES = token.l bison.y

EXTRA_DIST = \
	token.l			\
	token.l.in		\
	bison.y			\
	bison.y.in		\
	bison.h			\
	gen_instr.pl
