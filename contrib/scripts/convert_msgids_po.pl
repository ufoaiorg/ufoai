#!/usr/bin/perl

use strict;
use warnings;

my $enfile = "src/po/ufoai-en.po";
open(FILE, $enfile) || die "Could not read $enfile";
my $id = "";
my %ids = ();

my $started = 0;
my $fuzzy = 0;
foreach (<FILE>) {
	$_ =~ s/\r?\n//g;
	if ($_ =~ /.*fuzzy$/) {
		$fuzzy = 1;
		next;
	}
	if ($fuzzy == 0 && ($_ =~ /^msgid\s\"(.*_txt)\"$/ || $_ =~ /^msgid\s\"(mail_.*?)\"$/)) {
		$started = 1;
		$id = $1;
		next;
	} elsif ($_ =~ /^msgid.*/) {
		$started = 0;
		$fuzzy = 0;
	}

	if ($_ eq "") {
		$started = 0;
		$fuzzy = 0;
	} else {
		if ($started) {
			if ($_ !~ /^msgstr\s+\"\"$/) {
				$_ =~ s/^msgstr\s+//g;
				unless ($ids{$id}) {
					$ids{$id} = "msgid ";
				}
				$ids{$id} .= $_ . "\n";
			}
		}
	}
}
close(FILE);

#foreach (sort keys %ids) {
#print $_;
#print "=>\n";
#print $ids{$_};
#}

foreach (@ARGV) {
	my $sourcefile = $_;
	open(FILE, $sourcefile) || die "Could not read $sourcefile";
	open(OUT, ">$sourcefile.new");
	my %done = ();
	foreach (<FILE>) {
		my $line = $_;
		if ($_ =~ /^msgid\s\"(.*_txt)\"$/ || $_ =~ /^msgid\s\"(mail_.*?)\"$/) {
			my $local_id = $1;
			if ($done{$local_id}) {
				$ids{$local_id} = "";
			}
			if ($ids{$local_id}) {
				if (!$done{$ids{$local_id}}) {
					$line = $ids{$local_id};
				}
				$done{$line} = 1;
			}
			$done{$local_id} = 1;
		}
		print OUT $line;
	}
	close(OUT);
	close(FILE);
	system("mv $sourcefile.new $sourcefile");
}
