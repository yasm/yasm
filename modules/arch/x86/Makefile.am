# $IdPath$

noinst_LIBRARIES = libarch.a

libarch_a_SOURCES = \
	x86arch.h	\
	x86-int.h	\
	arch.c		\
	bytecode.c	\
	expr.c

INCLUDES = \
	-I$(top_srcdir)/src	\
	-I$(top_builddir)/intl

CFLAGS = @ANSI_CFLAGS@

EXTRA_DIST = \
	README		\
	instrs.dat
