#!/usr/bin/perl
#######################################
# Copyright (C) 2006 Werner Hoehrer
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
my $MODEL_IN		= 'in.md2';
my $MODEL_OUT		= 'out.md2';

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

package MD2_frame;
use constant FORMAT => (
	Scale			=> ['f', 3, 1 ],
	Translate		=> ['f', 3, 1 ],
	Name			=> 'a16',	# 16chars
	VertexData		=> 'a*'		# The whole rest
);

package MD3;
use base 'Parse::Binary';
use constant FORMAT => (
	Magic		=> 'I',		# magic number. must be equal to "IDP3"
	Version		=> 'I',		# md2 version. must be equal to 15
	Filename	=> 'a64',	# 64chars
	Flags		=> 'I',		#
	NumFrames	=> 'I',		# total number of frames
	NumTags		=> 'I',		# number of tags
	NumMeshes	=> 'I',		# number of meshes
	NumSkins	=> 'I',		# number of textures

	OffsetFrames	=> 'I',		# offset to frame data
	OffsetTags		=> 'I',		# offset to tags
	OffsetMeshes	=> 'I',		# offset to meshes
	OffsetEnd		=> 'I',		# offset to end of file

	MD3_mesh		=> ['a108', '{$NumMeshes}', 1 ],

	Data			=> 'a*'		# TODO: The whole rest .. currently without structure.
);

package MD3_mesh;
use base 'Parse::Binary';
use constant FORMAT => (
	ID			=> 'I',		# 4chars (IDP3)
	Name		=> 'a64',	# 64chars
	Flags		=> 'I',		#
	NumFrames	=> 'I',		# total number of frames
	NumSkins	=> 'I',		# number of textures
	NumVerts	=> 'I',		# number of vertices
	NumTris		=> 'I',		# number of tris
	OffsetTris	=> 'I',		# offset to tris
	OffsetSkins	=> 'I',		# offset to skins
	OffsetTCS	=> 'I',		# offset to ...
	OffsetVerts	=> 'I',		# offset to vertices
	MeshSize	=> 'I'		#
);

package Path;
use base 'Parse::Binary';
use constant FORMAT => ('a64');

package Model;

#######################################
# MD2 file functions
#######################################
sub md2_read ($) {
	my ($filename) = @_;

	# read model file
	my $model_file = MD2->new($filename);

	die "File has wrong magic number \"".$model_file->Magic."\".\n"
		unless ($model_file->Magic == 844121161); # equals "IDP2"

	die "File has wrong format version \"".$model_file->Version."\".\n"
		unless ($model_file->Version == 8);

	return $model_file;
}

sub md2_skins_list ($) {
	my ($model_file) = @_;

	for (my $i=0; $i < $model_file->NumSkins; $i++ ) {
		print "Skin ",$i," \"", $model_file->Path->[$i][0],"\"\n";
	}
}

sub md2_add_skinnum ($$) {
	my ($model_file, $newNumber) = @_;

	# Create skin-list if it isn't there yet
	if (!exists($model_file->{struct}->{Path})) {
		# print "DEBUG: Creating skin list.\n";
		$model_file->{struct}->{Path} = [];
	}

	# Append the new skin(s).
	for (my $i = $model_file->NumSkins; $i < $newNumber; $i++) {
		push (@{$model_file->Path}, ['dummy']);
	}

	# Update following offsets (after skin data) correctly.
	my $data_offset_delta = ($newNumber-$model_file->NumSkins) * 64;
	$model_file->struct->{OffsetST} += $data_offset_delta;		# update offset to s-t texture coordinates
	$model_file->struct->{OffsetTris} += $data_offset_delta;		# update offset to triangles
	$model_file->struct->{OffsetFrames} += $data_offset_delta;	# update offset to frame data
	$model_file->struct->{OffsetGLcmds} += $data_offset_delta;	# update offset to opengl commands
	$model_file->struct->{OffsetEnd} += $data_offset_delta;		# update offset to end of file

	$model_file->struct->{NumSkins} = $newNumber;

	return $model_file;
}

#######################################
# MD3 file functions
#######################################

sub md3_read ($) {
	my ($filename) = @_;

	# read model file
	my $model_file = MD3->new($filename);

	die "File has wrong magic number \"".$model_file->Magic."\".\n"
		unless ($model_file->Magic == 860898377); # equals "IDP3"

	die "File has wrong format version \"".$model_file->Version."\".\n"
		unless ($model_file->Version == 15);

	return $model_file;
}

#######################################
# Generic model file functions
#######################################

sub model_save ($$) {
	my ($model_file, $filename) = @_;

	# Save .md2 file
	$model_file->write($filename);
}

sub model_info ($) {
	my ($filename) = @_;

	my $model_file = model_read($filename);

	if ($filename =~ /.md2$/) {
		print "NumFrames: ", $model_file->NumFrames, " (max is 512)\n";
		print "NumSkins: ", $model_file->NumSkins, " (max is 32)\n";
		print "NumXYZ: ", $model_file->NumXYZ, " (max is 2048)\n";
		print "NumST: ", $model_file->NumST, "\n";
		print "NumTris: ", $model_file->NumTris, " (max is 4096)\n";
		print "NumGLcmds: ", $model_file->NumGLcmds, "\n";
	} elsif ($filename =~ /.md3$/) {
		print "NumFrames: ", $model_file->NumFrames, " (max is 1024)\n";
		print "NumTags: ", $model_file->NumTags, " (max is 16 per frame)\n";
		print "NumMeshes: ", $model_file->NumMeshes, " (max is 32)\n";
		print "NumSkins: ", $model_file->NumSkins, "\n";
		use Data::Dumper;
		$Data::Dumper::Useqq = 1;
		print Dumper($model_file);
		print "\n\n";
		print Dumper($model_file->struct);
		for (my $i=0; $i<$model_file->NumMeshes; $i++) {
		}
	}
}

sub model_read ($) {
	my ($filename) = @_;
	if ($filename =~ /.md2$/) {
		return md2_read($filename);
	} elsif ($filename =~ /.md3$/) {
		return md3_read($filename);
	} else {
		die "unknown file extension for '", $filename, "'\n";
	}
}

sub getString () {
	my $key = '';
	my $string = '';
	use Term::ReadKey;
	do {
		$key = ReadKey(0);
		$string .= $key;
	} while ($key ne "\n");

	chomp($string);
	return $string;
}

#######################################
# MAIN
#######################################

my $param_action;
# parse commandline parameters (md2-filenames)
if ( $#ARGV < 0 ) {
	die "Usage:\t$0 [skinedit|skinnum] [options...]\n";
} elsif ( $#ARGV >= 0 ) {
	$param_action = $ARGV[0];
	unless (
		($param_action eq 'skinedit') ||
		($param_action eq 'skinnum') ||
		($param_action eq 'skinsize') ||
		($param_action eq 'info')
		) {
		print "Unknown action: '", $param_action, "'\n";
		die "Usage:\t$0 [skinedit|skinnum|skinsize|info] [options...]\n";
	}
}

if ($param_action eq 'skinedit') {
	# We are changing skin-paths

	my @TextureString = ('');

	# parse commandline parameters (md2-filenames)
	if ( $#ARGV == 1 ) {
		$MODEL_IN = $MODEL_OUT = $ARGV[1];
		print "IN=OUT= \"$MODEL_IN\"\n";
	} elsif  ( $#ARGV == 2 ) {
		$MODEL_IN	= $ARGV[1];
		$MODEL_OUT	= $ARGV[2];
		if ($MODEL_OUT eq '-') {
			$MODEL_OUT = $MODEL_IN;
		}
		print "IN = \"$MODEL_IN\"\n";
		print "OUT= \"$MODEL_OUT\"\n";
	} elsif  ( $#ARGV >= 3 ) {
		$MODEL_IN	= $ARGV[1];
		$MODEL_OUT	= $ARGV[2];
		for ( my $i = 1; $i <= $#ARGV - 2; $i++ ) {
			$TextureString[$i] = $ARGV[$i+2];
		}
		print "IN = \"$MODEL_IN\"\n";
		print "OUT= \"$MODEL_OUT\"\n";

		print "TEX= \"$_\"\n" for ( @TextureString );
	}

	# read model file
	my $model_file = model_read($MODEL_IN);

	print $model_file->NumSkins, " Skin(s) found\n";

	# If no texture parameters are given and no skins found inside the file we create one.
	if ($#TextureString == 0) {
		while ($model_file->NumSkins == 0) {
			print "No skins to edit found in this file - adding one. (Use the option 'skinnum' to increase this value.)\n";
			$model_file = md2_add_skinnum($model_file, 1);
		}
	}

	if ($model_file->NumSkins > 0) {
		#just to prevent warnings
		if ( $#TextureString < $model_file->NumSkins ) {
			for ( my $i = $#TextureString + 1; $i < $model_file->NumSkins; $i++ ) {
				$TextureString[$i] = '';
			}
		}

		for (my $i=0; $i < $model_file->NumSkins; $i++ ) {
			print "Skin ",$i," old: \"", $model_file->Path->[$i][0],"\"\n";

			# get new texture-path from user if no filename was given per commandline parameter.
			if ($TextureString[$i] eq '') {
				print "Enter new path (Enter=Skip):";
				$TextureString[$i] = getString();
			}

			# replace texture-path
			if ($TextureString[$i] ne '') {
				$model_file->Path->[$i][0] = $TextureString[$i];
			}

			print "Skin ",$i," new: \"", $model_file->Path->[$i][0],"\"\n";
		}

		# save as another model file
		model_save($model_file, $MODEL_OUT);
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
		die "Usage:\t$0 skinnum [in.md2 [out.md2]]\n";
	} elsif ( $#ARGV == 1 ) {
		$MODEL_IN = $MODEL_OUT = $ARGV[1];
		print "IN=OUT= \"$MODEL_IN\"\n";
	} elsif  ( $#ARGV == 2 ) {
		$MODEL_IN	= $ARGV[1];
		$MODEL_OUT	= $ARGV[2];
		if ($MODEL_OUT eq '-') {
			$MODEL_OUT = $MODEL_IN;
		}
		print "IN = \"$MODEL_IN\"\n";
		print "OUT= \"$MODEL_OUT\"\n";
	}

	# read .md2 file
	my $model_file = md2_read($MODEL_IN);

	print $model_file->NumSkins, " Skin(s) found\n";

	# DEBUG
	#use Data::Dumper;
	#$Data::Dumper::Useqq = 1;
	#print Dumper($model_file->struct);

	# Print Skins
	md2_skins_list($model_file);

	# Ask for new skin-number
	#use Scalar::Util::Numeric qw(isint);
	my $NumSkins_new = $model_file->NumSkins;
	do { # TODO: as until we get a sane number (integer)
		print "Enter new skin number:";
		$NumSkins_new = getString();
	#} while (!isint($NumSkins_new));
	} while (($NumSkins_new == $model_file->NumSkins) || ($NumSkins_new <= 0) || ($NumSkins_new eq ''));

	if ($NumSkins_new == $model_file->NumSkins) {
		print "No change in skin-number.\n";
		return;
	}

	if ($NumSkins_new <= 0) {
		print "Invalid skin number given: ",$NumSkins_new, "\n";
		return;
	}

	# Add/remove skins and update offsets correctly
	if ($NumSkins_new > $model_file->NumSkins) {
		# So the magic (add skins and update offsets correctly)
		print "Adding skins and updating offsets ...\n";

		$model_file = md2_add_skinnum($model_file, $NumSkins_new);
	} else {
		# TODO: do the magic (remove skins and update offsets correctly)
		print "Removing skins and updating offsets ...\n";
		print "Not implemented yet, aborting!\n";
	}

	# Print Skins
	md2_skins_list($model_file);
	#$Data::Dumper::Useqq = 1;
	#print Dumper($model_file);

	# save as another model file
	model_save($model_file, $MODEL_OUT);
} elsif ($param_action eq 'skinsize') {
	# parse commandline parameters (md2-filenames)
	if ( $#ARGV < 1 || $#ARGV > 2) {
		die "Usage:\t$0 skinsize [in.md2 [out.md2]]\n";
	} elsif ( $#ARGV == 1 ) {
		$MODEL_IN = $MODEL_OUT = $ARGV[1];
		print "IN=OUT= \"$MODEL_IN\"\n";
	} elsif  ( $#ARGV == 2 ) {
		$MODEL_IN	= $ARGV[1];
		$MODEL_OUT	= $ARGV[2];
		if ($MODEL_OUT eq '-') {
			$MODEL_OUT = $MODEL_IN;
		}
		print "IN = \"$MODEL_IN\"\n";
		print "OUT= \"$MODEL_OUT\"\n";
	}

	my $model_file = md2_read($MODEL_IN);

	print "Changing skin sizes ...\n";
	print "Current size: ", $model_file->SkinWidth, "w x ", $model_file->SkinHeight, "h\n";

	# Ask for new skin width
	my $SkinWidth_new = $model_file->SkinWidth;
	my $SkinHeight_new = $model_file->SkinHeight;
	do { # TODO: as until we get a sane number (integer)
		print "Enter new width in pixel (",$model_file->SkinWidth,"):";
		$SkinWidth_new = getString();
	} while (($SkinWidth_new == $model_file->SkinWidth) || ($SkinWidth_new <= 0) || ($SkinWidth_new eq ''));

	# Ask for new skin height
	do { # TODO: as until we get a sane number (integer)
		print "Enter new height in pixel (",$model_file->SkinHeight ,"):";
		$SkinHeight_new = getString();
	} while (($SkinHeight_new == $model_file->SkinHeight) || ($SkinHeight_new <= 0) || ($SkinHeight_new eq ''));


	if (($SkinHeight_new == $model_file->SkinHeight)
	&& ($SkinWidth_new == $model_file->SkinWidth)) {
		print "No change in skin size.\n";
		return;
	}

	$model_file->struct->{SkinWidth} = $SkinWidth_new;
	$model_file->struct->{SkinHeight} = $SkinHeight_new;

	model_save($model_file, $MODEL_OUT);
} elsif ($param_action eq 'info') {
	model_info($ARGV[1]);
} else {
	print "Unknown action: '", $param_action, "'\n";
}



__END__

##########
# EOF
##########
