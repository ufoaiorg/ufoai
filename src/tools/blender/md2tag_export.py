#!BPY

"""
Name: 'MD2 tags (.tag)'
Blender: 242
Group: 'Export'
Tooltip: 'Export to Quake2 tag file format for UFO:AI (.tag).'
"""

__author__ = 'Werner Höhrer'
__version__ = '0.0.2'
__url__ = ["UFO: Aline Invasion, http://ufoai.sourceforge.net",
     "Support forum, http://ufo.myexp.de/phpBB2/index.php", "blender", "elysiun"]
__email__ = ["Werner Höhrer, bill_spam2:yahoo*de", "scripts"]
__bpydoc__ = """\
This script Exports a Quake 2 tag file as used in UFO:AI (MD2 TAG).

This file format equals the _parts_ of the MD3 format used for animated tags, but nothing more. i.e tagnames+positions

 Base code taken from the md2 exporter by Bob Holcomb. Many thanks.
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C): Werner Höhrer
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


import Blender
from Blender import *
from Blender.Draw import *
from Blender.BGL import *
from Blender.Window import *

import struct, string
from types import *



######################################################
# GUI Loader
######################################################

# Export globals
g_filename=Create("export.tag") # default value is set here.
g_filename_search=Create("")
g_scale=Create(1.0)		# default value is set here.
g_frames=Create(1)		# default value is set here.

# Events
EVENT_NOEVENT=1
EVENT_SAVE_MD2TAGS=2
EVENT_CHOOSE_FILENAME=3
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
	global g_frames
	global g_filename
	global EVENT_NOEVENT,EVENT_SAVE_MD2TAGS,EVENT_CHOOSE_FILENAME,EVENT_EXIT

	########## Titles
	glClear(GL_COLOR_BUFFER_BIT)
	glRasterPos2d(10, 120)
	Text("MD2 TAGs Export")

	######### Parameters GUI Buttons
	######### MD2 Filename text entry
	g_filename = String("MD2 TAGs file to save: ", EVENT_NOEVENT, 10, 75, 210, 18,
			g_filename.val, 255, "MD2 TAGs file to save")
	########## MD2 TAGs File Search Button
	Button("Search",EVENT_CHOOSE_FILENAME,220,75,80,18)

	########## Scale slider-default is 1/8 which is a good scale for md2->blender
	g_scale= Slider("Scale Factor: ", EVENT_NOEVENT, 10, 95, 210, 18,
			g_scale.val, 0.001, 20.0, 0, "Scale factor for MD2 TAGs");

	g_frames= Slider("NumFrames: ", EVENT_NOEVENT, 10, 55, 210, 18,
			g_frames.val, 1, MD2_MAX_FRAMES, 0, "Number of exported frames");

	######### Draw and Exit Buttons
	Button("Export",EVENT_SAVE_MD2TAGS , 10, 10, 80, 18)
	Button("Exit",EVENT_EXIT , 170, 10, 80, 18)

def event(evt, val):
	if (evt == QKEY and not val):
		Exit()

def bevent(evt):
	global g_filename
	global EVENT_NOEVENT,EVENT_SAVE_MD2TAGS,EVENT_EXIT

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
	def dump (self):
		print "MD2 tagname"
		print "tag: ",self.name
		print ""

class md2_tag:
	Row1	= []
	Row2	= []
	Row3	= []
	Row4	= []
	
	binary_format="<12f"	#little-endian (<), 12 floats (12f)	| See http://docs.python.org/lib/module-struct.html for more info.

	def __init__(self):
		Row1	= (0.0, 0.0, 0.0)
		Row2	= (0.0, 0.0, 0.0)
		Row3	= (0.0, 0.0, 0.0)
		Row4	= (0.0, 0.0, 0.0)
	def getSize(self):
		return struct.calcsize(self.binary_format)
	def save(self, file):
		temp_data=(
		self.Row1[0], self.Row1[1], self.Row1[2],
		self.Row2[0], self.Row2[1], self.Row2[2],
		self.Row3[0], self.Row3[1], self.Row3[2],
		self.Row4[0], self.Row4[1], self.Row4[2]
		)

		data=struct.pack(self.binary_format,
			temp_data[0], temp_data[1], temp_data[2],
			temp_data[3], temp_data[4], temp_data[5],
			temp_data[6], temp_data[7], temp_data[8],
			temp_data[9], temp_data[10], temp_data[11])
		file.write(data)
	def dump(self):
		print "MD2 Point Structure"
		print "Row1: ", self.Row1[0], " ",self.Row1[1] , " ",self.Row1[2]
		print "Row2: ", self.Row2[0], " ",self.Row2[1] , " ",self.Row2[2]
		print "Row3: ", self.Row3[0], " ",self.Row3[1] , " ",self.Row3[2]
		print "Row4: ", self.Row4[0], " ",self.Row4[1] , " ",self.Row4[2]
		print ""


class md2_tags_obj:
	#Header Structure
	ident=0			#int 0	This is used to identify the file
	version=0		#int 1	The version number of the file (Must be 1)
	num_tags=0		#int 2	The number of skins associated with the model
	num_frames=0		#int 3	The number of animation frames

	offset_names=0		#int 4	The offset in the file for the name data
	offset_tags=0		#int 5	The offset in the file for the tags data
	offset_end=0		#int 6	The end of the file offset
	offset_extract_end=0	#int 7	???
	binary_format="<8i" 	#little-endian (<), 8 integers (8i)

	#md2 tag data objects
	names=[]
	tags=[] # (consists of a number of frames)
	"""
	Example:
	tags = (
		(tag1_frame1, tag1_frame2, tag1_frame3, etc...),	# tag_frames1
		(tag2_frame1, tag2_frame2, tag2_frame3, etc...),	# tag_frames2
		(tag3_frame1, tag3_frame2, tag3_frame3, etc...)		# tag_frames3
	)
	"""

	def __init__ (self):
		self.names=[]
		self.tags=[]
	def getSize(self):
		return struct.calcsize(self.binary_format)
	def save(self, file):
		temp_data=[0]*8
		temp_data[0]=self.ident
		temp_data[1]=self.version
		temp_data[2]=self.num_tags
		temp_data[3]=self.num_frames
		temp_data[4]=self.offset_names
		temp_data[5]=self.offset_tags
		temp_data[6]=self.offset_end
		temp_data[7]=self.offset_extract_end
		data=struct.pack(self.binary_format, temp_data[0],temp_data[1],temp_data[2],temp_data[3],temp_data[4],temp_data[5],temp_data[6],temp_data[7])
		file.write(data)

		#write the names data
		for name in self.names:
			name.save(file)
		tag_count=0
		for tag_frames in self.tags:
			tag_count+=1
			frame_count=0
			print "Saving tag: ", tag_count

			for tag in tag_frames:
				frame_count+=1
				tag.save(file)
			print "  Frames saved: ", frame_count

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

"""
########################
		#write the skin data
		for skin in self.skins:
			skin.save(file)
		#save the texture coordinates
		for tex_coord in self.tex_coords:
			tex_coord.save(file)
		#save the face info
		for face in self.faces:
			face.save(file)
		#save the frames
		for frame in self.frames:
			frame.save(file)
			for vert in frame.vertices:
				vert.save(file)
		#save the GL command List
#		for cmd in self.GL_commands:
#			cmd.save(file)
"""

######################################################
# Apply a matrix to a vert and return a tuple.
######################################################
def apply_transform(verts, matrix):
	x, y, z = verts
	return\
	x*matrix[0][0] + y*matrix[1][0] + z*matrix[2][0] + matrix[3][0],\
	x*matrix[0][1] + y*matrix[1][1] + z*matrix[2][1] + matrix[3][1],\
	x*matrix[0][2] + y*matrix[1][2] + z*matrix[2][2] + matrix[3][2]

######################################################
# Fill MD2 data structure
######################################################
def fill_md2_tags(md2_tags, object):
	Blender.Window.DrawProgressBar(0.25,"Filling MD2 Data")

	# Set header information.
	md2_tags.ident=844121162
	md2_tags.version=1
	md2_tags.num_tags+=1

#	offset_names=0
#	offset_tags=0
#	offset_end=0
#	offset_extract_end=0


	# Add a name node to the tagnames data structure.
	md2_tags.names.append(md2_tagname(object.name))	# TODO: cut to 64 chars

	# Add a (empty) list of tags-positions (for each frame).
	tag_frames = []

	progress=0.5
	progressIncrement=0.25 / md2_tags.num_frames

	# Fill in each tag with its positions per frame
	for frame_counter in range(0,md2_tags.num_frames):
		progress+=progressIncrement
		Blender.Window.DrawProgressBar(progress, "Calculating Frame: "+str(frame_counter))

		#add a frame
		tag_frames.append(md2_tag())

		#set blender to the correct frame (so the objects have their new positions)
		Blender.Set("curframe", frame_counter)

		# Set first coordiantes to the location of the empty.
		tag_frames[frame_counter].Row1 = object.loc
		print tag_frames[frame_counter].Row1[0], " ",tag_frames[frame_counter].Row1[1]," ",tag_frames[frame_counter].Row1[2]
		# Calculate local axes ... starting from 'loc' (see http://wiki.blenderpython.org/index.php/Python_Cookbook#Apply_Matrix)
		# The scale of the empty is ignored right now.
		matrix=object.getMatrix()
		tag_frames[frame_counter].Row2 = apply_transform((1.0,0.0,0.0), matrix) #calculate point (loc + local-X * 1 )
		tag_frames[frame_counter].Row3 = apply_transform((0.0,1.0,0.0), matrix) #calculate point (loc + local-Y * 1 )
		tag_frames[frame_counter].Row4 = apply_transform((0.0,0.0,1.0), matrix) #calculate point (loc + local-Z * 1 )
		tag_frames[frame_counter].Row2 = (1.0,0.0,0.0)	# DEBUG - TODO: deleteme
		tag_frames[frame_counter].Row3 = (0.0,1.0,0.0)	# DEBUG - TODO: deleteme
		tag_frames[frame_counter].Row4 = (0.0,0.0,1.0)	# DEBUG - TODO: deleteme
		
		# Apply scale if it was set in the export dialog
		if (g_scale.val != 1.0):
			tag_frames[frame_counter].Row1 = ( tag_frames[frame_counter].Row1[0]* g_scale.val, tag_frames[frame_counter].Row1[1]* g_scale.val, tag_frames[frame_counter].Row1[2]* g_scale.val)
			tag_frames[frame_counter].Row2 = ( tag_frames[frame_counter].Row2[0]* g_scale.val, tag_frames[frame_counter].Row2[1]* g_scale.val, tag_frames[frame_counter].Row2[2]* g_scale.val)
			tag_frames[frame_counter].Row3 = ( tag_frames[frame_counter].Row3[0]* g_scale.val, tag_frames[frame_counter].Row3[1]* g_scale.val, tag_frames[frame_counter].Row3[2]* g_scale.val)
			tag_frames[frame_counter].Row4 = ( tag_frames[frame_counter].Row4[0]* g_scale.val, tag_frames[frame_counter].Row4[1]* g_scale.val, tag_frames[frame_counter].Row4[2]* g_scale.val)

	md2_tags.tags.append(tag_frames)

######################################################
# Save MD2 TAGs Format
######################################################
def save_md2_tags(filename):
	global g_frames

	print ""
	print "***********************************"
	print "MD2 TAG Export"
	print "***********************************"
	print ""

	Blender.Window.DrawProgressBar(0.0,"Beginning MD2 TAG Export")

	md2_tags=md2_tags_obj()  #blank md2 object to save

	#get the object
	mesh_objs = Blender.Object.GetSelected()

	#check there is a blender object selected
	if len(mesh_objs)==0:
		print "Fatal Error: Must select at least one 'Empty' to output a tag file"
		print "Found nothing"
		result=Blender.Draw.PupMenu("Must select an object to export%t|OK")
		return

	# Set frame number.
	md2_tags.num_frames=g_frames.val
	print "Frames to export: ",md2_tags.num_frames
	print ""	

	for object in mesh_objs:
		#check if it's an "Empty" mesh object
		if object.getType()!="Empty":
			print "Ignoring non-'Empty' object: ", object.getType()
		else:
			print "Found Empty with name ",object.name
			fill_md2_tags(md2_tags, object)
			Blender.Window.DrawProgressBar(1.0, "Writing to Disk")
	
	# Set offset of names
	temp_header = md2_tags_obj();
	md2_tags.offset_names= 0+temp_header.getSize();

	# Set offset of tags
	temp_name = md2_tagname("");
	md2_tags.offset_tags = md2_tags.offset_names + temp_name.getSize() * md2_tags.num_tags;
	
	# Set EOF offest value.
	temp_tag = md2_tag();
	md2_tags.offset_end = md2_tags.offset_tags + (temp_tag.getSize() * md2_tags.num_frames * md2_tags.num_tags)
	md2_tags.offset_extract_end=md2_tags.offset_end
	
	print ""
	md2_tags.dump()
	
	# Actually write it to disk.
	file=open(filename,"wb")
	md2_tags.save(file)
	file.close()

	# Cleanup
	md2_tags=0

	print "Closed the file"

"""
	#output all the frame names-user_frame_list is loaded during the validation
	for frame_set in user_frame_list:
		for counter in range(frame_set[1]-1, frame_set[2]):
			md2.frames[counter].name=frame_set[0]+"_"+str(counter-frame_set[1]+2)

	#compute these after everthing is loaded into a md2 structure
	header_size=17*4 #17 integers, and each integer is 4 bytes
	skin_size=64*md2.num_skins #64 char per skin * number of skins
	tex_coord_size=4*md2.num_tex_coords #2 short * number of texture coords
	face_size=12*md2.num_faces #3 shorts for vertex index, 3 shorts for tex index
	frames_size=(((12+12+16)+(4*md2.num_vertices)) * md2.num_frames) #frame info+verts per frame*num frames
#	GL_command_size=md2.num_GL_commands*4 #each is an int or float, so 4 bytes per

	#fill in the info about offsets
	md2.offset_skins=0+header_size
	md2.offset_tex_coords=md2.offset_skins+skin_size
	md2.offset_faces=md2.offset_tex_coords+tex_coord_size
	md2.offset_frames=md2.offset_faces+face_size
#	md2.offset_GL_commands=md2.offset_frames+frames_size
#	md2.offset_end=md2.offset_GL_commands+GL_command_size

"""
