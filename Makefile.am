# $IdPath$

SUBDIRS = libltdl m4 po .

AM_YFLAGS = -d
AM_CFLAGS = @MORE_CFLAGS@
AM_CPPFLAGS  = -DYASM_MODULEDIR="\"${pkglibdir}\""
AM_CPPFLAGS += -I$(top_srcdir)/check
AM_CPPFLAGS += @INCLTDL@

bin_PROGRAMS =
TESTS =
noinst_PROGRAMS =

include_HEADERS = libyasm.h

noinst_HEADERS = util.h

lib_LTLIBRARIES =
pkglib_LTLIBRARIES =

YASM_MODULES = 

BUILT_SOURCES =
# configure.lineno doesn't clean up after itself?
CLEANFILES = configure.lineno
EXTRA_DIST = config/config.rpath

EXTRA_DIST += check/Makefile.inc
EXTRA_DIST += tools/Makefile.inc
EXTRA_DIST += libyasm/Makefile.inc
EXTRA_DIST += modules/Makefile.inc
EXTRA_DIST += frontends/Makefile.inc

include check/Makefile.inc
include tools/Makefile.inc
include libyasm/Makefile.inc
include modules/Makefile.inc
include frontends/Makefile.inc

EXTRA_DIST += basename.c
EXTRA_DIST += dirname.c
EXTRA_DIST += out_test.sh
EXTRA_DIST += test_hd.pl
EXTRA_DIST += Artistic.txt
EXTRA_DIST += BSD.txt
EXTRA_DIST += GNU_GPL-2.0
EXTRA_DIST += GNU_LGPL-2.0
EXTRA_DIST += splint.sh
EXTRA_DIST += Mkfiles/Makefile.flat
EXTRA_DIST += Mkfiles/Makefile.dj
EXTRA_DIST += Mkfiles/dj/config.h
EXTRA_DIST += Mkfiles/vc/yasm-module.c
EXTRA_DIST += Mkfiles/vc/yasm.dep
EXTRA_DIST += Mkfiles/vc/yasm.dsp
EXTRA_DIST += Mkfiles/vc/yasm.dsw
EXTRA_DIST += Mkfiles/vc/yasm.mak
EXTRA_DIST += Mkfiles/vc/yasm.sln
EXTRA_DIST += Mkfiles/vc/yasm.suo
EXTRA_DIST += Mkfiles/vc/yasm.vcproj
EXTRA_DIST += Mkfiles/vc/config.h
EXTRA_DIST += Mkfiles/vc/libyasm/libyasm.dep
EXTRA_DIST += Mkfiles/vc/libyasm/libyasm.dsp
EXTRA_DIST += Mkfiles/vc/libyasm/libyasm.mak
EXTRA_DIST += Mkfiles/vc/libyasm/libyasm.vcproj
EXTRA_DIST += Mkfiles/vc/modules/build.bat
EXTRA_DIST += Mkfiles/vc/modules/modules.dep
EXTRA_DIST += Mkfiles/vc/modules/modules.dsp
EXTRA_DIST += Mkfiles/vc/modules/modules.mak
EXTRA_DIST += Mkfiles/vc/modules/modules.vcproj

# Until this gets fixed in automake
DISTCLEANFILES = libyasm/stamp-h libyasm/stamp-h[0-9]*

ACLOCAL_AMFLAGS = -I m4

distclean-local:
	-rm -rf results
