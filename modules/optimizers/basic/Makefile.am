# $IdPath$

noinst_LIBRARIES = liboptimizer.a

liboptimizer_a_SOURCES = \
	optimizer.c

INCLUDES = \
	-I$(top_srcdir)/src	\
	-I$(top_builddir)/intl

if DEV
CFLAGS = -ansi -pedantic -Wall -g
endif
