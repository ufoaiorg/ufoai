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
#
#	Change skin/shader paths
#		md2.pl skinedit [in.md2 [out.md2 [texturefile(s)]]]
#
#	Change number of skins in the file.
#		md2.pl skinnum [in.md2 [out.md2]]
#
#	Print model info
#		md2.pl info in.md2
#
#	Change skin sizes (MD2 only)
#		md2.pl skinsize in.md2
#
#	If [in.md2] is given it will also be used as outputfile by default.
#	Right now providing a texture file is only possible if three or more arguments are given.
#	You can provide multiple texture-files seperated by spaces.
#	To not re-type the output-file if the name is the same as the input file just use - instead.
#		e.g.: md2.pl skinedit model.md2 - texturefile
#######################################
use strict;
use warnings;

# Default values for filepaths
my $MODEL_IN		= 'in.md2';
my $MODEL_OUT		= 'out.md2';


#######################################
package Magic;
# Only used for reading header info of a file.
# Everything after the first 2 32bit ints is ignored.
use base 'Parse::Binary';
use constant FORMAT => (
	Magic		=> 'l',		# Magic number.
	Version		=> 'l'		# MDx version.
);
#######################################

#######################################
package MD2;
use base 'Parse::Binary';
use constant FORMAT => (
	Magic		=> 'l',		# magic number. must be equal to "IDP2"
	Version		=> 'l',		# md2 version. must be equal to 8
	SkinWidth		=> 'l',		# width of the texture
	SkinHeight		=> 'l',		# height of the texture
	FrameSize		=> 'l',		# size of one frame in bytes

	NumSkins		=> 'l',		# number of textures
	NumXYZ		=> 'l',		# number of vertices (x,y,z)
	NumST		=> 'l',		# number of texture coordinates (s,t)
	NumTris		=> 'l',		# number of triangles (md2 only supports tris)
	NumGLcmds	=> 'l',		# number of OpenGL commands
	NumFrames		=> 'l',		# total number of frames
	OffsetSkins		=> 'l',		# offset to skin names (64 bytes each)
	OffsetST		=> 'l',		# offset to s-t texture coordinates
	OffsetTris		=> 'l',		# offset to triangles
	OffsetFrames	=> 'l',		# offset to frame data
	OffsetGLcmds	=> 'l',		# offset to opengl commands
	OffsetEnd		=> 'l',		# offset to end of file
	MD2_path		=> ['a64', '{$NumSkins}', 1 ],	# 64chars * NumSkins -> see "package Path"
	Data			=> 'a*'		# TODO: The whole rest .. currently without structure.
);

package MD2_path;
use base 'Parse::Binary';
use constant FORMAT => ('Z64');		# 64chars (Z = 0-terminated)

#package MD2_frame;
#use constant FORMAT => (
#	Scale			=> ['f', 3, 1 ],
#	Translate		=> ['f', 3, 1 ],
#	Name			=> 'Z16',	# 16chars (Z = 0-terminated)
#	TODO: the whole rest
#);

#######################################
# For an overview of the MD3 spec see:
# http://www.icculus.org/homepages/phaethon/q3/formats/md3format.html
# Copyright of some of the comments used here: PhaethonH (phaethon@linux.ucla.edu)

#package MD3_header;	# Only used to get the important offset data.
#use base 'Parse::Binary';
#use constant FORMAT => (
#	Magic		=> 'l',		# magic number. must be equal to "IDP3"
#	Version		=> 'l',		# md2 version. must be equal to 15
#	Filename	=> 'Z64',	# 64chars (Z = 0-terminated)
#	Flags		=> 'l',		#
#	NumFrames	=> 'l',		# total number of frames
#	NumTags		=> 'l',		# number of tags
#	NumSurfaces	=> 'l',		# number of meshes
#	NumSkins	=> 'l',		# number of textures
#
#	OffsetFrames	=> 'l',		# offset to frame data
#	OffsetTags		=> 'l',		# offset to tags
#	OffsetMeshes	=> 'l',		# offset to meshes
#	OffsetEnd		=> 'l',		# offset to end of file
#
#	Data			=> 'a*'		# TODO: The whole rest .. currently without structure.
#);

package MD3;
# This package always assumes the order of data like: frame->tag->surface (i.e. in the order the "num" values are defined)
# @todo: If we ever encounter an md3 file that has a different order we might want to code a re-order function for sanitys sake.
use base 'Parse::Binary';
use constant FORMAT => (
	Magic		=> 'l',		# magic number. must be equal to "IDP3"
	Version		=> 'l',		# md2 version. must be equal to 15
	Filename	=> 'Z64',	# 64chars (Z = 0-terminated)
	Flags		=> 'l',		#
	NumFrames	=> 'l',		# Total number of frame objects.
	NumTags		=> 'l',		# Number of tag objects.
	NumSurfaces	=> 'l',		# Number of surface objects.
	NumSkins	=> 'l',		# Number of Skin objects. I should note that I have not seen an MD3 using this particular field for anything; this appears to be an artifact from the Quake 2 MD2 format. Surface objects have their own Shader field.

	OffsetFrames	=> 'l',		# offset to frame data
	OffsetTags	=> 'l',		# offset to tags
	OffsetSurfaces	=> 'l',		# offset to meshes
	OffsetEnd	=> 'l',		# offset to end of file

#	prelude_MD3_frame	=> 'a{$OffsetFrames-108}',	# Just in case $OffsetFrames is _not_ 108.
	MD3_frame	=> ['a56', '{$NumFrames}', 1 ],

	#prelude_MD3_tag	=> 'a{xxxx}',
	MD3_tag		=> ['a112', '{$NumTags}', 1 ],

	#prelude_MD3_surface	=> 'a{xxxx}',
	MD3_surface	=> ['a{$OffsetEnd - $OffsetSurfaces}', '{$NumSurfaces}', 1 ],

	Data			=> 'a*'		# @todo: The whole rest .. currently without structure.
);

package MD3_frame;		# @todo: Fixme. For some reason this doesn't parse correctly.
use base 'Parse::Binary';
use constant FORMAT => (
	#MinBounds	=> ['f', 3, 1 ],	#<-- needs extra module
	MinBoundsX	=> 'f',	# First corner of the bounding box.
	MinBoundsY	=> 'f',
	MinBoundsZ	=> 'f',

	#MaxBounds	=> ['f', 3, 1 ],
	MaxBoundsX	=> 'f',	# Second corner of the bounding box.
	MaxBoundsY	=> 'f',
	MaxBoundsZ	=> 'f',

	#LocalOrigin	=> ['f', 3, 1 ],
	LocalOriginX	=> 'f',	# Local origin, usually (0, 0, 0).
	LocalOriginY	=> 'f',
	LocalOriginZ	=> 'f',

	Radius		=> 'f',			# Radius of bounding sphere.
	Name		=> 'Z16'		# Name of Frame. 16chars (Z = 0-terminated)
);

package MD3_tag;
use base 'Parse::Binary';
use constant FORMAT => (
	Name		=> 'Z64',	# Name of the tag object. 64chars (Z = 0-terminated)
	#Loc		=> ['f', 3, 1 ],	# Coordinates of Tag object.
	LocX	=> 'f',
	LocY	=> 'f',
	LocZ	=> 'f',
	#AxisX		=> ['f', 3, 1 ],	# Orientation of Tag object.... ?
	#AxisY		=> ['f', 3, 1 ],
	#AxisZ		=> ['f', 3, 1 ]
	AxisX1	=> 'f',
	AxisX2	=> 'f',
	AxisX3	=> 'f',
	AxisY1	=> 'f',
	AxisY2	=> 'f',
	AxisY3	=> 'f',
	AxisZ1	=> 'f',
	AxisZ2	=> 'f',
	AxisZ3	=> 'f'
);

package MD3_surface;	# Equals MD2 "meshes"
# This package always assumes the order of data like: shader->tris->texcoord->vertex (altough I'm not sure this is the preferred).
# @todo: If we ever encounter an md3 file that has a different order we might want to code a re-order function for sanitys sake.
use base 'Parse::Binary';
use constant FORMAT => (
	ID			=> 'l',		# 4chars ("IDP3" or 860898377)
	Name		=> 'Z64',	# 64chars (Z = 0-terminated)
	Flags		=> 'l',		#
	NumFrames	=> 'l',		# Number of animation frames. This should match NUM_FRAMES in the MD3 header.
	NumShaders	=> 'l',		# Number of Shader objects defined in this Surface, with a limit of MD3_MAX_SHADERS. Current value of MD3_MAX_SHADERS is 256.
	NumVerts	=> 'l',		# Number of Vertex objects defined in this Surface, up to MD3_MAX_VERTS. Current value of MD3_MAX_VERTS is 4096.
	NumTris		=> 'l',		# Number of Triangle objects defined in this Surface, maximum of MD3_MAX_TRIANGLES. Current value of MD3_MAX_TRIANGLES is 8192.
	OffsetTris	=> 'l',		# Offset to tris (faces/polygons).
	OffsetShaders	=> 'l',		# Relative offset from the top of MD3_surface to where the list of Shader objects (texture paths) starts.
	OffsetST	=> 'l',		# Relative offset from the top of MD3_surface to where the list of St objects (Texture Coordinates, S-T vertices) starts.
	OffsetVerts	=> 'l',		# Relative offset from the top of MD3_surface to where the kist if vertex objects (x,y,z,n) starts.
	OffsetEnd	=> 'l',		# Relative offset from the top of MD3_surface to where the Surface object ends. (i.e. The size of the whole surface object).
	MD3_Shader	=> ['a68', '{$NumShaders}', 1 ],
	MD3_Triangle	=> ['a12', '{$NumTris}', 1 ],
	MD3_TexCoord	=> ['a8', '{$NumVerts}', 1 ],
	MD3_Vertex	=> ['a8', '{$NumVerts}', 1 ]
);

package MD3_Shader;
use base 'Parse::Binary';
use constant FORMAT => (
	Path		=> 'Z64',	# Filename of the texture. 64chars (Z = 0-terminated)
	ShaderIndex	=> 'l'		# Shader index number. unused
);

package MD3_Triangle;
use base 'Parse::Binary';
use constant FORMAT => (
	X	=> 'l',
	Y	=> 'l',
	Z	=> 'l'
);

package MD3_TexCoord;
use base 'Parse::Binary';
use constant FORMAT => (
	#ST	=> ['f', 2, 1],
	S	=> 'f',
	T	=> 'f'
);

package MD3_Vertex;
use base 'Parse::Binary';
use constant FORMAT => (
	#Loc		=> ['s', 3, 1 ],
	LocX		=> 's',	# Multiply with 1.0/64 to get original value.
	LocY		=> 's',
	LocZ		=> 's',
	Normal		=> 's'
);

#######################################

#######################################
package Model;

#######################################
# MD2 file functions
#######################################
sub md2_read ($) {
	my ($filename) = @_;

	# read model file

	my $mag = Magic->new($filename);

	if (($mag->Magic == 844121161)	# Magic number. Equals "IDP2"
	 && ($mag->Version == 8)) {		# File format version.
		my $md2 = MD2->new($filename);
		# @todo: More sanity checks?
		return $md2;
	} else {
		return 0;
	}
}

sub md2_skins_list ($) {
	my ($model_file) = @_;

	for (my $i=0; $i < $model_file->NumSkins; $i++ ) {
		print "Skin ",$i," \"", $model_file->MD2_path->[$i][0],"\"\n";
	}
}

sub md2_add_skinnum ($$) {
	my ($model_file, $newNumber) = @_;

	# Create skin-list if it isn't there yet
	if (!exists($model_file->{MD2_path})) {
		# print "DEBUG: Creating skin list.\n";
		$model_file->{MD2_path} = [];
	}

	# Append the new skin(s).
	for (my $i = $model_file->NumSkins; $i < $newNumber; $i++) {
		push (@{$model_file->MD2_path}, ['dummy']);
	}

	# Update following offsets (after skin data) correctly.
	my $data_offset_delta = ($newNumber-$model_file->NumSkins) * 64;
	$model_file->struct->{OffsetST} += $data_offset_delta;		# update offset to s-t texture coordinates
	$model_file->struct->{OffsetTris} += $data_offset_delta;		# update offset to triangles
	$model_file->struct->{OffsetFrames} += $data_offset_delta;	# update offset to frame data
	$model_file->struct->{OffsetGLcmds} += $data_offset_delta;	# update offset to opengl commands
	$model_file->struct->{OffsetEnd} += $data_offset_delta;		# update offset to end of file

	$model_file->struct->{NumSkins} = $newNumber;
}

#######################################
# MD3 file functions
#######################################

sub md3_read ($) {
	my ($filename) = @_;

	# read model file
	my $mag = Magic->new($filename);

	if (($mag->Magic == 860898377)	# Magic number. Equals "IDP3"
	 && ($mag->Version == 15)) {	# File format version.
		my $md3 = MD3->new($filename);
		# @todo: More sanity checks?
		if (length($md3->Data) != 0) {
			print "Error:\n",
			"\tData found behind MD3 structure.\n",
			"\tThis possibly indicates wrong offsets or number of entries\n",
			"in the header(s) in the original MD3 file.\n";
			return 0;
		}
		return $md3;
	} else {
		return 0;
	}
}

sub md3_skins_list ($) {
	my ($model_file) = @_;

	die "md3_skins_list only supports MD3 files!.\n"
		unless (ref($model_file) eq "MD3");

	for (my $i=0; $i < $model_file->NumSurfaces; $i++) {
		my $surface = $model_file->children->{'MD3_surface'}[$i];
		#use Data::Dumper;
		#$Data::Dumper::Useqq = 1;
		#print Dumper($surface);
		if ($surface->NumShaders > 0) {
			print $surface->NumShaders, " skins for surface \"", $surface->Name , "\" (", $i, ") found:\n";
			for (my $j=0; $j < $surface->NumShaders; $j++) {
				my $shader = $surface->children->{MD3_Shader};
				#print Dumper($shader);
				if ($shader->[$j]) {
					print " Skin ", $j, " \"", $shader->[$j]->Path, "\"\n";
				} else {
					print "Skin number set to ", $surface->NumShaders, ", but no entry found at ", $j, "!\n";
				}
			}
		} else {
			print "No skins found for surface \"", $surface->Name , "\" (", $i, ").\n";
		}
	}
}

sub md3_update_offsets ($) {
	my ($model) = @_;

	my $headerSize = (11 * 4) + 64;		# 108 = 11 * 'l'(4 byte) + Z64 (64 byte)
	my $ofsTags = $headerSize;
	my $ofsSurf = $headerSize;

	die "md3_update_offsets only supports MD3 files!.\n"
		unless (ref($model) eq "MD3");

	$model->set_field('OffsetFrames', $headerSize);

	# Calculate the size of the frames-array and add its size to the header-size (i.e. to the previous offset to the frames.)
	# TODO: there may be a better way to get the size of this static "MD3_frame" struct
	$ofsTags += ($model->NumFrames * length($model->MD3_frame->[0][0])) if ($model->NumFrames > 0);
	$model->set_field('OffsetTags', $ofsTags);

	# Calculate the size of the tags-array and add its size to the header-size (i.e. to the previous offset of )
	$ofsSurf = $ofsTags;
	$ofsSurf += ($model->NumTags *length($model->MD3_tag->[0][0])) if ($model->NumTags > 0);
	$model->set_field('OffsetSurfaces', $ofsSurf);

	# Update offsets inside all the MD3 surfaces.
	for (my $i = 0; $i < $model->NumSurfaces; $i++) {
		my $surface = $model->children->{'MD3_surface'}[$i];

		my $SurfaceHeaderSize = 11 * 4 + 64;	# 108 = 11 * 'l'(4 byte) + Z64 (64 byte)
		my $ofsShader = $SurfaceHeaderSize;
		my $ofsTris = $SurfaceHeaderSize;
		my $ofsST = $SurfaceHeaderSize;
		my $ofsVerts = $SurfaceHeaderSize;

		$surface->set_field('OffsetShaders', $SurfaceHeaderSize);

		$ofsTris += ($surface->NumShaders * length($surface->MD3_Shader->[0][0])) if ($surface->NumShaders > 0);
		$surface->set_field('OffsetTris', $ofsTris);
		$ofsST = $ofsVerts = $ofsTris;

		$ofsST += ($surface->NumTris * length($surface->MD3_Triangle->[0][0])) if ($surface->NumTris > 0);
		$surface->set_field('OffsetST', $ofsST);
		$ofsVerts = $ofsST;

		$ofsVerts += ($surface->NumVerts * length($surface->MD3_TexCoord->[0][0])) if ($surface->NumVerts > 0);
		$surface->set_field('OffsetVerts', $ofsVerts);

		$surface->set_field('OffsetEnd', $surface->size);
	}

	$model->set_field('OffsetEnd', $model->size);

	$model->refresh();
}


sub md3_add_skinnum ($$$) {
	my ($model_file, $mesh, $newNumber) = @_;
	# $mesh ... MD3 only. Index of the MD3 surface in the file.
	# $newNumber .. New amount of skins for this mesh.

	my $surface = $model_file->children->{'MD3_surface'}[$mesh];

	# Append the new skin(s).
	for (my $i = $surface->NumShaders; $i < $newNumber; $i++) {
		my $skinEntry = MD3_Shader->new();
		$skinEntry->set_field('Path', "dummy");

		push(@{$surface->children->{MD3_Shader}}, $skinEntry);
		$surface->set_field('NumShaders', $surface->NumShaders+1);
		$surface->refresh();
	}

	md3_update_offsets($model_file);
}

#######################################
# Generic model file functions
#######################################

sub model_read ($) {
	my ($filename) = @_;


	# Check if this is a MD2 file.
	my $model = md2_read($filename);

	if ($model == 0) {
		# Check if this is a MD3 file.
		$model = md3_read($filename);

		if ($model == 0) {
			print "Unknown file format of file '", $filename, "'.\n";
		} else {
			# MD3 file
			print "MD3 file found.\n";

			if (!($filename =~ /\.md3$/)) {
				print "Note: It is suggested to change the file extension to '.md3'.\n\n";
			}

			# Sanity check of data order.
			my $frames =	($model->NumFrames > 0);
			my $tags = 	($model->NumTags > 0);
			my $surfaces =	($model->NumSurfaces > 0);
			if ((($frames && $tags && $surfaces) && !(($model->OffsetFrames < $model->OffsetTags) && ($model->OffsetTags < $model->OffsetSurfaces)))
			|| (($frames && !$tags && $surfaces) && !($model->OffsetFrames < $model->OffsetSurfaces))
			|| ((!$frames && $tags && $surfaces) && !($model->OffsetTags < $model->OffsetSurfaces))
			|| (($frames && $tags && !$surfaces) && !($model->OffsetFrames < $model->OffsetTags))) {
					print "Frames:'". $frames ."' Tags:'". $tags ."' Surfaces:'". $surfaces ."' <--Used\n";
					print "Frames:'". $model->OffsetFrames ."' Tags:'". $model->OffsetTags ."' Surfaces:'". $model->OffsetSurfaces. "' <-- Number\n";
					die "Order of Frames/Tags/Surfaces data of the file is in a different order.\nThis script does not support this order (yet).\n";
			}

			return $model
		}
	} else {
		# MD2 file
		print "MD2 file found.\n";
		if (!($filename =~ /\.md2$/)) {
			print "Note: It is suggested to change the file extension to '.md2'.\n";
		}
		return $model;
	}
}

sub model_save ($$) {
	my ($model_file, $filename) = @_;

	print "Writing model to ", $filename, "\n";
	# Save model file
	$model_file->write($filename);
}

sub model_info ($) {
	my ($filename) = @_;

	my $model_file = model_read($filename);

	if (ref($model_file) eq "MD2") {
		print "NumFrames: ", $model_file->NumFrames, " (max is 512)\n";
		print "NumSkins: ", $model_file->NumSkins, " (max is 32)\n";
		print "NumXYZ: ", $model_file->NumXYZ, " (max is 2048)\n";
		print "NumST: ", $model_file->NumST, "\n";
		print "NumTris: ", $model_file->NumTris, " (max is 4096)\n";
		print "NumGLcmds: ", $model_file->NumGLcmds, "\n";
		print "SkinWidth: ", $model_file->SkinWidth, "\n";
		print "SkinHeight: ", $model_file->SkinHeight, "\n";
	} elsif (ref($model_file) eq "MD3") {
		print "NumFrames: ", $model_file->NumFrames, " (max is 1024)\n";
		print "NumTags: ", $model_file->NumTags, " (max is 16 per frame)\n";
		print "NumSurfaces: ", $model_file->NumSurfaces, " (max is 32)\n";
		#use Data::Dumper;
		#$Data::Dumper::Useqq = 1;
		#print Dumper($model_file);
		model_skins_list($model_file);	#debug
		for (my $i=0; $i<$model_file->NumSurfaces; $i++) {
			my $surface = $model_file->children->{'MD3_surface'}[$i];
			print "Name: ", $surface->Name, "\n";
			print "Flags: ", $surface->Flags, "\n";
			print "NumShaders: ", $surface->NumShaders, " (max is 256)\n";
			print "NumFrames: ", $surface->NumFrames, " (max is 1024)\n";
			print "NumTris: ", $surface->NumTris, " (max is 8192)\n";
			print "NumXYZ: ", $surface->NumVerts, " (max is 4096)\n";
			die "Mesh ",$i," has wrong magic number \"".$surface->ID."\".\n"
				unless ($surface->ID == 860898377); # equals "IDP3"
		}
	}
}

sub model_get_skin ($$$) {
	my ($model_file, $mesh, $i) = @_;
	# $mesh ... MD3 only. Index of the MD3 surface in the file.
	# $i ... Index of the Skin in the file (MD2) or the index of the skin in the surface (MD3).

	if (ref($model_file) eq "MD2") {
		return $model_file->MD2_path->[$i][0]
	} elsif (ref($model_file) eq "MD3") {
		my $surface = $model_file->children->{'MD3_surface'}[$mesh];
		return $surface->children->{MD3_Shader}->[$i]->Path;
	} else {
		die "Unknown filetype (\"", ref($model_file), "\") found in model_get_skin.\n";
	}
}

sub model_set_skin ($$$$) {
	my ($model_file, $mesh, $i, $skin) = @_;
	# $mesh ... MD3 only. Index of the MD3 surface in the file.
	# $i ... Index of the Skin in the file (MD2) or the idnex of the skin in the surface (MD3).
	# $skin ... New skin string.

	if (ref($model_file) eq "MD2") {
		$model_file->MD2_path->[$i][0] = $skin;
	} elsif (ref($model_file) eq "MD3") {
		my $surface = $model_file->children->{'MD3_surface'}[$mesh];
		my $s = $surface->children->{MD3_Shader}->[$i];
		$s->set_field('Path', $skin);
		$s->refresh();
	} else {
		die "Unknown filetype (\"", ref($model_file), "\") found in model_set_skin.\n";
	}
}

sub model_skins_list ($) {
	my ($model_file) = @_;
	if (ref($model_file) eq "MD2") {
		md2_skins_list($model_file);
	} elsif (ref($model_file) eq "MD3") {
		md3_skins_list($model_file);
	} else {
		die "Unknown filetype (\"", ref($model_file), "\") found in model_skins_list.\n";
	}
}

sub model_get_skinnum ($$) {
	my ($model_file, $mesh) = @_;
	# $mesh ... MD3 only. Index of the MD3 surface in the file.

	if (ref($model_file) eq "MD2") {
		return $model_file->NumSkins;
	} elsif (ref($model_file) eq "MD3") {
		return $model_file->children->{'MD3_surface'}[$mesh]->NumShaders;
	} else {
		die "Unknown filetype (\"", ref($model_file), "\") found in model_get_skinnum.\n";
	}
}

sub model_add_skinnum ($$$) {
	my ($model_file, $mesh, $num) = @_;
	# $mesh ... MD3 only. Index of the MD3 surface in the file.

	if (ref($model_file) eq "MD2") {
		md2_add_skinnum($model_file, $num);
	} elsif (ref($model_file) eq "MD3") {
		md3_add_skinnum($model_file, $mesh, $num);
	} else {
		die "Unknown filetype (\"", ref($model_file), "\") found in model_add_skinnum.\n";
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
# parse commandline parameters (model-filenames)
if ($#ARGV < 0) {
	die "Usage:\t$0 [skinedit|skinnum|skinsize|info] [options...]\n";
} elsif ($#ARGV >= 0) {
	$param_action = $ARGV[0];
	unless (
		($param_action eq 'skinedit') ||
		($param_action eq 'skinnum') ||
		($param_action eq 'skinsize') ||
		($param_action eq 'info')
		) {
		print "Unknown action: '", $param_action, "'.\n";
		die "Usage:\t$0 [skinedit|skinnum|skinsize|info] [options...]\n";
	}
}

if ($param_action eq 'skinedit') {
	# We are changing skin-paths

	my @TextureString = ('');

	# parse commandline parameters (md2-filenames)
	if ($#ARGV == 1) {
		$MODEL_IN = $MODEL_OUT = $ARGV[1];
		print "IN=OUT= \"$MODEL_IN\"\n";
	} elsif  ($#ARGV == 2) {
		$MODEL_IN	= $ARGV[1];
		$MODEL_OUT	= $ARGV[2];
		if ($MODEL_OUT eq '-') {
			$MODEL_OUT = $MODEL_IN;
		}
		print "IN = \"$MODEL_IN\"\n";
		print "OUT= \"$MODEL_OUT\"\n";
	} elsif ($#ARGV >= 3) {
		$MODEL_IN	= $ARGV[1];
		$MODEL_OUT	= $ARGV[2];
		if ($MODEL_OUT eq '-') {
			$MODEL_OUT = $MODEL_IN;
		}
		for (my $i = 1; $i <= $#ARGV - 2; $i++) {
			$TextureString[$i - 1] = $ARGV[$i+2];
		}
		print "IN = \"$MODEL_IN\"\n";
		print "OUT= \"$MODEL_OUT\"\n";

		print "TEX= \"$_\"\n" for (@TextureString);
	}

	# TODO: Commandline support
	my $mesh = 0;

	# read model file
	my $model_file = model_read($MODEL_IN);
	#use Data::Dumper;
	#$Data::Dumper::Useqq = 1;
	#print Dumper($model_file);

	print model_get_skinnum($model_file, $mesh), " Skin(s) found\n";

	# If no texture parameters are given and no skins found inside the file we create one.
	if ($#TextureString == 0) {
		while (model_get_skinnum($model_file, $mesh) == 0) {
			print "No skins to edit found in this file - adding one. (Use the option 'skinnum' to increase this value.)\n";
			model_add_skinnum($model_file, $mesh, 1);
		}
	}

	if (model_get_skinnum($model_file, $mesh) > 0) {
		#just to prevent warnings
		if ($#TextureString < model_get_skinnum($model_file, $mesh)) {
			for (my $i = $#TextureString + 1; $i < model_get_skinnum($model_file, $mesh); $i++ ) {
				$TextureString[$i] = '';
			}
		}

		for (my $i=0; $i < model_get_skinnum($model_file, $mesh); $i++) {
			print "Skin ",$i," old: \"", model_get_skin($model_file, $mesh, $i),"\"\n";

			# get new texture-path from user if no filename was given per commandline parameter.
			if ($TextureString[$i] eq '') {
				print "Enter new path (Enter=Skip):";
				$TextureString[$i] = getString();
			}

			# replace texture-path
			if ($TextureString[$i] ne '') {
				model_set_skin($model_file, $mesh, $i, $TextureString[$i]);
			}

			print "Skin ",$i," new: \"", model_get_skin($model_file, $mesh, $i),"\"\n";
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

	# parse commandline parameters (model-filenames)
	if ($#ARGV < 1 || $#ARGV > 2) {
		die "Usage:\t$0 skinnum [in.md2 [out.md2]]\n";
	} elsif ($#ARGV == 1) {
		$MODEL_IN = $MODEL_OUT = $ARGV[1];
		print "IN=OUT= \"$MODEL_IN\"\n";
	} elsif ($#ARGV == 2) {
		$MODEL_IN	= $ARGV[1];
		$MODEL_OUT	= $ARGV[2];
		if ($MODEL_OUT eq '-') {
			$MODEL_OUT = $MODEL_IN;
		}
		print "IN = \"$MODEL_IN\"\n";
		print "OUT= \"$MODEL_OUT\"\n";
	}

	# TODO: Commandline support
	my $mesh = 0;

	# read model file
	my $model_file = model_read($MODEL_IN);

	print model_get_skinnum($model_file, $mesh), " Skin(s) found\n";

	# DEBUG
	#use Data::Dumper;
	#$Data::Dumper::Useqq = 1;
	#print Dumper($model_file);

	# Print Skins
	model_skins_list($model_file);

	# Ask for new skin-number
	#use Scalar::Util::Numeric qw(isint);
	my $NumSkins_new = model_get_skinnum($model_file, $mesh);
	do { # TODO: as until we get a sane number (integer)
		print "Enter new skin number:";
		$NumSkins_new = getString();
	#} while (!isint($NumSkins_new));
	} while (($NumSkins_new == model_get_skinnum($model_file, $mesh)) || ($NumSkins_new <= 0) || ($NumSkins_new eq ''));

	if ($NumSkins_new == model_get_skinnum($model_file, $mesh)) {
		print "No change in skin-number.\n";
		return;
	}

	if ($NumSkins_new <= 0) {
		print "Invalid skin number given: ",$NumSkins_new, "\n";
		return;
	}

	# Add/remove skins and update offsets correctly
	if ($NumSkins_new > model_get_skinnum($model_file, $mesh)) {
		# So the magic (add skins and update offsets correctly)
		print "Adding skins and updating offsets ...\n";

		model_add_skinnum($model_file, $mesh, $NumSkins_new);
	} else {
		# TODO: do the magic (remove skins and update offsets correctly)
		print "Removing skins and updating offsets ...\n";
		print "Not implemented yet, aborting!\n";
	}

	# Print Skins
	model_skins_list($model_file);
	#$Data::Dumper::Useqq = 1;
	#print Dumper($model_file);

	# save as another model file
	model_save($model_file, $MODEL_OUT);
} elsif ($param_action eq 'skinsize') {
	# parse commandline parameters (md2-filenames)
	if ($#ARGV < 1 || $#ARGV > 2) {
		die "Usage:\t$0 skinsize [in.md2 [out.md2]]\n";
	} elsif ($#ARGV == 1) {
		$MODEL_IN = $MODEL_OUT = $ARGV[1];
		print "IN=OUT= \"$MODEL_IN\"\n";
	} elsif ($#ARGV == 2) {
		$MODEL_IN	= $ARGV[1];
		$MODEL_OUT	= $ARGV[2];
		if ($MODEL_OUT eq '-') {
			$MODEL_OUT = $MODEL_IN;
		}
		print "IN = \"$MODEL_IN\"\n";
		print "OUT= \"$MODEL_OUT\"\n";
	}

	die "Only md2 model format is supported here\n"
		unless ($MODEL_IN =~ /\.md2$/);

	my $model_file = model_read($MODEL_IN);

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
	print "Unknown action: '", $param_action, "'.\n";
}
