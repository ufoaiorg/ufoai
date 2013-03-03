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
This script exports a Quake 2 tag file as used in UFO:AI (md2 TAGs).

This file format equals the _parts_ of the MD3 format used for animated tags, but nothing more. i.e tagnames+positions

Base code taken from the md2 exporter by Bob Holcomb. Many thanks.
"""

bl_info = {
    "name": "UFO:AI md2 TAGs (.tag)",
    "description": "Export to md2 TAG file format used by UFO:AI (.tag).",
    "author": "DarkRain, based on the initial exporter of Werner Hoehrer",
    "version": (1, 0),
    "blender": (2, 63, 0),
    "location": "File > Export > UFO:AI md2 TAGs",
    "warning": '', # used for warning icon and text in addons panel
    "support": 'COMMUNITY',
    "category": "Export"}

import bpy
from bpy.props import *

from bpy_extras.io_utils import ExportHelper

import math
import mathutils

import struct
import string
from types import *

class md2_tagname:
	name=""
	binary_format="<64s"
	def __init__(self, string):
		self.name=string
	def getSize(self):
		return struct.calcsize(self.binary_format)
	def save(self, file):
		temp_data=self.name
		data=struct.pack(self.binary_format, bytes(temp_data[0:63], encoding='utf8'))
		file.write(data)
	def dump (self):
		print ("md2 tagname")
		print ("tag: ",self.name)
		print ("")

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

	def save(self, file):
		# Prepare temp data for export with transformed axes (matrix transform)
		temp_data=(
		# rotation matrix
		float(self.axis2[1]), -float(self.axis2[0]), -float(self.axis2[2]),
		-float(self.axis1[1]), float(self.axis1[0]), float(self.axis1[2]),
		-float(self.axis3[1]), float(self.axis3[0]), float(self.axis3[2]),
		# position
		-float(self.origin[1]), float(self.origin[0]), float(self.origin[2])
		)

		# Prepare serialised data for writing.
		data=struct.pack(self.binary_format,
			temp_data[0], temp_data[1], temp_data[2],
			temp_data[3], temp_data[4], temp_data[5],
			temp_data[6], temp_data[7], temp_data[8],
			temp_data[9], temp_data[10], temp_data[11])

		# Write it
		file.write(data)

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
	def save(self, file):
		temp_data=[0]*8
		temp_data[0] = self.ident
		temp_data[1] = self.version
		temp_data[2] = self.num_tags
		temp_data[3] = self.num_frames
		temp_data[4] = self.offset_names
		temp_data[5] = self.offset_tags
		temp_data[6] = self.offset_end
		temp_data[7] = self.offset_extract_end
		data=struct.pack(self.binary_format, temp_data[0],temp_data[1],temp_data[2],temp_data[3],temp_data[4],temp_data[5],temp_data[6],temp_data[7])
		file.write(data)

		#write the names data
		for name in self.names:
			name.save(file)
		tag_count = 0
		for tag_frames in self.tags:
			tag_count+=1
			frame_count=0
			print("Saving tag: ", tag_count)

			for tag in tag_frames:
				frame_count += 1
				tag.save(file)
			print("  Frames saved: ", frame_count, "(",self.num_frames, ")")
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

def add_frame(tag_frames, matrix, frame_counter):
	tag_frames.append(md2_tag())

	# Set first coordiantes to the location of the empty.
	tag_frames[frame_counter].origin = matrix.translation.copy()
	# Useful for DEBUG (slowdown!)
	# print (tag_frames[frame_counter].origin[0], " ",tag_frames[frame_counter].origin[1]," ",tag_frames[frame_counter].origin[2])

	tag_frames[frame_counter].axis1 = matrix.col[0].copy()
	tag_frames[frame_counter].axis2 = matrix.col[1].copy()
	tag_frames[frame_counter].axis3 = matrix.col[2].copy()

def fill_md2_tags(md2_tags, object, options):
	# Set header information.
	md2_tags.ident=844121162
	md2_tags.version=1
	md2_tags.num_tags+=1
	frame_counter = 0

	# Add a name node to the tagnames data structure.
	md2_tags.names.append(md2_tagname(object.name))

	# Add a (empty) list of tags-positions (for each frame).
	tag_frames = []

	if options.bExportAnimation:
		# Store currently set frame
		previous_curframe = bpy.context.scene.frame_current

		# Fill in each tag with its positions per frame
		print("Blender startframe:", bpy.context.scene.frame_start)
		print("Blender endframe:", bpy.context.scene.frame_end)
		for current_frame in range(bpy.context.scene.frame_start , bpy.context.scene.frame_end + 1):
			#set blender to the correct frame (so the objects have their new positions)
			bpy.context.scene.frame_set(current_frame)
			#print(current_frame, "(", frame_counter, ")") # DEBUG
			#add a frame
			add_frame(tag_frames, object.matrix_world, frame_counter)
			frame_counter += 1
		# Restore curframe from before the calculation.
		bpy.context.scene.frame_set(previous_curframe)
	else:
		add_frame(tag_frames, object.matrix_world, frame_counter)

	md2_tags.tags.append(tag_frames)

class Export_TAG(bpy.types.Operator, ExportHelper):
	"""Export to md2 TAG format used by UFO:AI (.tag)"""
	bl_idname = "export_md2.tag"
	bl_label = "Export to md2 TAG format used by UFO:AI (.tag)"

	filename = StringProperty(name="File Path",
		description="Filepath used for processing the script",
		maxlen= 1024,default= "")

	filename_ext = ".tag"

	bExportAnimation = BoolProperty(name="Export animation",
							description="default: False",
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

		if len(bpy.context.selected_objects[:]) == 0:
			raise NameError('Please, select one object!')

		# go into object mode before we start the actual export procedure
		bpy.ops.object.mode_set( mode="OBJECT" , toggle = False )

		md2_tags.num_frames = 1
		if self.bExportAnimation:
			md2_tags.num_frames = 1 + bpy.context.scene.frame_end - bpy.context.scene.frame_start

		for object in self.objects:
			#check if it's an "Empty" mesh object
			if object.type != 'EMPTY':
				print("Ignoring non-'Empty' object: " + object.type)
			else:
				print("Found Empty: " + object.name)
				fill_md2_tags(md2_tags, object, self)

		# Set offset of names
		temp_header = md2_tags_obj();
		md2_tags.offset_names= 0+temp_header.getSize();

		# Set offset of tags
		temp_name = md2_tagname("");
		md2_tags.offset_tags = md2_tags.offset_names + temp_name.getSize() * md2_tags.num_tags;

		# Set EOF offest value.
		temp_tag = md2_tag();
		md2_tags.offset_end = md2_tags.offset_tags + (48 * md2_tags.num_frames * md2_tags.num_tags)
		md2_tags.offset_extract_end = md2_tags.offset_tags + (64 * md2_tags.num_frames * md2_tags.num_tags)

		# md2_tags.dump()

		# Actually write it to disk.
		file_export=open(filepath,"wb")
		md2_tags.save(file_export)
		file_export.close()

		# Cleanup
		md2_tags=0

		return {'FINISHED'}

	def invoke(self, context, event):
		wm = context.window_manager
		wm.fileselect_add(self)
		return {'RUNNING_MODAL'}

def menuCB(self, context):
	self.layout.operator(Export_TAG.bl_idname, text="UFO:AI md2 TAGs (.tag)")

def register():
	bpy.utils.register_module(__name__)
	bpy.types.INFO_MT_file_export.append(menuCB)

def unregister():
	bpy.utils.unregister_module(__name__)
	bpy.types.INFO_MT_file_export.remove(menuCB)

if __name__ == "__main__":
	register()
