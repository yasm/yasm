#!/usr/bin/env perl

open(BFILE,$ARGV[0]) || die "Could not open \\$ARGV[0]\\: $!";

# This is needed to run correctly on systems with Perl version 5.8.0 or higher
# and a UTF-8 locale.  This so happens to describe Tyler's setup nicely. :)
binmode(BFILE);

while(!eof(BFILE)) {
    read(BFILE, $buffer, 1);
    printf("%02x \n", ord($buffer));
}
close(BFILE) || die "Could not close \\$ARGV[0]\\: $!";
