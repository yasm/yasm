# $IdPath$
AM_YFLAGS = -d

INCLUDES = -I$(top_srcdir)

noinst_PROGRAMS = re2c

re2c_SOURCES = \
	basics.h	\
	globals.h	\
	ins.h		\
	re.h		\
	token.h		\
	code.c		\
	dfa.h		\
	dfa.c		\
	main.c		\
	parse.h		\
	parser.y	\
	actions.c	\
	scanner.h	\
	scanner.c	\
	substr.h	\
	substr.c	\
	translate.c

EXTRA_DIST = \
	CHANGELOG		\
	NO_WARRANTY		\
	README			\
	scanner.re		\
	re2c.1			\
	bootstrap/re2c.man	\
	bootstrap/scanner.c	\
	doc/loplas.ps.gz	\
	doc/sample.bib		\
	examples/basemmap.c	\
	examples/c.re		\
	examples/cmmap.re	\
	examples/cnokw.re	\
	examples/cunroll.re	\
	examples/modula.re	\
	examples/sample.re	\
	examples/simple.re	\
	examples/rexx/README	\
	examples/rexx/rexx.l	\
	examples/rexx/scanio.c
