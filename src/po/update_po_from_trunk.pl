#!/usr/bin/env perl

use strict;
use LWP::Simple;

$|=1;

my $language = shift;

if (length($language)<1) {
	print "You must enter a language as parameter to run this script.\n";
	print "  For example : ".$0." fr\n";
	exit
}

my $branche_file="branch_$language.po";
my $trunk_file="trunk_$language.po";
my $out_file=$language.".log";

my $trunk_url = "http://ufoai.svn.sourceforge.net/svnroot/ufoai/ufoai/trunk/src/po/";
my $branche_url = "http://ufoai.svn.sourceforge.net/svnroot/ufoai/ufoai/branches/ufoai_2.0/src/po/";

print "Downloading files...\n";
getstore($trunk_url.$language.".po",$trunk_file);
getstore($branche_url.$language.".po",$branche_file);

open(BRANCHEFILE,  $branche_file)   or die "Impossible d'ouvrir $branche_file en lecture : $!";
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

print "\nThis script scans msgids the .po file from svn branche for a given language (here $language), and check in the trunk file if the msgid exists. If so, it compares the 2 msgstr (non case sensitive), and outputs the differences in a log file (here $out_file).\n";
print "Please note that only short msgstr (< 150 characters) will be compared.\n";
print "Note also that this script can only find msgids still in the trunk .po file. Many msgids in the branche file are not in the trunk anymore, and will therefore not be compared.\n";

my $compteur=0;

while (<BRANCHEFILE>) {     # chaque line est successivement affectée à $_
	if (/^msgid "(.*)"$/)
	{
		my $msgid=$1;
		if (length($msgid)>0) {
			#$msgid=lc($msgid);
			my $line="";
			until ($line =~ /^msgstr "(.*)"$/) {
				$line=<BRANCHEFILE>;
			}
			chomp($line);
			my $msgstr="";
			$msgstr=substr($line,8,length($line)-9);
			#$msgstr=lc($msgstr);
			my $trunk=find_msgstr ($msgid);
			if (($trunk ne $msgstr) && ($trunk ne "") && length($trunk)<150) {
				print OUTFILE "msgid: \"".$msgid."\"\n";
				print OUTFILE "  branche: ".$msgstr."\n";
				print OUTFILE "  trunk:   ".$trunk."\n";
				print OUTFILE "\n";
			}
			$compteur+=1;
			print "." if ($compteur%10==0)
			}
	}
}

print "\n";
    
close BRANCHEFILE;
close OUTFILE;
