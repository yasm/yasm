# $Id$

EXTRA_DIST += modules/objfmts/dbg/Makefile.inc
EXTRA_DIST += modules/objfmts/bin/Makefile.inc
EXTRA_DIST += modules/objfmts/elf/Makefile.inc
#!EXTRA_DIST += modules/objfmts/omf/Makefile.inc
EXTRA_DIST += modules/objfmts/coff/Makefile.inc
EXTRA_DIST += modules/objfmts/win32/Makefile.inc
EXTRA_DIST += modules/objfmts/xdf/Makefile.inc

include modules/objfmts/dbg/Makefile.inc
include modules/objfmts/bin/Makefile.inc
include modules/objfmts/elf/Makefile.inc
#!include modules/objfmts/omf/Makefile.inc
include modules/objfmts/coff/Makefile.inc
include modules/objfmts/win32/Makefile.inc
include modules/objfmts/xdf/Makefile.inc
