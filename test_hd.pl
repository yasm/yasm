#! /usr/bin/env perl
open(BFILE,$ARGV[0]) || die "Could not open \\$ARGV[0]\\: $!";
while(!eof(BFILE)) {
    read(BFILE, $buffer, 1);
    printf("%02x \n", ord($buffer));
}
close(BFILE) || die "Could not close \\$ARGV[0]\\: $!";
