#!/usr/bin/perl
#######################################
# Copyright (C) 2010 Tamás Fehérvári <geever@users.sourceforge.net>
#
#This program is free software; you can redistribute it and/or
#modify it under the terms of the GNU General Public License
#as published by the Free Software Foundation; either version 2
#of the License, or (at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
#See the GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program; if not, write to the Free Software
#Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#######################################

#######################################
# savegametool.pl
#		Script to convert (alter) saved games of UFO:AI
#
#
# Usage: savegametool.pl [option [option ... ]] <in-file> <out-file>
# Options:
#   * -x, --extract:		Convert compressed savegame to uncompressed
#   * -s, --strip-header	Remove the binary header from save
#
#######################################

use strict;
use warnings;
use vars qw/%args/;
use Getopt::Long;

eval 'use Compress::Zlib;';
if ($@) {
	print(STDERR "Please, install Compress::Zlib perl module to use this tool!\n");
	exit 127;
}

GetOptions (
	"x|extract"			=>		\$args{'extract'},
	"s|strip-header"	=>		\$args{'strip'}
);

sub help {
	print("Usage: $0 [options] <infile> <outfile>\n");
}


if (scalar(@ARGV) < 2) {
	help();
	exit 0;
}

if (!open(IN, '<'.$ARGV[0])) {
	print(STDERR "Cannot open input file '".$ARGV[0]."'!\n");
	exit 1;
}
if (!open(OUT, '>'.$ARGV[1])) {
	print(STDERR "Cannot open output file '".$ARGV[1]."'!\n");
	close(IN);
	exit 2;
}
binmode(IN);
binmode(OUT);

my $header;
my ($c, $e);

# read header
sysread(IN, $header, 180);
# read data
while(<IN>) {
	$c .= $_;
}
# extract
if (!$args{'extract'}) {
	$e = $c;
} else {
	my $i;
	my $status;
	($i, $status) = inflateInit( ) ;
	($e, $status) = $i->inflate($c) ;
	if ($status != &Z_OK && $status != &Z_STREAM_END) {
		print(STDERR "Corrupt Zlib stream or incompatible save.\n");
		exit(3);
	}
	# set compressed to qfalse
	$header =~ s/^(....)..../$1\x00\x00\x00\x00/;
	# remove trailing zero
	$e =~ s/\x00$//;
}
# write header
if (!$args{'strip'}) {
	print(OUT $header);
}
# write data
print(OUT $e);

close(OUT);
close(IN);
