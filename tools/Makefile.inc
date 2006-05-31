# $Id$

EXTRA_DIST += tools/re2c/Makefile.inc
EXTRA_DIST += tools/gap/Makefile.inc
EXTRA_DIST += tools/python-yasm/Makefile.inc

include tools/re2c/Makefile.inc
include tools/gap/Makefile.inc
include tools/python-yasm/Makefile.inc
