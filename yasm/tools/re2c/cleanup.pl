#! /usr/bin/env perl
my @sourcelines = <>;
my %usedlabel;
my %usedvar;
my $lastusedvarline = 0;
my $level = 0;
my $line = 1;
for (@sourcelines) {
    $usedlabel{"$1:"} = "$1:" if m/goto\s+(yy[0-9]*)\s*;/;
    $level = $level + 1 if m/\{/;
    $level = $level - 1 if m/\}/;
    if ($level < $usedvar{$lastusedvarline}) {
	$lastusedvarline = 0;
    }
    if (m/int\s+yyaccept/) {
	$usedvar{$line} = $level;
	$lastusedvarline = $line;
    } elsif (m/yyaccept/) {
	$usedvar{$lastusedvarline} = 1000;
    }
    $line = $line + 1;
}
$line = 1;
for (@sourcelines) {
    s/^(yy[0-9]*:)/$usedlabel{$1}/;
    if ($usedvar{$line} != 0 && $usedvar{$line} != 1000) {
	print "\n";
    } else {
    	print;
    }
    $line = $line + 1;
}
