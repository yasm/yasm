# $IdPath$

SUBDIRS = libltdl m4 po .

AM_YFLAGS = -d
AM_CFLAGS = @ANSI_CFLAGS@
AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/check \
	@INCLTDL@

bin_PROGRAMS = yasm
TESTS =
noinst_PROGRAMS =

yasm_SOURCES =
yasm_LDFLAGS =
yasm_LDADD = libyasm.la @LIBLTDL@ $(INTLLIBS) @LIBADD_DL@
#EXTRA_yasm_SOURCES = \
#	strsep.c mergesort.c

lib_LTLIBRARIES = libyasm.la
libyasm_la_LIBADD = @LTLIBOBJS@

BUILT_SOURCES =
# configure.lineno doesn't clean up after itself?
CLEANFILES = configure.lineno
EXTRA_DIST = config/config.rpath  \
	check/Makefile.inc	\
	tools/Makefile.inc	\
	src/Makefile.inc

include check/Makefile.inc
include tools/Makefile.inc
include src/Makefile.inc

EXTRA_DIST += \
	out_test.sh \
	test_hd.pl \
	COPYING.LIB-2.0 \
	splint.sh \
	Mkfiles/Makefile.flat \
	Mkfiles/Makefile.dj Mkfiles/dj/config.h \
	Mkfiles/Makefile.vc Mkfiles/vc/config.h

ACLOCAL_AMFLAGS = -I m4
