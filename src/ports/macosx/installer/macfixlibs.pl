#!/usr/bin/perl -W
use strict;

sub fixframeworkpath {
	my $old = shift;
	return $old if $old =~ /\.dylib$/;

	if($old =~ /\@executable_path\/\.\.\/Frameworks\/.*\/([^\/]+)$/) {
		return "/Library/Frameworks/$1.framework/$1";
	}
	return $old;
}

sub findlibs {
	my $target = shift;
	my $otool = `otool -L $target`;
	my @libs;
	foreach my $l (split /\n/,$otool) {
		chomp $l;
		$l =~ s/^\t*//;
		if($l =~ /([^\ ]+) \(/) {
			my $lib = $1;
			### ignore standard mac/darwin system libs and frameworks:
			next if $lib =~ /\/System\/Library\/Frameworks/;
			next if $lib =~ /\/usr\/lib/;
			next if $lib =~ /$target/; #skip self
			push @libs, $lib;
		}
	}
	return @libs;
}

my $libdir = 'UFO.app/Contents/Libraries';
my $fwdir = 'UFO.app/Contents/Frameworks';
my $binpath = 'UFO.app/Contents/MacOS/ufo';
my @a = findlibs 'UFO.app/Contents/MacOS/ufo';
my %alllibs;

sub addtoall {
	my $aref = shift;
	my @a = @{$aref};

	foreach my $x (@a) {
		next if defined $alllibs{$x};

		my $nx = fixframeworkpath $x;
		my @libslibs = findlibs $nx;
		my $ref = \@libslibs;
		$alllibs{$x} = $ref;

		next if $#libslibs < 0;
		addtoall( $ref );
	}
}

addtoall \@a;

sub installlib {
	my ($lib, $libsref) = @_;
	my @libs = @{$libsref};

	my $libname;
	my $newpath;
	my $fworlib;

	if($lib =~ /.*\/([^\/]+)$/) {
		$libname = $1;
	} else {
		die "wtf?\n";
	}

	print "Installing $lib...";
	my $nlib = fixframeworkpath $lib;

	if($lib =~ /\.dylib$/) {
		# && instead of || because error is reported as 'true'
		`cp $lib $libdir/` && die "failed\n"; 
		$newpath = "$libdir/$libname";
		$fworlib = "Libraries";
	} else {
		`cp $nlib $fwdir/` && die "failed\n"; 
		$newpath = "$fwdir/$libname";
		$fworlib = "Frameworks";
	}

	# change my id
	#print "install_name_tool -id \@executable_path/../$fworlib/$libname $newpath\n";
	`install_name_tool -id \@executable_path/../$fworlib/$libname $newpath` && die "failed\n";

	# change my deps
	foreach my $i (@libs) {
		next if $i eq $lib;
		
		my $fworlib2;
		my $libname2;

		if($i=~/\.dylib$/) {
			$fworlib2 = "Libraries";
		} else {
			$fworlib2 = "Frameworks";
		}

		if($i =~ /.*\/([^\/]+)$/) {
			$libname2 = $1;
		} else {
			die "wtf?\n";
		}

		my $ni = fixframeworkpath $i;

		#print "install_name_tool -change $i \@executable_path/../$fworlib2/$libname2 $newpath\n";
		`install_name_tool -change $ni \@executable_path/../$fworlib2/$libname2 $newpath\n` && die "failed\n";
	}

	print "done\n";
}

while ( my ($x,$y) = each %alllibs ) {
	installlib $x,$y,"yes"; 
}

## and, finally, change for app itself
print "Finalizing...";
foreach my $i (@a) {
	my $fworlib2;
	my $libname2;

	if($i=~/\.dylib$/) {
		$fworlib2 = "Libraries";
	} else {
		$fworlib2 = "Frameworks";
	}

	if($i =~ /.*\/([^\/]+)$/) {
		$libname2 = $1;
	} else {
		die "wtf?\n";
	}

	#print "install_name_tool -change $i \@executable_path/../$fworlib2/$libname2 $binpath\n";
	`install_name_tool -change $i \@executable_path/../$fworlib2/$libname2 $binpath\n` && die "failed\n";
}
print "done\n";

