#!/usr/bin/perl
#######################################
# Copyright (C) 2006 Werner H�rer
# Changed to read tag files M. Gerhardy
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
# Usage
#	md2_tag.pl [tagfile]
#######################################
use strict;
use warnings;

# Default values for filepaths
package MD2_tag;
use base 'Parse::Binary';

use constant FORMAT => (
	Indent		=> 'I',		#
	Version		=> 'I',		# tag version. must be equal to 1

	NumTags		=> 'I',		# number of textures
	NumFrames		=> 'I',		# number of vertices (x,y,z)

	OffsetNames		=> 'I',		# offset to skin names (64 bytes each)
	OffsetTags		=> 'I',		# offset to s-t texture coordinates
	OffsetEnd		=> 'I',		# offset to triangles
	OffsetExtractEnd	=> 'I',		# offset to frame data

#	Data			=> ['a4', '{$NumTags*$NumFrames}', 1 ]	# 4chars * NumTags * NumFrames
	Data			=> 'a*'		# the whole rest
);

#######################################
# MAIN
#######################################

my $tagfile = "";

# parse commandline paarameters (tag-filenames)
if ( $#ARGV < 0 ) {
	die "Usage:\tmd2_tag.pl [tagfile]\n";
} elsif ( $#ARGV == 0 ) {
	$tagfile = $ARGV[0];
}

# read .md2 file
my $md2tag_file = MD2_tag->new($tagfile);

die "File has wrong format version \"".$md2tag_file->Version."\".\n" unless ($md2tag_file->Version == 1);


print $md2tag_file->NumTags, " Tags found\n";
print $md2tag_file->NumFrames, " Frames found\n";

__END__

##########
# EOF
##########
