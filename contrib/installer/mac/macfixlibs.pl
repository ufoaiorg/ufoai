#!/usr/bin/perl -W
use strict;
use File::stat;
use Fcntl ':mode';

my $appdir = shift;
my @apps = @ARGV;
my @explicit_libs = ('libcurl');
my $libdir = "$appdir/Contents/Libraries";
my $fwdir = "$appdir/Contents/Frameworks";
my %alllibs;

# Replaces the relative framework path returned by otool with
# the path to the library in the framework top level directory.
# We will now only copy the framework from it's original location
# to the application bundle $appdir/Contents/Frameworks directory,
# removing the need to update all of the binary dependencies on
# a given framework.
sub fixframeworkpath {
	my $old = shift;
	return $old if $old =~ /\.dylib$/;

	if($old =~ /\@executable_path\/\.\.\/Frameworks\/.*\/([^\/]+)$/) {
		$old = "/Library/Frameworks/$1.framework/$1";
	}
	return $old;
}

# Given a relative path to a framework, returns a canonical path
# assuming that the framework is installed in /Library/Framework
sub frameworkdir {
	my $old = shift;
	return $old if $old =~ /\.dylib$/;

	if($old =~ /\@executable_path\/\.\.\/Frameworks\/.*\/([^\/]+)$/) {
		$old = "/Library/Frameworks/$1.framework";
	}
	return $old;
}

# Find all the libraries upon which a target binary depends,
# ignoring the standard Mac OS X system libraries
sub findlibs {
	my $target = shift;
	my $otool = `otool -L $target`;
	my @libs;
	foreach my $l (split /\n/,$otool) {
		chomp $l;
		$l =~ s/^\t*//;
		if($l =~ /([^\ ]+) \(/) {
			my $lib = $1;
			foreach my $extra (@explicit_libs) {
				goto doit if $lib =~ /$extra/;
			}
			### ignore standard mac/darwin system libs and frameworks:
			next if $lib =~ /\/System\/Library\/Frameworks/;
			next if $lib =~ /\/usr\/lib/;
			next if $lib =~ /$target/; #skip self
			doit: push @libs, $lib;
		}
	}
	return @libs;
}

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

sub installnametool {
	my ($libname, $newpath) = @_;

	# change my id
	#print "install_name_tool -id \@executable_path/../Libraries/$libname $newpath\n";
	`install_name_tool -id \@executable_path/../Libraries/$libname $newpath` && die "failed\n";
}

sub installlib {
	my ($lib, $libsref) = @_;
	my @libs = @{$libsref};

	my $libname;
	my $newpath;
	my $fworlib;
	my $writable = 1;
	my $oldperms;

	if($lib =~ /.*\/([^\/]+)$/) {
		$libname = $1;
	} else {
		die "wtf?\n";
	}

	print "Installing $libdir/$libname...";
	my $nlib = fixframeworkpath $lib;

	if($lib =~ /\.dylib$/) {
		# && instead of || because error is reported as 'true'
		my $fs = stat( $lib );
		$oldperms = $fs->mode;
		$writable = ($oldperms & S_IWUSR) >> 7;

		if ($libname =~ /libiconv/) {
			$libname = "libiconv.dylib";
		} elsif ($libname =~ /libintl/) {
			$libname = "libintl.dylib";
		}
		$newpath = "$libdir/$libname";
		if ( !$writable ) {
			my $newperms = (S_IWUSR | $oldperms);
			chmod $newperms, $newpath;
		}
		`cp -L -f $lib $newpath` && die "failed\n";

		installnametool $libname,$newpath;
	} else {
		my $framedir = frameworkdir $lib;
		`cp -L -R $framedir $fwdir/` && die "failed\n";
		$newpath = "$fwdir/$libname";
	}

	# change my deps
	foreach my $i (@libs) {
		next if $i eq $lib;

		# update references to dylibs, but not to frameworks,
		# since we just copy frameworks wholesale to the app bundle
		if($i!~/\.dylib$/) {
			next;
		}

		my $libname2;
		if($i =~ /.*\/([^\/]+)$/) {
			$libname2 = $1;
			if ($libname2 =~ /libiconv/) {
				$libname2 = "libiconv.dylib";
			} elsif ($libname2 =~ /libintl/) {
				$libname2 = "libintl.dylib";
			}
		} else {
			die "wtf?\n";
		}

		my $ni = fixframeworkpath $i;

		#print "install_name_tool -change $i \@executable_path/../Libraries/$libname2 $newpath\n";
		`install_name_tool -change $ni \@executable_path/../Libraries/$libname2 $newpath\n` && die "failed\n";
	}

	if ( !$writable ) {
		chmod $oldperms, $newpath;
	}

	print "done\n";
}

sub main {
	opendir(DIR, $libdir) or die $!;
	while (my $lib = readdir(DIR)) {
		next if $lib =~ /^\./;
		print "converting $lib\n";
		installnametool $lib,"$libdir/$lib";

		my @libs = findlibs "$libdir/$lib";
		foreach my $i (@libs) {
			next if $i eq $lib;

			# update references to dylibs, but not to frameworks,
			# since we just copy frameworks wholesale to the app bundle
			if($i!~/\.dylib$/) {
				next;
			}

			my $ni = fixframeworkpath $i;
			my $libname2;
			if($i =~ /.*\/([^\/]+)$/) {
				$libname2 = $1;
				if ($libname2 =~ /libiconv/) {
					$libname2 = "libiconv.dylib";
				} elsif ($libname2 =~ /libintl/) {
					$libname2 = "libintl.dylib";
				}
			} else {
				die "wtf?\n";
			}

			#print "install_name_tool -change $i \@executable_path/../Libraries/$libname2 $newpath\n";
			`install_name_tool -change $ni \@executable_path/../Libraries/$libname2 "$libdir/$lib"\n` && die "failed\n";
		}
	}
	closedir(DIR);

	foreach my $app (@apps) {
		my $binpath = "$appdir/Contents/$app";
		my @a = findlibs "$binpath";
		addtoall \@a;
	}

	while ( my ($x,$y) = each %alllibs ) {
		installlib $x,$y,"yes";
	}

	## and, finally, change for app(s) itself
	foreach my $app (@apps) {
		print "Finalizing $app...\n";
		my $binpath = "$appdir/Contents/$app";
		my @a = findlibs $binpath;
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
				if ($libname2 =~ /libiconv/) {
					$libname2 = "libiconv.dylib";
				} elsif ($libname2 =~ /libintl/) {
					$libname2 = "libintl.dylib";
				}
			} else {
				die "wtf?\n";
			}

			#print "install_name_tool -change $i \@executable_path/../$fworlib2/$libname2 $binpath\n";
			`install_name_tool -change $i \@executable_path/../$fworlib2/$libname2 $binpath\n` && die "failed\n";
		}
	}
	print "done\n";
}

main
