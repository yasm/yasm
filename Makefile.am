# $IdPath$

SUBDIRS = libltdl m4 po .

AM_YFLAGS = -d
AM_CFLAGS = @MORE_CFLAGS@
AM_CPPFLAGS = \
	-I$(top_srcdir)/check \
	@INCLTDL@

bin_PROGRAMS =
TESTS =
noinst_PROGRAMS =

include_HEADERS = libyasm.h

lib_LTLIBRARIES =

YASM_MODULES = 

BUILT_SOURCES =
# configure.lineno doesn't clean up after itself?
CLEANFILES = configure.lineno
EXTRA_DIST = config/config.rpath  \
	check/Makefile.inc	\
	tools/Makefile.inc	\
	libyasm/Makefile.inc	\
	modules/Makefile.inc	\
	frontends/Makefile.inc

include check/Makefile.inc
include tools/Makefile.inc
include libyasm/Makefile.inc
include modules/Makefile.inc
include frontends/Makefile.inc

EXTRA_DIST += \
	out_test.sh \
	test_hd.pl \
	COPYING.LIB-2.0 \
	splint.sh \
	Mkfiles/Makefile.flat \
	Mkfiles/Makefile.dj Mkfiles/dj/config.h \
	Mkfiles/Makefile.vc Mkfiles/vc/config.h

# Until this gets fixed in automake
DISTCLEANFILES = libyasm/stamp-h libyasm/stamp-h[0-9]*

ACLOCAL_AMFLAGS = -I m4

distclean-local:
	-rm -rf results
