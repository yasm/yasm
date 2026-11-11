# Makefile for cpp module.
# Copied from raw preprocessor module.

libyasm_a_SOURCES += modules/preprocs/cpp/cpp-preproc.c

YASM_MODULES += preproc_cpp

EXTRA_DIST += modules/preprocs/cpp/tests/Makefile.inc

include modules/preprocs/cpp/tests/Makefile.inc
