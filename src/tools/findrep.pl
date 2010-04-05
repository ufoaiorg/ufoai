#!/usr/bin/perl

# find&replace - customized script to replace used textures in out map-files
# Copyright (C) 2007, Werner Höhrer (hoehrer)
# Some code borrowed from the svn-clean script: Copyright (C) 2004, 2005, 2006 Simon Perreault
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#####################################################################
# TODO
# * implement "non-recursive"
#
# Changelog
#	2007-11-30	Added correct support of -p parameter.
#####################################################################

use strict;
use Cwd;
use File::Path;
use Getopt::Long;
use Pod::Usage;

my $CWD = getcwd;

my $quiet        = 0;
my $print        = 0;
my $help         = 0;
my $man          = 0;
my $nonrecursive = 0;
my $path         = $CWD;
GetOptions(
    "non-recursive|N" => \$nonrecursive,
    "quiet"           => \$quiet,
    "print"           => \$print,
    "help|?"          => \$help,
    "man"             => \$man
) or pod2usage(2);

pod2usage(1) if $help;
pod2usage( -exitstatus => 0, -verbose => 2 ) if $man;

my $file_regex = $ARGV[0] if @ARGV;
my $search = $ARGV[1] if @ARGV;
my $replace = $ARGV[2] if @ARGV;

#print "Path: ", $path, "\n"; #debug

# Get list of files
my @files_to_check = glob($file_regex);

use File::Find;

# default ARGV values
@ARGV = ('*.map', '', '') unless @ARGV;

sub replace_in_files {
	my $file = $File::Find::name;

	if (not $file =~ m/$file_regex/) { return; }

	print "Processing $file - " unless $quiet || $print; # string continued below.

	open(FH, $file) || die "Cannot open file: $file";
	my @lines = <FH>;
	close(FH);

	my $match = 0;

	foreach my $line (@lines){
		if ($print) {
			# Count if phrase was found only
			if ($line =~ m/$search/) {
				$match += 1;
			}
		} else {
			# Replace & Count
			if($line =~ s/$search/$replace/g){
				$match += 1;
			}
		}
	}

	if ($print) {
		if ($match) {
			print $file, " - ", $match, " matching line(s) found.\n";
		}
		return;
	}

	if($match){
		print "changed\n" unless $quiet;
		open(FS,">$file") || die "Cannot save $file";
		print FS @lines;
		close(FS);
	} else {
		print "nothing changed\n" unless $quiet;
	}
}

find(\&replace_in_files, $path);


__END__

=head1 NAME

find&replace - customized script to replace used textures in out map-files

=head1 SYNOPSIS

findrep [options] filename(s) search-pattern replace-pattern

=head1 DESCRIPTION

B<findrep> will scan the current directory recursively and find files
matching your pattern.
It'll then replace the given string with the new one.

The following command replaces the text "searchpattern" with "replacepattern" in all files ending with .txt:

=over 8

findrep .*\.txt$ "searchpattern" "replacepattern"

=back

=head1 OPTIONS

=item B<-N>, B<--non-recursive>

NOT IMPLEMENTED YET
Do not search recursively.

=item B<-q>, B<--quiet>

Do not print progress info. In particular, do not print a message each time a
file is examined, giving the name of the file.

=item B<-p>, B<--print>

Do not change anything. Instead, print the name of every file the "search-pattern" that was found in. (i.e. a simple search)

Example:

=over 8

findrep .*\.txt$ "searchpattern" -p

=back

=item B<-?>, B<-h>, B<--help>

Prints a brief help message and exits.

=item B<--man>

Prints the manual page and exits.

=back

=head1 AUTHOR

Werner Höhrer bill_spam2 -AT- yahoo.de>

=cut
