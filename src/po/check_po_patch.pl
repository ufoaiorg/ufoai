#!/usr/bin/env perl

use strict;

$|=1;

my $language = shift;
my $patch_file = shift;

if (length($patch_file)<1) {
	print "You must enter a language and the name of the file to compare po file as parameters to run this script.\n";
	print "  For example : ".$0." fr fr.patch.po\n";
	exit
}

my $trunk_file="$language.po";
my $out_file=$language.".log";

open(PATCHFILE,  $patch_file)   or die "Impossible d'ouvrir $patch_file en lecture : $!";
open(OUTFILE, ">".$out_file) or die "Impossible d'ouvrir $out_file en écriture : $!";

sub find_msgstr {
	my $msgid = shift;
	open(TRUNKFILE,  $trunk_file)   or die "Impossible d'ouvrir $trunk_file en lecture : $!";

	my $trunk_line="";
	my $test=0;
	while (<TRUNKFILE>) {
			unless (/^msgid "$msgid"$/i) {
				next;
			}
			$test=1;
			last;
	}
	while (($trunk_line=<TRUNKFILE>) && $test) {
		if ($trunk_line =~ /^msgstr "(.*)"$/) {
			$test=2;
			last
		}
	}
	if ($test != 2) {
			return "";
	}

	chomp($trunk_line);
	$trunk_line=substr($trunk_line,8,length($trunk_line)-9);
	#$trunk_line=lc($trunk_line);

	close TRUNKFILE;
	return $trunk_line;
}

print "\nThis script scans msgids the .po file from local revision for a given language (here $language), and looks for the differences with the file given in argument (here $patch_file). The output is written in the log file $out_file.\n";
print "Please note that only short msgstr (< 150 characters) will be compared.\n";

my $compteur=0;

while (<PATCHFILE>) {     # each line is successively affected to $_
	if (/^msgid "(.*)"$/)
	{
		my $msgid=$1;
		if (length($msgid)>0) {
			#$msgid=lc($msgid);
			my $line="";
			until ($line =~ /^msgstr "(.*)"$/) {
				$line=<PATCHFILE>;
			}
			chomp($line);
			my $msgstr="";
			$msgstr=substr($line,8,length($line)-9);
			#$msgstr=lc($msgstr);
			my $trunk=find_msgstr ($msgid);
			if (($trunk ne $msgstr) && ($trunk ne "") && length($trunk)<150) {
				print OUTFILE "msgid: \"".$msgid."\"\n";
				print OUTFILE "  $patch_file: ".$msgstr."\n";
				print OUTFILE "  $trunk_file: ".$trunk."\n";
				print OUTFILE "\n";
			}
			$compteur+=1;
			print "." if ($compteur%10==0)
			}
	}
}

print "\n";

close PATCHFILE;
close OUTFILE;
