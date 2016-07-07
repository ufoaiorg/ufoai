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

'''
This is script an importer and exporter for the Quake2 "MD2" file format.

The frames are named <frameName><N> with :<br>
 - <N> the frame number<br>
 - <frameName> the name choosen at the last marker (or 'frame' if the last marker has no name or if there is no last marker)

Skins are set using image textures in materials, if it is longer than 63 characters it is truncated.

Thanks to Bob Holcomb for MD2_NORMALS taken from his exporter.
Thanks to David Henry for the documentation about the MD2 file format.
'''

bl_info = {
	"name": "Quake2 MD2 format",
	"description": "Importer and exporter for Quake2 file format (.md2)",
	"author": "DarkRain, based on the work of Bob Holcomb/Sebastian Lieberknecht/Dao Nguyen/Bernd Meyer/Damien Thebault/Erwan Mathieu/Takehiko Nawata",
	"version": (1, 1),
	"blender": (2, 63, 0),
	"location": "File > Import/Export > Quake2 MD2",
	"warning": "", # used for warning icon and text in addons panel
	"support": 'COMMUNITY',
	"category": "Import-Export"}

import bpy
from bpy.props import *

from bpy_extras.io_utils import ExportHelper, ImportHelper, unpack_list, unpack_face_list
from bpy_extras.image_utils import load_image

from math import pi
from mathutils import Matrix

import struct
import random
import os
import shutil

MD2_MAX_TRIANGLES = 4096
MD2_MAX_VERTS = 2048
MD2_MAX_FRAMES = 1024		#512 - 1024 is an UFO:AI extension.
MD2_MAX_SKINS = 32
MD2_MAX_SKINNAME = 63
MD2_NORMALS = ((-0.525731, 0.000000, 0.850651),
				(-0.442863, 0.238856, 0.864188),
				(-0.295242, 0.000000, 0.955423),
				(-0.309017, 0.500000, 0.809017),
				(-0.162460, 0.262866, 0.951056),
				( 0.000000, 0.000000, 1.000000),
				( 0.000000, 0.850651, 0.525731),
				(-0.147621, 0.716567, 0.681718),
				( 0.147621, 0.716567, 0.681718),
				( 0.000000, 0.525731, 0.850651),
				( 0.309017, 0.500000, 0.809017),
				( 0.525731, 0.000000, 0.850651),
				( 0.295242, 0.000000, 0.955423),
				( 0.442863, 0.238856, 0.864188),
				( 0.162460, 0.262866, 0.951056),
				(-0.681718, 0.147621, 0.716567),
				(-0.809017, 0.309017, 0.500000),
				(-0.587785, 0.425325, 0.688191),
				(-0.850651, 0.525731, 0.000000),
				(-0.864188, 0.442863, 0.238856),
				(-0.716567, 0.681718, 0.147621),
				(-0.688191, 0.587785, 0.425325),
				(-0.500000, 0.809017, 0.309017),
				(-0.238856, 0.864188, 0.442863),
				(-0.425325, 0.688191, 0.587785),
				(-0.716567, 0.681718,-0.147621),
				(-0.500000, 0.809017,-0.309017),
				(-0.525731, 0.850651, 0.000000),
				( 0.000000, 0.850651,-0.525731),
				(-0.238856, 0.864188,-0.442863),
				( 0.000000, 0.955423,-0.295242),
				(-0.262866, 0.951056,-0.162460),
				( 0.000000, 1.000000, 0.000000),
				( 0.000000, 0.955423, 0.295242),
				(-0.262866, 0.951056, 0.162460),
				( 0.238856, 0.864188, 0.442863),
				( 0.262866, 0.951056, 0.162460),
				( 0.500000, 0.809017, 0.309017),
				( 0.238856, 0.864188,-0.442863),
				( 0.262866, 0.951056,-0.162460),
				( 0.500000, 0.809017,-0.309017),
				( 0.850651, 0.525731, 0.000000),
				( 0.716567, 0.681718, 0.147621),
				( 0.716567, 0.681718,-0.147621),
				( 0.525731, 0.850651, 0.000000),
				( 0.425325, 0.688191, 0.587785),
				( 0.864188, 0.442863, 0.238856),
				( 0.688191, 0.587785, 0.425325),
				( 0.809017, 0.309017, 0.500000),
				( 0.681718, 0.147621, 0.716567),
				( 0.587785, 0.425325, 0.688191),
				( 0.955423, 0.295242, 0.000000),
				( 1.000000, 0.000000, 0.000000),
				( 0.951056, 0.162460, 0.262866),
				( 0.850651,-0.525731, 0.000000),
				( 0.955423,-0.295242, 0.000000),
				( 0.864188,-0.442863, 0.238856),
				( 0.951056,-0.162460, 0.262866),
				( 0.809017,-0.309017, 0.500000),
				( 0.681718,-0.147621, 0.716567),
				( 0.850651, 0.000000, 0.525731),
				( 0.864188, 0.442863,-0.238856),
				( 0.809017, 0.309017,-0.500000),
				( 0.951056, 0.162460,-0.262866),
				( 0.525731, 0.000000,-0.850651),
				( 0.681718, 0.147621,-0.716567),
				( 0.681718,-0.147621,-0.716567),
				( 0.850651, 0.000000,-0.525731),
				( 0.809017,-0.309017,-0.500000),
				( 0.864188,-0.442863,-0.238856),
				( 0.951056,-0.162460,-0.262866),
				( 0.147621, 0.716567,-0.681718),
				( 0.309017, 0.500000,-0.809017),
				( 0.425325, 0.688191,-0.587785),
				( 0.442863, 0.238856,-0.864188),
				( 0.587785, 0.425325,-0.688191),
				( 0.688191, 0.587785,-0.425325),
				(-0.147621, 0.716567,-0.681718),
				(-0.309017, 0.500000,-0.809017),
				( 0.000000, 0.525731,-0.850651),
				(-0.525731, 0.000000,-0.850651),
				(-0.442863, 0.238856,-0.864188),
				(-0.295242, 0.000000,-0.955423),
				(-0.162460, 0.262866,-0.951056),
				( 0.000000, 0.000000,-1.000000),
				( 0.295242, 0.000000,-0.955423),
				( 0.162460, 0.262866,-0.951056),
				(-0.442863,-0.238856,-0.864188),
				(-0.309017,-0.500000,-0.809017),
				(-0.162460,-0.262866,-0.951056),
				( 0.000000,-0.850651,-0.525731),
				(-0.147621,-0.716567,-0.681718),
				( 0.147621,-0.716567,-0.681718),
				( 0.000000,-0.525731,-0.850651),
				( 0.309017,-0.500000,-0.809017),
				( 0.442863,-0.238856,-0.864188),
				( 0.162460,-0.262866,-0.951056),
				( 0.238856,-0.864188,-0.442863),
				( 0.500000,-0.809017,-0.309017),
				( 0.425325,-0.688191,-0.587785),
				( 0.716567,-0.681718,-0.147621),
				( 0.688191,-0.587785,-0.425325),
				( 0.587785,-0.425325,-0.688191),
				( 0.000000,-0.955423,-0.295242),
				( 0.000000,-1.000000, 0.000000),
				( 0.262866,-0.951056,-0.162460),
				( 0.000000,-0.850651, 0.525731),
				( 0.000000,-0.955423, 0.295242),
				( 0.238856,-0.864188, 0.442863),
				( 0.262866,-0.951056, 0.162460),
				( 0.500000,-0.809017, 0.309017),
				( 0.716567,-0.681718, 0.147621),
				( 0.525731,-0.850651, 0.000000),
				(-0.238856,-0.864188,-0.442863),
				(-0.500000,-0.809017,-0.309017),
				(-0.262866,-0.951056,-0.162460),
				(-0.850651,-0.525731, 0.000000),
				(-0.716567,-0.681718,-0.147621),
				(-0.716567,-0.681718, 0.147621),
				(-0.525731,-0.850651, 0.000000),
				(-0.500000,-0.809017, 0.309017),
				(-0.238856,-0.864188, 0.442863),
				(-0.262866,-0.951056, 0.162460),
				(-0.864188,-0.442863, 0.238856),
				(-0.809017,-0.309017, 0.500000),
				(-0.688191,-0.587785, 0.425325),
				(-0.681718,-0.147621, 0.716567),
				(-0.442863,-0.238856, 0.864188),
				(-0.587785,-0.425325, 0.688191),
				(-0.309017,-0.500000, 0.809017),
				(-0.147621,-0.716567, 0.681718),
				(-0.425325,-0.688191, 0.587785),
				(-0.162460,-0.262866, 0.951056),
				( 0.442863,-0.238856, 0.864188),
				( 0.162460,-0.262866, 0.951056),
				( 0.309017,-0.500000, 0.809017),
				( 0.147621,-0.716567, 0.681718),
				( 0.000000,-0.525731, 0.850651),
				( 0.425325,-0.688191, 0.587785),
				( 0.587785,-0.425325, 0.688191),
				( 0.688191,-0.587785, 0.425325),
				(-0.955423, 0.295242, 0.000000),
				(-0.951056, 0.162460, 0.262866),
				(-1.000000, 0.000000, 0.000000),
				(-0.850651, 0.000000, 0.525731),
				(-0.955423,-0.295242, 0.000000),
				(-0.951056,-0.162460, 0.262866),
				(-0.864188, 0.442863,-0.238856),
				(-0.951056, 0.162460,-0.262866),
				(-0.809017, 0.309017,-0.500000),
				(-0.864188,-0.442863,-0.238856),
				(-0.951056,-0.162460,-0.262866),
				(-0.809017,-0.309017,-0.500000),
				(-0.681718, 0.147621,-0.716567),
				(-0.681718,-0.147621,-0.716567),
				(-0.850651, 0.000000,-0.525731),
				(-0.688191, 0.587785,-0.425325),
				(-0.587785, 0.425325,-0.688191),
				(-0.425325, 0.688191,-0.587785),
				(-0.425325,-0.688191,-0.587785),
				(-0.587785,-0.425325,-0.688191),
				(-0.688191,-0.587785,-0.425325))

class MD2:
	def __init__(self, options):
		self.options = options
		self.object = None
		self.ident = 844121161
		self.version = 8
		return

	def setObject(self, object):
		self.object = object
		self.mesh = self.object.to_mesh(bpy.context.scene, True, 'PREVIEW')

	def makeObject(self):
		print("Creating mesh", end='')
		# Create the mesh
		mesh = bpy.data.meshes.new(self.name)

		# Prepare vertices and faces
		mesh.vertices.add(self.numVerts)
		mesh.tessfaces.add(self.numTris)
		print('.', end='')

		# Verts
		mesh.vertices.foreach_set("co", unpack_list(self.frames[0]))
		mesh.transform(Matrix.Rotation(-pi / 2, 4, 'Z'))
		print('.', end='')

		# Tris
		mesh.tessfaces.foreach_set("vertices_raw", unpack_face_list([face[0] for face in self.tris]))
		print('.', end='')

		# Skins
		mesh.tessface_uv_textures.new()
		if self.numSkins > 0:
			material = bpy.data.materials.new(self.name)
			for skin in self.skins:
				skinImg = Util.loadImage(skin, self.filePath)
				if skinImg == None:
					skinImg = bpy.data.images.new(os.path.join(self.filePath, skin), self.skinWidth, self.skinHeight)
				skinImg.mapping = 'UV'
				skinImg.name = skin
				skinTex = bpy.data.textures.new(self.name + skin, type='IMAGE')
				skinTex.image = skinImg
				matTex = material.texture_slots.add()
				matTex.texture = skinTex
				matTex.texture_coords = 'UV'
				matTex.use_map_color_diffuse = True
				matTex.use_map_alpha = True
				matTex.uv_layer = mesh.tessface_uv_textures[0].name
			mesh.materials.append(material)
		print('.', end='')

		# UV
		mesh.tessface_uv_textures[0].data.foreach_set("uv_raw", unpack_list([self.uvs[i] for i in unpack_face_list([face[1] for face in self.tris])]))
		if self.numSkins > 0:
			image = mesh.materials[0].texture_slots[0].texture.image
			if image != None:
				for uv in mesh.tessface_uv_textures[0].data:
					uv.image = image
		print('.', end='')

		mesh.validate()
		mesh.update()
		obj = bpy.data.objects.new(mesh.name, mesh)
		base = bpy.context.scene.objects.link(obj)
		bpy.context.scene.objects.active = obj
		base.select = True
		print("Done")

		# Animate
		if self.options.fImportAnimation and self.numFrames > 1:
			for i, frame in enumerate(self.frames):
				progressStatus = i / self.numFrames * 100
				#bpy.context.scene.frame_set(i + 1)
				obj.shape_key_add(from_mix=False)
				mesh.vertices.foreach_set("co", unpack_list(frame))
				mesh.transform(Matrix.Rotation(-pi / 2, 4, 'Z'))
				mesh.shape_keys.key_blocks[i].value = 1.0
				mesh.shape_keys.key_blocks[i].keyframe_insert("value", frame=i + 1)
				if i < len(self.frames) - 1:
					mesh.shape_keys.key_blocks[i].value = 0.0
					mesh.shape_keys.key_blocks[i].keyframe_insert("value", frame=i + 2)
				if i > 0:
					mesh.shape_keys.key_blocks[i].value = 0.0
					mesh.shape_keys.key_blocks[i].keyframe_insert("value", frame=i)
				print("Animating - progress: %3i%%\r" % int(progressStatus), end='')
			print("Animating - progress: 100%.")

		bpy.context.scene.update()
		print("Model imported")

	def write(self, filePath):
		self.skinWidth, self.skinHeight, skins = Util.getSkins(self.mesh, self.options.eTextureNameMethod)
		if self.skinWidth < 1:
			self.skinWidth = 64
		if self.skinHeight < 1:
			self.skinHeight = 64
		self.numSkins = len(skins)
		self.numVerts = len(self.mesh.vertices)
		self.numUV, uvList, uvDict = self.buildTexCoord()
		self.numTris = len(self.mesh.tessfaces)

		if self.options.fSkipGLCommands:
			self.numGLCmds = 0
		else:
			self.numGLCmds = 1 + self.buildGLcommands()

		self.numFrames = 1
		if self.options.fExportAnimation:
			self.numFrames = 1 + bpy.context.scene.frame_end - bpy.context.scene.frame_start

		self.frameSize = struct.calcsize("<6f16s") + struct.calcsize("<4B") * self.numVerts

		self.ofsSkins = struct.calcsize("<17i")
		self.ofsUV = self.ofsSkins + struct.calcsize("<64s") * self.numSkins
		self.ofsTris = self.ofsUV + struct.calcsize("<2h") * self.numUV
		self.ofsFrames = self.ofsTris + struct.calcsize("<6H") * self.numTris
		self.ofsGLCmds = self.ofsFrames + self.frameSize * self.numFrames
		self.ofsEnd = self.ofsGLCmds + struct.calcsize("<i") * self.numGLCmds

		file = open(filePath, "wb")
		try:
			# write header
			data = struct.pack("<17i",
								self.ident,
								self.version,
								self.skinWidth,
								self.skinHeight,
								self.frameSize,
								self.numSkins,
								self.numVerts,
								self.numUV, #  number of texture coordinates
								self.numTris,
								self.numGLCmds,
								self.numFrames,
								self.ofsSkins,
								self.ofsUV,
								self.ofsTris,
								self.ofsFrames,
								self.ofsGLCmds,
								self.ofsEnd)
			file.write(data)

			# write skin file names
			for iSkin, (skinPath, skinName) in enumerate(skins):
				sourcePath = bpy.path.abspath(skinPath)

				if self.options.fCopyTextureSxS:
					destPath = os.path.join(os.path.dirname(filePath), os.path.basename(sourcePath))
					print("Copying texture %s to %s" % (sourcePath, destPath))
					try:
						shutil.copy(sourcePath, destPath)
					except:
						print("Copying texture %s to %s failed." % (sourcePath, destPath))
					if self.options.eTextureNameMethod == 'FILEPATH':
						skinName = destPath

				if len(skinName) > MD2_MAX_SKINNAME:
					print("WARNING: The texture name '%s' is too long. It was automatically truncated." % skinName)
					if self.options.eTextureNameMethod == 'FILEPATH':
						skinName = os.path.basename(skinName)

				data = struct.pack("<64s", bytes(skinName[0:MD2_MAX_SKINNAME], encoding="utf8"))
				file.write(data) # skin name


			for uv in uvList:
				data = struct.pack("<2h",
									int(uv[0] * self.skinWidth),
									int((1 - uv[1]) * self.skinHeight)
									)
				file.write(data) # uv
			del uvList

			for face in self.mesh.tessfaces:
				# 0,2,1 for good cw/ccw
				data = struct.pack("<3H",
									face.vertices[0],
									face.vertices[2],
									face.vertices[1]
									)
				file.write(data) # vert index
				uvs = self.mesh.tessface_uv_textures.active.data[face.index].uv
				data = struct.pack("<3H",
									uvDict[(uvs[0][0], uvs[0][1])],
									uvDict[(uvs[2][0], uvs[2][1])],
									uvDict[(uvs[1][0], uvs[1][1])],
									)
				file.write(data) # uv index
			del uvDict

			if self.options.fExportAnimation and self.numFrames > 1:
				timeLineMarkers = []
				for marker in bpy.context.scene.timeline_markers:
					timeLineMarkers.append(marker)

				# sort the markers. The marker with the frame number closest to 0 will be the first marker in the list.
				# The marker with the biggest frame number will be the last marker in the list
				timeLineMarkers.sort(key=lambda marker: marker.frame)
				markerIdx = 0

				# delete markers at same frame positions
				if len(timeLineMarkers) > 1:
					markerFrame = timeLineMarkers[len(timeLineMarkers) - 1].frame
					for i in range(len(timeLineMarkers) - 2, -1, -1):
						if timeLineMarkers[i].frame == markerFrame:
							del timeLineMarkers[i]
						else:
							markerFrame = timeLineMarkers[i].frame

				# calculate shared bounding box
				if self.options.fUseSharedBoundingBox:
					self.bbox_min = None
					self.bbox_max = None
					for frame in range(bpy.context.scene.frame_start, bpy.context.scene.frame_end + 1):
						bpy.context.scene.frame_set(frame)
						self.calcSharedBBox()

				for frame in range(bpy.context.scene.frame_start, bpy.context.scene.frame_end + 1):
					frameIdx = frame - bpy.context.scene.frame_start + 1
					#Display the progress status of the export in the console
					progressStatus = frameIdx / self.numFrames * 100
					print("Export progress: %3i%%\r" % int(progressStatus), end='')

					bpy.context.scene.frame_set(frame)

					if len(timeLineMarkers) != 0:
						if markerIdx + 1 != len(timeLineMarkers):
							if frame >= timeLineMarkers[markerIdx + 1].frame:
								markerIdx += 1
						name = timeLineMarkers[markerIdx].name
					else:
						name = "frame"

					self.outFrame(file, name + str(frameIdx))
			else:
				if self.options.fUseSharedBoundingBox:
					self.bbox_min = None
					self.bbox_max = None
					self.calcSharedBBox()
				self.outFrame(file)

			# gl commands
			if not self.options.fSkipGLCommands:
				for glCmd in self.glCmdList:
					data = struct.pack("<i", glCmd[0])
					file.write(data)
					for cmd in glCmd[1]:
						data = struct.pack("<ffI",
											cmd[0],
											cmd[1],
											cmd[2])
						file.write(data)
				# NULL command
				data = struct.pack("<I", 0)
				file.write(data)

		finally:
			file.close()
		print("Export progress: 100% - Model exported.")

	def read(self, filePath):
		self.filePath = filePath
		self.name = os.path.splitext(os.path.basename(filePath))[0]
		self.skins = []
		self.uvs = [(0.0, 0.0)]
		self.tris = []
		self.frames = []

		print("Reading: %s" % self.filePath, end='')
		progressStatus = 0.0
		inFile = open(self.filePath, "rb")
		try:
			buff = inFile.read(struct.calcsize("<17i"))
			data = struct.unpack("<17i", buff)

			if data[0] != self.ident or data[1] != self.version:
				raise NameError("Invalid MD2 file")

			self.skinWidth = max(1, data[2])
			self.skinHeight = max(1, data[3])
			self.numSkins = data[5]
			self.numVerts = data[6]
			self.numUV = data[7]
			self.numTris = data[8]
			if self.options.fImportAnimation:
				self.numFrames = data[10]
			else:
				self.numFrames = 1
			self.ofsSkins = data[11]
			self.ofsUV = data[12]
			self.ofsTris = data[13]
			self.ofsFrames = data[14]

			# Skins
			if self.numSkins > 0:
				inFile.seek(self.ofsSkins, 0)
				for i in range(self.numSkins):
					buff = inFile.read(struct.calcsize("<64s"))
					data = struct.unpack("<64s", buff)
					self.skins.append(Util.asciiz(data[0].decode("utf-8", "replace")))
			print('.', end='')

			# UV
			inFile.seek(self.ofsUV, 0)
			for i in range(self.numUV):
				buff = inFile.read(struct.calcsize("<2h"))
				data = struct.unpack("<2h", buff)
				self.uvs.append((data[0] / self.skinWidth, 1 - (data[1] / self.skinHeight)))
			print('.', end='')

			# Tris
			inFile.seek(self.ofsTris, 0)
			for i in range(self.numTris):
				buff = inFile.read(struct.calcsize("<6H"))
				data = struct.unpack("<6H", buff)
				self.tris.append(((data[0], data[2], data[1]), (data[3] + 1, data[5] + 1, data[4] + 1)))
			print('.', end='')

			# Frames
			inFile.seek(self.ofsFrames, 0)
			for i in range(self.numFrames):
				buff = inFile.read(struct.calcsize("<6f16s"))
				data = struct.unpack("<6f16s", buff)
				verts = []
				for j in range(self.numVerts):
					buff = inFile.read(struct.calcsize("<4B"))
					vert = struct.unpack("<4B", buff)
					verts.append((data[0] * vert[0] + data[3], data[1] * vert[1] + data[4], data[2] * vert[2] + data[5]))
				self.frames.append(verts)
			print('.', end='')
		finally:
			inFile.close()
		print("Done")

	def calcSharedBBox(self):
		mesh = self.object.to_mesh(bpy.context.scene, True, 'PREVIEW')

		mesh.transform(self.object.matrix_world)
		mesh.transform(Matrix.Rotation(pi / 2, 4, 'Z'))

		min = [mesh.vertices[0].co[0],
			   mesh.vertices[0].co[1],
			   mesh.vertices[0].co[2]]
		max = [mesh.vertices[0].co[0],
			   mesh.vertices[0].co[1],
			   mesh.vertices[0].co[2]]

		for vert in mesh.vertices:
			for i in range(3):
				if vert.co[i] < min[i]:
					min[i] = vert.co[i]
				if vert.co[i] > max[i]:
					max[i] = vert.co[i]

		if self.bbox_min == None:
			self.bbox_min = [min[0], min[1], min[2]]
			self.bbox_max = [max[0], max[1], max[2]]
		else:
			for i in range(3):
				if self.bbox_min[i] > min[i]:
					self.bbox_min[i] = min[i]
				if self.bbox_max[i] < max[i]:
					self.bbox_max[i] = max[i]

	def outFrame(self, file, frameName = "frame"):
		mesh = self.object.to_mesh(bpy.context.scene, True, 'PREVIEW')

		mesh.transform(self.object.matrix_world)
		mesh.transform(Matrix.Rotation(pi / 2, 4, 'Z'))

		if not self.options.fUseSharedBoundingBox:
			###### compute the bounding box ###############
			min = [mesh.vertices[0].co[0],
				   mesh.vertices[0].co[1],
				   mesh.vertices[0].co[2]]
			max = [mesh.vertices[0].co[0],
				   mesh.vertices[0].co[1],
				   mesh.vertices[0].co[2]]

			for vert in mesh.vertices:
				for i in range(3):
					if vert.co[i] < min[i]:
						min[i] = vert.co[i]
					if vert.co[i] > max[i]:
						max[i] = vert.co[i]
			########################################
		else:
			min = self.bbox_min
			max = self.bbox_max

		# BL: some caching to speed it up:
		# -> sd_ gets the vertices between [0 and 255]
		#    which is our important quantization.
		dx = max[0] - min[0] if max[0] - min[0] != 0.0 else 1.0
		dy = max[1] - min[1] if max[1] - min[1] != 0.0 else 1.0
		dz = max[2] - min[2] if max[2] - min[2] != 0.0 else 1.0
		sdx = dx / 255.0
		sdy = dy / 255.0
		sdz = dz / 255.0
		isdx = 255.0 / dx
		isdy = 255.0 / dy
		isdz = 255.0 / dz

		# note about the scale: self.object.scale is already applied via matrix_world
		data = struct.pack("<6f16s",
							# writing the scale of the model
							sdx,
							sdy,
							sdz,
							## now the initial offset (= min of bounding box)
							min[0],
							min[1],
							min[2],
							# and finally the name.
							bytes(frameName, encoding="utf8"))

		file.write(data) # frame header

		for vert in mesh.vertices:
			# find the closest normal for every vertex
			for iN in range(162):
				dot = vert.normal[1] * MD2_NORMALS[iN][0] + \
					-vert.normal[0] * MD2_NORMALS[iN][1] + \
					vert.normal[2] * MD2_NORMALS[iN][2]

				if iN == 0 or dot > maxDot:
					maxDot = dot
					bestNormalIndex = iN

			# and now write the normal.
			data = struct.pack("<4B",
								int((vert.co[0] - min[0]) * isdx),
								int((vert.co[1] - min[1]) * isdy),
								int((vert.co[2] - min[2]) * isdz),
								bestNormalIndex)

			file.write(data) # write vertex and normal

	def buildTexCoord(self):
		# Create an UV coord dictionary to avoid duplicate entries and save space
		meshTextureFaces = self.mesh.tessface_uv_textures.active.data
		uvList = []
		uvDict = {}
		uvCount = 0
		for uv in [(uvs[0], uvs[1]) for meshTextureFace in meshTextureFaces for uvs in meshTextureFace.uv]:
			if uv not in uvDict.keys():
				uvList.append(uv)
				uvDict[uv] = uvCount
				uvCount += 1

		return uvCount, uvList, uvDict

	def findStripLength(self, startTri, startVert):
		meshTextureFaces = self.mesh.tessface_uv_textures.active.data
		numFaces = len(self.mesh.tessfaces)

		self.cmdVerts = []
		self.cmdTris = []
		self.cmdUV = []
		self.used[startTri] = 2

		self.cmdVerts.append(self.mesh.tessfaces[startTri].vertices_raw[startVert % 3])
		self.cmdVerts.append(self.mesh.tessfaces[startTri].vertices_raw[(startVert + 2) % 3])
		self.cmdVerts.append(self.mesh.tessfaces[startTri].vertices_raw[(startVert + 1) % 3])
		self.cmdUV.append(meshTextureFaces[startTri].uv[startVert %3])
		self.cmdUV.append(meshTextureFaces[startTri].uv[(startVert + 2) % 3])
		self.cmdUV.append(meshTextureFaces[startTri].uv[(startVert + 1) % 3])

		stripCount = 1
		self.cmdTris.append(startTri)
		m1 = self.mesh.tessfaces[startTri].vertices_raw[(startVert + 2) % 3]
		m2 = self.mesh.tessfaces[startTri].vertices_raw[(startVert + 1) % 3]

		for triCounter in range(startTri + 1, numFaces):
			for k in range(3):
				if(self.mesh.tessfaces[triCounter].vertices_raw[k] == m1) and (self.mesh.tessfaces[triCounter].vertices_raw[(k + 1) % 3] == m2):
					if(self.used[triCounter] == 0):
						if(stripCount % 2 == 1):  #is this an odd tri
							m1 = self.mesh.tessfaces[triCounter].vertices_raw[(k + 2) % 3]
						else:
							m2 = self.mesh.tessfaces[triCounter].vertices_raw[(k + 2) % 3]

						self.cmdVerts.append(self.mesh.tessfaces[triCounter].vertices_raw[(k + 2) % 3])
						self.cmdUV.append(meshTextureFaces[triCounter].uv[(k + 2) % 3])
						stripCount += 1
						self.cmdTris.append(triCounter)

						self.used[triCounter] = 2
						triCounter = startTri + 1 # restart looking

		#clear used counter
		for usedCounter in range(numFaces):
			if self.used[usedCounter] == 2:
				self.used[usedCounter] = 0

		return stripCount

	def findFanLength(self, startTri, startVert):
		meshTextureFaces = self.mesh.tessface_uv_textures.active.data
		numFaces = len(self.mesh.tessfaces)

		self.cmdVerts = []
		self.cmdTris = []
		self.cmdUV = []
		self.used[startTri] = 2

		self.cmdVerts.append(self.mesh.tessfaces[startTri].vertices_raw[startVert % 3])
		self.cmdVerts.append(self.mesh.tessfaces[startTri].vertices_raw[(startVert + 2) % 3])
		self.cmdVerts.append(self.mesh.tessfaces[startTri].vertices_raw[(startVert + 1) % 3])
		self.cmdUV.append(meshTextureFaces[startTri].uv[startVert % 3])
		self.cmdUV.append(meshTextureFaces[startTri].uv[(startVert + 2) % 3])
		self.cmdUV.append(meshTextureFaces[startTri].uv[(startVert + 1) % 3])

		fanCount = 1
		self.cmdTris.append(startTri)
		m2 = self.mesh.tessfaces[startTri].vertices_raw[(startVert) % 3]
		m1 = self.mesh.tessfaces[startTri].vertices_raw[(startVert + 1) % 3]

		for triCounter in range(startTri + 1, numFaces):
			for k in range(3):
				if (self.mesh.tessfaces[triCounter].vertices_raw[k] == m1) and (self.mesh.tessfaces[triCounter].vertices_raw[(k + 1) % 3] == m2):
					if(self.used[triCounter] == 0):
						m1 = self.mesh.tessfaces[triCounter].vertices_raw[(k + 2) % 3]
						self.cmdVerts.append(self.mesh.tessfaces[triCounter].vertices_raw[(k + 2) % 3])
						self.cmdUV.append(meshTextureFaces[triCounter].uv[(k + 2) % 3])
						fanCount += 1
						self.cmdTris.append(triCounter)

						self.used[triCounter] = 2
						triCounter=startTri + 1 #restart looking

		#clear used counter
		for usedCounter in range(numFaces):
			if self.used[usedCounter] == 2:
				self.used[usedCounter] = 0

		return fanCount

	def buildGLcommands(self):
		numFaces = len(self.mesh.tessfaces)
		self.used = [0] * numFaces
		numCommands = 0
		self.glCmdList = []

		for triCounter in range(numFaces):
			if self.used[triCounter] == 0:
				#intialization
				bestLength = 0
				bestType = 0
				bestVerts = []
				bestTris = []
				bestUV = []

				for startVert in range(3):
					cmdLength = self.findFanLength(triCounter, startVert)
					if (cmdLength > bestLength):
						bestType = 1
						bestLength = cmdLength
						bestVerts = self.cmdVerts
						bestTris = self.cmdTris
						bestUV = self.cmdUV

					cmdLength = self.findStripLength(triCounter, startVert)
					if (cmdLength > bestLength):
						bestType = 0
						bestLength = cmdLength
						bestVerts = self.cmdVerts
						bestTris = self.cmdTris
						bestUV = self.cmdUV

				#mark tris as used
				for usedCounter in range(bestLength):
					self.used[bestTris[usedCounter]] = 1

				cmd = []
				if bestType == 0:
					num = bestLength + 2
				else:
					num = (-(bestLength + 2))

				numCommands += 1
				for cmdCounter in range(bestLength + 2):
					# (u,v) in blender -> (u,1-v)
					cmd.append((bestUV[cmdCounter][0], 1.0-bestUV[cmdCounter][1], bestVerts[cmdCounter]))
					numCommands += 3

				self.glCmdList.append((num, cmd))

		del self.used, bestVerts, bestUV, bestTris, self.cmdVerts, self.cmdUV, self.cmdTris
		return numCommands

class Util:
	# deletes an object from Blender (remove + unlink)
	@staticmethod
	def deleteObject(object):
		bpy.context.scene.objects.unlink(object)
		bpy.data.objects.remove(object)

	# duplicates the given object and returns it
	@staticmethod
	def duplicateObject(object):
		# backup the current object selection and current active object
		selObjects = bpy.context.selected_objects[:]
		actObject = bpy.context.active_object

		# deselect all selected objects
		bpy.ops.object.select_all(action='DESELECT')
		# select the object which we want to duplicate
		object.select = True

		# duplicate the selected object
		bpy.ops.object.duplicate()

		# the duplicated object is automatically selected
		copyObj = bpy.context.selected_objects[0]

		# select all objects which have been previously selected and make active the previous active object
		bpy.context.scene.objects.active = actObject
		for obj in selObjects:
			obj.select = True

		return copyObj

	@staticmethod
	def applyModifiers(object):
		if len(object.modifiers) == 0:
			return object

		modifier = object.modifiers.new('Triangulate-Export','TRIANGULATE')
		mesh = object.to_mesh(bpy.context.scene, True, 'PREVIEW')
		modifiedObj = bpy.data.objects.new(mesh.name, mesh)
		bpy.context.scene.objects.link(modifiedObj)
		object.modifiers.remove(modifier)

		return modifiedObj

	# returns the mesh of the object and return object.data (mesh)
	@staticmethod
	def triangulateMesh(object):
		modifier = object.modifiers.new('Triangulate-Export','TRIANGULATE')

	@staticmethod
	def getSkins(mesh, method):
		skins = []
		width = -1
		height = -1
		for material in mesh.materials:
			for texSlot in material.texture_slots:
				if not texSlot or texSlot.texture.type != 'IMAGE':
					continue
				if any(texSlot.texture.image.filepath in skin for skin in skins):
					continue
				if method == 'BASENAME':
					texname = os.path.basename(texSlot.texture.image.filepath)
				elif method == 'FILEPATH':
					texname = texSlot.texture.image.filepath
				else:
					texname = texSlot.texture.image.name
				skins.append((texSlot.texture.image.filepath, texname))
				if texSlot.texture.image.size[0] > width:
					width = texSlot.texture.image.size[0]
				if texSlot.texture.image.size[1] > height:
					height = texSlot.texture.image.size[1]
		return width, height, skins

	@staticmethod
	def loadImage(imagePath, filePath):
		# Handle ufoai skin name format
		fileName = os.path.basename(imagePath)
		if imagePath[0] == '.':
			for ext in ['.png', '.jpg', '.jpeg']:
				fileName = imagePath[1:] + ext
				if os.path.isfile(os.path.join(os.path.dirname(imagePath), fileName)):
					break
				elif os.path.isfile(os.path.join(os.path.dirname(filePath), fileName)):
					break
			else:
				fileName = imagePath[1:]
		image = load_image(fileName, os.path.dirname(imagePath), recursive=False)
		if image is not None:
			return image
		image = load_image(fileName, os.path.dirname(filePath), recursive=False)
		if image is not None:
			return image
		return None

	@staticmethod
	def asciiz(s):
		for i, c in enumerate(s):
			if ord(c) == 0:
				return s[:i]

class ObjectInfo:
	def __init__(self, object):
		self.vertices = -1
		self.faces = 0
		self.status = ('','')
		self.frames = 1 + bpy.context.scene.frame_end - bpy.context.scene.frame_start
		self.isMesh = object and object.type == 'MESH'

		if self.isMesh:
			originalObject = object
			mesh = object.data

			self.skinWidth, self.skinHeight, self.skins = Util.getSkins(mesh, 'DATANAME')

			try:
				# apply the modifiers
				object = Util.applyModifiers(object)
				mesh = object.data

				mesh.update(calc_tessface=True)
				self.status = (str(len(mesh.vertices)) + " vertices", str(len(mesh.tessfaces)) + " faces")
				self.faces = len(mesh.tessfaces)
				self.vertices = len(mesh.vertices)
				self.isUnwrapped = len(mesh.tessface_uv_textures) > 0

			finally:
				if object.name != originalObject.name:
					originalObject.select = True
					bpy.context.scene.objects.active = originalObject
					Util.deleteObject(object)
		print(originalObject.name + ": ", self.status)

class Export_MD2(bpy.types.Operator, ExportHelper):
	'''Export selection to Quake2 file format (.md2)'''
	bl_idname = "export_quake.md2"
	bl_label = "Export selection to Quake2 file format (.md2)"
	filename_ext = ".md2"

	fSkipGLCommands = BoolProperty(name="Skip GL Commands",
									description="Skip GL Commands to reduce file size",
									default=True)

	fExportAnimation = BoolProperty(name="Export animation",
									description="Export all frames",
									default=False)

	typesExportSkinNames = [
							('DATANAME', "Data block name", "Use image datablock names"),
							('FILEPATH', "File path", "Use image file paths"),
							('BASENAME', "File name", "Use image file names"),
							]
	eTextureNameMethod = EnumProperty(name="Skin names",
										description="Choose skin naming method",
										items=typesExportSkinNames,)

	fCopyTextureSxS = BoolProperty(name="Copy texture(s) next to .md2",
									description="Try to copy textures to md2 directory (won't overwrite files)",
									default=False)

	fUseSharedBoundingBox = BoolProperty(name="Use shared bounding box across frames",
									description="Calculate a shared bounding box from all frames (used to avoid wobbling in static vertices but wastes resolution)",
									default=False)

	def __init__(self):
		try:
			self.object = bpy.context.selected_objects[0]
		except:
			self.object = None

		# go into object mode before we start the actual export procedure
		bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

		self.info = ObjectInfo(self.object)

	def execute(self, context):

		props = self.properties
		filePath = self.filepath
		filePath = bpy.path.ensure_ext(filePath, self.filename_ext)

		object = self.object
		originalObject = object

		if self.info.frames > MD2_MAX_FRAMES and self.fExportAnimation:
			raise RuntimeError("There are too many frames (%i), at most %i are supported in md2" % (info.frames, MD2_MAX_FRAMES))

		object = Util.duplicateObject(object)
		Util.triangulateMesh(object)

		object.data.update(calc_tessface=True)

		# save the current frame to reset it after export
		if self.fExportAnimation:
			frame = bpy.context.scene.frame_current

		try:
			md2 = MD2(self)
			md2.setObject(object)
			md2.write(filePath)
		finally:
			if object.name != originalObject.name:
				originalObject.select = True
				bpy.context.scene.objects.active = originalObject
				Util.deleteObject(object)
			if self.fExportAnimation:
				bpy.context.scene.frame_set(frame)

		self.report({'INFO'},  "Model '%s' exported" % originalObject.name)
		return {'FINISHED'}

	def invoke(self, context, event):
		if not context.selected_objects:
			self.report({'ERROR'}, "Please, select an object to export!")
			return {'CANCELLED'}

		# check constrains
		if len(bpy.context.selected_objects) > 1:
			self.report({'ERROR'}, "Please, select exactly one object to export!")
			return {'CANCELLED'}
		if not self.info.isMesh:
			self.report({'ERROR'}, "Only meshes can be exported (selected object is of type '%s')" % (self.object.type))
			return {'CANCELLED'}
		if self.info.faces > MD2_MAX_TRIANGLES:
			self.report({'ERROR'},
				"Object has too many (triangulated) faces (%i), at most %i are supported in md2" % (self.info.faces, MD2_MAX_TRIANGLES))
			return {'CANCELLED'}
		if self.info.vertices > MD2_MAX_VERTS:
			self.report({'ERROR'},
				"Object has too many vertices (%i), at most %i are supported in md2" % (self.info.vertices, MD2_MAX_VERTS))
			return {'CANCELLED'}
		if not self.info.isUnwrapped:
			self.report({'ERROR'}, "Mesh must be unwrapped")
			return {'CANCELLED'}
		if len(self.info.skins) < 1:
			self.report({'ERROR'}, "There must be at least one skin")
			return {'CANCELLED'}
		if len(self.info.skins) > MD2_MAX_SKINS:
			self.report({'ERROR'}, "There are too many skins (%i), at most %i are supported in md2" % (len(self.info.skins), MD2_MAX_SKINS))
			return {'CANCELLED'}

		wm = context.window_manager
		wm.fileselect_add(self)
		return {'RUNNING_MODAL'}

class Import_MD2(bpy.types.Operator, ImportHelper):
	'''Import Quake2 format file (md2)'''
	bl_idname = "import_quake.md2"
	bl_label = "Import Quake2 format mesh (md2)"

	fImportAnimation = BoolProperty(name="Import animation",
									description="Import all frames",
									default=True)

	def __init__(self):
		if len(bpy.context.selected_objects) > 0:
			# go into object mode before we start the actual import procedure
			bpy.ops.object.mode_set(mode='OBJECT', toggle = False)

	def execute(self, context):
		# deselect all objects
		bpy.ops.object.select_all(action='DESELECT')

		md2 = MD2(self)
		md2.read(self.filepath)
		md2.makeObject()

		bpy.context.scene.update()
		self.report({'INFO'},  "File '%s' imported" % self.filepath)
		return {'FINISHED'}

def menu_func_export(self, context):
	self.layout.operator(Export_MD2.bl_idname, text="Quake II's MD2 (.md2)")

def menu_func_import(self, context):
	self.layout.operator(Import_MD2.bl_idname, text="Quake II's MD2 (.md2)")

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
