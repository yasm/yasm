# $Id$

# These utility programs have to be built for BUILD host in cross-build.
# This makes things rather non-standard automake

noinst_PROGRAMS += gap

gap_SOURCES =
EXTRA_DIST += tools/gap/gap.c
EXTRA_DIST += tools/gap/perfect.c
EXTRA_DIST += tools/gap/perfect.h
EXTRA_DIST += tools/gap/standard.h
gap_LDADD  = gap.$(OBJEXT)
gap_LDADD += gap-perfect.$(OBJEXT)
gap_LDADD += gap-phash.$(OBJEXT)
gap_LDADD += gap-xmalloc.$(OBJEXT)
gap_LDADD += gap-xstrdup.$(OBJEXT)
gap_LINK = $(CCLD_FOR_BUILD) -o $@

gap.$(OBJEXT): tools/gap/gap.c
	$(CC_FOR_BUILD) $(DEFAULT_INCLUDES) $(INCLUDES) -c -o $@ `test -f tools/gap/gap.c || echo '$(srcdir)/'`tools/gap/gap.c

gap-perfect.$(OBJEXT): tools/gap/perfect.c
	$(CC_FOR_BUILD) $(DEFAULT_INCLUDES) $(INCLUDES) -c -o $@ `test -f tools/gap/perfect.c || echo '$(srcdir)/'`tools/gap/perfect.c

gap-phash.$(OBJEXT): libyasm/phash.c
	$(CC_FOR_BUILD) $(DEFAULT_INCLUDES) $(INCLUDES) -c -o $@ `test -f libyasm/phash.c || echo '$(srcdir)/'`libyasm/phash.c

gap-xmalloc.$(OBJEXT): libyasm/xmalloc.c
	$(CC_FOR_BUILD) $(DEFAULT_INCLUDES) $(INCLUDES) -c -o $@ `test -f libyasm/xmalloc.c || echo '$(srcdir)/'`libyasm/xmalloc.c

gap-xstrdup.$(OBJEXT): libyasm/xstrdup.c
	$(CC_FOR_BUILD) $(DEFAULT_INCLUDES) $(INCLUDES) -c -o $@ `test -f libyasm/xstrdup.c || echo '$(srcdir)/'`libyasm/xstrdup.c

