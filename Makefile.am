# $IdPath$

SUBDIRS = intl po doc

CFLAGS = @ANSI_CFLAGS@
INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_srcdir)/check

bin_PROGRAMS = yasm
TESTS =
noinst_PROGRAMS =

yasm_SOURCES =
yasm_LDADD = $(INTLLIBS)

BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = \
	check/Makefile.inc	\
	src/Makefile.inc

include check/Makefile.inc
include src/Makefile.inc

EXTRA_DIST += \
	config/install-sh config/missing config/mkinstalldirs \
	config/config.guess config/config.sub COPYING.LIB-2.0 \
	Mkfiles/Makefile.flat \
	Mkfiles/Makefile.dj Mkfiles/config.h.dj \
	Mkfiles/Makefile.vc Mkfiles/config.h.vc
