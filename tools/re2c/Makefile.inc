# $IdPath$

noinst_PROGRAMS += re2c

re2c_SOURCES = \
	tools/re2c/basics.h	\
	tools/re2c/globals.h	\
	tools/re2c/ins.h		\
	tools/re2c/re.h		\
	tools/re2c/token.h		\
	tools/re2c/code.c		\
	tools/re2c/dfa.h		\
	tools/re2c/dfa.c		\
	tools/re2c/re2c.c		\
	tools/re2c/parse.h		\
	tools/re2c/re2c-parser.y	\
	tools/re2c/actions.c	\
	tools/re2c/scanner.h	\
	tools/re2c/scanner.c	\
	tools/re2c/substr.h	\
	tools/re2c/substr.c	\
	tools/re2c/translate.c

BUILT_SOURCES += \
	re2c-parser.c \
	re2c-parser.h

CLEANFILES += \
	re2c-parser.c \
	re2c-parser.h

EXTRA_DIST += \
	tools/re2c/CHANGELOG			\
	tools/re2c/NO_WARRANTY			\
	tools/re2c/README			\
	tools/re2c/scanner.re			\
	tools/re2c/re2c.1			\
	tools/re2c/bootstrap/re2c.man		\
	tools/re2c/bootstrap/scanner.c		\
	tools/re2c/doc/loplas.ps.gz		\
	tools/re2c/doc/sample.bib		\
	tools/re2c/examples/basemmap.c		\
	tools/re2c/examples/c.re		\
	tools/re2c/examples/cmmap.re		\
	tools/re2c/examples/cnokw.re		\
	tools/re2c/examples/cunroll.re		\
	tools/re2c/examples/modula.re		\
	tools/re2c/examples/sample.re		\
	tools/re2c/examples/simple.re		\
	tools/re2c/examples/rexx/README		\
	tools/re2c/examples/rexx/rexx.l		\
	tools/re2c/examples/rexx/scanio.c	\
	tools/re2c/cleanup.pl
