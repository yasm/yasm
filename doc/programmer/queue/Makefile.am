# $IdPath$
# Transforms queue manpage into various output formats.
# The queue manpage is courtesy of FreeBSD.

noinst_DATA = queue.html queue.ps

EXTRA_DIST = queue.3

queue.html: queue.3
	groff -mmandoc -Thtml queue.3 >queue.html

queue.ps: queue.3
	groff -mmandoc -Tps queue.3 >queue.ps
