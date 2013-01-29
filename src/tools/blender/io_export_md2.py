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

#New Header Data
bl_info = {
    "name": "Quake2 MD2 format",
    "description": "Export to Quake2 file format for UFO:AI (.md2)",
    "author": "DarkRain, based on the work of Dao Nguyen, Bernd Meyer, metaio GmbH, and Damien Thebault and Erwan Mathieu",
    "version": (1, 0),
    "blender": (2, 63, 0),
    "location": "File > Export > Quake2 MD2",
    "warning": '', # used for warning icon and text in addons panel
    "support": 'COMMUNITY',
    "category": "Import-Export"}


import bpy
from bpy.props import *

from bpy_extras.io_utils import ExportHelper
from datetime import datetime

import math
from math import pi
import mathutils

import struct
import random
import os


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
	def __init__(self, anim):
		self.anim = anim
		self.object = None
		self.progressBarDisplayed = -10
		return

	def setObject(self, object):
		self.object = object

	def write(self, filename):
		self.ident = 'IDP2'
		self.version = 8

		self.skinwidth = 2**10-1 #1023
		self.skinheight = 2**10-1 #1023

		# self.framesize : see below

		mesh = self.object.data

		skins = Util.getSkins(mesh)

		self.num_skins = len(skins)
		self.num_xyz = len(mesh.vertices)
		self.num_st = len(mesh.polygons)*3
		self.num_tris = len(mesh.polygons)
		self.num_glcmds = self.num_tris * (1+3*3) + 1
		if self.anim:
			self.num_frames = 1 + bpy.context.scene.frame_end - bpy.context.scene.frame_start

		else:
			self.num_frames = 1

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
			bin = struct.pack('<4B16i',
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
			file.write(bin)

			# write skin file names
			for skin in skins:
				if len(skin) > 63:
					print("WARNING: The texture name '"+skin+"' is too long. It is automatically truncated.")

				bin = struct.pack('<64s', bytes(skin[0:63], encoding='utf8'))
				file.write(bin) # skin name


			#define meshTextureFaces
			if len(mesh.uv_layers) != 0:
				meshUVLayer = mesh.uv_layers[0].data
				for face in mesh.polygons:
					for loopIndex in face.loop_indices:
						try:
							uvs = meshUVLayer[loopIndex].uv
						except:
							uvs = [0,0]

						# (u,v) in blender -> (u,1-v)
						bin = struct.pack('<2h',
										  int(uvs[0]*self.skinwidth),
										  int((1-uvs[1])*self.skinheight)
										  )
						file.write(bin) # uv
						# (uv index is : face.index*3+i)

			for face in mesh.polygons:
				# 0,2,1 for good cw/ccw
				bin = struct.pack('<3h',
				                  face.vertices[0],
				                  face.vertices[2],
				                  face.vertices[1]
				                )
				file.write(bin) # vert index
				bin = struct.pack('<3h',
				                  face.index*3 + 0,
				                  face.index*3 + 2,
				                  face.index*3 + 1,
				                  )

				file.write(bin) # uv index

			if self.anim:
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
					markerFrame = timeLineMarkers[len(timeLineMarkers)-1].frame
					for i in range(len(timeLineMarkers)-2, -1, -1):
						if timeLineMarkers[i].frame == markerFrame:
							del timeLineMarkers[i]
						else:
							markerFrame = timeLineMarkers[i].frame

				for frame in range(bpy.context.scene.frame_start , bpy.context.scene.frame_end + 1):

					#Display the progress status of the exportation in the console
					#problem: "Carriage return" doesn't function properly with python 3.x
					progressStatus = math.floor((frame-bpy.context.scene.frame_start)/(self.num_frames+1)*100)
					progressStatusString = ("Exportation progress: "+str((progressStatus//10)*10)+"%")
					if progressStatus - self.progressBarDisplayed >= 10:
						print(progressStatusString)
					self.progressBarDisplayed = (progressStatus//10)*10
					if frame == self.num_frames:
						print("Exportation progress: 100% - model exported")

					bpy.context.scene.frame_set(frame)
					(min, max) = self.findMinMax()

					if len(timeLineMarkers) != 0:
						if markerIdx + 1 != len(timeLineMarkers):
							if frame >= timeLineMarkers[markerIdx + 1].frame:
								markerIdx += 1
						name = timeLineMarkers[markerIdx].name
					else:
						name = 'frame'

					self.outFrame(file, min, max, name + str(frame))
			else:
				(min, max) = self.findMinMax()
				self.outFrame(file, min, max)

			# gl commands
			meshUVLayer = mesh.uv_layers[0].data
			for face in mesh.polygons:
				bin = struct.pack('<i', 3)
				file.write(bin)
				# 0,2,1 for good cw/ccw (also flips/inverts normal)
				for vert in [0,2,1]:
					try:
						uvs = meshUVLayer[face.loop_indices[vert]].uv
					except:
						uvs = [0,0]
					# (u,v) in blender -> (u,1-v)
					bin = struct.pack('<ffI',
						uvs[0],
						(1.0 - uvs[1]),
						face.vertices[vert])

					file.write(bin)
			# NULL command
			bin = struct.pack('<I', 0)
			file.write(bin)
		finally:
			file.close()

	def findMinMax(self, min=None, max=None):
		mesh = self.object.to_mesh(bpy.context.scene, True, 'PREVIEW')

		mesh.transform(self.object.matrix_world)
		mesh.transform(mathutils.Matrix.Rotation(pi/2, 4, 'Z')) # Hoehrer: rotate 90 degrees

		if min == None:
			min = [mesh.vertices[0].co[0],
			       mesh.vertices[0].co[1],
			       mesh.vertices[0].co[2]]
		if max == None:
			max = [mesh.vertices[0].co[0],
			       mesh.vertices[0].co[1],
			       mesh.vertices[0].co[2]]

		for vert in mesh.vertices:
			for i in range(3):
				if vert.co[i] < min[i]:
					min[i] = vert.co[i]
				if vert.co[i] > max[i]:
					max[i] = vert.co[i]

		return (min, max)

	def outFrame(self, file, min, max, frameName = 'frame'):
		mesh = self.object.to_mesh(bpy.context.scene, True, 'PREVIEW')

		mesh.transform(self.object.matrix_world)
		mesh.transform(mathutils.Matrix.Rotation(pi/2, 4, 'Z')) # Hoehrer: rotate 90 degrees

		bin = struct.pack('<6f16s',
			self.object.scale[0]*(max[0]-min[0])/255.,
			self.object.scale[1]*(max[1]-min[1])/255.,
			self.object.scale[2]*(max[2]-min[2])/255.,
			min[0],
			min[1],
			min[2],
			bytes(frameName, encoding='utf8'))

		file.write(bin) # frame header
		for vert in mesh.vertices:
			for i in range(162):
				dot =  vert.normal[1]*MD2_NORMALS[i][0] + \
				      -vert.normal[0]*MD2_NORMALS[i][1] + \
				       vert.normal[2]*MD2_NORMALS[i][2]
				if (i==0) or (dot > maxDot):
					maxDot = dot
					bestNormalIndex = i

			bin = struct.pack('<4B',
			                  int((vert.co[0]-min[0])/(max[0]-min[0])*255.),
			                  int((vert.co[1]-min[1])/(max[1]-min[1])*255.),
			                  int((vert.co[2]-min[2])/(max[2]-min[2])*255.),
			                  bestNormalIndex)

			file.write(bin) # vertex

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
		return mesh

	@staticmethod
	def getSkins(mesh):
		skins = []
		for material in mesh.materials:
			for texSlot in material.texture_slots:
				if texSlot != None:
					if texSlot.texture.type == "IMAGE":
						# get the image path and convert it to a relative one (if it is not already a relative path)
						texname = texSlot.texture.image.name
						skins.append(texname)
		return skins

class ObjectInfo:
	def __init__(self, object):
		self.triang = False
		self.vertices = -1
		self.faces = -1
		self.status = ('','')

		self.ismesh = object and object.type == 'MESH'

		if self.ismesh:
			originalObject = object
			mesh = object.data

			self.skins = Util.getSkins(mesh)

			for face in mesh.polygons:
				if len(face.vertices) == 4:
					self.triang = True
					break

			tmpObjectName = Util.pickName()
			try:
				if self.triang:
					object = Util.duplicateObject(object, tmpObjectName)
					mesh = Util.triangulateMesh(object)

				self.status = (str(len(mesh.vertices)) + ' vertices', str(len(mesh.polygons)) + ' faces')

			finally:
				if object.name == tmpObjectName:
					originalObject.select = True
					bpy.context.scene.objects.active = originalObject
					Util.deleteObject(object)

class Export_MD2(bpy.types.Operator, ExportHelper):
	"""Export to Quake2 MD2 file format for UFO:AI (.md2)"""
	bl_idname = "export_ufoai.md2"
	bl_label = "Export to Quake2 MD2 file format for UFO:AI (.md2)"

	filename = StringProperty(name="File Path",
		description="Filepath used for processing the script",
		maxlen= 1024,default= "")

	filename_ext = ".md2"

	val_anim = BoolProperty(name="Export animation",
							description="default: True",
							default=True)

	# id_export   = 1
	# id_cancel   = 2
	# id_anim     = 3
	# id_update   = 4
	# id_help     = 5

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

		if len(bpy.context.selected_objects[:]) == 0:
			raise NameError('Please, select one object!')

		if len(bpy.context.selected_objects[:]) > 1:
			raise NameError('Please, select one and only one object!')


		object = self.object
		originalObject = object

		if object.type != 'MESH':
			raise NameError('Selected object must be a mesh!')

		# different name each time or we can't unlink it later
		tmpObjectName = Util.pickName()
		mesh = object.data

		if self.info.triang:
			object = Util.duplicateObject(object, tmpObjectName)
			mesh = Util.triangulateMesh(object)

		if self.val_anim:
			frame =bpy.context.scene.frame_current

		try:
			md2 = MD2(self.val_anim)
			md2.setObject(object)
			md2.write(filepath)
		finally:
			if object.name == tmpObjectName:
				originalObject.select = True
				bpy.context.scene.objects.active = originalObject
				Util.deleteObject(object);
			if self.val_anim:
				bpy.context.scene.frame_set(frame)

			self.report({'INFO'},  "Model '"+originalObject.name+"' exported")
		return {'FINISHED'}

	def invoke(self, context, event):
		wm = context.window_manager
		wm.fileselect_add(self)
		return {'RUNNING_MODAL'}


def menuCB(self, context):
	self.layout.operator(Export_MD2.bl_idname, text="Quake2 MD2 (.md2)")

def register():
	bpy.utils.register_module(__name__)
	bpy.types.INFO_MT_file_export.append(menuCB)

def unregister():
	bpy.utils.unregister_module(__name__)
	bpy.types.INFO_MT_file_export.remove(menuCB)

if __name__ == "__main__":
	register()
