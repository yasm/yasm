#!/usr/bin/perl -w
# $Id: gen_instr.pl,v 1.7 2001/05/30 21:39:53 mu Exp $
# Generates bison.y and token.l from instrs.dat for YASM
#
#    Copyright (C) 2001  Michael Urman
#
#    This file is part of YASM.
#
#    YASM is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    YASM is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

use strict;
use Getopt::Long;
my $VERSION = "0.0.1";

# useful constants for instruction arrays
use constant INST	=> 0;
use constant OPERANDS	=> 1;
use constant OPSIZE	=> 2;
use constant OPCODE	=> 3;
use constant EFFADDR	=> 4;
use constant IMM	=> 5;
use constant CPU	=> 6;

# default options
my $instrfile = 'instrs.dat';
my $tokenfile = 'token.l';
my $tokensource;
my $grammarfile = 'bison.y';
my $grammarsource;
my $showversion;
my $showusage;
my $dry_run;

# allow overrides
my $gotopts = GetOptions ( 'input=s' => \$instrfile,
			   'token=s' => \$tokenfile,
			   'sourcetoken=s' => \$tokensource,
			   'grammar=s' => \$grammarfile,
			   'sourcegrammar=s' => \$grammarsource,
			   'version' => \$showversion,
			   'n|dry-run' => \$dry_run,
			   'help|usage' => \$showusage,
			 );

&showusage and exit 1 unless $gotopts;
&showversion if $showversion;
&showusage if $showusage;
exit 0 if $showversion or $showusage;


my $instrlist = &read_instructions ($instrfile);

exit 0 if $dry_run; # done with simple verification, so exit

unless ($dry_run)
{
    &output_lex ($tokenfile, $tokensource, $instrlist);
    &output_yacc ($grammarfile, $grammarsource, $instrlist);
}

# print version for --version, etc.
sub showversion
{
    print "YASM gen_instr.pl $VERSION\n";
}

# print usage information for --help, etc.
sub showusage
{
    print <<"EOF";
Usage: gen_instrs.pl [-i input] [-t tokenfile] [-g grammarfile]
    -i, --input	         instructions file (default: $instrfile)
    -t, --token	         token output file (default: $tokenfile)
    -st, --sourcetoken   token input file (default: $tokenfile.in)
    -g, --grammar        grammar output file (default: $grammarfile)
    -sg, --sourcegrammar grammar input file (default: $grammarfile.in)
    -v, --version        show version and exit
    -h, --help, --usage  show this message and exit
    -n, --dry-run        verify input file without writing output files
EOF
}

# read in instructions, and verify they're valid (well, mostly)
sub read_instructions ($)
{
    my $instrfile = shift || die;
    open INPUT, "< $instrfile" or die "Cannot open '$instrfile' for reading: $!\n";
    my @instr;
    my %instr;
    my $valid_regs = join '|', qw(
	REG_AL REG_AH REG_AX REG_EAX
	REG_BL REG_BH REG_BX REG_EBX
	REG_CL REG_CH REG_CX REG_ECX
	REG_DL REG_DH REG_DX REG_EDX
	REG_SI REG_ESI REG_DI REG_EDI
	REG_BP REG_EBP
	REG_CS REG_DS REG_ES REG_FS REG_GS REG_SS
	ONE XMMREG MMXREG segreg CRREG_NOTCR4 CR4 DRREG
	fpureg FPUREG_NOTST0 ST0 ST1 ST2 ST3 ST4 ST5 ST6 ST7 mem imm
	imm8 imm16 imm32 imm64 imm80 imm128
	imm8x imm16x imm32x imm64x imm80x imm128x
	rm8 rm16 rm32 rm1632 rm64 rm80 rm128
	rm8x rm16x rm32x rm1632x rm64x rm80x rm128x
	reg8 reg16 reg32 reg1632 reg64 reg80 reg128
	reg8x reg16x reg32x reg1632x reg64x reg80x reg128x
	mem8 mem16 mem32 mem1632 mem64 mem80 mem128
	mem8x mem16x mem32x mem1632x mem64x mem80x mem128x
    );
    my $valid_cpus = join '|', qw(
	8086 186 286 386 486 P4 P5 P6
	FPU MMX KATMAI SSE SSE2
	AMD ATHLON 3DNOW
	SMM
	CYRIX
	UNDOC OBS PRIV PROT
    );
    while (<INPUT>)
    {
	next if /^\s*(?:;.*)$/;
	if (m/^:(\w+)\t+(\w+)$/)
	{
	    $instr{$1} = $2;	# simple scalar => alias
	    next;
	}
	my ($inst, $op, $size, $opcode, $eff, $imm, $cpu) = split /\t+/;
	eval {
	    die "Invalid instruction name\n"
		if $inst !~ m/^\w+$/o;
	    die "Invalid Operands\n"
		if $op !~ m/^(?:TO\s)?nil|(?:$valid_regs)(?:,(?:$valid_regs)){0,2}$/oi;
	    die "Invalid Operation Size\n"
		if $size !~ m/^nil|16|32|128$/oi;
	    die "Invalid Opcode\n"
		if $opcode !~ m/[0-9A-F]{2}(,[0-9A-F]{2})?(\+\$\d)?/oi;
	    die "Invalid Effective Address\n"
		if $eff !~ m/nil|(\$?\d[ir]?(,\$?\d))/oi;
	    die "Invalid Immediate Operand\n"
		if $imm !~ m/nil|((\$\d[r]?|8|16|32|[0-9A-F]{2})(,\$\d|(8|16|32[s]?))?)/oi;
	    die "Invalid CPU\n"
		if $cpu !~ m/^(?:$valid_cpus)(?:,(?:$valid_cpus))*$/o;
	};
	die "Malformed Instruction at $instrfile line $.: $@" if $@;
	die "Multiple Definiton for alias $inst at $instrfile line $.\n"
	    if exists $instr{$inst} and not ref $instr{$inst};
	push @{$instr{$inst}}, [$inst, $op, $size, $opcode, $eff, $imm, $cpu];
    }
    close INPUT;
    return (\%instr);
}

sub output_lex ($@)
{
    my $tokenfile = shift or die;
    my $tokensource = shift;
    $tokensource ||= "$tokenfile.in";
    my $instrlist = shift or die;

    open IN, "< $tokensource" or die "Cannot open '$tokensource' for reading: $!\n";
    open TOKEN, "> $tokenfile" or die "Cannot open '$tokenfile' for writing: $!\n";
    while (<IN>)
    {
	# Replace token.l.in /* @INSTRUCTIONS@ */ with generated content
	if (m{/[*]\s*[@]INSTRUCTIONS[@]\s*[*]/})
	{
	    foreach my $inst (sort keys %$instrlist)
	    {
		printf TOKEN "%-12s{ return %-16s }\n", $inst,
		    (ref $instrlist->{$inst}) ?
			"\Uins_$inst;\E" : "\Uins_$instrlist->{$inst};\E";
	    }
	}
	else
	{
	    print TOKEN $_;
	}
    }
    close IN;
    close TOKEN;
}

# helper functions for yacc output
sub rule_header ($ $ $)
{
    my ($rule, $tokens, $count) = splice (@_);
    $count ? "    | $tokens {\n" : "$rule: $tokens {\n"; 
}
sub rule_footer ()
{
    return "    }\n";
}
sub cond_action ( $ $ $ $ $ $ $ $ )
{
    my ($rule, $tokens, $count, $regarg, $val, $func, $a_eax, $a_args)
	= splice (@_);
    return rule_header ($rule, $tokens, $count) . <<"EOF" . rule_footer;
        if (\$$regarg == $val) {
            $func(@$a_eax);
        }
        else {
            $func (@$a_args);
        }
EOF
}

#sub action ( $ $ $ $ $ )
sub action ( @ $ )
{
    my ($rule, $tokens, $func, $a_args, $count) = splice @_;
    return rule_header ($rule, $tokens, $count)
	. "        $func (@$a_args);\n"
	. rule_footer; 
}

sub get_token_number ( $ $ )
{
    my ($tokens, $str) = splice @_;
    $tokens =~ s/$str.*/x/; # hold its place
    my @f = split /\s+/, $tokens;
    return scalar @f;
}

sub output_yacc ($@)
{
    my $grammarfile = shift or die;
    my $grammarsource = shift;
    $grammarsource ||= "$grammarfile.in";
    my $instrlist = shift or die;

    open IN, "< $grammarsource" or die "Cannot open '$grammarsource' for reading: $!\n";
    open GRAMMAR, "> $grammarfile" or die "Cannot open '$grammarfile' for writing: $!\n";

    while (<IN>)
    {
	if (m{/[*]\s*[@]TOKENS[@]\s*[*]/})
	{
	    my $len = length("%token");
	    print GRAMMAR "%token";
	    foreach my $inst (sort keys %$instrlist)
	    {
		if ($len + length("INS_$inst") < 76)
		{
		    print GRAMMAR " INS_\U$inst\E";
		    $len += length(" INS_$inst");
		}
		else
		{
		    print GRAMMAR "\n%token INS_\U$inst\E";
		    $len = length("%token INS_$inst");
		}
	    }
	    print GRAMMAR "\n";
	}
	elsif (m{/[*]\s*[@]TYPES[@]\s*[*]/})
	{
	    my $len = length("%type <bc>");
	    print GRAMMAR "%type <bc>";
	    foreach my $inst (sort keys %$instrlist)
	    {
		next unless ref $instrlist->{$inst};
		if ($len + length($inst) < 76)
		{
		    print GRAMMAR " $inst";
		    $len += length(" $inst");
		}
		else
		{
		    print GRAMMAR "\n%type <bc> $inst";
		    $len = length("%type <bc> $inst");
		}
	    }
	    print GRAMMAR "\n";
	}
	elsif (m{/[*]\s*[@]INSTRUCTIONS[@]\s*[*]/})
	{
	    # list every kind of instruction that instrbase can be
	    print GRAMMAR "instrbase:    ",
		    join( "\n    | ", sort grep {ref $instrlist->{$_}} keys %$instrlist), "\n;\n";

	    my ($ONE, $AL, $AX, $EAX);	# need the outer scope

	    # list the arguments and actions (buildbc)
	    foreach my $instrname (sort keys %$instrlist)
	    {
		# I'm still convinced this is a hack.  The idea is if
		# within an instruction we see certain versions of the
		# opcodes with ONE, or REG_E?A[LX],imm(8|16|32).  If we
		# do, defer generation of the action, as we may need to
		# fold it into another version with a conditional to
		# generate the more efficient variant of the opcode
		# BUT, if we don't fold it in, we have to generate the
		# original version we would have otherwise.
		($ONE, $AL, $AX, $EAX) = (0, 0, 0, 0);
		my $count = 0;
		next unless ref $instrlist->{$instrname};
		foreach my $inst (@{$instrlist->{$instrname}}) {
		    # build the instruction in pieces.

		    # rulename = instruction
		    my $rule = "$inst->[INST]";

		    # tokens it eats: instruction and arguments
		    # nil => no arguments
		    my $tokens = "\Uins_$inst->[INST]\E";
		    $tokens .= " $inst->[OPERANDS]" if $inst->[OPERANDS] ne 'nil';
		    $tokens =~ s/,/ ',' /g;
		    my $to = $tokens =~ m/\bTO\b/ ? 1 : 0;  # offset args

		    my $func = "BuildBC_Insn";

		    # Create the argument list for BuildBC
		    my @args;

		    # First argument is always &$$
		    push @args, '&$$,';

		    # opcode size
		    push @args, "$inst->[OPSIZE],";
		    $args[-1] =~ s/nil/0/;

		    # number of bytes of opcodes
		    push @args, (scalar(()=$inst->[OPCODE] =~ m/(,)/)+1) . ",";

		    # opcode piece 1 (and 2 if attached)
		    push @args, $inst->[OPCODE];
		    $args[-1] =~ s/,/, /;
		    $args[-1] =~ s/([0-9A-Fa-f]{2})/0x$1/g;
		    $args[-1] =~ s/\$(\d+)/"\$" . ($1*2+$to)/eg;
		    $args[-1] .= ',';

		    # opcode piece 2 (if not attached)
		    push @args, "0," if $inst->[OPCODE] !~ m/,/o;
		    # opcode piece 3 (if not attached)
		    push @args, "0," if $inst->[OPCODE] !~ m/,.*,/o;

		    # effective addresses
		    push @args, $inst->[EFFADDR];
		    $args[-1] =~ s/,/, /;
		    $args[-1] =~ s/nil/(effaddr *)NULL, 0/;
		    $args[-1] =~ s/\$(\d+)([ri])?/"\$".($1*2+$to).($2||'')/eg;
		    $args[-1] =~ s/(\$\d+[ri]?)/\&$1/; # Just the first!
		    $args[-1] =~ s/\&(\$\d+)r/ConvertRegToEA((effaddr *)NULL, $1)/;
		    $args[-1] =~ s[\&(\$\d+)i,\s*(\d+)]
			["ConvertImmToEA((effaddr *)NULL, \&$1, ".($2/8)."), 0"]e;
		    $args[-1] .= ',';

		    die $args[-1] if $args[-1] =~ m/\d+[ri]/;

		    # immediate sources
		    push @args, $inst->[IMM];
		    $args[-1] =~ s/,/, /;
		    $args[-1] =~ s/nil/(immval *)NULL, 0/;
		    $args[-1] =~ s/\$(\d+)(r)?/"\$".($1*2+$to).($2||'')/eg;
		    $args[-1] =~ s/(\$\d+r?)/\&$1/; # Just the first!
		    $args[-1] =~ s[^([0-9A-Fa-f]+),]
			[ConvertIntToImm((immval *)NULL, 0x$1),];

		    # divide the second, and only the second, by 8 bits/byte
		    $args[-1] =~ s#(,\s*)(\d+)(s)?#$1 . ($2/8)#eg;
		    $args[-1] .= ($3||'') eq 's' ? ', 1' : ', 0';

		    $args[-1] =~ s/(\&\$\d+)(r)?/$1/;
		    $args[-1] .= ($2||'') eq 'r' ? ', 1' : ', 0';

		    die $args[-1] if $args[-1] =~ m/\d+[ris]/;
		    
		    # see if we match one of the cases to defer
		    if (($inst->[OPERANDS]||"") =~ m/,ONE/)
		    {
			$ONE = [ $rule, $tokens, $func, \@args];
		    }
		    elsif (($inst->[OPERANDS]||"") =~ m/REG_AL,imm8/)
		    {
			$AL = [ $rule, $tokens, $func, \@args];
		    }
		    elsif (($inst->[OPERANDS]||"") =~ m/REG_AX,imm16/)
		    {
			$AX = [ $rule, $tokens, $func, \@args];
		    }
		    elsif (($inst->[OPERANDS]||"") =~ m/REG_EAX,imm32/)
		    {
			$EAX = [ $rule, $tokens, $func, \@args];
		    }

		    # or if we've deferred and we match the folding version
		    elsif ($ONE and ($inst->[OPERANDS]||"") =~ m/imm8/)
		    {
			my $immarg = get_token_number ($tokens, "imm8");

			$ONE->[4] = 1;
			print GRAMMAR cond_action ($rule, $tokens, $count++, "$immarg.val", 1, $func, $ONE->[3], \@args);
		    }
		    elsif ($AL and ($inst->[OPERANDS]||"") =~ m/reg8,imm/)
		    {
			$AL->[4] = 1;
			my $regarg = get_token_number ($tokens, "reg8");

			print GRAMMAR cond_action ($rule, $tokens, $count++, $regarg, 0, $func, $AL->[3], \@args);
		    }
		    elsif ($AX and ($inst->[OPERANDS]||"") =~ m/reg16,imm/)
		    {
			$AX->[4] = 1;
			my $regarg = get_token_number ($tokens, "reg16");

			print GRAMMAR cond_action ($rule, $tokens, $count++, $regarg, 0, $func, $AX->[3], \@args);
		    }
		    elsif ($EAX and ($inst->[OPERANDS]||"") =~ m/reg32,imm/)
		    {
			$EAX->[4] = 1;
			my $regarg = get_token_number ($tokens, "reg32");

			print GRAMMAR cond_action ($rule, $tokens, $count++, $regarg, 0, $func, $EAX->[3], \@args);
		    }

		    # otherwise, generate the normal version
		    else
		    {
			print GRAMMAR action ($rule, $tokens, $func, \@args, $count++);
		    }
		}

		# catch deferreds that haven't been folded in.
		if ($ONE and not $ONE->[4])
		{
		    print GRAMMAR action (@$ONE, $count++);
		}
		if ($AL and not $AL->[4])
		{
		    print GRAMMAR action (@$AL, $count++);
		}
		if ($AX and not $AL->[4])
		{
		    print GRAMMAR action (@$AX, $count++);
		}
		if ($EAX and not $AL->[4])
		{
		    print GRAMMAR action (@$EAX, $count++);
		}
		
		# print error action
		# ASSUMES: at least one previous action exists
		print GRAMMAR "    | \Uins_$instrname\E error {\n";
		print GRAMMAR "        Error (ERR_EXP_SYNTAX, (char *)NULL);\n";
		print GRAMMAR "    }\n";

		# terminate the rule
		print GRAMMAR ";\n";
	    }
	}
	else
	{
	    print GRAMMAR $_;
	}
    }
    close IN;
    close GRAMMAR;
}
