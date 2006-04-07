# $Id$

SUBDIRS = po .

AM_YFLAGS = -d
AM_CFLAGS = @MORE_CFLAGS@

bin_PROGRAMS =
dist_man_MANS =
TESTS =
noinst_PROGRAMS = genstring

check_PROGRAMS = test_hd

test_hd_SOURCES = test_hd.c

include_HEADERS = libyasm.h

noinst_HEADERS = util.h

BUILT_SOURCES =

# configure.lineno doesn't clean up after itself?
CLEANFILES = configure.lineno
EXTRA_DIST = config/config.rpath

# _stdint.h doesn't clean up after itself?
CONFIG_CLEAN_FILES = _stdint.h

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
EXTRA_DIST += Mkfiles/dj/_stdint.h
EXTRA_DIST += Mkfiles/vc/yasm.sln
EXTRA_DIST += Mkfiles/vc/yasm.vcproj
EXTRA_DIST += Mkfiles/vc/config.h
EXTRA_DIST += Mkfiles/vc/_stdint.h
EXTRA_DIST += Mkfiles/vc/genmacro/genmacro.vcproj
EXTRA_DIST += Mkfiles/vc/genmacro/run.bat
EXTRA_DIST += Mkfiles/vc/genmodule/genmodule.vcproj
EXTRA_DIST += Mkfiles/vc/genmodule/run.bat
EXTRA_DIST += Mkfiles/vc/genstring/genstring.vcproj
EXTRA_DIST += Mkfiles/vc/genstring/run.bat
EXTRA_DIST += Mkfiles/vc/genversion/genversion.vcproj
EXTRA_DIST += Mkfiles/vc/genversion/run.bat
EXTRA_DIST += Mkfiles/vc/libyasm/libyasm.vcproj
EXTRA_DIST += Mkfiles/vc/modules/modules.vcproj
EXTRA_DIST += Mkfiles/vc/re2c/re2c.vcproj
EXTRA_DIST += Mkfiles/vc/re2c/run.bat
EXTRA_DIST += Mkfiles/vc/gap/gap.vcproj
EXTRA_DIST += Mkfiles/vc/gap/run.bat
EXTRA_DIST += Mkfiles/vc8/yasm.sln
EXTRA_DIST += Mkfiles/vc8/yasm.vcproj
EXTRA_DIST += Mkfiles/vc8/config.h
EXTRA_DIST += Mkfiles/vc8/_stdint.h
EXTRA_DIST += Mkfiles/vc8/readme.vc8.txt
EXTRA_DIST += Mkfiles/vc8/yasm.rules
EXTRA_DIST += Mkfiles/vc8/genmacro/genmacro.vcproj
EXTRA_DIST += Mkfiles/vc8/genmacro/run.bat
EXTRA_DIST += Mkfiles/vc8/genmodule/genmodule.vcproj
EXTRA_DIST += Mkfiles/vc8/genmodule/run.bat
EXTRA_DIST += Mkfiles/vc8/genstring/genstring.vcproj
EXTRA_DIST += Mkfiles/vc8/genstring/run.bat
EXTRA_DIST += Mkfiles/vc8/genversion/genversion.vcproj
EXTRA_DIST += Mkfiles/vc8/genversion/run.bat
EXTRA_DIST += Mkfiles/vc8/libyasm/libyasm.vcproj
EXTRA_DIST += Mkfiles/vc8/modules/modules.vcproj
EXTRA_DIST += Mkfiles/vc8/re2c/re2c.vcproj
EXTRA_DIST += Mkfiles/vc8/re2c/run.bat
EXTRA_DIST += Mkfiles/vc8/gap/gap.vcproj
EXTRA_DIST += Mkfiles/vc8/gap/run.bat

# Until this gets fixed in automake
DISTCLEANFILES = libyasm/stamp-h libyasm/stamp-h[0-9]*

ACLOCAL_AMFLAGS = -I m4

distclean-local:
	-rm -rf results

if BUILD_MAN
MAINTAINERCLEANFILES = $(dist_man_MANS)
endif

# genstring build
genstring_SOURCES =
EXTRA_DIST += genstring.c
genstring_LDADD = genstring.$(OBJEXT)
genstring_LINK = $(CCLD_FOR_BUILD) -o $@

genstring.$(OBJEXT): genstring.c
	$(CC_FOR_BUILD) $(DEFAULT_INCLUDES) $(INCLUDES) -c -o $@ `test -f genstring.c || echo '$(srcdir)/'`genstring.c

