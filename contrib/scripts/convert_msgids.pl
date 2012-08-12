#!/usr/bin/perl

use strict;
use warnings;

my $file = "../../src/po/ufoai-en.po";

open(FILE, $file) || die "Could not read $file";
my $started = 0;
foreach (<FILE>) {
	$_ =~ s/\r?\n//g;
	if ($_ =~ /^msgid\s\"(.*_txt)\"$/ || $_ =~ /^msgid\s\"(mail_.*?)\"$/) {
		if ($started) {
			print "\"\n}\n";
		}
		print "msgid $1 {\ntext \"_";
		$started = 1;
		next;
	} elsif ($_ =~ /^msgid.*/) {
		if ($started) {
			print "\"\n}\n";
			$started = 0;
		}
		next;
	}

	if ($_ eq "") {
		if ($started) {
			print "\"\n}\n\n";
			$started = 0;
		}
	} else {
		if ($started) {
			$_ =~ s/msgstr\s+//g;
			$_ =~ s/^\"//g;
			$_ =~ s/\"$//g;
			$_ =~ s/\\n/\n/g;
			print $_;
		}
	}
}
close(FILE);
