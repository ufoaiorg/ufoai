#!/usr/bin/perl

use strict;

my @logs;
unless ($ARGV[0]) {
	my $SVNURL = "https://ufoai.svn.sourceforge.net/svnroot/ufoai/ufoai";
	@logs = `svn log $SVNURL`
} else {
	open(SVNLOGS, "<$ARGV[0]" ) || die "Could not open svn logs from '$ARGV[0]'\n";
	@logs = <SVNLOGS>;
	close(SVNLOGS);
}
my $year = "2010";
my $startmonth = "05";
my $month = "July";

#r29285 | tlh2000 | 2010-04-10 09:07:27 +0200 (Sa, 10. Apr 2010) | 2 Zeilen
my $matchstart = "$year-$startmonth-";
my $lines = 0;
my $print = 0;
my $commits = 0;
print "{{news\n|title=Monthly update for $month, $year\n|author=mattn\n|date=$year-$startmonth-01T08:41:15\n|content=\n";
foreach (@logs) {
	if ($lines && $print) {
		s/^\s+//g;
		#print "$print $_" if ($_);
		print "$_" if ($_);
		$lines--;
	} elsif ($_ =~ /^r\d/) {
		my @values = split(/\s*\|\s*/, $_);
		if ($values[2] =~ /$matchstart/) {
			$print = $values[2];
			$commits++;
		} elsif ($print) {
			print "In total, $commits commits were made to UFO:AI repository in $month.\n}}\n";
			exit 0;
		}
		my @tmp = split(/\s*/, $values[3]);
		$lines = $tmp[0] + 1;
	}
}

die "Unexpected end";
