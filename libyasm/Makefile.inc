# $IdPath$

lib_LTLIBRARIES += libyasm.la
libyasm_la_SOURCES = \
	libyasm/bytecode.c	\
	libyasm/bytecode.h	\
	libyasm/bc-int.h	\
	libyasm/errwarn.h	\
	libyasm/expr.c		\
	libyasm/expr.h		\
	libyasm/expr-int.h	\
	libyasm/symrec.c	\
	libyasm/symrec.h	\
	libyasm/linemgr.h	\
	libyasm/util.h		\
	libyasm/coretype.h	\
	libyasm/file.c		\
	libyasm/file.h		\
	libyasm/section.c	\
	libyasm/section.h	\
	libyasm/arch.c		\
	libyasm/arch.h		\
	libyasm/dbgfmt.h	\
	libyasm/objfmt.h	\
	libyasm/optimizer.h	\
	libyasm/parser.h	\
	libyasm/preproc.h	\
	libyasm/intnum.c	\
	libyasm/intnum.h	\
	libyasm/floatnum.c	\
	libyasm/floatnum.h	\
	libyasm/hamt.c		\
	libyasm/hamt.h		\
	libyasm/bitvect.c	\
	libyasm/bitvect.h	\
	libyasm/valparam.c	\
	libyasm/valparam.h	\
	libyasm/errwarn.c	\
	libyasm/linemgr.c	\
	libyasm/xmalloc.c	\
	libyasm/xstrdup.c	\
	libyasm/strcasecmp.c
libyasm_la_LIBADD = @LTLIBOBJS@
libyasm_la_LDFLAGS = -no-undefined

EXTRA_DIST += \
	libyasm/tests/Makefile.inc

include libyasm/tests/Makefile.inc

EXTRA_libyasm_la_SOURCES = \
	libyasm/compat-queue.h
