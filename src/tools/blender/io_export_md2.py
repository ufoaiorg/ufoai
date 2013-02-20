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

"""
This script exports a mesh object to the Quake2 "MD2" file format.

If you want to choose the name of the animations, you have to go to the timeline, choose the starting frame, choose 'Frame', 'Add Marker' (M) and then set a name with 'Frame', 'Name Marker' (Ctrl M).

The frames are named <frameName><N> with :<br>
 - <N> the frame number<br>
 - <frameName> the name choosen at the last marker (or 'frame' if the last marker has no name or if there is no last marker)

Usually, importers expect that the name don't have any digit (to be able to detect where the frame name starts and where the frame number starts.

Skins are set using image textures in materials -- the image *name*, not the path is used as skin name, if it is longer than 63 characters it is truncated.

Thanks to Bob Holcomb for MD2_NORMALS taken from his exporter.<br>
Thanks to David Henry for the documentation about the MD2 file format.
"""

bl_info = {
    "name": "Quake2 MD2 format",
    "description": "Export to Quake2 file format for UFO:AI (.md2)",
    "author": "DarkRain, based on the work of Sebastian Lieberknecht/Dao Nguyen/Bernd Meyer and Damien Thebault/Erwan Mathieu",
    "version": (1, 2),
    "blender": (2, 63, 0),
    "location": "File > Export > Quake2 MD2",
    "warning": '', # used for warning icon and text in addons panel
    "support": 'COMMUNITY',
    "category": "Import-Export"}

import bpy
from bpy.props import *

from bpy_extras.io_utils import ExportHelper

import math
import mathutils

import struct
import random
import os
import shutil

MD2_MAX_TRIANGLES=4096
MD2_MAX_VERTS=2048
MD2_MAX_FRAMES=1024		#512 - 1024 is an UFO:AI extension.
MD2_MAX_SKINS=32
MD2_MAX_SKINNAME=63
MD2_NORMALS=((-0.525731, 0.000000, 0.850651),
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
		self.progressBarDisplayed = -10
		return

	def setObject(self, object):
		self.object = object

	def write(self, filename):
		self.version = 8

		self.skinwidth = 2**10-1 #1023
		self.skinheight = 2**10-1 #1023

		mesh = self.object.data

		skins = Util.getSkins(mesh, self.options.eTextureNameMethod)

		self.num_skins = len(skins)
		self.num_xyz = len(mesh.vertices)
		self.num_st = len(mesh.tessfaces)*3
		self.num_tris = len(mesh.tessfaces)
		if self.options.fSkipGLCommands:
			self.num_glcmds = 0
		else:
			self.num_glcmds = self.num_tris * (1+3*3) + 1

		self.num_frames = 1
		if self.options.fExportAnimation:
			self.num_frames = 1 + bpy.context.scene.frame_end - bpy.context.scene.frame_start

		self.framesize = 40+4*self.num_xyz

		self.ofs_skins = 68 # size of the header
		self.ofs_st = self.ofs_skins + 64*self.num_skins
		self.ofs_tris = self.ofs_st + 4*self.num_st
		self.ofs_frames = self.ofs_tris + 12*self.num_tris
		self.ofs_glcmds = self.ofs_frames + self.framesize*self.num_frames
		self.ofs_end = self.ofs_glcmds + 4*self.num_glcmds

		file = open(filename, 'wb')
		try:
			# write header
			data = struct.pack('<4B16i',
			                  ord('I'),
			                  ord('D'),
			                  ord('P'),
			                  ord('2'),
			                  self.version,
			                  self.skinwidth,
			                  self.skinheight,
			                  self.framesize,
			                  self.num_skins,
			                  self.num_xyz,
			                  self.num_st, #  number of texture coordinates
			                  self.num_tris,
			                  self.num_glcmds,
			                  self.num_frames,
			                  self.ofs_skins,
			                  self.ofs_st,
			                  self.ofs_tris,
			                  self.ofs_frames,
			                  self.ofs_glcmds,
			                  self.ofs_end)
			file.write(data)

			# write skin file names
			for iSkin, (skinPath, skinName) in enumerate(skins):

				filePath = bpy.path.abspath(skinPath)

				if self.options.fCopyTextureSxS:
					destPath = os.path.join(os.path.dirname(filename), os.path.basename(filePath))
					print("Copying texture %s to %s" % (filePath, destPath))
					try:
						shutil.copy(filePath, destPath)
					except:
						print("Copying texture %s to %s failed." % (filePath, destPath))
					if self.options.eTextureNameMethod == 'FilePath':
						skinName = destPath

				if len(skinName) > MD2_MAX_SKINNAME:
					print("WARNING: The texture name '"+skinName+"' is too long. It was automatically truncated.")
					if self.options.eTextureNameMethod == 'FilePath':
						skinName = os.path.basename(filePath)

				data = struct.pack('<64s', bytes(skinName[0:MD2_MAX_SKINNAME], encoding='utf8'))
				file.write(data) # skin name


			#define meshTextureFaces
			if len(mesh.tessface_uv_textures) != 0:
				meshTextureFaces = mesh.tessface_uv_textures.active.data
			else:
				print("WARNING: The Object lacks uv data")
				meshTextureFaces = mesh.tessfaces

			for meshTextureFace in meshTextureFaces:
				try:
					uvs = meshTextureFace.uv
				except:
					uvs = ([0,0],[0,0],[0,0])

				# (u,v) in blender -> (u,1-v)
				data = struct.pack('<6h',
								  int(uvs[0][0]*self.skinwidth),
								  int((1-uvs[0][1])*self.skinheight),
								  int(uvs[1][0]*self.skinwidth),
								  int((1-uvs[1][1])*self.skinheight),
								  int(uvs[2][0]*self.skinwidth),
								  int((1-uvs[2][1])*self.skinheight),
								  )
				file.write(data) # uv
				# (uv index is : face.index*3+i)

			for face in mesh.tessfaces:
				# 0,2,1 for good cw/ccw
				data = struct.pack('<3H',
				                  face.vertices[0],
				                  face.vertices[2],
				                  face.vertices[1]
				                )
				file.write(data) # vert index
				data = struct.pack('<3H',
				                  face.index*3 + 0,
				                  face.index*3 + 2,
				                  face.index*3 + 1,
				                  )

				file.write(data) # uv index

			if self.options.fExportAnimation:
				min = None
				max = None

				timeLineMarkers =[]
				for marker in bpy.context.scene.timeline_markers:
					timeLineMarkers.append(marker)

				# sort the markers. The marker with the frame number closest to 0 will be the first marker in the list.
				# The marker with the biggest frame number will be the last marker in the list
				timeLineMarkers.sort(key=lambda marker: marker.frame)
				markerIdx = 0

				# delete markers at same frame positions
				if len(timeLineMarkers) > 1:
					markerFrame = timeLineMarkers[len(timeLineMarkers) - 1].frame
					for i in range(len(timeLineMarkers)-2, -1, -1):
						if timeLineMarkers[i].frame == markerFrame:
							del timeLineMarkers[i]
						else:
							markerFrame = timeLineMarkers[i].frame

				# BL: to fix: 1 is assumed to be the frame start (this is
				# hardcoded sometimes...)
				for frame in range(bpy.context.scene.frame_start , bpy.context.scene.frame_end + 1):
					frameIdx = frame - bpy.context.scene.frame_start
					#Display the progress status of the export in the console
					progressStatus = math.floor(frameIdx / self.num_frames * 100)
					if progressStatus - self.progressBarDisplayed >= 10:
						# only show major updates (>=10%)
						print("Export progress: %3i%%\r" % int(progressStatus), end=' ')
						self.progressBarDisplayed = progressStatus

					if frameIdx + 1 == self.num_frames:
						print("Export progress: %3i%% - Model exported." % 100)

					bpy.context.scene.frame_set(frame)

					if len(timeLineMarkers) != 0:
						if markerIdx + 1 != len(timeLineMarkers):
							if frame >= timeLineMarkers[markerIdx + 1].frame:
								markerIdx += 1
						name = timeLineMarkers[markerIdx].name
					else:
						name = 'frame'

					self.outFrame(file, name + str(frameIdx))
			else:
				self.outFrame(file)

			# gl commands TODO: implement me
			if not self.options.fSkipGLCommands:
				for meshTextureFace in meshTextureFaces:
					try:
						uvs = meshTextureFace.uv
					except:
						uvs = ([0,0],[0,0],[0,0])
					data = struct.pack('<i', 3)
					file.write(data)
					# 0,2,1 for good cw/ccw (also flips/inverts normal)
					for vert in [0,2,1]:
						# (u,v) in blender -> (u,1-v)
						data = struct.pack('<ffI',
							uvs[vert][0],
							(1.0 - uvs[vert][1]),
							face.vertices[vert])

						file.write(data)
				# NULL command
				data = struct.pack('<I', 0)
				file.write(data)
		finally:
			file.close()

	def outFrame(self, file, frameName = 'frame'):
		mesh = self.object.to_mesh(bpy.context.scene, True, 'PREVIEW')

		mesh.transform(self.object.matrix_world)
		mesh.transform(mathutils.Matrix.Rotation(math.pi/2, 4, 'Z'))

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

		# BL: some caching to speed it up:
		# -> sd_ gets the vertices between [0 and 255]
		#    which is our important quantization.
		sdx = (max[0]-min[0]) / 255.0
		sdy = (max[1]-min[1]) / 255.0
		sdz = (max[2]-min[2]) / 255.0
		isdx = 255.0 / (max[0]-min[0])
		isdy = 255.0 / (max[1]-min[1])
		isdz = 255.0 / (max[2]-min[2])

		# note about the scale: self.object.scale is already applied via matrix_world
		data = struct.pack('<6f16s',
			# writing the scale of the model
			sdx,
			sdy,
			sdz,
			## now the initial offset (= min of bounding box)
			min[0],
			min[1],
			min[2],
			# and finally the name.
			bytes(frameName, encoding='utf8'))

		file.write(data) # frame header

		for vert in mesh.vertices:

			# find the closest normal for every vertex
			for iN in range(162):
				dot =  vert.normal[1]*MD2_NORMALS[iN][0] + \
				      -vert.normal[0]*MD2_NORMALS[iN][1] + \
				       vert.normal[2]*MD2_NORMALS[iN][2]

				if iN==0 or dot > maxDot:
					maxDot = dot
					bestNormalIndex = iN

			# and now write the normal.
			data = struct.pack('<4B',
			                  int((vert.co[0]-min[0])*isdx),
			                  int((vert.co[1]-min[1])*isdy),
			                  int((vert.co[2]-min[2])*isdz),
			                  bestNormalIndex)

			file.write(data) # write vertex and normal

class Util:
	@staticmethod
	def pickName():
		name = '_MD2Obj_'+str(random.random())
		return name[0:20]

	# deletes an object from Blender (remove + unlink)
	@staticmethod
	def deleteObject(object):
		bpy.context.scene.objects.unlink(object)
		bpy.data.objects.remove(object)

	# duplicates the given object and returns it
	@staticmethod
	def duplicateObject(object, name):
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

		# rename the object with the given name
		copyObj.name = name

		# select all objects which have been previously selected and make active the previous active object
		bpy.context.scene.objects.active = actObject
		for obj in selObjects:
			obj.select = True

		return copyObj

	@staticmethod
	def applyModifiers(object, possibleNewName):
		# backup the current object selection and current active object
		selObjects = bpy.context.selected_objects[:]
		actObject = bpy.context.active_object

		# deselect all selected objects
		bpy.ops.object.select_all(action='DESELECT')
		# select the object which we want to apply modifiers to
		object.select = True

		# duplicate the selected object
		bpy.ops.object.duplicate()

		# the duplicated object is automatically selected
		modifiedObj = bpy.context.selected_objects[0]

		# now apply all modifiers except the Armature modifier...
		for modifier in modifiedObj.modifiers:
			if modifier.type == "ARMATURE":
				# these must stay for the animation
				continue

			# all others can be applied.
			bpy.ops.object.modifier_apply(modifier=modifier.name)

		# select all objects which have been previously selected and make active the previous active object
		bpy.context.scene.objects.active = actObject
		for obj in selObjects:
			obj.select = True

		if modifiedObj.name == object.name:
			# no modifier applied.
			return object

		# modifiers were applied:
		modifiedObj.name = possibleNewName

		return modifiedObj

	# returns the mesh of the object and return object.data (mesh)
	@staticmethod
	def triangulateMesh(object):
		mesh = object.data

		# make the object the active object!
		bpy.context.scene.objects.active = object

		bpy.ops.object.mode_set( mode="EDIT" , toggle = False )
		bpy.ops.mesh.select_all(action="SELECT")

		bpy.ops.mesh.quads_convert_to_tris()
		bpy.ops.object.mode_set( mode="OBJECT" , toggle = False )

		mesh.update(calc_tessface=True)
		return mesh

	@staticmethod
	def getSkins(mesh, method):
		skins = []
		for material in mesh.materials:
			for texSlot in material.texture_slots:
				if not texSlot or texSlot.texture.type != "IMAGE":
					continue
				if method == 'BaseName':
					texname = os.path.basename(texSlot.texture.image.filepath)
				elif method == 'FilePath':
					texname = texSlot.texture.image.filepath
				else:
					texname = texSlot.texture.image.name
				skins.append((texSlot.texture.image.filepath, texname))

		return skins

class ObjectInfo:
	def __init__(self, object):
		self.triang = True
		self.vertices = -1
		self.faces = 0
		self.status = ('','')
		self.frames = 1 + bpy.context.scene.frame_end - bpy.context.scene.frame_start
		self.isMesh = object and object.type == 'MESH'

		if self.isMesh:
			originalObject = object
			mesh = object.data

			self.skins = Util.getSkins(mesh, 'DataName')

			tmpObjectName = Util.pickName()
			try:
				# apply the modifiers
				object = Util.applyModifiers(object, tmpObjectName)

				if object.name != tmpObjectName: # not yet copied: do it now.
					object = Util.duplicateObject(object, tmpObjectName)
				mesh = Util.triangulateMesh(object)

				self.status = (str(len(mesh.vertices)) + ' vertices', str(len(mesh.tessfaces)) + ' faces')
				self.cTessFaces = len(mesh.tessfaces)
				self.vertices = len(mesh.vertices)

			finally:
				if object.name == tmpObjectName:
					originalObject.select = True
					bpy.context.scene.objects.active = originalObject
					Util.deleteObject(object)
		print(self.status)

class Export_MD2(bpy.types.Operator, ExportHelper):
	"""Export selection to Quake2 file format (.md2)"""
	bl_idname = "export_quake.md2"
	bl_label = "Export selection to Quake2 file format (.md2)"

	filename = StringProperty(name="File Path",
		description="Filepath used for processing the script",
		maxlen= 1024,default= "")

	filename_ext = ".md2"

	fSkipGLCommands = BoolProperty(name="Skip GL Commands",
							description="Skip GL Commands to reduce file size",
							default=True)

	fExportAnimation = BoolProperty(name="Export animation",
							description="default: False",
							default=False)

	typesExportSkinNames = [
							('DataName', 'Data block name', 'Use image datablock names'),
							('FilePath', 'File path', 'Use image file paths'),
							('BaseName', 'File name', 'Use image file names'),
							]
	eTextureNameMethod = EnumProperty(name="Skin names",
							description="Choose skin naming method",
							items=typesExportSkinNames,)

	fCopyTextureSxS = BoolProperty(name="Copy texture(s) next to .md2",
							description="default: False",
							default=False)

	# id_export   = 1
	# id_cancel   = 2
	# id_anim     = 3
	# id_update   = 4
	# id_help     = 5
	# id_basename = 6

	def __init__(self):
		try:
			self.object = bpy.context.selected_objects[0]
		except:
			self.object = None

		# go into object mode before we start the actual export procedure
		bpy.ops.object.mode_set( mode="OBJECT" , toggle = False )

		self.info = ObjectInfo(self.object)

	def execute(self, context):

		props = self.properties
		filepath = self.filepath
		filepath = bpy.path.ensure_ext(filepath, self.filename_ext)

		object = self.object
		originalObject = object

		# different name each time or we can't unlink it later
		tmpObjectName = Util.pickName()

		object = Util.applyModifiers(object, tmpObjectName)

		mesh = object.data

		if self.info.triang:
			if object.name != tmpObjectName: # not yet copied: do it now.
				object = Util.duplicateObject(object, tmpObjectName)
			mesh = Util.triangulateMesh(object)

		if object.type != 'MESH':
			raise NameError('Selected object must be a mesh!')

		if self.info.frames > MD2_MAX_FRAMES and self.fExportAnimation:
			raise NameError("There are too many frames (%i), at most %i are supported in md2" % (info.frames, MD2_MAX_FRAMES))

		# save the current frame to reset it after export
		if self.fExportAnimation:
			frame = bpy.context.scene.frame_current

		try:
			md2 = MD2(self)
			md2.setObject(object)
			md2.write(filepath)
		finally:
			if object.name == tmpObjectName:
				originalObject.select = True
				bpy.context.scene.objects.active = originalObject
				Util.deleteObject(object)
			if self.fExportAnimation:
				bpy.context.scene.frame_set(frame)

			self.report({'INFO'},  "Model '"+originalObject.name+"' exported")
		return {'FINISHED'}

	def invoke(self, context, event):

		if not context.selected_objects:
			self.report({'ERROR'}, "Please, select an object to export!")
			return {'CANCELLED'}

		# check constrains
		if len(bpy.context.selected_objects) > 1:
			self.report({'ERROR'}, "Please, select exactly one object to export!")
			return {'CANCELLED'}
		obj = bpy.context.selected_objects[0]
		info = ObjectInfo(obj)
		if not info.isMesh:
			self.report({'ERROR'}, "Only meshes can be exported (selected object is of type '%s')" % (obj.type))
			return {'CANCELLED'}
		if info.faces > MD2_MAX_TRIANGLES:
			self.report({'ERROR'},
			   "Object has too many (triangulated) faces (%i), at most %i are supported in md2" % (info.faces, MD2_MAX_TRIANGLES))
			return {'CANCELLED'}
		if info.vertices > MD2_MAX_VERTS:
			self.report({'ERROR'},
			   "Object has too many vertices (%i), at most %i are supported in md2" % (info.vertices, MD2_MAX_VERTS))
			return {'CANCELLED'}
		if info.frames > MD2_MAX_FRAMES:
			self.report({'ERROR'},
			   "There are too many frames (%i), at most %i are supported in md2" % (info.frames, MD2_MAX_FRAMES))
			return {'CANCELLED'}

		wm = context.window_manager
		wm.fileselect_add(self)
		return {'RUNNING_MODAL'}

def menuCB(self, context):
	self.layout.operator(Export_MD2.bl_idname, text="Quake II's MD2 (.md2)")

def register():
	bpy.utils.register_module(__name__)
	bpy.types.INFO_MT_file_export.append(menuCB)

def unregister():
	bpy.utils.unregister_module(__name__)
	bpy.types.INFO_MT_file_export.remove(menuCB)

if __name__ == "__main__":
	register()
