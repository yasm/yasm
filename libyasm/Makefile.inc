# $IdPath: yasm/libyasm/Makefile.inc,v 1.17 2003/03/15 05:07:47 peter Exp $

lib_LTLIBRARIES += libyasm.la
libyasm_la_SOURCES = \
	libyasm/bytecode.c	\
	libyasm/expr.c		\
	libyasm/symrec.c	\
	libyasm/file.c		\
	libyasm/section.c	\
	libyasm/arch.c		\
	libyasm/intnum.c	\
	libyasm/floatnum.c	\
	libyasm/hamt.c		\
	libyasm/bitvect.c	\
	libyasm/valparam.c	\
	libyasm/errwarn.c	\
	libyasm/linemgr.c	\
	libyasm/xmalloc.c	\
	libyasm/xstrdup.c	\
	libyasm/strcasecmp.c	\
	libyasm/mergesort.c	\
	libyasm/strsep.c
libyasm_la_LDFLAGS = -no-undefined

modincludedir = $(includedir)/libyasm
modinclude_HEADERS = \
	libyasm/bytecode.h	\
	libyasm/bc-int.h	\
	libyasm/errwarn.h	\
	libyasm/expr.h		\
	libyasm/expr-int.h	\
	libyasm/symrec.h	\
	libyasm/linemgr.h	\
	libyasm/coretype.h	\
	libyasm/file.h		\
	libyasm/section.h	\
	libyasm/arch.h		\
	libyasm/dbgfmt.h	\
	libyasm/objfmt.h	\
	libyasm/optimizer.h	\
	libyasm/parser.h	\
	libyasm/preproc.h	\
	libyasm/intnum.h	\
	libyasm/floatnum.h	\
	libyasm/hamt.h		\
	libyasm/bitvect.h	\
	libyasm/valparam.h	\
	libyasm/compat-queue.h

EXTRA_DIST += \
	libyasm/tests/Makefile.inc

include libyasm/tests/Makefile.inc
