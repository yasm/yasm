# $IdPath$

noinst_PROGRAMS += re2c

re2c_CFLAGS =
re2c_SOURCES  = tools/re2c/main.c
re2c_SOURCES += tools/re2c/basics.h
re2c_SOURCES += tools/re2c/globals.h
re2c_SOURCES += tools/re2c/ins.h
re2c_SOURCES += tools/re2c/re.h
re2c_SOURCES += tools/re2c/token.h
re2c_SOURCES += tools/re2c/code.c
re2c_SOURCES += tools/re2c/dfa.h
re2c_SOURCES += tools/re2c/dfa.c
re2c_SOURCES += tools/re2c/parse.h
re2c_SOURCES += tools/re2c/re2c-parser.y
re2c_SOURCES += tools/re2c/actions.c
re2c_SOURCES += tools/re2c/scanner.h
re2c_SOURCES += tools/re2c/scanner.c
re2c_SOURCES += tools/re2c/substr.h
re2c_SOURCES += tools/re2c/substr.c
re2c_SOURCES += tools/re2c/translate.c

BUILT_SOURCES += re2c-parser.c
BUILT_SOURCES += re2c-parser.h

CLEANFILES += re2c-parser.c
CLEANFILES += re2c-parser.h

EXTRA_DIST += tools/re2c/CHANGELOG
EXTRA_DIST += tools/re2c/NO_WARRANTY
EXTRA_DIST += tools/re2c/README
EXTRA_DIST += tools/re2c/scanner.re
EXTRA_DIST += tools/re2c/re2c.1
EXTRA_DIST += tools/re2c/bootstrap/re2c.man
EXTRA_DIST += tools/re2c/bootstrap/scanner.c
EXTRA_DIST += tools/re2c/doc/loplas.ps.gz
EXTRA_DIST += tools/re2c/doc/sample.bib
EXTRA_DIST += tools/re2c/examples/basemmap.c
EXTRA_DIST += tools/re2c/examples/c.re
EXTRA_DIST += tools/re2c/examples/cmmap.re
EXTRA_DIST += tools/re2c/examples/cnokw.re
EXTRA_DIST += tools/re2c/examples/cunroll.re
EXTRA_DIST += tools/re2c/examples/modula.re
EXTRA_DIST += tools/re2c/examples/sample.re
EXTRA_DIST += tools/re2c/examples/simple.re
EXTRA_DIST += tools/re2c/examples/rexx/README
EXTRA_DIST += tools/re2c/examples/rexx/rexx.l
EXTRA_DIST += tools/re2c/examples/rexx/scanio.c
EXTRA_DIST += tools/re2c/cleanup.pl
