# $IdPath$

pkglib_LTLIBRARIES += objfmt_elf.la

objfmt_elf_la_SOURCES = modules/objfmts/elf/elf.c
objfmt_elf_la_SOURCES += modules/objfmts/elf/elf.h
objfmt_elf_la_SOURCES += modules/objfmts/elf/elf-objfmt.c
objfmt_elf_la_LDFLAGS = -module -avoid-version -no-undefined
objfmt_elf_la_LIBADD = libyasm.la
YASM_MODULES += -dlopen objfmt_elf.la

EXTRA_DIST += modules/objfmts/elf/tests/Makefile.inc

include modules/objfmts/elf/tests/Makefile.inc
