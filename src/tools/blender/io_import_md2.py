# ***** BEGIN GPL LICENSE BLOCK *****

# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

# ***** END GPL LICENCE BLOCK *****

bl_info = {
	"name": "Quake2 MD2 format",
	"description": "Import from Quake2 file format (.md2)",
	"author": "DarkRain, based on original by Bob Holcomb",
	"version": (1, 0),
	"blender": (2, 63, 0),
	"location": "File > Import > Quake2 MD2",
	"warning": '', # used for warning icon and text in addons panel
	"support": 'COMMUNITY',
	"category": "Import"}

import bpy
import mathutils
import struct
import os
from bpy_extras.image_utils import load_image
from bpy_extras.io_utils import ImportHelper, unpack_list, unpack_face_list
from bpy.props import *
from math import pi

class Util:
	@staticmethod
	def loadImage(imagePath, filePath):
		image = load_image(imagePath, os.path.dirname(filePath), recursive=False)
		if image is not None:
			return image
		return load_image(imagePath, os.path.dirname(filePath), recursive=False, place_holder=True)

	@staticmethod
	def asciiz(s):
		for i, c in enumerate(s):
			if ord(c) == 0:
				return s[:i]

class MD2:
	def __init__ (self, filepath, animate):
		self.ident = 844121161
		self.version = 8
		self.headerFmt = '<17i'
		self.filepath = filepath
		self.name = os.path.splitext(os.path.basename(filepath))[0]
		self.import_animation = animate
		self.skins = []
		self.tex_coord = [(0.0, 0.0)]
		self.tris = []
		self.frames = []

	def read(self):
		print('Reading: ' + self.filepath, end = '')
		progressStatus = 0.0
		inFile = open(self.filepath, 'rb')
		try:
			buff = inFile.read(struct.calcsize(self.headerFmt))
			data = struct.unpack(self.headerFmt, buff)

			if data[0] != self.ident or data[1] != self.version:
				raise NameError('Invalid MD2 file')

			self.skin_width = float(max(1, data[2]))
			self.skin_height = float(max(1, data[3]))
			self.frame_size = data[4]
			self.num_skins = data[5]
			self.num_vert = data[6]
			self.num_tex_coord = data[7]
			self.num_tris = data[8]
			if self.import_animation:
				self.num_frames = data[10]
			else:
				self.num_frames = 1
			self.off_skins = data[11]
			self.off_tex_coord = data[12]
			self.off_tris = data[13]
			self.off_frames = data[14]

			# Skins
			if self.num_skins > 0:
				inFile.seek(self.off_skins, 0)
				for i in range(self.num_skins):
					buff = inFile.read(struct.calcsize('<64s'))
					data = struct.unpack('<64s', buff)
					self.skins.append(Util.asciiz(data[0].decode('utf-8', 'replace')))
			print('.', end = '')

			# UV
			inFile.seek(self.off_tex_coord, 0)
			for i in range(self.num_tex_coord):
				buff = inFile.read(struct.calcsize('<2h'))
				data = struct.unpack('<2h', buff)
				self.tex_coord.append((data[0] / self.skin_width, 1 - (data[1] / self.skin_height)))
			print('.', end = '')

			# Tris
			inFile.seek(self.off_tris, 0)
			for i in range(self.num_tris):
				buff = inFile.read(struct.calcsize('<6H'))
				data = struct.unpack('<6H', buff)
				self.tris.append(((data[0], data[2], data[1]), (data[3] + 1, data[5] + 1, data[4] + 1)))
			print('.', end = '')

			# Frames
			inFile.seek(self.off_frames, 0)
			for i in range(self.num_frames):
				buff = inFile.read(struct.calcsize('<6f16s'))
				data = struct.unpack('<6f16s', buff)
				verts = []
				for j in range(self.num_vert):
					buff = inFile.read(struct.calcsize('<4B'))
					vert = struct.unpack('<4B', buff)
					verts.append((data[0] * vert[0] + data[3], data[1] * vert[1] + data[4], data[2] * vert[2] + data[5]))
				self.frames.append(verts)
			print('.', end = '')
		finally:
			inFile.close()
		print('Done')

	def makeObject(self):
		print('Creating mesh', end = '')
		width = max(1.0, float(self.skin_width))
		height = max(1.0, float(self.skin_height))
		# Skins
		if self.num_skins > 0:
			material = bpy.data.materials.new(self.name)
			for skin in self.skins:
				skinImg = Util.loadImage(skin, self.name)
				skinTex = bpy.data.textures.new(self.name + skin, type='IMAGE')
				skinTex.image = skinImg
				matTex = material.texture_slots.add()
				matTex.texture = skinTex
				matTex.texture_coords = 'UV'
				matTex.use_map_color_diffuse = True
				matTex.use_map_alpha = True
		print('.', end = '')

		# Create the mesh
		mesh = bpy.data.meshes.new(self.name)
		if self.num_skins > 0:
			mesh.materials.append(material)
		mesh.vertices.add(len(self.frames[0]))
		mesh.tessfaces.add(self.num_tris)
		mesh.tessface_uv_textures.new()
		print('.', end = '')

		# Verts
		mesh.vertices.foreach_set('co', unpack_list(self.frames[0]))
		mesh.transform(mathutils.Matrix.Rotation(-pi/2, 4, 'Z'))
		print('.', end = '')

		# Tris
		mesh.tessfaces.foreach_set('vertices_raw', unpack_face_list([face[0] for face in self.tris]))
		print('.', end = '')

		# UV
		mesh.tessface_uv_textures[0].data.foreach_set('uv_raw', unpack_list([self.tex_coord[i] for i in unpack_face_list([face[1] for face in self.tris])]))
		if self.num_skins > 0:
			image = mesh.materials[0].texture_slots[0].texture.image
			if image != None:
				for uv in mesh.tessface_uv_textures[0].data:
					uv.image = image
		print('.', end = '')

		mesh.validate()
		mesh.update()
		obj = bpy.data.objects.new(mesh.name, mesh)
		base = bpy.context.scene.objects.link(obj)
		bpy.context.scene.objects.active = obj
		base.select = True
		print('Done')

		# Animate
		if self.import_animation:
			for i, frame in enumerate(self.frames):
				progressStatus = i / self.num_frames * 100
				#bpy.context.scene.frame_set(i + 1)
				shKey = obj.shape_key_add(from_mix=False)
				mesh.vertices.foreach_set('co', unpack_list(frame))
				mesh.transform(mathutils.Matrix.Rotation(-pi / 2, 4, 'Z'))
				mesh.shape_keys.key_blocks[i].value = 1.0
				mesh.shape_keys.key_blocks[i].keyframe_insert('value',frame = i + 1)
				if i < len(self.frames) - 1:
					mesh.shape_keys.key_blocks[i].value = 0.0
					mesh.shape_keys.key_blocks[i].keyframe_insert('value',frame = i + 2)
				if i > 0:
					mesh.shape_keys.key_blocks[i].value = 0.0
					mesh.shape_keys.key_blocks[i].keyframe_insert('value',frame = i)
				print("Animating - progress: %3i%%\r" % int(progressStatus), end = '')
			print("Animating - progress: 100%.")
		print('Model imported')

class Import_MD2(bpy.types.Operator, ImportHelper):
	'''Import Quake2 format file (md2)'''
	bl_idname = 'import_quake.md2'
	bl_label = 'Import Quake2 format mesh (md2)'

	filepath = StringProperty(name = 'File Path',
		description = 'Filepath used for processing the script',
		maxlen = 1024, default = '')

	filename_ext = 'md2'

	bImportAnimation = BoolProperty(name = 'Import animation',
		description = 'default: False',
		default = False)

	def execute(self, context):
		filePath = self.filepath
		filePath = bpy.path.ensure_ext(self.filepath, self.filename_ext)

		# deselect all objects
		bpy.ops.object.select_all(action='DESELECT')

		md2 = MD2(self.filepath, self.bImportAnimation)
		md2.read()
		md2.makeObject()

		bpy.context.scene.frame_set(bpy.context.scene.frame_current)
		bpy.context.scene.update()
		self.report({'INFO'},  "File '" + self.filepath + "' imported")
		return {'FINISHED'}

def menuCB(self, context):
	self.layout.operator(Import_MD2.bl_idname, text="Quake II's MD2 (.md2)")

def register():
	bpy.utils.register_module(__name__)
	bpy.types.INFO_MT_file_import.append(menuCB)

def unregister():
	bpy.utils.unregister_module(__name__)
	bpy.types.INFO_MT_file_import.remove(menuCB)

if __name__ == "__main__":
	register()
