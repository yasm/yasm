# $IdPath$
# Transforms queue manpage into various output formats.
# The queue manpage is courtesy of FreeBSD.

noinst_DATA = bitvect.ps

EXTRA_DIST = bitvect.3

bitvect.ps: bitvect.3
	$(GROFF) -mmandoc -Tps bitvect.3 >bitvect.ps
