# $IdPath$

SUBDIRS = libltdl tools m4 po

CFLAGS = @ANSI_CFLAGS@
AM_YFLAGS = -d
INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/check \
	@INCLTDL@

bin_PROGRAMS = yasm
TESTS =
noinst_PROGRAMS =

yasm_SOURCES =
yasm_LDADD = @LIBLTDL@ @LIBOBJS@ $(INTLLIBS) @LIBADD_DL@

lib_LTLIBRARIES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = config/config.rpath  \
	check/Makefile.inc	\
	src/Makefile.inc

include check/Makefile.inc
include src/Makefile.inc

EXTRA_DIST += \
	strsep.c mergesort.c \
	COPYING.LIB-2.0 \
	splint.sh \
	Mkfiles/Makefile.flat \
	Mkfiles/Makefile.dj Mkfiles/dj/config.h \
	Mkfiles/Makefile.vc Mkfiles/vc/config.h

ACLOCAL_AMFLAGS = -I m4

SUFFIXES = .re
.re.c: tools/re2c/re2c$(EXEEXT)
	tools/re2c/re2c -b $< | tools/re2c/cleanup.pl > $@
