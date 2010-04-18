#!BPY

"""
Name: 'MD2 -new- (.md2)...'
Blender: 245
Group: 'Export'
Tooltip: 'Export to Quake2 file format (.md2) -new-'
"""

__version__ = '0.1'
__author__  = [ 'Damien Thébault', 'Erwan Mathieu' ]
__url__     = [ 'https://metabolsim.no-ip.org/trac/metabolsim' ]
__email__   = [ 'Damien Thébault <damien!thebault#laposte!net>', 'Erwan Mathieu <cyberwan#laposte!net>' ]

__bpydoc__ = """\
This script exports a mesh object to the Quake2 "MD2" file format.

If you want to choose the name of the animations, you have to go to the timeline, choose the starting frame, choose 'Frame', 'Add Marker' (M) and then set a name with 'Frame', 'Name Marker' (Ctrl M).

The frames are named <frameName><N> with :<br>
 - <N> the frame number<br>
 - <frameName> the name choosen at the last marker (or 'frame' if the last marker has no name or if there is no last marker)

Skins are set using image textures in materials. If a skin path is too long (>63 chars), the basename will be used.

Usually, importers expect that the name don't have any digit (to be able to detect where the frame name starts and where the frame number starts.

Thanks to Bob Holcomb for MD2_NORMALS taken from his exporter.<br>
Thanks to David Henry for the documentation about the MD2 file format.
"""

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
import struct
import random

# TODO : if mode is 'solid', separate vertices and use the face's normal
# TODO : save skins

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
	def __init__(self, anim, basename):
		self.anim = anim
		self.basename = basename
		self.object = None
		return

	def setObject(self, object):
		self.object = object

	def write(self, filename):
		self.ident = 'IDP2'
		self.version = 8

		self.skinwidth = 2**10-1
		self.skinheight = 2**10-1

		# self.framesize : see below

		mesh = self.object.getData(mesh=True)

		skins = Util.getSkins(mesh)

		self.num_skins = len(skins)
		self.num_xyz = len(mesh.verts)
		self.num_st = len(mesh.faces)*3
		self.num_tris = len(mesh.faces)
		self.num_glcmds = self.num_tris * (1+3*3) + 1
		if self.anim:
			self.num_frames = 1 + Blender.Get('endframe') \
			                    - Blender.Get('staframe')
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
			bin = struct.pack('<4s16i',
			                  self.ident,
			                  self.version,
			                  self.skinwidth,
			                  self.skinheight,
			                  self.framesize,
			                  self.num_skins,
			                  self.num_xyz,
			                  self.num_st,
			                  self.num_tris,
			                  self.num_glcmds,
			                  self.num_frames,
			                  self.ofs_skins,
			                  self.ofs_st,
			                  self.ofs_tris,
			                  self.ofs_frames,
			                  self.ofs_glcmds,
			                  self.ofs_end)
			file.write(bin) # header

			for skin in skins:
				if len(skin) > 63 or self.basename:
					skin = Blender.sys.basename(skin)
				bin = struct.pack('<64s', skin[0:63])
				file.write(bin) # skin name

			for face in mesh.faces:
				try:
					uvs = face.uv
				except:
					uvs = ([0,0],[0,0],[0,0])

				# (u,v) in blender -> (u,1-v)
				bin = struct.pack('<6h',
				                  uvs[0][0]*self.skinwidth,
				                  (1-uvs[0][1])*self.skinheight,
				                  uvs[1][0]*self.skinwidth,
				                  (1-uvs[1][1])*self.skinheight,
				                  uvs[2][0]*self.skinwidth,
				                  (1-uvs[2][1])*self.skinheight,
				                  )
				file.write(bin) # uv
				# (uv index is : face.index*3+i)

			for face in mesh.faces:
				# 0,2,1 for good cw/ccw
				bin = struct.pack('<3h',
				                  face.verts[0].index,
				                  face.verts[2].index,
				                  face.verts[1].index,
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
				for frame in range(1, self.num_frames+1):
					Blender.Set('curframe', frame)
					(min, max) = self.findMinMax(min, max)

				timeLine = Blender.Scene.GetCurrent().getTimeLine()
				frameNames = timeLine.getMarked()
				name = 'frame'
				for frame in range(1, self.num_frames+1):
					percent = (frame-Blender.Get('staframe')) / \
					          ( 1. + self.num_frames)
					Blender.Window.DrawProgressBar(percent,
					           str(int(percent*100)) + '%' + \
					           ' : frame ' + str(frame))
					Blender.Set('curframe', frame)
					if frame in frameNames:
						name = frameNames[frame][0]
						if name == '':
							name = 'frame'
					self.outFrame(file, min, max, name + str(frame))
			else:
				(min, max) = self.findMinMax()
				self.outFrame(file, min, max)

			# gl commands
			for face in mesh.faces:
				try:
					uvs = face.uv
				except:
					uvs = ([0,0],[0,0],[0,0])
				bin = struct.pack('<i', 3)
				file.write(bin)
				# 0,2,1 for good cw/ccw (also flips/inverts normal)
				for vert in [0,2,1]:
					# (u,v) in blender -> (u,1-v)
					bin = struct.pack('<ffI',
						uvs[vert][0],
						(1.0 - uvs[vert][1]),
						face.verts[vert].index)
					file.write(bin)
			# NULL command
			bin = struct.pack('<I', 0)
			file.write(bin)
		finally:
			file.close()

	def findMinMax(self, min=None, max=None):
		mesh = Blender.Mesh.New()
		mesh.getFromObject(self.object.name)
		mesh.transform(self.object.getMatrix())
		mesh.transform(Blender.Mathutils.RotationMatrix(90, 4, 'z')) # Hoehrer: rotate 90 degrees

		if min == None:
			min = [mesh.verts[0].co[0],
			       mesh.verts[0].co[1],
			       mesh.verts[0].co[2]]
		if max == None:
			max = [mesh.verts[0].co[0],
			       mesh.verts[0].co[1],
			       mesh.verts[0].co[2]]

		for vert in mesh.verts:
			for i in range(3):
				if vert.co[i] < min[i]:
					min[i] = vert.co[i]
				if vert.co[i] > max[i]:
					max[i] = vert.co[i]

		return (min, max)

	def outFrame(self, file, min, max, frameName = 'frame'):
		mesh = Blender.Mesh.New()
		mesh.getFromObject(self.object.name)
		mesh.transform(self.object.getMatrix())
		mesh.transform(Blender.Mathutils.RotationMatrix(90, 4, 'z')) # Hoehrer: rotate 90 degrees

		bin = struct.pack('<6f16s',
		                  self.object.getSize()[0]*(max[0]-min[0])/255.,
		                  self.object.getSize()[1]*(max[1]-min[1])/255.,
		                  self.object.getSize()[2]*(max[2]-min[2])/255.,
		                  min[0],
		                  min[1],
		                  min[2],
		                  frameName)
		file.write(bin) # frame header
		for vert in mesh.verts:
			for i in range(162):
				dot =  vert.no[1]*MD2_NORMALS[i][0] + \
				      -vert.no[0]*MD2_NORMALS[i][1] + \
				       vert.no[2]*MD2_NORMALS[i][2]
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

	@staticmethod
	def duplicateObject(object, name):
		object.select(True)
		# duplicate selected object
		Blender.Object.Duplicate(mesh=True)
		# select the duplicated object
		object = Blender.Object.GetSelected()[0]
		object.setName(name)
		return object

	@staticmethod
	def getTriangMesh(object):
		mesh = object.getData(mesh=True)
		mesh.sel = True
		mesh.quadToTriangle()
		return mesh

	@staticmethod
	def getSkins(mesh):
		skins = []
		for material in mesh.materials:
			for texture in material.getTextures():
				if texture != None and texture.tex != None:
					if texture.tex.getType() == "Image":
						imgfile = texture.tex.getImage().getFilename()[2:]
						skins.append(imgfile)
		return skins

class ObjectInfo:
	def __init__(self, object):
		self.triang = False
		self.verts = -1
		self.faces = -1
		self.status = ('','')

		self.ismesh = object and object.getType() == 'Mesh'

		if self.ismesh:
			originalObject = object

			mesh = object.getData(mesh=True)

			self.skins = Util.getSkins(mesh)

			for face in mesh.faces:
				if len(face) == 4:
					self.triang = True
					break

			tmpObjectName = Util.pickName()
			try:
				if self.triang:
					object = Util.duplicateObject(object, tmpObjectName)
					mesh = Util.getTriangMesh(object)

				self.status = (str(len(mesh.verts)) + ' vertices',
							str(len(mesh.faces)) + ' faces')
			finally:
				if object.getName() == tmpObjectName:
					originalObject.select(True)
					Blender.Scene.GetCurrent().objects.unlink(object)

class GUI:
	id_export   = 1
	id_cancel   = 2
	id_anim     = 3
	id_update   = 4
	id_help     = 5
	id_basename = 6
	val_anim     = True
	val_basename = True

	str_export = 'Export to MD2...'

	def __init__(self):
		try:
			self.object = Blender.Object.GetSelected()[0]
		except:
			self.object = None

		self.info = ObjectInfo(self.object)

	def draw(self):
		(width, height) = Blender.Window.GetAreaSize()

		# export - cancel - help
		eWidth = Blender.Draw.GetStringWidth(self.str_export)+20
		if self.info.ismesh:
			Blender.Draw.PushButton(self.str_export, self.id_export,
			                        width-eWidth-11, height-37, eWidth, 21)

		Blender.Draw.PushButton('Cancel', self.id_cancel,
		                        width-eWidth-11, height-37-24, eWidth, 21)

		Blender.Draw.PushButton('Help', self.id_help,
		                        width-eWidth-11, height-100, eWidth, 21)

		# *n* frames - export animation
		left    = width-eWidth-140
		farLeft = left-120
		nframes = 1+Blender.Get('endframe')-Blender.Get('staframe')
		Blender.Draw.Label(str(nframes)+' frames',
		                   farLeft, height-100, left-40-farLeft, 20)
		if nframes <= 512:
			Blender.Draw.Toggle('Export animation',
			                    self.id_anim, left-40, height-100, 140, 20,
			                    self.val_anim)
		else:
			self.val_anim = False
			Blender.Draw.Label('Too many frames!',
			                   left, height-100, 100, 20)
			Blender.Draw.Label('(' + str(nframes) + ', max : 512)',
			                   left, height-115, 100, 20)
		# *n* skins - use full path
		skinstatus = str(len(self.info.skins))+' skin'
		if len(self.info.skins) > 1:
			skinstatus += 's'
		Blender.Draw.Label(skinstatus,
		                   farLeft, height-120, left-40-farLeft, 20)
		Blender.Draw.Toggle('Export only basenames',
		                    self.id_basename, left-40, height-120, 140, 20,
		                    self.val_basename)
		# object : *name* - update
		if self.object != None:
			Blender.Draw.Label('Object : ' + self.object.getName(),
			                   farLeft, height-150, left-farLeft, 20)
		else:
			Blender.Draw.Label('Object : None',
			                   farLeft, height-150, left-farLeft, 20)
		if self.info.ismesh:
			if self.info.triang:
				status = '- Will be triangulated'
			else:
				status = '- No need to triangulate'
			Blender.Draw.Label(status,
			                   left, height-150, width-left, 20)

			for i in range(len(self.info.status)):
				Blender.Draw.Label('- ' + self.info.status[i],
				                   left, height-150-(i+1)*15,
				                   width-left, 20)

		else:
			status = 'Not a mesh!'
			Blender.Draw.Label(status,
			                   width-Blender.Draw.GetStringWidth(status)-40,
			                   height-37, eWidth, 21)

	def button_event(self, evt):
		if evt == self.id_export:
			Blender.Window.FileSelector(self.export, self.str_export,
			                            Blender.sys.makename(ext='.md2'))
		if evt == self.id_anim:
			self.val_anim = not self.val_anim
		if evt == self.id_basename:
			self.val_basename = not self.val_basename
		if evt == self.id_cancel:
			Blender.Draw.Exit()
		if evt == self.id_update:
			updateGuiObject()
			Blender.Draw.Redraw()
		if evt == self.id_help:
			Blender.ShowHelp('export_md2_new.py')

	def export(self, filename):

		if Blender.sys.exists(filename) == 1:
			overwrite = Blender.Draw.PupMenu('File already exists, overwrite?%t|Yes%x1|No%x0')
			if overwrite == 0:
				return

		Blender.Window.WaitCursor(True)

		object = self.object
		originalObject = object

		if object.getType() != 'Mesh':
			raise NameError, 'Selected object must be a mesh!'

		# different name each time or we can't unlink it later
		tmpObjectName = Util.pickName()

		mesh = object.getData(mesh=True)

		if self.info.triang:
			object = Util.duplicateObject(object, tmpObjectName)
			mesh = Util.getTriangMesh(object)

		if self.val_anim:
			frame = Blender.Get('curframe')

		try:
			md2 = MD2(self.val_anim, self.val_basename)
			md2.setObject(object)
			md2.write(filename)
		finally:
			if object.getName() == tmpObjectName:
				originalObject.select(True)
				Blender.Scene.GetCurrent().objects.unlink(object)
			if self.val_anim:
				Blender.Set('curframe', frame)
			Blender.Window.DrawProgressBar(1.0, '')
			Blender.Window.WaitCursor(False)
			Blender.Draw.Exit()


if __name__ == '__main__':
	gui = GUI()
	Blender.Draw.Register(gui.draw, None, gui.button_event)
