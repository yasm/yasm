# $IdPath$

TESTS += \
	bitvect_test	\
	floatnum_test

noinst_PROGRAMS += \
	bitvect_test	\
	floatnum_test

bitvect_test_SOURCES = \
	src/tests/bitvect_test.c	\
	$(YASMPARSERFILES)		\
	$(YASMPREPROCFILES)		\
	$(YASMOPTIMIZERFILES)		\
	$(YASMOBJFMTFILES)		\
	$(YASMARCHFILES)		\
	$(CHECKFILES)
bitvect_test_LDFLAGS =
bitvect_test_LDADD = libyasm.la @LIBLTDL@ @LIBOBJS@ $(INTLLIBS) @LIBADD_DL@

#bytecode_test_SOURCES = \
#	src/tests/bytecode_test.c	\
#	$(YASMPARSERFILES)		\
#	$(YASMPREPROCFILES)		\
#	$(YASMOPTIMIZERFILES)		\
#	$(YASMOBJFMTFILES)		\
#	$(YASMARCHFILES)		\
#	$(CHECKFILES)
#bytecode_test_LDFLAGS =
#bytecode_test_LDADD = libyasm.la @LIBLTDL@ @LIBOBJS@ $(INTLLIBS) @LIBADD_DL@

floatnum_test_SOURCES = \
	src/tests/floatnum_test.c	\
	$(CHECKFILES)
floatnum_test_LDFLAGS =
floatnum_test_LDADD = libyasm.la @LIBLTDL@ @LIBOBJS@ $(INTLLIBS) @LIBADD_DL@

#memexpr_test_SOURCES = \
#	src/tests/memexpr_test.c	\
#	$(YASMPARSERFILES)		\
#	$(YASMPREPROCFILES)		\
#	$(YASMOPTIMIZERFILES)		\
#	$(YASMOBJFMTFILES)		\
#	$(YASMARCHFILES)		\
#	$(CHECKFILES)
#memexpr_test_LDFLAGS =
#memexpr_test_LDADD = libyasm.la @LIBLTDL@ @LIBOBJS@ $(INTLLIBS) @LIBADD_DL@
