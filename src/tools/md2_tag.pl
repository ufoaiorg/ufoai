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
	Indent		=> 'I',			# should be 844121162
	Version		=> 'I',			# tag version. must be equal to 1

	NumTags		=> 'I',			# number of textures
	NumFrames		=> 'I',		# number of vertices (x,y,z)

	OffsetNames		=> 'I',		# offset to tag names (64 bytes each)
	OffsetTags		=> 'I',		# offset
	OffsetEnd		=> 'I',		# offset
	OffsetExtractEnd	=> 'I',	# offset

	TagName			=> ['a64', '{$NumTags}', 1 ],	# 64chars * NumTags
	Data			=> ['a16', '{$NumTags*$NumFrames}', 1 ]	# 4chars * 4 * NumTags * NumFrames
);

package TagName;
use base 'Parse::Binary';
use constant FORMAT => ('a64');

package Data;
use base 'Parse::Binary';
use constant FORMAT => ('a16');


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

die "File has wrong magic number \"".$md2tag_file->Indent."\".\n" unless ($md2tag_file->Indent == 844121162);
die "File has wrong format version \"".$md2tag_file->Version."\".\n" unless ($md2tag_file->Version == 1);


print $md2tag_file->NumTags, " Tags found\n";
print $md2tag_file->NumFrames, " Frames found\n";
print $md2tag_file->NumFrames, " Tagnames:\n";

for (my $i = 0; $i < $md2tag_file->NumTags; $i++) {
	print "- $md2tag_file->TagData->[$i][0]\n"; #only debug
	print "- ".$md2tag_file->TagData->[$i][0]."\n";
}

__END__

##########
# EOF
##########
