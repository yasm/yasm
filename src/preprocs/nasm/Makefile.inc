# $IdPath$

#lib_LTLIBRARIES += yasm-nasm.la

yasm_nasm_la_SOURCES += \
	src/preprocs/nasm/nasm-preproc.c \
	src/preprocs/nasm/nasm-pp.h \
	src/preprocs/nasm/nasm-pp.c \
	src/preprocs/nasm/nasmlib.h \
	src/preprocs/nasm/nasmlib.c \
	src/preprocs/nasm/nasm-eval.h \
	src/preprocs/nasm/nasm-eval.c

src/preprocs/nasm/nasm-pp.c: nasm-macros.c

nasm-macros.c: src/preprocs/nasm/macros.pl src/preprocs/nasm/standard.mac
	$(PERL) src/preprocs/nasm/macros.pl src/preprocs/nasm/standard.mac

BUILT_SOURCES += nasm-macros.c

EXTRA_DIST += src/preprocs/nasm/macros.pl \
	src/preprocs/nasm/standard.mac

#yasm_nasm_la_LDFLAGS = -module -avoid-version
#yasm_nasm_la_LIBADD = libyasm.la
#yasm_LDADD += -dlopen yasm-nasm.la
