# Test script to compare transform-matrix vlaues in Blender
# Load in blender text editor, select one or more empties and hit [ALT][P]

# ***** BEGIN GPL LICENSE BLOCK *****
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
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****

import Blender
from Blender import *
from Blender.Window import *
from Blender.Object import *
from Blender.Mathutils import *

import math
import struct, string
from types import *

objects = Blender.Object.GetSelected()
for object in objects:
	if object.getType()=="Empty":
		origin = object.getLocation('worldspace')
		matrix = object.getMatrix('worldspace')
		print "Name", object.name
		print " Origin:", origin[0], " ",origin[1]," ",origin[2]
		print " Matrix:"
		print "", matrix[0][0], " ",matrix[0][1]," ",matrix[0][2]
		print "", matrix[1][0], " ",matrix[1][1]," ",matrix[1][2]
		print "", matrix[2][0], " ",matrix[2][1]," ",matrix[2][2]
