# $IdPath$

TESTS += \
	bitvect_test	\
	floatnum_test

noinst_PROGRAMS += \
	bitvect_test	\
	floatnum_test

bitvect_test_SOURCES = \
	src/tests/bitvect_test.c	\
	src/errwarn.c			\
	$(YASMBASEFILES)		\
	$(YASMPARSERFILES)		\
	$(YASMPREPROCFILES)		\
	$(YASMOPTIMIZERFILES)		\
	$(YASMOBJFMTFILES)		\
	$(YASMARCHFILES)		\
	$(CHECKFILES)
bitvect_test_LDFLAGS = -export-dynamic
bitvect_test_LDADD = @LIBLTDL@ @LIBOBJS@ $(INTLLIBS) @LIBADD_DL@

#bytecode_test_SOURCES = \
#	src/tests/bytecode_test.c	\
#	src/errwarn.c			\
#	$(YASMBASEFILES)		\
#	$(YASMPARSERFILES)		\
#	$(YASMPREPROCFILES)		\
#	$(YASMOPTIMIZERFILES)		\
#	$(YASMOBJFMTFILES)		\
#	$(YASMARCHFILES)		\
#	$(CHECKFILES)
#bytecode_test_LDFLAGS = -export-dynamic
#bytecode_test_LDADD = @LIBLTDL@ @LIBOBJS@ $(INTLLIBS) @LIBADD_DL@

floatnum_test_SOURCES = \
	src/tests/floatnum_test.c	\
	src/errwarn.c			\
	src/globals.c			\
	src/hamt.c			\
	src/bitvect.c			\
	src/xmalloc.c			\
	src/xstrdup.c			\
	$(CHECKFILES)
floatnum_test_LDFLAGS = -export-dynamic
floatnum_test_LDADD = @LIBLTDL@ @LIBOBJS@ $(INTLLIBS) @LIBADD_DL@

#memexpr_test_SOURCES = \
#	src/tests/memexpr_test.c	\
#	src/errwarn.c			\
#	$(YASMBASEFILES)		\
#	$(YASMPARSERFILES)		\
#	$(YASMPREPROCFILES)		\
#	$(YASMOPTIMIZERFILES)		\
#	$(YASMOBJFMTFILES)		\
#	$(YASMARCHFILES)		\
#	$(CHECKFILES)
#memexpr_test_LDFLAGS = -export-dynamic
#memexpr_test_LDADD = @LIBLTDL@ @LIBOBJS@ $(INTLLIBS) @LIBADD_DL@
