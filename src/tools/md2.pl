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
#######################################

#######################################
# Description
#	The script currently just rellaces the texture-path from an md2 file with 
# Usage
#	md2.pl [in.md2 [out.md2 [texturefile]]]
#
#	If [in.md2] is given it will also be used as outputfile
#	Right now providing a texture file is only possible if exactly three arguments are given.
#######################################
use strict;
use warnings;

# Default values for filepaths
my $MD2IN		= 'in.md2';
my $MD2OUT		= 'out.md2';
my $NewSkinPath	='texture.jpg';

package MD2;
use base 'Parse::Binary';
use constant FORMAT => (
	Magic		=> 'I',		# magic number. must be equal to "IDP2"
	Version		=> 'I',		# md2 version. must be equal to 8
	SkinWidth		=> 'I',		# width of the texture
	SkinHeight	=> 'I',		# height of the texture
	FrameSize	=> 'I',		# size of one frame in bytes
	
	NumSkins		=> 'I',		# number of textures
	NumXYZ		=> 'I',		# number of vertices (x,y,z)
	NumST		=> 'I',		# number of texture coordinates (s,t)
	NumTris		=> 'I',		# number of triangles (md2 only supports tris)
	NumGLcmds	=> 'I',		# number of OpenGL commands
	NumFrames	=> 'I',		# total number of frames
	OffsetSkins	=> 'I',		# offset to skin names (64 bytes each)
	OffsetST		=> 'I',		# offset to s-t texture coordinates
	OffsetTris		=> 'I',		# offset to triangles
	OffsetFrames	=> 'I',		# offset to frame data
	OffsetGLcmds	=> 'I',		# offset to opengl commands
	OffsetEnd		=> 'I',		# offset to end of file
	Path			=> ['a64', '{$NumSkins}', 1 ],	# 64chars * NumSkins
	Data		=> 'a*'			# all of the rest.
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
# MAIN
#######################################

my $InputString = '';

# parse commandline paarameters (md2-filenames)
if ( $#ARGV < 0 ) {
	print "Usage:\tmd2.pl [in.md2 [out.md2 [texturefile]]]\n";
} elsif ( $#ARGV == 0 ) {
	$MD2IN = $MD2OUT = $ARGV[0];
	print "IN=OUT= \"$MD2IN\"\n";
} elsif  ( $#ARGV == 1 ) {
	$MD2IN	= $ARGV[0];
	$MD2OUT	= $ARGV[1];
	print "IN = \"$MD2IN\"\n";
	print "OUT= \"$MD2OUT\"\n";
}elsif  ( $#ARGV == 2 ) {
	$MD2IN	= $ARGV[0];
	$MD2OUT	= $ARGV[1];
	$InputString	= $ARGV[2];
	print "IN = \"$MD2IN\"\n";
	print "OUT= \"$MD2OUT\"\n";
	print "TEX= \"$InputString\"\n";
}

# read .md2 file
my $md2_file = MD2->new($MD2IN);

if ($md2_file->Magic != 844121161) { #844121161 equals "IDP2"
	die "File has wrong magic number \"".$md2_file->Magic."\".\n";
}
if ($md2_file->Version != 8) {
	die "File has wrong format version \"".$md2_file->Version."\".\n";
}

print "Skin old: \"", $md2_file->Path->[0][0],"\"\n";

# get new texture-path from user
if ($InputString eq '') {
	print "Enter new path (Enter=$NewSkinPath):";
	
	my $key = '';
	use Term::ReadKey;
	do {
		$key = ReadKey(0);
		$InputString .= $key;
	} while ($key ne "\n");

	chomp($InputString);
}

# replace texture-path
if ($InputString ne '') {
	$md2_file->Path->[0][0] = $InputString;
} else {
	print "Defaulting to \"$NewSkinPath\"\n";
	$md2_file->Path->[0][0] = $NewSkinPath;
}

print "Skin new: \"", $md2_file->Path->[0][0],"\"\n";

# save as another .md2 file
$md2_file->write($MD2OUT);

__END__

##########
# EOF
##########
