# $IdPath$
# Transforms queue manpage into various output formats.

all: queue.html queue.ps

queue.html: queue.3
	groff -mmandoc -Thtml queue.3 >queue.html

queue.ps: queue.3
	groff -mmandoc -Tps queue.3 >queue.ps
