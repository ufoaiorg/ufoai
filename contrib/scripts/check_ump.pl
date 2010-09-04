#!/usr/bin/perl
use strict;

sub readDir
{
	my $dir = shift || die "No dir given in sub readDir\n";
	my $status = opendir (DIR, "$dir");
	my @files = ();
	unless ( $status ) {
 		print "Could not read $dir\n";
 		return ();
	}

	@files = readdir ( DIR );
	closedir (DIR);
	@files;
}

sub checkump
{
	my $dir = shift || die "No dir given in sub check\n";
	my $file = shift || undef;
	my $found = 0;
	my $entity = "";
	my $tilesize = 0;
	my $line = 0;
	my $assembly = "";
	my $base = "";
	my $tile = "";
	my $assemblyTile = "";
	my %tiles = ();
	my $tilefound = 1;
	my $noEOLprint = 0;
	my $last = 0;
	my $cols = 0;
	my $rows = 0;
	my @data = ();

	foreach ( readDir( $dir ) ) {
		next if $_ =~ /^\.|(CVS)/;
		next if ( -d "$dir/$_" ); # umps are only in main maps folder - not in subdirs
		next unless $_ =~ /\.ump$/;
		next if ( $file && $_ !~ /$file/i );
		print "==============================================\n";
		print "ump: $_\n";
		open ( UMP, "<$dir/$_" ) || die "Could not open $dir/$_\n";
		$line = $tilesize = 0;
		$base = $assembly = $tile = $assemblyTile = "";
		%tiles = ();
		foreach ( <UMP> ) {
			$line++;
			# only {, } or newline
			if ( $_ =~ /\r\n$/ ) {
				unless ($noEOLprint) {
					printf("You should use unix line endings here\n");
				}
				$noEOLprint = 1;
			}
			if ( $_ =~ /^\}\r?\n$/ ) {
				$assembly = "";
				if ($tile && $last) {
					$last = $rows - $last;
					die "Error in line $line: Row count ($last) doesn't match size ($rows)\n";
				}
				$tile = "";
				next;
			}
			if ( $_ =~ /^\}$/ ) {
				die "Missing newline at end of file\n";
			}
			next if ( $_ =~ /^\{\r?\n$/ );
			next if ( $_ =~ /^\r?\n$/ );
			next if ( $_ =~ /^\/\// );
			if ( $assembly ) {
				unless ( $base ) {
					die "...error: no base defined\n";
				}
				# new assembly?
				unless ( $_ =~ /^assembly\s+(.*?)\r?\n/ ) {
					if ( $_=~ /^\+(.*?)\s+\"\d+\s+\d+\"/ ) {
						$assemblyTile = $1;
						$tilefound = 0;
						foreach ( keys %tiles ) {
							if ( $_ eq $assemblyTile ) {
								$tilefound = 1;
							}
						}
						unless ( $tilefound ) {
							die "Error - tile '$assemblyTile' not found in assembly '$assembly' at line $line\n";
						}
					} elsif ( $_ =~ /^size\s+\"(\d+\s+\d+)\"/ ) {
						print "  \\.. size: '$1'\n";
					} else {
						die "Error in line $line\n  near: '$_'\n";
					}
				}
			} elsif ( $tile ) {
				if ( $_ =~ /^((\d+)\s+(\d+))\r?\n$/ ) {
					print "  \\.. size: '$2 $3'\n";
					$tilesize = 1;
					$cols = $2;
					$last = $rows = $3;
				} elsif ( ! $tilesize ) {
					die "Error in line $line - missing size for tile $tile\n";
				} else {
					my @count = ();
					my $count = 0;
					if ( $last > 0 ) {
						$last--;
						chop($_);
						@count = split(/\s+/, $_);
						$count = scalar(@count);
						if ($count != $cols) {
							die "Error in line $line - wrong tile defition - size does not match ($count/$cols) (column count)\n";
						}
					} else {
						die "Error in line $line - wrong tile defition - size does not match ($count/$cols) (not enough rows)\n";
					}
				}
			}
			if ( $_ =~ /^base\s+(.*?)\r?\n/ ) {
				$base = $1;
				$assembly = "";
				print "Use base: '$base'\n";
			} elsif ( $_ =~ /^tile\s+\+(.*?)\r?\n/ ) {
				$tile = $1;
				$tilesize = 0;
				$assembly = "";
				print "...found tile '$tile'\n";
				if ( defined $tiles{$tile} ) {
					die "Error - tile definition of tile '$tile' already exists at line $line\n";
				}
				$tiles{$tile} = 1;
				unless ( $base ) {
					die "Error - tiledefinition before base definition\n";
				} elsif (!-e "$dir/$base$tile.map") {
					die "Tile $tile does not exists: '$base$tile'\n";
				}
			} elsif ( $_ =~ /^assembly\s+(.*?)\r?\n/ ) {
				$assembly = $1;
				print "...found assembly '$assembly'\n";
			}
		}
		print "==============================================\n";
		print "\n";
		close ( UMP );
		$found++;
	}
	return $found;
}

#read the given dir
my $file = $ARGV[0] || undef;
print "=====================================================\n";
print "Umpchecker for UFO:AI (http://sf.net/projects/ufoai)\n";
print "Will search for non existing tiles and some other\n";
print "potential errors in ump-files\n";
print "=====================================================\n";

checkump(".", $file);

print "...finished\n"
