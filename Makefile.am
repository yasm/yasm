SUBDIRS = po .

ACLOCAL_AMFLAGS = -I m4
AM_YFLAGS = -d
AM_CFLAGS = @MORE_CFLAGS@

bin_PROGRAMS =
dist_man_MANS =
notrans_dist_man_MANS =
TESTS =
TESTS_ENVIRONMENT =
noinst_PROGRAMS = genstring

check_PROGRAMS = test_hd

test_hd_SOURCES = test_hd.c

include_HEADERS = libyasm.h
nodist_include_HEADERS = libyasm-stdint.h

noinst_HEADERS = util.h

BUILT_SOURCES =
MAINTAINERCLEANFILES =
DISTCLEANFILES =
SUFFIXES =

# configure.lineno doesn't clean up after itself?
CLEANFILES = configure.lineno
EXTRA_DIST = config/config.rpath

# libyasm-stdint.h doesn't clean up after itself?
CONFIG_CLEAN_FILES = libyasm-stdint.h
CONFIG_CLEAN_FILES += YASM-VERSION-FILE
CONFIG_CLEAN_FILES += YASM-VERSION.h

EXTRA_DIST += tools/Makefile.inc
EXTRA_DIST += libyasm/Makefile.inc
EXTRA_DIST += modules/Makefile.inc
EXTRA_DIST += frontends/Makefile.inc

include tools/Makefile.inc

YASM_MODULES = 
lib_LIBRARIES = libyasm.a
libyasm_a_SOURCES =
nodist_libyasm_a_SOURCES =
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
EXTRA_DIST += YASM-VERSION-GEN.sh
EXTRA_DIST += CMakeLists.txt
EXTRA_DIST += ConfigureChecks.cmake
EXTRA_DIST += config.h.cmake
EXTRA_DIST += libyasm-stdint.h.cmake
EXTRA_DIST += cmake/CMakeLists.txt
EXTRA_DIST += cmake/modules/CMakeLists.txt
EXTRA_DIST += cmake/modules/DummyCFile.c
EXTRA_DIST += cmake/modules/VersionGen.cmake
EXTRA_DIST += cmake/modules/YasmMacros.cmake
EXTRA_DIST += frontends/CMakeLists.txt
EXTRA_DIST += frontends/tasm/CMakeLists.txt
EXTRA_DIST += frontends/vsyasm/CMakeLists.txt
EXTRA_DIST += frontends/yasm/CMakeLists.txt
EXTRA_DIST += frontends/yasm/genstring.py
EXTRA_DIST += frontends/yasm/yasm-plugin.c
EXTRA_DIST += frontends/yasm/yasm-plugin.h
EXTRA_DIST += libyasm/CMakeLists.txt
EXTRA_DIST += libyasm/cmake-module.c
EXTRA_DIST += modules/arch/CMakeLists.txt
EXTRA_DIST += modules/arch/lc3b/CMakeLists.txt
EXTRA_DIST += modules/arch/x86/CMakeLists.txt
EXTRA_DIST += modules/CMakeLists.txt
EXTRA_DIST += modules/dbgfmts/CMakeLists.txt
EXTRA_DIST += modules/dbgfmts/codeview/CMakeLists.txt
EXTRA_DIST += modules/dbgfmts/dwarf2/CMakeLists.txt
EXTRA_DIST += modules/dbgfmts/null/CMakeLists.txt
EXTRA_DIST += modules/dbgfmts/stabs/CMakeLists.txt
EXTRA_DIST += modules/listfmts/CMakeLists.txt
EXTRA_DIST += modules/listfmts/nasm/CMakeLists.txt
EXTRA_DIST += modules/objfmts/bin/CMakeLists.txt
EXTRA_DIST += modules/objfmts/CMakeLists.txt
EXTRA_DIST += modules/objfmts/coff/CMakeLists.txt
EXTRA_DIST += modules/objfmts/dbg/CMakeLists.txt
EXTRA_DIST += modules/objfmts/elf/CMakeLists.txt
EXTRA_DIST += modules/objfmts/macho/CMakeLists.txt
EXTRA_DIST += modules/objfmts/rdf/CMakeLists.txt
EXTRA_DIST += modules/objfmts/xdf/CMakeLists.txt
EXTRA_DIST += modules/parsers/CMakeLists.txt
EXTRA_DIST += modules/parsers/gas/CMakeLists.txt
EXTRA_DIST += modules/parsers/nasm/CMakeLists.txt
EXTRA_DIST += modules/preprocs/CMakeLists.txt
EXTRA_DIST += modules/preprocs/cpp/CMakeLists.txt
EXTRA_DIST += modules/preprocs/gas/CMakeLists.txt
EXTRA_DIST += modules/preprocs/nasm/CMakeLists.txt
EXTRA_DIST += modules/preprocs/raw/CMakeLists.txt
EXTRA_DIST += plugins/README
EXTRA_DIST += plugins/dbg/CMakeLists.txt
EXTRA_DIST += plugins/dbg/dbg-objfmt.c
EXTRA_DIST += plugins/dbg/init_plugin.c
EXTRA_DIST += plugins/dbg/README
EXTRA_DIST += plugins/x86/CMakeLists.txt
EXTRA_DIST += plugins/x86/init_plugin.c
EXTRA_DIST += plugins/x86/README
EXTRA_DIST += tools/CMakeLists.txt
EXTRA_DIST += tools/genmacro/CMakeLists.txt
EXTRA_DIST += tools/genperf/CMakeLists.txt
EXTRA_DIST += tools/re2c/CMakeLists.txt
EXTRA_DIST += Mkfiles/Makefile.flat
EXTRA_DIST += Mkfiles/Makefile.dj
EXTRA_DIST += Mkfiles/dj/config.h
EXTRA_DIST += Mkfiles/dj/libyasm-stdint.h

# Until this gets fixed in automake
DISTCLEANFILES += libyasm/stamp-h libyasm/stamp-h[0-9]*

dist-hook: YASM-VERSION-FILE YASM-VERSION.h
	cp YASM-VERSION-FILE $(distdir)/version
	cp YASM-VERSION.h $(distdir)/Mkfiles/dj/

YASM-VERSION-FILE: $(top_srcdir)/YASM-VERSION-GEN.sh
	$(top_srcdir)/YASM-VERSION-GEN.sh

distclean-local:
	-rm -rf results
if HAVE_PYTHON
	-rm -rf build
endif

# Until gets fixed libyasm cross-build for target system
# use fixinstall-yasm-XXX make targets
all-local: python-build
install-exec-hook: python-install
uninstall-hook: python-uninstall

if BUILD_MAN
MAINTAINERCLEANFILES += $(dist_man_MANS) $(notrans_dist_man_MANS)
endif

# genstring build
genstring_SOURCES =
EXTRA_DIST += genstring.c
genstring_LDADD = genstring.$(OBJEXT)
genstring_LINK = $(CCLD_FOR_BUILD) -o $@

genstring.$(OBJEXT): genstring.c
	$(CC_FOR_BUILD) $(DEFAULT_INCLUDES) $(INCLUDES) -c -o $@ `test -f genstring.c || echo '$(srcdir)/'`genstring.c

