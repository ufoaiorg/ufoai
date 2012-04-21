#!BPY

"""
Name: 'MD2 tags (.tag)'
Blender: 244
Group: 'Export'
Tooltip: 'Export to Quake2 tag file format for UFO:AI (.tag).'
"""

__author__ = 'Werner Hoehrer'
__version__ = '0.0.13'
__url__ = ["UFO: Alien Invasion, http://ufoai.sourceforge.net",
     "Support forum, http://ufoai.org/forum/", "blender", "ufoai"]
__email__ = ["Werner Hoehrer, bill_spam2:yahoo*de", "scripts"]
__bpydoc__ = """\
This script exports a Quake 2 tag file as used in UFO:AI (MD2 TAG).

This file format equals the _parts_ of the MD3 format used for animated tags, but nothing more. i.e tagnames+positions

Base code taken from the md2 exporter by Bob Holcomb. Many thanks.
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C): Werner Hoehrer
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
# --------------------------------------------------------------------------
# Changelog
# 	0.0.13	Destructavator fixed off-by-one frame bug
# 	0.0.12	Hoehrer	matrix transformation for rotation matrix seems to be ok now. Importing still needs to be fixed/synced though!
# 	0.0.11	Hoehrer	Some cleanup, updated status messages + played around a bit to get tag-rotation to work (with no success yet)
# 	0.0.10	Hoehrer	changed getLocation() to getLocation('worldspace').
# --------------------------------------------------------------------------

import Blender
from Blender import *
from Blender.Draw import *
from Blender.BGL import *
from Blender.Window import *
from Blender.Object import *
from Blender.Mathutils import *

import math
import struct, string
from types import *

######################################################
# BEGIN ----------------------------------------------
# Additional functions used by Ex- and Importer
######################################################

######################################################
# Returns the string from a null terminated string.
######################################################
def asciiz (s):
	n = 0
	while (ord(s[n]) != 0):
		n = n + 1
	return s[0:n]

######################################################
# Apply a matrix to a vert and return a tuple.
#	http://en.wikibooks.org/wiki/Blender_3D:_Blending_Into_Python/Cookbook#Apply_Matrix
######################################################
#def apply_transform(verts, matrix):
#	x, y, z = verts
#	return\
#	x*matrix[0][0] + y*matrix[1][0] + z*matrix[2][0] + matrix[3][0],\
#	x*matrix[0][1] + y*matrix[1][1] + z*matrix[2][1] + matrix[3][1],\
#	x*matrix[0][2] + y*matrix[1][2] + z*matrix[2][2] + matrix[3][2]

######################################################
# Get Angle between 3 points
#	Get the angle between line AB and BC where B is the elbo.
#	http://en.wikibooks.org/wiki/Blender_3D:_Blending_Into_Python/Cookbook#Get_Angle_between_3_points
#	Uses Blender.Mathutils.AngleBetweenVecs and Blender.Mathutils.Vector
######################################################
def getAng3pt3d(avec, bvec, cvec):
	avec, bvec, cvec = Vector(avec), Vector(bvec), Vector(cvec)

	try:
		ang = AngleBetweenVecs(avec - bvec,  cvec - bvec)
		if ang != ang:
			raise  "ERROR angle between Vecs"
		else:
			return ang
	except:
		print '\tAngleBetweenVecs failed, zero length?'
		return 0

######################################################
# Convert the three axes and the location into a transform-matrix. (dunno why the location is used at all?)
#
# Md3 tag matrix transform code (see also load_md2_tags) is balantly stolen from blender md3 importer :)
# Many thanks to PhaethonH, Bob Holcomb, Robert (Tr3B) Beckebans and further (www.gametutorials.com) DigiBen! and PhaethonH
######################################################
def MatrixSetupTransform(forward, left, up, origin):
	return [
		[forward[0],	forward[1],	forward[2],	origin[0]],
		[left[0],	left[1],	left[2],	origin[1]],
		[up[0],	up[1],	up[2],	origin[2]],
		[0.0,	0.0,		0.0,		1.0]
	]


######################################################
# Additional functions used by Ex- and Importer
# END ------------------------------------------------
######################################################

######################################################
# GUI Loader
######################################################

# Export globals
g_filename		= Create("default.tag") # default value is set here.
g_filename_search	= Create("")
g_scale			= Create(1.0)		# default value is set here.

# Events
EVENT_NOEVENT=1
EVENT_CHOOSE_FILENAME=2
EVENT_SAVE_MD2TAGS=3
EVENT_LOAD_MD2TAGS=4
EVENT_EXIT=100

######################################################
# MD2 TAG Constants
######################################################
MD2_MAX_TAGS=4096
MD2_MAX_FRAMES=512

######################################################
# Callbacks for Window functions
######################################################
def filename_callback(input_filename):
	global g_filename
	g_filename.val=input_filename

def draw_gui():
	global g_scale
	global g_filename
	global EVENT_NOEVENT,EVENT_SAVE_MD2TAGS,EVENT_CHOOSE_FILENAME,EVENT_EXIT, EVENT_LOAD_MD2TAGS

	########## Titles
	glClear(GL_COLOR_BUFFER_BIT)
	glRasterPos2d(10, 120)
	Text("MD2 TAGs Export & Import")

	######### Parameters GUI Buttons
	######### MD2 Filename text entry
	left = 10
	width = 200
	g_filename = String("File: ", EVENT_NOEVENT, left, 95, width-left, 18,
			g_filename.val, 255, "MD2 TAGs file to load/save (Import/Export).")
	########## MD2 TAGs File Search Button
	Button("Search",EVENT_CHOOSE_FILENAME, width, 95, 50, 18)

	########## Scale slider-default is 1/8 which is a good scale for md2->blender
	g_scale= Slider("Scale: ", EVENT_NOEVENT,
			left, 75, width-left, 18,
			g_scale.val, 0.001, 100.0, 0, "Scale factor for MD2 TAGs (Import/Export)");

	g_frames_info = Label('Frames: ' + str(1 + Blender.Get('endframe') - Blender.Get('staframe')),
			left, 55, width-left, 20)

	number_of_tags = num_tags()
	if (number_of_tags > 0):
		objects_info = 'Tags: ' + str(number_of_tags)
	else:
		objects_info = 'No tags (Empties) selected!'

	g_objects_info = Label(objects_info,
			left, 35, width-left, 20)

	######### Draw and Exit Buttons
	Button("Export",EVENT_SAVE_MD2TAGS , 10, 10, 80, 18)
	Button("Import",EVENT_LOAD_MD2TAGS , 90, 10, 80, 18)
	Button("Exit",EVENT_EXIT , 170, 10, 80, 18)

def event(evt, val):
	if (evt == QKEY and not val):
		Exit()

def bevent(evt):
	global g_filename
	global EVENT_NOEVENT,EVENT_SAVE_MD2TAGS,EVENT_EXIT,EVENT_LOAD_MD2TAGS

	######### Manages GUI events
	if (evt==EVENT_EXIT):
		Blender.Draw.Exit()
	elif (evt==EVENT_CHOOSE_FILENAME):
		FileSelector(filename_callback, "MD2 TAG File Selection")
	elif (evt==EVENT_SAVE_MD2TAGS):
		if (g_filename.val == "model"):
			save_md2_tags("blender.tag")
			Blender.Draw.Exit()
			return
		else:
			save_md2_tags(g_filename.val)
			Blender.Draw.Exit()
			return
	elif (evt==EVENT_LOAD_MD2TAGS):
		if (g_filename.val == "model"):
			load_md2_tags("blender.tag")
			Blender.Draw.Exit()
			return
		else:
			load_md2_tags(g_filename.val)
			Blender.Draw.Exit()
			return

Register(draw_gui, event, bevent)


######################################################
# MD2 TAG data structures
######################################################
class md2_tagname:
	name=""
	binary_format="<64s"
	def __init__(self, string):
		self.name=string
	def getSize(self):
		return struct.calcsize(self.binary_format)
	def save(self, file):
		temp_data=self.name
		data=struct.pack(self.binary_format, temp_data)
		file.write(data)
	def load(self, file):
		temp_data = file.read(self.getSize())
		data=struct.unpack(self.binary_format, temp_data)
		self.name=asciiz(data[0])
		return self
	def dump (self):
		print "MD2 tagname"
		print "tag: ",self.name
		print ""

class md2_tag:
	####################################
	# Save-function in MD2 exporter:
	#	x1=-mesh.verts[vert_counter].no[1]
	#	y1=mesh.verts[vert_counter].no[0]
	#	z1=mesh.verts[vert_counter].no[2]
	# => md2file(x,y,z) = blender(-y,x,z)
	# Load-function in MD2 importer:
	#	mesh.verts[j].co[0]=y
	#	mesh.verts[j].co[1]=-x
	#	mesh.verts[j].co[2]=z
	# => blender(x,y,z) = md2file(y,-x,z)
	####################################
	global g_scale
	axis1	= []
	axis2	= []
	axis3	= []
	origin	= []

	binary_format="<12f"	#little-endian (<), 12 floats (12f)	| See http://docs.python.org/lib/module-struct.html for more info.


	def __init__(self):
		origin	= Mathutils.Vector(0.0, 0.0, 0.0)
		axis1	= Mathutils.Vector(0.0, 0.0, 0.0)
		axis2	= Mathutils.Vector(0.0, 0.0, 0.0)
		axis3	= Mathutils.Vector(0.0, 0.0, 0.0)

	def getSize(self):
		return struct.calcsize(self.binary_format)

	# Function + general data-structure stolen from https://intranet.dcc.ufba.br/svn/indigente/trunk/scripts/md3_top.py
	# Kudos to Bob Holcomb and Damien McGinnes
	def foo(self, vet):
		for i in xrange(0,3):
			vet[i] = float(vet[i])
		return vet

	def save(self, file):
		# Prepare temp data for export with transformed axes (matrix transform)
		temp_data=(
		# rotation matrix
		float(self.axis2[1]), -float(self.axis2[0]), -float(self.axis2[2]),
		float(-self.axis1[1]), float(self.axis1[0]), float(self.axis1[2]),
		float(-self.axis3[1]), float(self.axis3[0]), float(self.axis3[2]),
		# position
		float(-self.origin[1]), float(self.origin[0]), float(self.origin[2])
		)

		# Apply scale to tempdata if it was set in the dialog.
		if (g_scale.val != 1.0):
			temp_data[9] = temp_data[9] * g_scale.val
			temp_data[10] = temp_data[10] * g_scale.val
			temp_data[11] = temp_data[11] * g_scale.val

		# Prepare serialised data for writing.
		data=struct.pack(self.binary_format,
			temp_data[0], temp_data[1], temp_data[2],
			temp_data[3], temp_data[4], temp_data[5],
			temp_data[6], temp_data[7], temp_data[8],
			temp_data[9], temp_data[10], temp_data[11])

		# Write it
		file.write(data)

	def load(self, file):
		# Read data from file with the defined size.
		temp_data = file.read(self.getSize())

		# De-serialize the data.
		data = struct.unpack(self.binary_format, temp_data)

		# Parse data from file into pythin md2 structure
		self.axis1	= (data[0], data[1], data[2])
		self.axis2	= (data[3], data[4], data[5])
		self.axis3	= (data[6], data[7], data[8])
		self.origin	= (data[9], data[10], data[11])

		# Convert the orientation of the axes to the blender format.
		# TODO: fix transform to match the one for saving!
		self.origin = (self.origin[1], -self.origin[0], self.origin[2])
		#self.axis1 = (self.axis1[1], -self.axis1[0], self.axis1[2])
		#self.axis2 = (self.axis2[1], -self.axis2[0], self.axis2[2])
		#self.axis3 = (self.axis3[1], -self.axis3[0], self.axis3[2])

		# Apply scale to imported data if it was set in the dialog.
		if (g_scale.val != 1.0):
			self.origin = (
				self.origin[0] * g_scale.val,
				self.origin[1] * g_scale.val,
				self.origin[2] * g_scale.val
				)

		return self

	def dump(self):
		print "MD2 Point Structure"
		print "origin: ", self.origin[0], " ",self.origin[1] , " ",self.origin[2]
		print "axis1:  ", self.axis1[0], " ",self.axis1[1] , " ",self.axis1[2]
		print "axis2:  ", self.axis2[0], " ",self.axis2[1] , " ",self.axis2[2]
		print "axis3:  ", self.axis3[0], " ",self.axis3[1] , " ",self.axis3[2]
		print ""


class md2_tags_obj:
	#Header Structure
	ident = 0		#int 0	This is used to identify the file
	version = 0		#int 1	The version number of the file (Must be 1)
	num_tags = 0		#int 2	The number of skins associated with the model
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
			print "Saving tag: ", tag_count

			for tag in tag_frames:
				frame_count += 1
				tag.save(file)
			print "  Frames saved: ", frame_count, "(",self.num_frames, ")"
	def load(self, file):
		temp_data = file.read(self.getSize())
		#print "TEST", temp_data# DEBUG
		data = struct.unpack(self.binary_format, temp_data)

		self.ident = data[0]
		self.version = data[1]

		if (self.ident!=844121162 or self.version!=1):
			print "Not a valid MD2 TAG file"
			Exit()

		self.num_tags		= data[2]
		self.num_frames		= data[3]
		self.offset_names	= data[4]
		#self.offset_tags	= temp_data[5]	# TODO: why was temp_data used here?
		#self.offset_end	= temp_data[6]	# TODO: why was temp_data used here?
		self.offset_tags	= data[5]
		self.offset_end		= data[6]
		self.offset_extract_end	= data[7]

		self.dump()

		# Read names data
		for tag_counter in range(0,self.num_tags):
			temp_name = md2_tagname("")
			self.names.append(temp_name.load(file))
		print "Reading of ", tag_counter, " names finished."

		for tag_counter in range(0,self.num_tags):
			frames = []
			for frame_counter in range(0,self.num_frames):
				temp_tag = md2_tag()
				temp_tag.load(file)
				#if (frame_counter < 10): #DEBUG
				frames.append(temp_tag)
			self.tags.append(frames)
			#for frame_counter in range(0,10):	#DEBUG
			#	self.tags[tag_counter][frame_counter].dump()	#DEBUG

			print " Reading of ", frame_counter+1, " frames finished."
		print "Reading of ", tag_counter+1, " framelists finished."
		return self
	def dump (self):
		print "Header Information"
		print "ident: ",		self.ident
		print "version: ",		self.version
		print "number of tags: ",	self.num_tags
		print "number of frames: ",	self.num_frames
		print "offset names: ",		self.offset_names
		print "offset tags: ",		self.offset_tags
		print "offset end: ",		self.offset_end
		print "offset extract end: ",	self.offset_extract_end
		print ""

######################################################
# Fill MD2 data structure
######################################################
def fill_md2_tags(md2_tags, object):
	global g_scale
	Blender.Window.DrawProgressBar(0.0, "Filling MD2 Data")

	# Set header information.
	md2_tags.ident=844121162
	md2_tags.version=1
	md2_tags.num_tags+=1

	# Add a name node to the tagnames data structure.
	md2_tags.names.append(md2_tagname(object.name))	# TODO: cut to 64 chars

	# Add a (empty) list of tags-positions (for each frame).
	tag_frames = []

	progress = 0.0
	progressIncrement = 1.0 / md2_tags.num_frames

	# Store currently set frame
	previous_curframe = Blender.Get("curframe")

	frame_counter = 0
	# Fill in each tag with its positions per frame
	print "Blender startframe:", Blender.Get('staframe')
	print "Blender endframe:", Blender.Get('endframe')
	for current_frame in range(Blender.Get('staframe') , Blender.Get('endframe') + 1):
		#print current_frame, "(", frame_counter, ")" # DEBUG
		progress+=progressIncrement
		Blender.Window.DrawProgressBar(progress, "Tag:"+ str(md2_tags.num_tags) +" Frame:" + str(frame_counter))

		#add a frame
		tag_frames.append(md2_tag())

		#set blender to the correct frame (so the objects have their new positions)
		Blender.Set("curframe", current_frame - 1)

		# Set first coordiantes to the location of the empty.
		tag_frames[frame_counter].origin = object.getLocation('worldspace')
		# print tag_frames[frame_counter].origin[0], " ",tag_frames[frame_counter].origin[1]," ",tag_frames[frame_counter].origin[2] # Useful for DEBUG (slowdown!)

		matrix = object.getMatrix('worldspace')
		tag_frames[frame_counter].axis1 = (matrix[0][0], matrix[0][1], matrix[0][2])
		tag_frames[frame_counter].axis2 = (matrix[1][0], matrix[1][1], matrix[1][2])
		tag_frames[frame_counter].axis3 = (matrix[2][0], matrix[2][1], matrix[2][2])
		frame_counter += 1

	# Restore curframe from before the calculation.
	Blender.Set("curframe", previous_curframe)

	md2_tags.tags.append(tag_frames)

######################################################
# Count the available tags
######################################################
def num_tags():
	mesh_objs = Blender.Object.GetSelected()
	num_tags = 0
	for object in mesh_objs:
		if object.getType()=="Empty":
			num_tags += 1

	return num_tags

######################################################
# Save MD2 TAGs Format
######################################################
def save_md2_tags(filename):
	print ""
	print "***********************************"
	print "MD2 TAG Export"
	print "***********************************"
	print ""

	Blender.Window.DrawProgressBar(0.0,"Beginning MD2 TAG Export")

	md2_tags=md2_tags_obj()	# Blank md2 object to be saved.

	#get the object
	mesh_objs = Blender.Object.GetSelected()

	#check there is a blender object selected
	if len(mesh_objs)==0:
		print "Fatal Error: Must select at least one 'Empty' to output a tag file"
		print "Found nothing"
		result=Blender.Draw.PupMenu("Must select an object to export%t|OK")
		return

	# Set frame number.
	md2_tags.num_frames = 1 + Blender.Get('endframe') - Blender.Get('staframe')

	print "Frames to export: ",md2_tags.num_frames
	print ""

	for object in mesh_objs:
		#check if it's an "Empty" mesh object
		if object.getType() != "Empty":
			print "Ignoring non-'Empty' object: ", object.getType()
		else:
			print "Found Empty: ",object.name
			fill_md2_tags(md2_tags, object)

	Blender.Window.DrawProgressBar(0.0, "Writing to Disk")

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

	print ""
	md2_tags.dump()

	# Actually write it to disk.
	file_export=open(filename,"wb")
	md2_tags.save(file_export)
	file_export.close()

	# Cleanup
	md2_tags=0

	Blender.Window.DrawProgressBar(1.0,"") # clear progressbar
	print "Done Writing. Closed the file"

######################################################
# Load&animate MD2 TAGs Format
######################################################
def load_md2_tags(filename):
	print ""
	print "***********************************"
	print "MD2 TAG Import"
	print "***********************************"
	print ""

	Blender.Window.DrawProgressBar(0.0,"Beginning MD2 TAG Import")

	md2_tags=md2_tags_obj()	# Blank md2 object to load to.

	print "Opening the file ..."
	Blender.Window.DrawProgressBar(0.1, "Loading from Disk")
	file_import=open(filename,"rb")
	md2_tags.load(file_import)
	file_import.close()
	print "Closed the file."

	print ""
	md2_tags.dump()

	# Get current Scene.
	scene = Scene.getCurrent()

	Blender.Window.DrawProgressBar(0.2, "Reading md2 tag data")

	progress=0.3
	progressIncrement=0.6 / (md2_tags.num_frames * md2_tags.num_tags)

	# Store currently set frame
	previous_curframe = Blender.Get("curframe")

	for tag in range(0,md2_tags.num_tags):
		# Get name & frame-data of current tag.
		tag_name = md2_tags.names[tag]
		tag_frames = md2_tags.tags[tag]

		# Create object.
		blender_tag = Object.New('Empty')
		blender_tag.setName(tag_name.name)

		# Activate name-visibility for this object.
		blender_tag.setDrawMode(blender_tag.getDrawMode() | 8)   # 8="drawname"

		# Link Object to current Scene
		scene.link(blender_tag)

		for frame_counter in range(0,md2_tags.num_frames):
		#for frame_counter in range(0,10): #DEBUG
			progress += progressIncrement
			Blender.Window.DrawProgressBar(progress, "Calculating Frame "+str(frame_counter)+" | "+tag_name.name)
			#set blender to the correct frame (so the objects' data will be set in this frame)
			Blender.Set("curframe", frame_counter+1)
			#Blender.Set("curframe", frame_counter) # use this if imported emties do not match up with imported md2 files

			# Note: Quake3 uses left-hand geometry
			origin	= tag_frames[frame_counter].origin
			forward	= tag_frames[frame_counter].axis1
			left	= tag_frames[frame_counter].axis2
			up	= tag_frames[frame_counter].axis3

			transform = MatrixSetupTransform(forward, left, up, origin)
			##print transform #DEBUG
			transform2 = Blender.Mathutils.Matrix(transform[0], transform[1], transform[2], transform[3])
			blender_tag.setMatrix(transform2)

			# Set object position.
			blender_tag.setLocation(origin[0], origin[1], origin[2])

			# Insert keyframe for current frame&object.
			blender_tag.insertIpoKey(LOCROTSIZE)

	Blender.Window.DrawProgressBar(1.0, "MD2 TAG Import")

	# Restore curframe from before the calculation.
	Blender.Set("curframe", previous_curframe)

	Blender.Window.RedrawAll()
