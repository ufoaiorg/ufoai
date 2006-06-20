#!/usr/bin/perl
use strict;

my %count = ();
my %teamcount = ();

sub readDir
{
	my $dir = shift || die "No dir given in sub readDir\n";
	my $status = opendir (DIR, "$dir");
	my @files = ();
	unless ( $status ) {
 		print "Could not read $dir\n";
 		return ();
	}

	@files = sort readdir ( DIR );
	closedir (DIR);
	@files;
}

sub check
{
	my $dir = shift || die "No dir given in sub check\n";
	my $file = shift || undef;
	my $found = 0;
	my $entity = "";
	my $line = 0;
	my $teamfound = 0;
	my $team = 0; # search for team id in info_player_start (multiplayer)
	foreach ( readDir( $dir ) ) {
		next if $_ =~ /^\.|(CVS)/;
		if ( -d "$dir/$_" ) {
			$found += check("$dir/$_", $file);
			next;
		}
		next unless $_ =~ /\.map$/;
		next if $_ =~ /^(tutorial)|(prefab)|(autosave)/i;
		next if ( $file && $_ !~ /$file/i );
		print "==============================================\n";
		print "map: $dir/$_\n";
		open ( MAP, "<$dir/$_" ) || die "Could not open $dir/$_\n";
		$team = $line = 0;
		%count = ();
		%teamcount = ();
		foreach ( <MAP> ) {
			$line++;
			unless ( $team ) {
				m/^\"classname\"\s+\"(info_\w+_start)\"/ig;
				if ( !$1 ) {
					m/^\"team\"\s+\"(\d+)\"/ig;
					next unless $1;
					$teamcount{$1}++;
					$teamfound = 1;
					next;
				}
				$entity = $1;
			} else {
				if ( m/^\}\n$/ ) {
					$team = 0;
					print "Error - no team for $entity (near line $line)\n";
					next;
				}
				m/^\"team\"\s+\"(\d+)\"/ig;
				next unless ( $1 );
				$teamcount{$1}++;
				$team = 0;
				next;
			}
			$count{$entity}++;
			if ( $entity eq "info_player_start" ) {
				if ( ! $teamfound ) {
					$team = 1;
				}
				$teamfound = 0;
			} elsif ( $entity eq "info_ugv_start" ) {
				if ( ! $teamfound ) {
					$team = 1;
				}
				$teamfound = 0;
			} else {
				$team = 0;
			}

		}
		foreach ( keys %count ) {
			print "... $_ => $count{$_}\n";
			if ( $_ eq "info_player_start" ) {
				foreach ( sort keys %teamcount ) {
					print "  \\... team $_ $teamcount{$_}\n";
				}
			}
		}
		print "==============================================\n";
		print "\n";
		close ( MAP );
		$found++;
	}
	return $found;
}

#read the given dir
my $file = $ARGV[0] || undef;
print "=====================================================\n";
print "Mapchecker for UFO:AI (http://sf.net/projects/ufoai)\n";
print "Will search the maps for start positions\n";
print "=====================================================\n";

check(".", $file);

print "...finished\n"

