# $IdPath$

noinst_PROGRAMS = lemon

lemon_SOURCES = \
	tools/lemon/lemon.c

EXTRA_DIST += \
	tools/lemon/Makefile.inc	\
	tools/lemon/lemon.html		\
	tools/lemon/lempar.c		\
	tools/lemon/README
