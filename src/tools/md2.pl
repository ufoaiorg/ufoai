#!/usr/bin/perl
#######################################
# Copyright (C) 2006 Werner Höhrer
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
# Resources:
# 	MD2 format description  and implemention for C++
#		http://tfc.duke.free.fr/old/models/md2.htm
#
#	Parse::Binary documentation
#		http://search.cpan.org/~smueller/Parse-Binary-0.10/lib/Parse/Binary.pm
#
#	The 'pack' commands character/string descriptions (used in Parse::Binary)
#		http://www.unix.org.ua/orelly/perl/perlnut/c05_106.htm
#
#######################################

#######################################
# Description
#	The script currently just replaces the texture-path from an md2 file with new ones.
# Usage
#	md2.pl skinedit [in.md2 [out.md2 [texturefile(s)]]]
#	md2.pl skinnum [in.md2 [out.md2]]
#
#	If [in.md2] is given it will also be used as outputfile.
#	Right now providing a texture file is only possible if three or more arguments are given.
#	You can provide multiple texture-files seperated by spaces.
#	To not re-type the output-file if the name is the same as the input file just use - instead.
#		e.g.: md2.pl skinedit model.md2 - texurefile
#######################################
use strict;
use warnings;

# Default values for filepaths
my $MD2IN		= 'in.md2';
my $MD2OUT		= 'out.md2';
my $NewSkinPath		= 'texture.jpg';

package MD2;
use base 'Parse::Binary';
use constant FORMAT => (
	Magic		=> 'I',		# magic number. must be equal to "IDP2"
	Version		=> 'I',		# md2 version. must be equal to 8
	SkinWidth		=> 'I',		# width of the texture
	SkinHeight		=> 'I',		# height of the texture
	FrameSize		=> 'I',		# size of one frame in bytes

	NumSkins		=> 'I',		# number of textures
	NumXYZ		=> 'I',		# number of vertices (x,y,z)
	NumST		=> 'I',		# number of texture coordinates (s,t)
	NumTris		=> 'I',		# number of triangles (md2 only supports tris)
	NumGLcmds	=> 'I',		# number of OpenGL commands
	NumFrames		=> 'I',		# total number of frames
	OffsetSkins		=> 'I',		# offset to skin names (64 bytes each)
	OffsetST		=> 'I',		# offset to s-t texture coordinates
	OffsetTris		=> 'I',		# offset to triangles
	OffsetFrames	=> 'I',		# offset to frame data
	OffsetGLcmds	=> 'I',		# offset to opengl commands
	OffsetEnd		=> 'I',		# offset to end of file
	Path			=> ['a64', '{$NumSkins}', 1 ],	# 64chars * NumSkins -> see "package Path"
	Data			=> 'a*'		# TODO: The whole rest .. currently without structure.
);

package Path;
use base 'Parse::Binary';
use constant FORMAT => ('a64');

#package MD2_frame;
#use constant FORMAT => (
##typedef struct
##{
##    float       scale[3];       // scale values
##    float       translate[3];   // translation vector
##    char        name[16];       // frame name
##    vertex_t    verts[1];       // first vertex of this frame
##} frame_t;
#};

#######################################
# MD2 file functions
#######################################
sub md2_read ($) {
	my ($filename) = @_;

	# read .md2 file
	my $md2_file = MD2->new($filename);

	die "File has wrong magic number \"".$md2_file->Magic."\".\n"
		unless ($md2_file->Magic == 844121161); #844121161 equals "IDP2"

	die "File has wrong format version \"".$md2_file->Version."\".\n"
		unless ($md2_file->Version == 8);
	
	return $md2_file;
}

sub md2_save ($$) {
	my ($md2_file, $filename) = @_;

	# Save .md2 file
	$md2_file->write($filename);
}

sub md2_skins_list ($) {
	my ($md2_file) = @_;

	for (my $i=0; $i < $md2_file->NumSkins; $i++ ) {
		print "Skin ",$i," \"", $md2_file->Path->[$i][0],"\"\n";
	}
}

#######################################
# MAIN
#######################################

my $param_action;
# parse commandline parameters (md2-filenames)
if ( $#ARGV < 0 ) {
	die "Usage:\tmd2.pl [skinedit|skinnum] [options...]\n";
} elsif ( $#ARGV >= 0 ) {
	$param_action = $ARGV[0];
	unless (
		($param_action eq 'skinedit') ||
		($param_action eq 'skinnum')
		) {
		print "Unknown action: '", $param_action, "'\n";
		die "Usage:\tmd2.pl [skinedit|skinnum] [options...]\n";
	}
}

if ($param_action eq 'skinedit') {
	# We are changing skin-paths

	my @TextureString = ('');

	# parse commandline parameters (md2-filenames)
	if ( $#ARGV == 1 ) {
		$MD2IN = $MD2OUT = $ARGV[1];
		print "IN=OUT= \"$MD2IN\"\n";
	} elsif  ( $#ARGV == 2 ) {
		$MD2IN	= $ARGV[1];
		$MD2OUT	= $ARGV[2];
		if ($MD2OUT eq '-') {
			$MD2OUT = $MD2IN;
		}
		print "IN = \"$MD2IN\"\n";
		print "OUT= \"$MD2OUT\"\n";
	} elsif  ( $#ARGV >= 3 ) {
		$MD2IN	= $ARGV[1];
		$MD2OUT	= $ARGV[2];
		for ( my $i = 1; $i <= $#ARGV - 2; $i++ ) {
			$TextureString[$i] = $ARGV[$i+2];
		}
		print "IN = \"$MD2IN\"\n";
		print "OUT= \"$MD2OUT\"\n";

		print "TEX= \"$_\"\n" for ( @TextureString );
	}

	# read .md2 file
	my $md2_file = md2_read($MD2IN);

	print $md2_file->NumSkins, " Skin(s) found\n";

	if ($md2_file->NumSkins > 0) {
		#just to prevent warnings
		if ( $#TextureString < $md2_file->NumSkins ) {
			for ( my $i = $#TextureString + 1; $i < $md2_file->NumSkins; $i++ ) {
				$TextureString[$i] = '';
			}
		}

		for (my $i=0; $i < $md2_file->NumSkins; $i++ ) {
			print "Skin ",$i," old: \"", $md2_file->Path->[$i][0],"\"\n";

			# get new texture-path from user if no filename was given per commandline parameter.
			if ($TextureString[$i] eq '') {
				print "Enter new path (Enter=Skip):";

				my $key = '';
				use Term::ReadKey;
				do {
					$key = ReadKey(0);
					$TextureString[$i] .= $key;
				} while ($key ne "\n");

				chomp($TextureString[$i]);
			}

			# replace texture-path
			if ($TextureString[$i] ne '') {
				$md2_file->Path->[$i][0] = $TextureString[$i];
			}

			print "Skin ",$i," new: \"", $md2_file->Path->[$i][0],"\"\n";
			
		}
		
		# save as another .md2 file
		md2_save($md2_file, $MD2OUT);
	} else {
		print "No skins to edit found in this file.\n";
		print "You might want to add some first.\n";
	}
} elsif ($param_action eq 'skinnum') {
	# we are adding/removing skins
	# TODO: this is undocumented and untested right now
	# TODO: add proper commandline handling (maybe use extra file?)
	
	# parse commandline parameters (md2-filenames)
	if ( $#ARGV < 1 || $#ARGV > 2) {
		die "Usage:\tmd2.pl skinnum [in.md2 [out.md2]]\n";
	} elsif ( $#ARGV == 1 ) {
		$MD2IN = $MD2OUT = $ARGV[1];
		print "IN=OUT= \"$MD2IN\"\n";
	} elsif  ( $#ARGV == 2 ) {
		$MD2IN	= $ARGV[1];
		$MD2OUT	= $ARGV[2];
		if ($MD2OUT eq '-') {
			$MD2OUT = $MD2IN;
		}
		print "IN = \"$MD2IN\"\n";
		print "OUT= \"$MD2OUT\"\n";
	} 
	
	# read .md2 file
	my $md2_file = md2_read($MD2IN);

	print $md2_file->NumSkins, " Skin(s) found\n";

	# DEBUG
	#use Data::Dumper;
	#print Dumper($md2_file->struct);
	
	# Print Skins
	md2_skins_list($md2_file);
	
	# Ask for new skin-number
	#use Scalar::Util::Numeric qw(isint);
	use Term::ReadKey;
	my $NumSkins_new = $md2_file->NumSkins;
	do { # TODO: as until we get a sane number (integer)
		print "Enter new skin number:";

		my $key = '';
		$NumSkins_new = '';
		do {
			$key = ReadKey(0);
			$NumSkins_new .= $key;
		} while ($key ne "\n");

		chomp($NumSkins_new);
		
	#} while (!isint($NumSkins_new));
	} while (($NumSkins_new == $md2_file->NumSkins) || ($NumSkins_new <= 0) || ($NumSkins_new eq ''));
	
	if ($NumSkins_new == $md2_file->NumSkins) {
		print "No change in skin-number.\n";
		return;
	}
	
	if ($NumSkins_new <= 0) {
		print "Invalid skin number given: ",$NumSkins_new, "\n";
		return;
	}
	
	# Add/remove skins and update offsets correctly
	if ($NumSkins_new > $md2_file->NumSkins) {
		# TODO: do the magic (add skins and update offsets correctly)
		print "Adding skins and updating offsets ...\n";
		for (my $i = $md2_file->NumSkins; $i < $NumSkins_new; $i++) {
			push (@{$md2_file->Path}, ['dummy']);
		}
		
		# Update following offsets (after skin data) correctly.
		my $data_offset_delta = ($NumSkins_new-$md2_file->NumSkins) * 64;
		$md2_file->struct->{OffsetST} += $data_offset_delta;		# update offset to s-t texture coordinates
		$md2_file->struct->{OffsetTris} += $data_offset_delta;		# update offset to triangles
		$md2_file->struct->{OffsetFrames} += $data_offset_delta;	# update offset to frame data
		$md2_file->struct->{OffsetGLcmds} += $data_offset_delta;	# update offset to opengl commands
		$md2_file->struct->{OffsetEnd} += $data_offset_delta;		# update offset to end of file
		
		$md2_file->struct->{NumSkins} = $NumSkins_new;
	} else {
		# TODO: do the magic (remove skins and update offsets correctly)
		print "Removing skins and updating offsets ...\n";
		print "Not implemented yet, aborting!\n";
	}

	# Print Skins
	md2_skins_list($md2_file);
	#print Dumper($md2_file);

	# save as another .md2 file
	md2_save($md2_file, $MD2OUT);
} else {
	print "Unknown action: '", $param_action, "'\n";
}



__END__

##########
# EOF
##########
