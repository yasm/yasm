# $IdPath$

YASMPARSERFILES += \
	src/parsers/nasm/nasm-parser.c		\
	src/parsers/nasm/nasm-defs.h		\
	src/parsers/nasm/nasm-bison.y		\
	nasm-bison.h				\
	nasm-token.c

nasm-token.c: $(srcdir)/src/parsers/nasm/nasm-token.re re2c$(EXEEXT) $(srcdir)/tools/re2c/cleanup.pl
	re2c$(EXEEXT) -b $(srcdir)/src/parsers/nasm/nasm-token.re | $(PERL) $(srcdir)/tools/re2c/cleanup.pl > $@

BUILT_SOURCES += \
	nasm-bison.c				\
	nasm-bison.h				\
	nasm-token.c

CLEANFILES += \
	nasm-bison.c				\
	nasm-bison.h				\
	nasm-token.c

EXTRA_DIST += \
	src/parsers/nasm/nasm-token.re
