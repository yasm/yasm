# $IdPath$

YASMPARSERFILES += \
	src/parsers/nasm/nasm-parser.c		\
	src/parsers/nasm/nasm-defs.h		\
	nasm-bison.y				\
	nasm-bison.h				\
	nasm-token.l

if DEV

nasm-token.l nasm-bison.y: $(srcdir)/src/arch/x86/instrs.dat $(srcdir)/src/parsers/nasm/token.l.in $(srcdir)/src/parsers/nasm/bison.y.in $(srcdir)/src/parsers/nasm/gen_instr.pl
	$(PERL) $(srcdir)/src/parsers/nasm/gen_instr.pl -i $(srcdir)/src/arch/x86/instrs.dat --sourcetoken $(srcdir)/src/parsers/nasm/token.l.in -t nasm-token.l --sourcegrammar $(srcdir)/src/parsers/nasm/bison.y.in -g nasm-bison.y

else

nasm-token.l: $(srcdir)/nasm-token.l
	@echo Warning: Not generating nasm-token.l from nasm-token.l.in.
	@echo Run configure with --enable-dev to enable generation.
	cp $(srcdir)/nasm-token.l .
nasm-token.c: $(srcdir)/nasm-token.c
	@echo Warning: Not generating nasm-token.c from nasm-token.l.
	@echo Run configure with --enable-dev to enable generation.
	cp $(srcdir)/nasm-token.c .
nasm-bison.y: $(srcdir)/nasm-bison.y
	@echo Warning: Not generating nasm-bison.y from nasm-bison.y.in.
	@echo Run configure with --enable-dev to enable generation.
	cp $(srcdir)/nasm-bison.y .
nasm-bison.c: $(srcdir)/nasm-bison.c
	@echo Warning: Not generating nasm-bison.c from nasm-bison.y.
	@echo Run configure with --enable-dev to enable generation.
	cp $(srcdir)/nasm-bison.c .
nasm-bison.h: $(srcdir)/nasm-bison.h
	@echo Warning: Not generating nasm-bison.h from nasm-bison.y.
	@echo Run configure with --enable-dev to enable generation.
	cp $(srcdir)/nasm-bison.h .

endif

noinst_SCRIPTS = src/parsers/nasm/gen_instr.pl

BUILT_SOURCES += \
	nasm-bison.y				\
	nasm-bison.c				\
	nasm-bison.h				\
	nasm-token.l				\
	nasm-token.c

CLEANFILES += \
	nasm-bison.y				\
	nasm-bison.c				\
	nasm-bison.h				\
	nasm-token.l				\
	nasm-token.c

EXTRA_DIST += \
	src/parsers/nasm/token.l.in		\
	src/parsers/nasm/bison.y.in		\
	src/parsers/nasm/gen_instr.pl
