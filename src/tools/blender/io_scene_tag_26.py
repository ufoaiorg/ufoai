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

'''
This script exports a Quake 2 tag file as used in UFO:AI (md2 TAGs).

This file format equals the _parts_ of the MD3 format used for animated tags, but nothing more. i.e tagnames+positions

Base code taken from the md2 exporter by Bob Holcomb. Many thanks.
'''

bl_info = {
	"name": "UFO:AI md2 TAGs (.tag)",
	"description": "Export to md2 TAG file format used by UFO:AI (.tag).",
	"author": "DarkRain, based on the work of Werner Hoehrer and Bob Holcomb",
	"version": (1, 0),
	"blender": (2, 63, 0),
	"location": "File > Import/Export > UFO:AI md2 TAGs",
	"warning": '', # used for warning icon and text in addons panel
	"support": 'COMMUNITY',
	"category": "Import-Export"}

import bpy
from bpy.props import *
from bpy_extras.io_utils import ExportHelper, ImportHelper
import mathutils

import struct

MD2_MAX_TAGS = 4096
MD2_MAX_FRAMES = 1024

def asciiz(s):
	for i, c in enumerate(s):
		if ord(c) == 0:
			return s[:i]

class Md2TagName:
	name = ''
	format = "<64s"

	def __init__(self, string):
		self.name = string

	def getSize(self):
		return struct.calcsize(self.format)

	def save(self, file):
		data = struct.pack(self.format, bytes(self.name[0:63], encoding="utf-8"))
		file.write(data)

	def load(self, file):
		buff = file.read(self.getSize())
		data = struct.unpack(self.format, buff)
		self.name = asciiz(data[0].decode("utf-8", "replace"))
		return self

class Md2Tag:
	axis1 = []
	axis2 = []
	axis3 = []
	origin = []

	format = "<12f"	#little-endian (<), 12 floats (12f)	| See http://docs.python.org/lib/module-struct.html for more info.

	def __init__(self):
		origin = mathutils.Vector((0.0, 0.0, 0.0))
		axis1 = mathutils.Vector((0.0, 0.0, 0.0))
		axis2 = mathutils.Vector((0.0, 0.0, 0.0))
		axis3 = mathutils.Vector((0.0, 0.0, 0.0))

	def getSize(self):
		return struct.calcsize(self.format)

	# Function + general data-structure stolen from https://intranet.dcc.ufba.br/svn/indigente/trunk/scripts/md3_top.py
	# Kudos to Bob Holcomb and Damien McGinnes

	def save(self, file):
		# Prepare serialised data for export with transformed axes (matrix transform)
		data = struct.pack(self.format,
							# rotation matrix
							float(self.axis2[1]), -float(self.axis2[0]), -float(self.axis2[2]),
							-float(self.axis1[1]), float(self.axis1[0]), float(self.axis1[2]),
							-float(self.axis3[1]), float(self.axis3[0]), float(self.axis3[2]),
							# position
							-float(self.origin[1]), float(self.origin[0]), float(self.origin[2])
							)

		# Write it
		file.write(data)

	def load(self, file):
		# Read data from file with the defined size.
		buff = file.read(self.getSize())

		# De-serialize the data.
		data = struct.unpack(self.format, buff)

		# Parse data from file into python md2 structure
		self.axis2 = (-data[1], data[0], -data[2])
		self.axis1 = (data[4], -data[3], data[5])
		self.axis3 = (data[7], -data[6], data[8])
		self.origin = (data[10], -data[9], data[11])

		return self

class Md2TagsObj:
	#Header Structure
	ident = 0		#int 0	This is used to identify the file
	version = 0		#int 1	The version number of the file (Must be 1)
	numTags = 0		#int 2	The number of tags associated with the model
	numFrames = 0		#int 3	The number of animation frames

	ofsNames = 0	#int 4	The offset in the file for the name data
	ofsTags = 0 	#int 5	The offset in the file for the tags data
	ofsEnd = 0		#int 6	The end of the file offset
	ofsExtractEnd = 0	#int 7	???
	format = "<8i" 	#little-endian (<), 8 integers (8i)

	#md2 tag data objects
	names = []
	tags = [] # (consists of a number of frames)

	'''
	"tags" example:
	tags = (
			(tag1_frame1, tag1_frame2, tag1_frame3, etc...),	# tagFrames1
			(tag2_frame1, tag2_frame2, tag2_frame3, etc...),	# tagFrames2
			(tag3_frame1, tag3_frame2, tag3_frame3, etc...)		# tagFrames3
			)
	'''

	def __init__ (self):
		self.names = []
		self.tags = []

	def getSize(self):
		return struct.calcsize(self.format)

	def save(self, file):
		data = struct.pack(self.format,
							self.ident,
							self.version,
							self.numTags,
							self.numFrames,
							self.ofsNames,
							self.ofsTags,
							self.ofsEnd,
							self.ofsExtractEnd,
							)
		file.write(data)

		#write the names data
		for name in self.names:
			name.save(file)
		tagCount = 0
		for tagFrames in self.tags:
			tagCount += 1
			frameCount = 0
			print("Saving tag: ", tagCount)

			for tag in tagFrames:
				frameCount += 1
				tag.save(file)
			print("Frames saved: ", frameCount, "(",self.numFrames, ")")

	def load(self, file):
		buff = file.read(self.getSize())
		data = struct.unpack(self.format, buff)

		self.ident = data[0]
		self.version = data[1]

		if (self.ident != 844121162 or self.version != 1):
			raise NameError("Not a valid MD2 TAG file")

		self.numTags = data[2]
		self.numFrames = data[3]
		self.ofsNames = data[4]
		self.ofsTags = data[5]

		# Read names data
		file.seek(self.ofsNames, 0)
		for tagCounter in range(self.numTags):
			tempName = Md2TagName("")
			self.names.append(tempName.load(file))
		print("Reading of ", tagCounter, " names finished.")

		file.seek(self.ofsTags, 0)
		for tagCounter in range(self.numTags):
			frames = []
			for frameCounter in range(self.numFrames):
				tempTag = Md2Tag()
				tempTag.load(file)
				frames.append(tempTag)
			self.tags.append(frames)
			print("Reading of ", frameCounter + 1, " frames finished.")

		print("Reading of ", tagCounter + 1, " framelists finished.")
		return self

def outFrame(tagFrames, matrix, frameCounter):
	tagFrames.append(Md2Tag())

	# Set first coordiantes to the location of the empty.
	tagFrames[frameCounter].origin = matrix.translation.copy()
	tagFrames[frameCounter].axis1 = matrix.col[0].copy()
	tagFrames[frameCounter].axis2 = matrix.col[1].copy()
	tagFrames[frameCounter].axis3 = matrix.col[2].copy()

def inFrame(blenderTag, frame):
	#bpy.context.scene.frame_set(frameCount)
	transform = mathutils.Matrix(((frame.axis1[0], frame.axis2[0], frame.axis3[0], frame.origin[0]),
					  (frame.axis1[1], frame.axis2[1], frame.axis3[1], frame.origin[1]),
					  (frame.axis1[2], frame.axis2[2], frame.axis3[2], frame.origin[2]),
					  (0.0, 0.0, 0.0, 1.0)))
	blenderTag.matrix_world = transform

def fillMd2Tags(md2Tags, object, options):
	# Set header information.
	md2Tags.ident = 844121162
	md2Tags.version = 1
	md2Tags.numTags += 1
	frameCounter = 0

	# Add a name node to the tagnames data structure.
	md2Tags.names.append(Md2TagName(object.name))

	# Add a (empty) list of tags-positions (for each frame).
	tagFrames = []

	if options.bExportAnimation:
		# Store currently set frame
		previousCurFrame = bpy.context.scene.frame_current

		# Fill in each tag with its positions per frame
		print("Blender startframe: ", bpy.context.scene.frame_start)
		print("Blender endframe: ", bpy.context.scene.frame_end)
		for currentFrame in range(bpy.context.scene.frame_start , bpy.context.scene.frame_end + 1):
			#set blender to the correct frame (so the objects have their new positions)
			bpy.context.scene.frame_set(currentFrame)
			#add a frame
			outFrame(tagFrames, object.matrix_world, frameCounter)
			frameCounter += 1
		# Restore curframe from before the calculation.
		bpy.context.scene.frame_set(previousCurFrame)
	else:
		outFrame(tagFrames, object.matrix_world, frameCounter)

	md2Tags.tags.append(tagFrames)

class Export_TAG(bpy.types.Operator, ExportHelper):
	'''Export to md2 TAG format used by UFO:AI (.tag)'''
	bl_idname = "export_md2.tag"
	bl_label = "Export to md2 TAG format used by UFO:AI (.tag)"
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
		filePath = self.filepath
		filePath = bpy.path.ensure_ext(filePath, self.filename_ext)
		md2Tags = Md2TagsObj()

		md2Tags.numFrames = 1
		if self.bExportAnimation:
			md2Tags.numFrames = 1 + bpy.context.scene.frame_end - bpy.context.scene.frame_start

		if len(bpy.context.selected_objects) == 0:
			raise NameError("Please, select one object!")
		elif len(bpy.context.selected_objects) > MD2_MAX_TAGS:
			raise NameError("Too many objects (%i) selected, at most %i tags supported" % (len(bpy.context.selected_objects), MD2_MAX_TAGS))
		if md2Tags.numFrames > MD2_MAX_FRAMES:
			raise NameError("Too many frames (%i), at most %i are supported" % (md2Tags.numFrames, MD2_MAX_TAGS))

		# go into object mode before we start the actual export procedure
		bpy.ops.object.mode_set( mode='OBJECT', toggle=False )

		for object in self.objects:
			#check if it's an "Empty" mesh object
			if object.type != 'EMPTY':
				print("Ignoring non-'Empty' object: " + object.type)
			else:
				print("Found Empty: " + object.name)
				fillMd2Tags(md2Tags, object, self)

		# Set offset of names
		tempHeader = Md2TagsObj();
		md2Tags.ofsNames = tempHeader.getSize();

		# Set offset of tags
		tempName = Md2TagName('');
		md2Tags.ofsTags = md2Tags.ofsNames + tempName.getSize() * md2Tags.numTags;

		# Set EOF offest value.
		tempTag = Md2Tag();
		md2Tags.ofsEnd = md2Tags.ofsTags + (tempTag.getSize() * md2Tags.numFrames * md2Tags.numTags)
		md2Tags.ofsExtractEnd = md2Tags.ofsTags + (64 * md2Tags.numFrames * md2Tags.numTags)

		# Actually write it to disk.
		fileExport = open(filePath, "wb")
		try:
			md2Tags.save(fileExport)
		finally:
			fileExport.close()

		# Cleanup
		md2Tags = 0

		return {'FINISHED'}

	def invoke(self, context, event):
		wm = context.window_manager
		wm.fileselect_add(self)
		return {'RUNNING_MODAL'}

class Import_TAG(bpy.types.Operator, ImportHelper):
	'''Import from md2 TAG format used by UFO:AI (.tag)'''
	bl_idname = "import_md2.tag"
	bl_label = "Import from md2 TAG format used by UFO:AI (.tag)"

	bImportAnimation = BoolProperty(name="Import animation",
					description="Import all frames (default: False)",
					default=True)

	def __init__(self):
		if len(bpy.context.selected_objects) > 0:
			# go into object mode before we start the actual import procedure
			bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

	######################################################
	# Load md2 TAGs Format
	######################################################
	def execute(self, context):
		md2Tags = Md2TagsObj()

		# deselect all objects
		bpy.ops.object.select_all(action='DESELECT')

		fileImport = open(self.filepath, "rb")
		try:
			md2Tags.load(fileImport)
		finally:
			fileImport.close()

		scene = bpy.context.scene

		for tag in range(md2Tags.numTags):
			# Get name & frame-data of current tag.
			name = md2Tags.names[tag]

			# Create object.
			blenderTag = bpy.data.objects.new(name=name.name, object_data=None)

			# Activate name-visibility for this object.
			blenderTag.show_name = True

			# Link Object to current Scene
			scene.objects.link(blenderTag)
			blenderTag.select = True
			scene.objects.active = blenderTag

			if self.bImportAnimation:
				for i, frame in enumerate(md2Tags.tags[tag]):
					inFrame(blenderTag, frame)
					# Insert keyframe.
					blenderTag.keyframe_insert(data_path='location', frame=i + 1)
					blenderTag.keyframe_insert(data_path='rotation_euler', frame=i + 1)
					blenderTag.keyframe_insert(data_path='scale', frame=i + 1)
			else:
				inFrame(blenderTag, md2Tags.tags[tag][0])

		scene.frame_set(scene.frame_current)
		scene.update()
		return {'FINISHED'}

	def invoke(self, context, event):
		wm = context.window_manager
		wm.fileselect_add(self)
		return {'RUNNING_MODAL'}

def menu_func_export(self, context):
	self.layout.operator(Export_TAG.bl_idname, text="UFO:AI md2 TAGs (.tag)")

def menu_func_import(self, context):
	self.layout.operator(Import_TAG.bl_idname, text="UFO:AI md2 TAGs (.tag)")

def register():
	bpy.utils.register_module(__name__)
	bpy.types.INFO_MT_file_export.append(menu_func_export)
	bpy.types.INFO_MT_file_import.append(menu_func_import)

def unregister():
	bpy.utils.unregister_module(__name__)
	bpy.types.INFO_MT_file_export.remove(menu_func_export)
	bpy.types.INFO_MT_file_import.remove(menu_func_import)

if __name__ == "__main__":
	register()
