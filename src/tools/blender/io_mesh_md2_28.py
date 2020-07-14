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
	"author": "DarkRain, based on the work of Michael Heilmann/Kostas Michalopoulos/Bob Holcomb/Sebastian Lieberknecht/Dao Nguyen/Bernd Meyer/Damien Thebault/Erwan Mathieu/Takehiko Nawata",
	"version": (1, 3),
	"blender": (2, 80, 0),
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

# An MD2 frame given by its name and its (pre-transformed) vertices.
class MD2Frame:
	# @param name the name of the frame (a string)
	# @param verts the vertices of the frame (a list)
	def __init__(self, name, verts):
		self.verts = verts
		self.name =  name

class MD2:
	def __init__(self, options):
		self.options = options
		self.object = None
		self.ident = 844121161
		self.version = 8
		return

	def setObject(self, object):
		self.object = object
		self.mesh = self.object.to_mesh(preserve_all_data_layers=True, depsgraph=bpy.context.evaluated_depsgraph_get())
		self.mesh.update(calc_edges=True)
		self.mesh.calc_loop_triangles()

	def makeObject(self):
		win = bpy.context.window_manager
		win.progress_begin(0, 100)
		# Create the mesh
		mesh = bpy.data.meshes.new(self.name)

		# Prepare vertices and faces
		mesh.vertices.add(self.numVerts)
		mesh.loops.add(self.numTris*3)
		mesh.polygons.add(self.numTris)

		# Verts
		mesh.vertices.foreach_set('co', unpack_list(self.frames[0].verts))
		mesh.transform(Matrix.Rotation(-pi / 2, 4, 'Z'))

		# Tris
		mesh.loops.foreach_set('vertex_index', unpack_list([vi for vi,ui in self.tris]))
		mesh.polygons.foreach_set('loop_start', [i*3 for i in range(self.numTris)])
		mesh.polygons.foreach_set('loop_total', [3 for i in range(self.numTris)])

		# Skins
		if self.numSkins > 0:
			for skin in self.skins:
				material = bpy.data.materials.new(skin)
				material.use_nodes = True
				node = material.node_tree.nodes['Principled BSDF']
				skinImg = Util.loadImage(skin, self.filePath)
				if skinImg == None:
					skinImg = bpy.data.images.new(os.path.join(self.filePath, skin), self.skinWidth, self.skinHeight)
				skinImg.name = skin
				skinTex = material.node_tree.nodes.new('ShaderNodeTexImage')
				skinTex.image = skinImg
				material.node_tree.links.new(node.inputs['Base Color'], skinTex.outputs['Color'])
			mesh.materials.append(material)

		# UV
		uv_layer = mesh.uv_layers.new()
		uv_layer.data.foreach_set('uv', unpack_list([self.uvs[i] for i in unpack_list([ui for vi,ui in self.tris])]))

		mesh.update()
		mesh.validate()
		obj = bpy.data.objects.new(mesh.name, mesh)
		base = bpy.context.scene.collection.objects.link(obj)
		bpy.context.view_layer.objects.active = obj
		obj.select_set(True)

		# Animate
		if self.options.fImportAnimation and self.numFrames > 1:
			for i, frame in enumerate(self.frames):
				progressStatus = i / self.numFrames * 100
				#bpy.context.scene.frame_set(i + 1)
				obj.shape_key_add(name=frame.name, from_mix=False)
				mesh.vertices.foreach_set("co", unpack_list(frame.verts))
				mesh.transform(Matrix.Rotation(-pi / 2, 4, 'Z'))
				mesh.shape_keys.key_blocks[i].value = 1.0
				mesh.shape_keys.key_blocks[i].keyframe_insert("value", frame = i + 1)
				if i < len(self.frames) - 1:
					mesh.shape_keys.key_blocks[i].value = 0.0
					mesh.shape_keys.key_blocks[i].keyframe_insert("value", frame = i + 2)
				if i > 0:
					mesh.shape_keys.key_blocks[i].value = 0.0
					mesh.shape_keys.key_blocks[i].keyframe_insert("value", frame = i)
				win.progress_update(int(progressStatus))

		bpy.context.view_layer.update()
		win.progress_end()

	def write(self, filePath):
		win = bpy.context.window_manager
		win.progress_begin(0, 100)
		self.skinWidth, self.skinHeight, skins = Util.getSkins(self.mesh, self.options.eTextureNameMethod)
		if self.skinWidth < 1:
			self.skinWidth = 64
		if self.skinHeight < 1:
			self.skinHeight = 64
		self.numSkins = len(skins)
		self.numVerts = len(self.mesh.vertices)
		self.numUV, uvList, uvDict = self.buildTexCoord()
		self.numTris = len(self.mesh.loop_triangles)

		if self.options.fSkipGLCommands:
			self.numGLCmds = 0
		else:
			self.numGLCmds = 1 + self.buildGLcommands()

		if self.options.fExportAnimation:
			self.numFrames = 1 + bpy.context.scene.frame_end - bpy.context.scene.frame_start
		else:
			self.numFrames = 1

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

			for face in self.mesh.loop_triangles:
				# 0,2,1 for good cw/ccw
				data = struct.pack("<3H",
									face.vertices[0],
									face.vertices[2],
									face.vertices[1]
									)
				file.write(data) # vert index
				uvs = [self.mesh.uv_layers.active.data[face.loops[0]].uv,
						self.mesh.uv_layers.active.data[face.loops[1]].uv,
						self.mesh.uv_layers.active.data[face.loops[2]].uv]
				data = struct.pack("<3H",
									uvDict[(uvs[0][0], uvs[0][1])],
									uvDict[(uvs[2][0], uvs[2][1])],
									uvDict[(uvs[1][0], uvs[1][1])],
									)
				file.write(data) # uv index
			del uvDict

			self.object.to_mesh_clear()

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
					#Display the progress status of the export
					progressStatus = frameIdx / self.numFrames * 100
					win.progress_update(int(progressStatus))

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
		win.progress_end()

	def read(self, filePath):
		self.filePath = filePath
		self.name = os.path.splitext(os.path.basename(filePath))[0]
		self.skins = []
		self.uvs = [(0.0, 0.0)]
		self.tris = []
		self.frames = []

		inFile = open(self.filePath, "rb")
		try:
			buff = inFile.read(struct.calcsize("<17i"))
			data = struct.unpack("<17i", buff)

			if data[0] != self.ident or data[1] != self.version:
				raise TypeError("Invalid MD2 file")

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

			# UV
			inFile.seek(self.ofsUV, 0)
			for i in range(self.numUV):
				buff = inFile.read(struct.calcsize("<2h"))
				data = struct.unpack("<2h", buff)
				self.uvs.append((data[0] / self.skinWidth, 1 - (data[1] / self.skinHeight)))

			# Tris
			inFile.seek(self.ofsTris, 0)
			for i in range(self.numTris):
				buff = inFile.read(struct.calcsize("<6H"))
				data = struct.unpack("<6H", buff)
				self.tris.append(((data[0], data[2], data[1]), (data[3] + 1, data[5] + 1, data[4] + 1)))

			# Frames
			inFile.seek(self.ofsFrames, 0)
			for i in range(self.numFrames):
				buff = inFile.read(struct.calcsize("<6f16s"))
				data = struct.unpack("<6f16s", buff)
				name = Util.asciiz(data[6].decode("utf-8", "replace"))
				verts = []
				for j in range(self.numVerts):
					buff = inFile.read(struct.calcsize("<4B"))
					vert = struct.unpack("<4B", buff)
					verts.append((data[0] * vert[0] + data[3], data[1] * vert[1] + data[4], data[2] * vert[2] + data[5]))
				self.frames.append(MD2Frame(name,verts))
		finally:
			inFile.close()

	def getMesh(self):
		depsgraph=bpy.context.evaluated_depsgraph_get()
		self.mesh = self.object.evaluated_get(depsgraph).to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
		self.mesh.transform(self.object.matrix_world)
		self.mesh.transform(Matrix.Rotation(pi / 2, 4, 'Z'))

	def getMinMax(self):
		mins = [self.mesh.vertices[0].co[0],
			   self.mesh.vertices[0].co[1],
			   self.mesh.vertices[0].co[2]]
		maxs = [self.mesh.vertices[0].co[0],
			   self.mesh.vertices[0].co[1],
			   self.mesh.vertices[0].co[2]]
		for vert in self.mesh.vertices:
			for i in range(3):
				if vert.co[i] < mins[i]:
					mins[i] = vert.co[i]
				if vert.co[i] > maxs[i]:
					maxs[i] = vert.co[i]
		return mins, maxs


	def calcSharedBBox(self):
		self.getMesh()

		mins, maxs = self.getMinMax()

		if self.bbox_min == None:
			self.bbox_min = [mins[0], mins[1], mins[2]]
			self.bbox_max = [maxs[0], maxs[1], maxs[2]]
		else:
			for i in range(3):
				if self.bbox_min[i] > mins[i]:
					self.bbox_min[i] = mins[i]
				if self.bbox_max[i] < maxs[i]:
					self.bbox_max[i] = maxs[i]
		self.object.to_mesh_clear()

	def outFrame(self, file, frameName = "frame"):
		self.getMesh()

		# As MD2 stores quantized vertices, we need to perform quantization.
		# The quantization is that triples of floats (the position coordinates)
		# of the vertices need to be quantized to triples of bytes.
		#
		# To achieve that, we proceed in two steps:
		# a) normalize the x,y,z values to a range between -1,+1.
		# We can achieve that by computing the bounding box of the model
		# in terms of its maximum "max" and its minimum "min"
		if not self.options.fUseSharedBoundingBox:
			mins, maxs = self.getMinMax()
		else:
			mins = self.bbox_min
			maxs = self.bbox_max

		# and compute the diameter of the bounding box "dia = max - min"
		# and its midpoint "mid = min + (max - min)*0.5". Given some vertex
		# v we can now translate the model to have its bounding box minimum
		# at (0,0,0)  and scale it by along each axis by the inverse of its
		# diameter. Given a vertex v = (v.x,v.y,v.z) we transform each
		# component v.k, k in {x,y,z}
		#	v'.k = (v.k - min.k)/dia.k
		# Note that "dia.k" might be equal "0" in that case,
		# we fix set "dia.k=1". We have just bounded the
		# values into the range of [0,1]: To see that simply note that
		#    min.k <= v.k <= max.k
		# => 0 <= v.k - min.k <= max.k - min.k
		# As max.k - min.k = dia.k
		# => 0 <= (v.k - min.k)/dia.k <= 1.0.
		#
		# However, we want to "spread" our values not in the range of [0,1],
		# but rather in the range [0,255] which can be easily realized by
		# a slight adjustment to the computation of v'.k
		#	  v'.k = (v.k - min.k)/(dia.k/255)
		# <=> v'.k = (v.k - min.k)*(255/dia.k)
		# An advantage is, that 255/dia.k can be precomputed!
		# We can show now that the values are actually within the range of [0,255].
		# From the previous "proof" we have.
		#  0 <= (v.k - min.k)/dia.k <= 1.0
		# and multiplying by 255 gives
		# => 0*255 <= (v.k - min.k)*255/dia.k <= 1.0 * 255
		# => 0 <= (v.k - min.k)*/(255/dia.k) <= 255
		# as desired. We call 255/dia.k in the following code is.k = 255/dia.k.
		# Note that we can reverse this transformation
		# (v.k - min.k)*255/dia.k = v'.k
		# v.k - min.k = v'.k * (dia.k / 255)
		# v.k = v'.k * (dia.k / 255) + min.k
		# and call dia.k / 255 = s.k in the folowing code.

		dia_x = maxs[0] - mins[0]
		dia_y = maxs[1] - mins[1]
		dia_z = maxs[2] - mins[2]
		if dia_x == 0.0 :
			dia_x = 1.0
		if dia_y == 0.0 :
			dia_y = 1.0
		if dia_z == 0.0 :
			dia_z = 1.0
		s_x = dia_x / 255.0
		s_y = dia_y / 255.0
		s_z = dia_z / 255.0
		t_x = mins[0]
		t_y = mins[1]
		t_z = mins[2]

		# note about the scale: self.object.scale is already applied via matrix_world
		data = struct.pack("<6f16s",
							# writing the scale of the model
							s_x,
							s_y,
							s_z,
							# write the translation of the model
							t_x,
							t_y,
							t_z,
							# and finally the name.
							bytes(frameName, encoding="utf8"))

		file.write(data) # frame header

		is_x = 255.0 / dia_x
		is_y = 255.0 / dia_y
		is_z = 255.0 / dia_z

		for vert in self.mesh.vertices:
			# find the closest normal for every vertex
			maxDot = -99999.0
			bestNormalIndex = 1
			for iN in range(162):
				dot = vert.normal[1] * MD2_NORMALS[iN][0] + \
					- vert.normal[0] * MD2_NORMALS[iN][1] + \
					  vert.normal[2] * MD2_NORMALS[iN][2]

				if iN == 0 or dot > maxDot:
					maxDot = dot
					bestNormalIndex = iN

			# and now write the vertex and the normal.
			data = struct.pack("<4B",
								int((vert.co[0] - t_x) * is_x),
								int((vert.co[1] - t_y) * is_y),
								int((vert.co[2] - t_z) * is_z),
								bestNormalIndex)

			file.write(data) # write vertex and normal
		self.object.to_mesh_clear()

	def buildTexCoord(self):
		# Create an UV coord dictionary to avoid duplicate entries and save space
		meshTextureFaces = self.mesh.uv_layers.active.data
		uvList = []
		uvDict = {}
		uvCount = 0
		for uv in [(meshTextureFace.uv[0], meshTextureFace.uv[1]) for meshTextureFace in meshTextureFaces]:
			if uv not in uvDict.keys():
				uvList.append(uv)
				uvDict[uv] = uvCount
				uvCount += 1

		return uvCount, uvList, uvDict

	def findStripLength(self, startTri, startVert):
		meshTextureFaces = self.mesh.uv_layers.active.data
		numFaces = len(self.mesh.loop_triangles)

		self.cmdVerts = []
		self.cmdTris = []
		self.cmdUV = []
		self.used[startTri] = 2

		self.cmdVerts.append(self.mesh.loop_triangles[startTri].vertices[startVert % 3])
		self.cmdVerts.append(self.mesh.loop_triangles[startTri].vertices[(startVert + 2) % 3])
		self.cmdVerts.append(self.mesh.loop_triangles[startTri].vertices[(startVert + 1) % 3])
		self.cmdUV.append(meshTextureFaces[self.mesh.loop_triangles[startTri].loops[startVert %3]].uv)
		self.cmdUV.append(meshTextureFaces[self.mesh.loop_triangles[startTri].loops[(startVert + 2) % 3]].uv)
		self.cmdUV.append(meshTextureFaces[self.mesh.loop_triangles[startTri].loops[(startVert + 1) % 3]].uv)

		stripCount = 1
		self.cmdTris.append(startTri)
		m1 = self.mesh.loop_triangles[startTri].vertices[(startVert + 2) % 3]
		m2 = self.mesh.loop_triangles[startTri].vertices[(startVert + 1) % 3]

		for triCounter in range(startTri + 1, numFaces):
			for k in range(3):
				if(self.mesh.loop_triangles[triCounter].vertices[k] == m1) and (self.mesh.loop_triangles[triCounter].vertices[(k + 1) % 3] == m2):
					if(self.used[triCounter] == 0):
						if(stripCount % 2 == 1):  #is this an odd tri
							m1 = self.mesh.loop_triangles[triCounter].vertices[(k + 2) % 3]
						else:
							m2 = self.mesh.loop_triangles[triCounter].vertices[(k + 2) % 3]

						self.cmdVerts.append(self.mesh.loop_triangles[triCounter].vertices[(k + 2) % 3])
						self.cmdUV.append(meshTextureFaces[self.mesh.loop_triangles[triCounter].loops[(k + 2) % 3]].uv)
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
		meshTextureFaces = self.mesh.uv_layers.active.data
		numFaces = len(self.mesh.loop_triangles)

		self.cmdVerts = []
		self.cmdTris = []
		self.cmdUV = []
		self.used[startTri] = 2

		self.cmdVerts.append(self.mesh.loop_triangles[startTri].vertices[startVert % 3])
		self.cmdVerts.append(self.mesh.loop_triangles[startTri].vertices[(startVert + 2) % 3])
		self.cmdVerts.append(self.mesh.loop_triangles[startTri].vertices[(startVert + 1) % 3])
		self.cmdUV.append(meshTextureFaces[self.mesh.loop_triangles[startTri].loops[startVert % 3]].uv)
		self.cmdUV.append(meshTextureFaces[self.mesh.loop_triangles[startTri].loops[(startVert + 2) % 3]].uv)
		self.cmdUV.append(meshTextureFaces[self.mesh.loop_triangles[startTri].loops[(startVert + 1) % 3]].uv)

		fanCount = 1
		self.cmdTris.append(startTri)
		m2 = self.mesh.loop_triangles[startTri].vertices[(startVert) % 3]
		m1 = self.mesh.loop_triangles[startTri].vertices[(startVert + 1) % 3]

		for triCounter in range(startTri + 1, numFaces):
			for k in range(3):
				if (self.mesh.loop_triangles[triCounter].vertices[k] == m1) and (self.mesh.loop_triangles[triCounter].vertices[(k + 1) % 3] == m2):
					if(self.used[triCounter] == 0):
						m1 = self.mesh.loop_triangles[triCounter].vertices[(k + 2) % 3]
						self.cmdVerts.append(self.mesh.loop_triangles[triCounter].vertices[(k + 2) % 3])
						self.cmdUV.append(meshTextureFaces[self.mesh.loop_triangles[triCounter].loops[(k + 2) % 3]].uv)
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
		numFaces = len(self.mesh.loop_triangles)
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
		bpy.data.objects.remove(object, do_unlink=True)

	# duplicates the given object and returns it
	@staticmethod
	def duplicateObject(object):
		# backup the current object selection and current active object
		selObjects = bpy.context.selected_objects[:]
		actObject = bpy.context.active_object

		# deselect all selected objects
		bpy.ops.object.select_all(action='DESELECT')
		# select the object which we want to duplicate
		object.select_set(True)

		# duplicate the selected object
		bpy.ops.object.duplicate()

		# the duplicated object is automatically selected
		copyObj = bpy.context.selected_objects[0]

		# select all objects which have been previously selected and make active the previous active object
		bpy.context.view_layer.objects.active = actObject
		for obj in selObjects:
			obj.select_set(True)

		return copyObj

	@staticmethod
	def getSkins(mesh, method):
		skins = []
		width = -1
		height = -1
		for material in mesh.materials:
			for node in material.node_tree.nodes:
				if not node or node.type != 'BSDF_PRINCIPLED':
					continue
				base = node.inputs['Base Color']
				for link in base.links:
					texture = link.from_node
					if texture.type != 'TEX_IMAGE' or texture.image == None:
						continue
					if method == 'BASENAME':
						texname = os.path.basename(texture.image.filepath)
					elif method == 'FILEPATH':
						texname = texture.image.filepath
					else:
						texname = material.name
					skins.append((texture.image.filepath, texname))
					if texture.image.size[0] > width:
						width = texture.image.size[0]
					if texture.image.size[1] > height:
						height = texture.image.size[1]
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
			mesh = object.to_mesh(preserve_all_data_layers=True, depsgraph=bpy.context.evaluated_depsgraph_get())
			self.skinWidth, self.skinHeight, self.skins = Util.getSkins(mesh, 'DATANAME')

			try:
				mesh.update(calc_edges=True)
				mesh.calc_loop_triangles()

				self.status = (str(len(mesh.vertices)) + " vertices", str(len(mesh.loop_triangles)) + " faces")
				self.faces = len(mesh.loop_triangles)
				self.vertices = len(mesh.vertices)
				self.isUnwrapped = len(mesh.uv_layers) > 0

			finally:
				object.to_mesh_clear()

class Export_MD2(bpy.types.Operator, ExportHelper):
	'''Export selection to Quake2 file format (.md2)'''
	bl_idname = "export_quake.md2"
	bl_label = "Export selection to Quake2 file format (.md2)"
	filename_ext = ".md2"

	fSkipGLCommands: BoolProperty(name="Skip GL Commands",
									description="Skip GL Commands to reduce file size",
									default=True)

	fExportAnimation: BoolProperty(name="Export animation",
									description="Export all frames",
									default=False)

	typesExportSkinNames = [
							('DATANAME', "Material name", "Use material names"),
							('FILEPATH', "File path", "Use image file paths"),
							('BASENAME', "File name", "Use image file names"),
							]
	eTextureNameMethod: EnumProperty(name="Skin names",
										description="Choose skin naming method",
										items=typesExportSkinNames,)

	fCopyTextureSxS: BoolProperty(name="Copy texture(s) next to .md2",
									description="Try to copy textures to md2 directory (won't overwrite files)",
									default=False)

	fUseSharedBoundingBox: BoolProperty(name="Use shared bounding box across frames",
									description="Calculate a shared bounding box from all frames (used to avoid wobbling in static vertices but wastes resolution)",
									default=False)

	def __init__(self):
		try:
			self.object = bpy.context.selected_objects[0]
		except:
			self.object = None


	def execute(self, context):

		props = self.properties
		filePath = self.filepath
		filePath = bpy.path.ensure_ext(filePath, self.filename_ext)

		object = self.object
		originalObject = object

		if self.info.frames > MD2_MAX_FRAMES and self.fExportAnimation:
			raise RuntimeError("There are too many frames (%i), at most %i are supported in md2" % (info.frames, MD2_MAX_FRAMES))

		object = Util.duplicateObject(object)

		# save the current frame to reset it after export
		if self.fExportAnimation:
			frame = bpy.context.scene.frame_current

		try:
			md2 = MD2(self)
			md2.setObject(object)
			md2.write(filePath)
		finally:
			if object.name != originalObject.name:
				originalObject.select_set(True)
				bpy.context.view_layer.objects.active = originalObject
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
		# go into object mode before we start the actual export procedure
		bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

		self.info = ObjectInfo(self.object)
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

	fImportAnimation: BoolProperty(name="Import animation",
									description="Import all frames",
									default=True)


	def execute(self, context):
		#bpy.ops.object.mode_set(mode='OBJECT', toggle = False)
		# deselect all objects
		bpy.ops.object.select_all(action='DESELECT')

		md2 = MD2(self)
		md2.read(self.filepath)
		md2.makeObject()

		bpy.context.view_layer.update()
		self.report({'INFO'},  "File '%s' imported" % self.filepath)
		return {'FINISHED'}

def menu_func_export(self, context):
	self.layout.operator(Export_MD2.bl_idname, text="Quake II's MD2 (.md2)")

def menu_func_import(self, context):
	self.layout.operator(Import_MD2.bl_idname, text="Quake II's MD2 (.md2)")

__classes__ = (Import_MD2, Export_MD2)

# One of the two functions Blender calls.
def register():
	for cls in __classes__:
		bpy.utils.register_class(cls)
	bpy.types.TOPBAR_MT_file_export.append(menu_func_export)
	bpy.types.TOPBAR_MT_file_import.append(menu_func_import)

# One of the two functions Blender calls.
def unregister():
	for cls in __classes__:
		bpy.utils.unregister_class(cls)
	bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
	bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)

if __name__ == "__main__":
	register()
