# $Id$

SUBDIRS = po .

AM_YFLAGS = -d
AM_CFLAGS = @MORE_CFLAGS@

bin_PROGRAMS =
man_MANS =
TESTS =
noinst_PROGRAMS =

check_PROGRAMS = test_hd

test_hd_SOURCES = test_hd.c

include_HEADERS = libyasm.h

noinst_HEADERS = util.h

BUILT_SOURCES =
# configure.lineno doesn't clean up after itself?
CLEANFILES = configure.lineno
EXTRA_DIST = config/config.rpath

# automake doesn't distribute mkinstalldirs?
EXTRA_DIST += config/mkinstalldirs

EXTRA_DIST += tools/Makefile.inc
EXTRA_DIST += libyasm/Makefile.inc
EXTRA_DIST += modules/Makefile.inc
EXTRA_DIST += frontends/Makefile.inc

include tools/Makefile.inc

YASM_MODULES = 
lib_LIBRARIES = libyasm.a
libyasm_a_SOURCES =
include modules/Makefile.inc

include libyasm/Makefile.inc
include frontends/Makefile.inc
include m4/Makefile.inc

EXTRA_DIST += out_test.sh
EXTRA_DIST += Artistic.txt
EXTRA_DIST += BSD.txt
EXTRA_DIST += GNU_GPL-2.0
EXTRA_DIST += GNU_LGPL-2.0
EXTRA_DIST += splint.sh
EXTRA_DIST += Mkfiles/Makefile.flat
EXTRA_DIST += Mkfiles/Makefile.dj
EXTRA_DIST += Mkfiles/dj/config.h
EXTRA_DIST += Mkfiles/vc/yasm.sln
EXTRA_DIST += Mkfiles/vc/yasm.vcproj
EXTRA_DIST += Mkfiles/vc/config.h
EXTRA_DIST += Mkfiles/vc/genmacro/genmacro.vcproj
EXTRA_DIST += Mkfiles/vc/genmacro/run.bat
EXTRA_DIST += Mkfiles/vc/libyasm/libyasm.vcproj
EXTRA_DIST += Mkfiles/vc/modules/modules.vcproj
EXTRA_DIST += Mkfiles/vc/re2c/re2c.vcproj
EXTRA_DIST += Mkfiles/vc/re2c/run.bat

# Until this gets fixed in automake
DISTCLEANFILES = libyasm/stamp-h libyasm/stamp-h[0-9]*

ACLOCAL_AMFLAGS = -I m4

distclean-local:
	-rm -rf results

if BUILD_MAN
.xml.1:
	$(XMLTO) -o $(@D) man $<
.xml.7:
	$(XMLTO) -o $(@D) man $<
else
.xml.1:
	@echo "Not rebuilding man page from $<"
	cp `test -f $*.1 || echo '$(srcdir)/'`$*.1 $@
.xml.7:
	@echo "Not rebuilding man page from $<"
	cp `test -f $*.7 || echo '$(srcdir)/'`$*.7 $@
endif

BUILT_SOURCES += $(man_MANS)
DISTCLEANFILES += $(man_MANS)
EXTRA_DIST += $(man_MANS)
