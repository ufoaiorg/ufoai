#!/usr/bin/perl
use strict;

my $extra = "";

sub readDir
{
	my $dir = shift || die "No dir given in sub readDir\n";
	my $status = opendir (DIR, "$dir");
	my @files = ();
	unless ( $status )
	{
 		print "Could not read $dir\n";
 		return ();
	}

	@files = readdir ( DIR );
	closedir (DIR);
	@files;
}

sub compile
{
	my $dir = shift || die "No dir given in sub compile\n";
	my $found = 0;
	print "...entering $dir\n";
	foreach ( readDir( $dir ) )
	{
		next if $_ =~ /^\.|(CVS)/;
		if ( -d "$dir/$_" )
		{
			print "...found dir $_\n";
			$found += compile("$dir/$_");
 			print "...dir $dir/$_ finished\n";
			next;
		}
		next unless $_ =~ /\.map$/;
		next if $_ =~ /^(tutorial)|(prefab)/i;
		$_ =~ s/\.map$//;
		unless ( -e "$dir/$_.bsp" )
		{
			print "..found $dir/$_\n";
			if (system("qbsp3 $dir/$_.map") != 0)
			{
				die "qbsp3 failed: $@";
			}
			if (system("qrad3 $extra $dir/$_.bsp") != 0)
			{
				die "qrad3 failed: $@";
			}
			$found++;
		}
		else
		{
			print "..already compiled $_\n";
		}
	}
	return $found;
}

#read the given dir
my $dir = $ARGV[0] || ".";
print "=====================================================\n";
print "Mapcompiler for UFO:AI (http://sf.net/projects/ufoai)\n";
print "Giving base/maps as parameter or start from base/maps\n";
print "will compile all maps were no bsp-file exists\n";
print "Keep in mind that qbsp3 and qrad3 needs to be in path\n";
print "=====================================================\n";

die "Dir $dir does not exists\n" if ( !-e $dir );


print "Found ". compile($dir) ." maps\n";

print "...finished\n"
