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

"""
This script imports a Quake 2 tag file as used in UFO:AI (md2 TAGs).

This file format equals the _parts_ of the MD3 format used for animated tags, but nothing more. i.e tagnames+positions

Base code taken from the md2 exporter by Bob Holcomb. Many thanks.
"""

bl_info = {
    "name": "UFO:AI md2 TAGs (.tag)",
    "description": "Import from md2 TAG file format used by UFO:AI (.tag).",
    "author": "DarkRain, based on the initial importer of Werner Hoehrer",
    "version": (1, 0),
    "blender": (2, 63, 0),
    "location": "File > Import > UFO:AI md2 TAGs",
    "warning": '', # used for warning icon and text in addons panel
    "support": 'COMMUNITY',
    "category": "Import"}

import bpy
from bpy.props import *

from bpy_extras.io_utils import ImportHelper

import math
import mathutils

import struct
import string
from types import *

def asciiz(s):
	for i, c in enumerate(s):
		if ord(c) == 0:
			return s[:i]

class md2_tagname:
	name=""
	binary_format="<64s"
	def __init__(self, string):
		self.name=string
	def getSize(self):
		return struct.calcsize(self.binary_format)
	def load(self, file):
		temp_data = file.read(self.getSize())
		data=struct.unpack(self.binary_format, temp_data)
		self.name=asciiz(data[0].decode('utf-8', 'replace'))
		return self

class md2_tag:
	axis1	= []
	axis2	= []
	axis3	= []
	origin	= []

	binary_format="<12f"	#little-endian (<), 12 floats (12f)	| See http://docs.python.org/lib/module-struct.html for more info.

	def __init__(self):
		origin	= mathutils.Vector((0.0, 0.0, 0.0))
		axis1	= mathutils.Vector((0.0, 0.0, 0.0))
		axis2	= mathutils.Vector((0.0, 0.0, 0.0))
		axis3	= mathutils.Vector((0.0, 0.0, 0.0))

	def getSize(self):
		return struct.calcsize(self.binary_format)

	# Function + general data-structure stolen from https://intranet.dcc.ufba.br/svn/indigente/trunk/scripts/md3_top.py
	# Kudos to Bob Holcomb and Damien McGinnes

	def load(self, file):
		# Read data from file with the defined size.
		temp_data = file.read(self.getSize())

		# De-serialize the data.
		data = struct.unpack(self.binary_format, temp_data)

		# Parse data from file into python md2 structure
		self.axis2	= (-data[1], data[0], -data[2])
		self.axis1	= (data[4], -data[3], data[5])
		self.axis3	= (data[7], -data[6], data[8])
		self.origin	= (data[10], -data[9], data[11])

		return self

class md2_tags_obj:
	#Header Structure
	ident = 0		#int 0	This is used to identify the file
	version = 0		#int 1	The version number of the file (Must be 1)
	num_tags = 0		#int 2	The number of tags associated with the model
	num_frames = 0		#int 3	The number of animation frames

	offset_names = 0	#int 4	The offset in the file for the name data
	offset_tags = 0 	#int 5	The offset in the file for the tags data
	offset_end = 0		#int 6	The end of the file offset
	offset_extract_end = 0	#int 7	???
	binary_format = "<8i" 	#little-endian (<), 8 integers (8i)

	#md2 tag data objects
	names = []
	tags = [] # (consists of a number of frames)

	"""
	"tags" example:
	tags = (
		(tag1_frame1, tag1_frame2, tag1_frame3, etc...),	# tag_frames1
		(tag2_frame1, tag2_frame2, tag2_frame3, etc...),	# tag_frames2
		(tag3_frame1, tag3_frame2, tag3_frame3, etc...)		# tag_frames3
	)
	"""

	def __init__ (self):
		self.names = []
		self.tags = []

	def getSize(self):
		return struct.calcsize(self.binary_format)

	def load(self, file):
		temp_data = file.read(self.getSize())
		data = struct.unpack(self.binary_format, temp_data)

		self.ident = data[0]
		self.version = data[1]

		if (self.ident!=844121162 or self.version!=1):
			raise NameError("Not a valid MD2 TAG file")

		self.num_tags		= data[2]
		self.num_frames		= data[3]
		self.offset_names	= data[4]
		self.offset_tags	= data[5]
		#self.offset_end		= data[6]
		#self.offset_extract_end	= data[7]

		# Read names data
		file.seek(self.offset_names, 0)
		for tag_counter in range(self.num_tags):
			temp_name = md2_tagname("")
			self.names.append(temp_name.load(file))
		print("Reading of ", tag_counter, " names finished.")

		file.seek(self.offset_tags, 0)
		for tag_counter in range(self.num_tags):
			frames = []
			for frame_counter in range(self.num_frames):
				temp_tag = md2_tag()
				temp_tag.load(file)
				frames.append(temp_tag)
			self.tags.append(frames)

			print("Reading of ", frame_counter+1, " frames finished.")
		print("Reading of ", tag_counter+1, " framelists finished.")
		return self

	def dump (self):
		print ("Header Information")
		print ("ident: ",		self.ident)
		print ("version: ",		self.version)
		print ("number of tags: ",	self.num_tags)
		print ("number of frames: ",	self.num_frames)
		print ("offset names: ",		self.offset_names)
		print ("offset tags: ",		self.offset_tags)
		print ("offset end: ",		self.offset_end)
		print ("offset extract end: ",	self.offset_extract_end)
		print ("")

def add_frame(blender_tag, frame):
	#bpy.context.scene.frame_set(frame_count)
	transform = mathutils.Matrix(((frame.axis1[0], frame.axis2[0], frame.axis3[0], frame.origin[0]),
								  (frame.axis1[1], frame.axis2[1], frame.axis3[1], frame.origin[1]),
								  (frame.axis1[2], frame.axis2[2], frame.axis3[2], frame.origin[2]),
								  (0.0, 0.0, 0.0, 1.0)))
	blender_tag.matrix_world = transform

class Import_TAG(bpy.types.Operator, ImportHelper):
	"""Import from md2 TAG format used by UFO:AI (.tag)"""
	bl_idname = "import_md2.tag"
	bl_label = "Import from md2 TAG format used by UFO:AI (.tag)"

	filename = StringProperty(name="File Path",
		description="Filepath used for processing the script",
		maxlen= 1024, default= "")

	filename_ext = ".tag"

	bImportAnimation = BoolProperty(name="Import animation",
							description="Import all frames (default: False)",
							default=False)

	def __init__(self):
		try:
			self.objects = bpy.context.selected_objects
		except:
			self.objects = None

	######################################################
	# Save md2 TAGs Format
	######################################################
	def execute(self, context):
		filepath = self.filepath
		filepath = bpy.path.ensure_ext(filepath, self.filename_ext)
		md2_tags = md2_tags_obj()

		# deselect all objects
		bpy.ops.object.select_all(action='DESELECT')

		file_import=open(filepath,"rb")
		md2_tags.load(file_import)
		file_import.close()

		scene = bpy.context.scene

		for tag in range(md2_tags.num_tags):
			# Get name & frame-data of current tag.
			name = md2_tags.names[tag]

			# Create object.
			blender_tag = bpy.data.objects.new(name=name.name, object_data=None)

			# Activate name-visibility for this object.
			blender_tag.show_name = True

			# Link Object to current Scene
			scene.objects.link(blender_tag)
			blender_tag.select = True
			scene.objects.active = blender_tag

			if self.bImportAnimation:
				for i, frame in enumerate(md2_tags.tags[tag]):
					add_frame(blender_tag, frame)
					# Insert keyframe.
					blender_tag.keyframe_insert(data_path = 'location', frame = i + 1)
					blender_tag.keyframe_insert(data_path = 'rotation_euler', frame = i + 1)
					blender_tag.keyframe_insert(data_path = 'scale', frame = i + 1)
			else:
				add_frame(blender_tag, md2_tags.tags[tag][0])

		scene.frame_set(scene.frame_current)
		scene.update()
		return {'FINISHED'}

	def invoke(self, context, event):
		wm = context.window_manager
		wm.fileselect_add(self)
		return {'RUNNING_MODAL'}

def menuCB(self, context):
	self.layout.operator(Import_TAG.bl_idname, text="UFO:AI md2 TAGs (.tag)")

def register():
	bpy.utils.register_module(__name__)
	bpy.types.INFO_MT_file_import.append(menuCB)

def unregister():
	bpy.utils.unregister_module(__name__)
	bpy.types.INFO_MT_file_import.remove(menuCB)

if __name__ == "__main__":
	register()
