# $IdPath$

lib_LTLIBRARIES += libyasm.la

libyasm_la_SOURCES  = libyasm/bytecode.c
libyasm_la_SOURCES += libyasm/expr.c
libyasm_la_SOURCES += libyasm/symrec.c
libyasm_la_SOURCES += libyasm/file.c
libyasm_la_SOURCES += libyasm/section.c
libyasm_la_SOURCES += libyasm/arch.c
libyasm_la_SOURCES += libyasm/objfmt.c
libyasm_la_SOURCES += libyasm/intnum.c
libyasm_la_SOURCES += libyasm/floatnum.c
libyasm_la_SOURCES += libyasm/hamt.c
libyasm_la_SOURCES += libyasm/bitvect.c
libyasm_la_SOURCES += libyasm/valparam.c
libyasm_la_SOURCES += libyasm/errwarn.c
libyasm_la_SOURCES += libyasm/linemgr.c
libyasm_la_SOURCES += libyasm/assocdat.c
libyasm_la_SOURCES += libyasm/assocdat.h
libyasm_la_SOURCES += libyasm/xmalloc.c
libyasm_la_SOURCES += libyasm/xstrdup.c
libyasm_la_SOURCES += libyasm/strcasecmp.c
libyasm_la_SOURCES += libyasm/mergesort.c
libyasm_la_SOURCES += libyasm/strsep.c

libyasm_la_LDFLAGS = -no-undefined

modincludedir = $(includedir)/libyasm

modinclude_HEADERS  = libyasm/bytecode.h
modinclude_HEADERS += libyasm/bc-int.h
modinclude_HEADERS += libyasm/errwarn.h
modinclude_HEADERS += libyasm/expr.h
modinclude_HEADERS += libyasm/expr-int.h
modinclude_HEADERS += libyasm/symrec.h
modinclude_HEADERS += libyasm/linemgr.h
modinclude_HEADERS += libyasm/coretype.h
modinclude_HEADERS += libyasm/file.h
modinclude_HEADERS += libyasm/section.h
modinclude_HEADERS += libyasm/arch.h
modinclude_HEADERS += libyasm/dbgfmt.h
modinclude_HEADERS += libyasm/objfmt.h
modinclude_HEADERS += libyasm/optimizer.h
modinclude_HEADERS += libyasm/parser.h
modinclude_HEADERS += libyasm/preproc.h
modinclude_HEADERS += libyasm/intnum.h
modinclude_HEADERS += libyasm/floatnum.h
modinclude_HEADERS += libyasm/hamt.h
modinclude_HEADERS += libyasm/bitvect.h
modinclude_HEADERS += libyasm/valparam.h
modinclude_HEADERS += libyasm/compat-queue.h

EXTRA_DIST += libyasm/tests/Makefile.inc

include libyasm/tests/Makefile.inc
