noinst_LIBRARIES = libpreproc.a

libpreproc_a_SOURCES = \
	preproc.c

INCLUDES = \
	-I$(top_srcdir)/src	\
	-I../../../intl

if DEV
CFLAGS = -ansi -pedantic -Wall -g
endif
