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
	my $recursive = shift || 0;
	my $found = 0;
	my $entity = "";
	my $line = 0;
	my $teamfound = 0;
	my $team = 0; # search for team id in info_player_start (multiplayer)
	my $brush = 0; # search for brushes inside of the entity (bmodels)
	my $align = 0; # check alignment of spawns
	my $brushcount = 0;
	my $ent_end = 0;
	my $spawnflags = 0;
	my $lightsday = 0;
	my $lightsnight = 0;
	foreach ( readDir( $dir ) ) {
		next if $_ =~ /^\.|(CVS)/;
		if ( -d "$dir/$_" && $recursive ) {
			$found += check("$dir/$_", $file);
			next;
		}
		next unless $_ =~ /\.map$/;
		next if $_ =~ /^(tutorial)|(prefab)|(autosave)/i;
		next if ( $file && $_ !~ /$file/i );
		print "==============================================\n";
		print "map: $dir/$_\n";
		open ( MAP, "<$dir/$_" ) || die "Could not open $dir/$_\n";
		$team = $line = $spawnflags = $lightsday = $lightsnight = 0;
		%count = ();
		%teamcount = ();
		foreach ( <MAP> ) {
			$line++;
			unless ( $team || $brush || $align) {
				m/^\"classname\"\s+\"(\w+)\"/ig;
				if ( !$1 ) {
					m/^\"team\"\s+\"(\d+)\"/ig;
					if ( $1 ) {
						$teamcount{$1}++;
						$teamfound = 1;
					}
					m/^\"spawnflags\"\s+\"(\d+)\"/ig;
					if ( $1 ) {
						$spawnflags = $1;
						if ( $entity =~ /light/ ) {
							# FIXME: Check the bitmask here
							if ( $spawnflags eq 1 ) {
								$lightsday++;
							}
						}
					}
					next;
				}
				# new entity - reset spawnflags
				$spawnflags = 0;
				$entity = $1;
				if ( $entity =~ /light/ ) {
					$lightsnight++;
				}
			} else {
				if ($team) {
					if ( m/^\}\n$/ ) {
						$team = 0;
						print "Error - no team for $entity (near line $line)\n";
						next;
					}
					m/^\"team\"\s+\"(\d+)\"/ig;
					next unless ( $1 );
					$teamcount{$1}++;
					$team = 0;
				} elsif ($brush) {
					if ( m/^\}\n$/ ) {
						if ($ent_end) {
							$brush = 0;
							if ($brushcount > 1) {
								print "Warning - func_breakables with more than one brush found - might break pathfinding ($brushcount - line $line)\n";
							}
							$ent_end = 0;
						} else {
							$brushcount++;
							$ent_end = 1;
						}
					} elsif ( m/^\{\n$/ ) {
						$ent_end = 0;
					}
				} elsif ($align) {
					if (/\"origin\"/) {
						m/^\"origin\"\s+\"-*(\d+) -*(\d+) -*(\d+)\"/ig;
						if (($1 % 32) - 16 || ($2 % 32) - 16) {
							print "Error - found misaligned $entity at line $line\n";
						}
						$align = 0;
					}
				}
				next;
			}
			$count{$entity}++;
			if ( $entity eq "info_player_start" ) {
				if ( ! $teamfound ) {
					$team = 1;
				}
				$align = 1;
				$teamfound = 0;
			} elsif ( $entity eq "info_ugv_start" ) {
				if ( ! $teamfound ) {
					$team = 1;
				}
				$align = 1;
				$teamfound = 0;
			} elsif ( $entity =~ /info_.*_start/ ) {
				$align = 1;
			} elsif ( $entity eq "func_breakable" ) {
				$brush = 1;
				$brushcount = 0;
				$ent_end = 0;
			} else {
				$team = 0;
				$brush = 0;
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
		if ( $lightsnight ) {
			print "... night lights => $lightsnight\n";
			print "... day lights => $lightsday\n";
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
my $rec = 0;
print "=====================================================\n";
print "Mapchecker for UFO:AI (http://sf.net/projects/ufoai)\n";
print "Will search the maps for start positions\n";
print "Options:\n";
print "-r --recursive - go into directories\n";
print "[filename] - only maps where filename is included in name\n";
print "=====================================================\n";

if ( $file eq "--recursion" || $file eq "-r" ) {
	$file = "";
	$rec = 1;
}
check("../../base/maps", $file, $rec);

print "...finished\n"
