#!/usr/bin/perl
my @sourcelines = <>;
my %usedlabel;
for (@sourcelines) {
    $usedlabel{"$1:"} = "$1:" if m/goto\s+(yy[0-9]*)\s*;/
}
for (@sourcelines) {
    s/^(yy[0-9]*:)/$usedlabel{$1}/;
    print;
}
