#!BPY

"""
Name: 'MD2 tags (.tag)'
Blender: 242
Group: 'Export'
Tooltip: 'Export to Quake2 tag file format for UFO:AI (.tag).'
"""

__author__ = 'Werner Höhrer'
__version__ = '0.0.1'
__url__ = ["UFO: Aline Invasion, http://ufoai.sourceforge.net",
     "Support forum, http://ufo.myexp.de/phpBB2/index.php", "blender", "elysiun"]
__email__ = ["Werner Höhrer, bill_spam2:yahoo*de", "scripts"]
__bpydoc__ = """\
This script Exports a Quake 2 tag file as used in UFO:AI (MD2 TAG).

 Additional help from: Bob Holcomb (md2 exporter)
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
g_filename=Create("export.tag")
#g_frame_filename=Create("default")

#g_filename_search=Create("")
#g_frame_search=Create("default")

#user_frame_list=[]

#Globals
g_scale=Create(1.0)
g_frames=Create(1)

# Events
EVENT_NOEVENT=1
EVENT_SAVE_MD2=2
EVENT_CHOOSE_FILENAME=3
EVENT_CHOOSE_FRAME=4
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

"""
def frame_callback(input_frame):
	global g_frame_filename
	g_frame_filename.val=input_frame
"""
def draw_gui():
	global g_scale
	global g_frames
	global g_filename
	#global g_frame_filename
	global EVENT_NOEVENT,EVENT_SAVE_MD2,EVENT_CHOOSE_FILENAME,EVENT_CHOOSE_FRAME,EVENT_EXIT

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
                    1.0, 0.001, 20.0, 0, "Scale factor for MD2 TAGs");

	g_frames= Slider("Number of Frames: ", EVENT_NOEVENT, 10, 55, 210, 18,
                    1, 1, MD2_MAX_FRAMES, 0, "Number of exported frames");

	######### Draw and Exit Buttons
	Button("Export",EVENT_SAVE_MD2 , 10, 10, 80, 18)
	Button("Exit",EVENT_EXIT , 170, 10, 80, 18)

def event(evt, val):	
	if (evt == QKEY and not val):
		Exit()

def bevent(evt):
	global g_filename
#	global g_frame_filename
	global EVENT_NOEVENT,EVENT_SAVE_MD2,EVENT_EXIT

	######### Manages GUI events
	if (evt==EVENT_EXIT):
		Blender.Draw.Exit()
	elif (evt==EVENT_CHOOSE_FILENAME):
		FileSelector(filename_callback, "MD2 TAG File Selection")
	elif (evt==EVENT_CHOOSE_FRAME):
		FileSelector(frame_callback, "Frame Selection")
	elif (evt==EVENT_SAVE_MD2):
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
	def __init__(self):
		self.name=""
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

	binary_format="<12BB"
	def __init__(self):
		self.vertices=[0]*12
	def save(self, file):
		temp_data=[0]*12
		temp_data[0]=self.Row1[0]
		temp_data[1]=self.Row1[1]
		temp_data[2]=self.Row1[2]
		
		temp_data[3]=self.Row2[0]
		temp_data[4]=self.Row2[1]
		temp_data[5]=self.Row2[2]
		
		temp_data[6]=self.Row3[0]
		temp_data[7]=self.Row3[1]
		temp_data[8]=self.Row3[2]
		
		temp_data[9]=self.Row4[0]
		temp_data[10]=self.Row4[1]
		temp_data[11]=self.Row4[2]
		
		data=struct.pack(self.binary_format, temp_data[0], temp_data[1], temp_data[2], temp_data[3], temp_data[4], temp_data[5], temp_data[6], temp_data[7], temp_data[8])
		file.write(data)
	def dump(self):
		print "MD2 Point Structure"
		print "Row1_x: ", self.Row1[0]
		print "Row1_y: ", self.Row1[1]
		print "Row1_z: ", self.Row1[2]
		print "Row2_x: ", self.Row2[0]
		print "Row2_y: ", self.Row2[1]
		print "Row2_z: ", self.Row2[2]
		print "Row3_x: ", self.Row3[0]
		print "Row3_y: ", self.Row3[1]
		print "Row3_z: ", self.Row3[2]
		print "Row4_x: ", self.Row4[0]
		print "Row4_y: ", self.Row4[1]
		print "Row4_z: ", self.Row4[2]
		print ""

"""
class md2_face:
	vertex_index=[]
	texture_index=[]
	binary_format="<3h3h"
	def __init__(self):
		self.vertex_index = [ 0, 0, 0 ]
		self.texture_index = [ 0, 0, 0]
	def save(self, file):
		temp_data=[0]*6
		#swap vertices around so they draw right
		temp_data[0]=self.vertex_index[0]
		temp_data[1]=self.vertex_index[2]
		temp_data[2]=self.vertex_index[1]
		#swap texture vertices around so they draw right
		temp_data[3]=self.texture_index[0]
		temp_data[4]=self.texture_index[2]
		temp_data[5]=self.texture_index[1]
		data=struct.pack(self.binary_format,temp_data[0],temp_data[1],temp_data[2],temp_data[3],temp_data[4],temp_data[5])
		file.write(data)
	def dump (self):
		print "MD2 Face Structure"
		print "vertex 1 index: ", self.vertex_index[0]
		print "vertex 2 index: ", self.vertex_index[1]
		print "vertex 3 index: ", self.vertex_index[2]
		print "texture 1 index: ", self.texture_index[0]
		print "texture 2 index: ", self.texture_index[1]
		print "texture 3 index: ", self.texture_index[2]
		print ""
		

class md2_tex_coord:
	u=0
	v=0
	binary_format="<2h"
	def __init__(self):
		self.u=0
		self.v=0
	def save(self, file):
		temp_data=[0]*2
		temp_data[0]=self.u
		temp_data[1]=self.v
		data=struct.pack(self.binary_format, temp_data[0], temp_data[1])
		file.write(data)
	def dump (self):
		print "MD2 Texture Coordinate Structure"
		print "texture coordinate u: ",self.u
		print "texture coordinate v: ",self.v
		print ""
		
class md2_frame:
	tags=[]
	binary_format="<3f3f16s"

	def __init__(self):
		self.scale=[0.0]*3
		self.translate=[0.0]*3
		self.name=""
		self.vertices=[]
	def save(self, file):
		temp_data=[0]*7
		temp_data[0]=float(self.scale[0])
		temp_data[1]=float(self.scale[1])
		temp_data[2]=float(self.scale[2])
		temp_data[3]=float(self.translate[0])
		temp_data[4]=float(self.translate[1])
		temp_data[5]=float(self.translate[2])
		temp_data[6]=self.name
		data=struct.pack(self.binary_format, temp_data[0],temp_data[1],temp_data[2],temp_data[3],temp_data[4],temp_data[5],temp_data[6])
		file.write(data)
	def dump (self):
		print "MD2 Frame"
		print "scale x: ",self.scale[0]
		print "scale y: ",self.scale[1]
		print "scale z: ",self.scale[2]
		print "translate x: ",self.translate[0]
		print "translate y: ",self.translate[1]
		print "translate z: ",self.translate[2]
		print "name: ",self.name
		print ""
"""
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

#	TagName			=> ['a64', '{$NumTags}', 1 ],	# 64chars * NumTags
#	Data			=> ['f12', '{$NumTags*$NumFrames}', 1 ]	# 12 floats * NumTags * NumFrames

	names=[]
	tags=[] #(consists of a number of frames)
	frames=[]
	
	def __init__ (self):
		self.names=[]
		self.tags=[]
		self.frames=[]
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

	def dump (self):
		print "Header Information"
		print "ident: ", self.ident
		print "version: ", self.version
		print "number of tags: ", self.num_tags
		print "number of frames: ", self.num_frames
		print "offset names: ", self.offset_names
		print "offset tags: ", self.offset_tags
		print "offset end: ",self.offset_end
		print "offset extract end: ",self.offset_extract_end		
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
"""
######################################################
# Validation
######################################################
def validation(object):
	global user_frame_list

	#move the object to the origin if it's not already there
#	if object.loc!=(0.0, 0.0, 0.0):
#		object.setLocation(0.0,0.0,0.0)
#		print "Model not centered at origin-Centering"
#		result=Blender.Draw.PupMenu("Model not centered at origin-Centering")

	#resize the object in case it is not the right size
#	if object.size!=(1.0,1.0,1.0):
#		object.setSize(1.0,1.0,1.0)
#		print "Object is scaled-You should scale the mesh verts, not the object"
#		result=Blender.Draw.PupMenu("Object is scaled-You should scale the mesh verts, not the object")
		
#	if object.rot!=(0.0,0.0,0.0):
#		object.rot=(0.0,0.0,0.0)
#		print "Object is rotated-You should rotate the mesh verts, not the object"
#		result=Blender.Draw.PupMenu("Object is rotated-You should rotate the mesh verts, not the object")
	
	#get access to the mesh data
#	mesh=object.getData(False, True) #get the object (not just name) and the Mesh, not NMesh
	

	#verify frame list data
#	user_frame_list=get_frame_list()	
#	temp=user_frame_list[len(user_frame_list)-1]
#	temp_num_frames=temp[2]

	#verify tags/frames counts are within MD2 TAGs standard
	tag_count=len(mesh.tags)
#	frame_count=temp_num_frames
	
	if tag_count>MD2_MAX_TAGS:
		print "Number of triangles exceeds MD2 TAG standard: ", face_count,">",MD2_MAX_TAGS
		result=Blender.Draw.PupMenu("Number of triangles exceeds MD2 TAG standard: Continue?%t|YES|NO")
		if(result==2):
			return False

#	if frame_count>MD2_MAX_FRAMES:
#		print "Number of frames exceeds MD2 TAG standard of",frame_count,">",MD2_MAX_FRAMES
#		result=Blender.Draw.PupMenu("Number of frames exceeds MD2 TAG standard: Continue?%t|YES|NO")
#		if(result==2):
#			return False

	#model is OK
	return True
"""
# Apply a matrix to a vert and return a tuple.
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
	#global defines
	#global user_frame_list
	
	Blender.Window.DrawProgressBar(0.25,"Filling MD2 Data")
	
	#get a Mesh, not NMesh
	#mesh=object.getData(False, True)	
	
	#load up some intermediate data structures
	tex_list={}
	tex_count=0
	#create the vertex list from the first frame
	Blender.Set("curframe", 1)
	
	#header information
	md2_tags.ident=844121162
	md2_tags.version=1	
	md2_tags.num_tags+=1

#	offset_names=0
#	offset_tags=0
#	offset_end=0
#	offset_extract_end=0


	#add a name node to the tagnames data structure
	md2_tags.names.append(md2_tagname())
	
	
	#add a listz of tags-positions (for esach frame)
	tag_frames = []
	
	progress=0.5
	progressIncrement=0.25 / md2_tags.num_frames

	#fill in each frame with frame info and all the vertex data for that frame
	for frame_counter in range(0,md2_tags.num_frames):
		progress+=progressIncrement
		Blender.Window.DrawProgressBar(progress, "Calculating Frame: "+str(frame_counter))
			
		#add a frame
		tag_frames.append(md2_tag())
		
		#set blender to the correct frame (so the objects have their new positions)
		Blender.Set("curframe", frame_counter)
	
		# TODO: get xyz from object
		tag_frames[frame_counter].Row1 = object.loc
					
		# calculate local axes ... starting from loc (see http://wiki.blenderpython.org/index.php/Python_Cookbook#Apply_Matrix)
		# Scale is ignored right now.
		matrix=object.getMatrix()
		tag_frames[frame_counter].Row3 = apply_transform((1.0,0.0,0.0), matrix) #calculate point (loc + local-X * 1 )
		tag_frames[frame_counter].Row3 = apply_transform((0.0,1.0,0.0), matrix) #calculate point (loc + local-Y * 1 )
		tag_frames[frame_counter].Row4 = apply_transform((0.0,0.0,1.0), matrix) #calculate point (loc + local-Z * 1 )

	md2_tags.tags.append(tag_frames)

	md2_tags.offset_tags += 64 # one string 64 bytes??? (see tag_name)
	md2_tags.offset_end = md2_tags.offset_tags + (12 * 4 * md2_tags.num_frames) # 12 floats each 4 byte ??? for every frame

"""
######################################################
# Get Frame List
######################################################
def get_frame_list():
	global g_frame_filename
	frame_list=[]

	if g_frame_filename.val=="default":
		return MD2_FRAME_NAME_LIST

	else:
	#check for file
		if (Blender.sys.exists(g_frame_filename.val)==1):
			#open file and read it in
			file=open(g_frame_filename.val,"r")
			lines=file.readlines()
			file.close()

			#check header (first line)
			if lines[0]<>"# MD2 Frame Name List":
				print "its not a valid file"
				result=Blender.Draw.PupMenu("This is not a valid frame definition file-using default%t|OK")
				return MD2_FRAME_NAME_LIST
			else:
				#read in the data
				num_frames=0
				for counter in range(1, len(lines)):
					current_line=lines[counter]
					if current_line[0]=="#":
						#found a comment
						pass
					else:
						data=current_line.split()
						frame_list.append([data[0],num_frames+1, num_frames+int(data[1])])
						num_frames+=int(data[1])
				return frame_list
		else:
			print "Cannot find file"
			result=Blender.Draw.PupMenu("Cannot find frame definion file-using default%t|OK")
			return MD2_FRAME_NAME_LIST
"""

"""
######################################################
# Globals for GL command list calculations
######################################################
used_tris=[]
strip_verts=[]
strip_st=[]
strip_tris=[]
strip_count=0

######################################################
# Tri-Strip/Tri-Fan functions
######################################################
def find_strip_length(md2, start_tri, start_vert):
	print "Finding strip length"
	
	global used
	global strip_verts
	global strip_st
	global strip_tris

	m1=m2=0

	used[start_tri]=2
	
	strip_verts=[0]*0
	strip_tris=[0]*0
	strip_st=[0]*0
	
	
	strip_verts.append(md2.faces[start_tri].vertex_index[start_vert%3])
	strip_verts.append(md2.faces[start_tri].vertex_index[(start_vert+2)%3])
	strip_verts.append(md2.faces[start_tri].vertex_index[(start_vert+1)%3])
	
	strip_st.append(md2.faces[start_tri].texture_index[start_vert%3])
	strip_st.append(md2.faces[start_tri].texture_index[(start_vert+2)%3])
	strip_st.append(md2.faces[start_tri].texture_index[(start_vert+1)%3])

	strip_count=1
	strip_tris.append(start_tri)

	m1=md2.faces[start_tri].vertex_index[(start_vert+2)%3]
	m2=md2.faces[start_tri].vertex_index[(start_vert+1)%3]

	for tri_counter in range(start_tri+1, md2.num_faces):
		for k in range(0,3):
			if(md2.faces[tri_counter].vertex_index[k]!=m1) or (md2.faces[tri_counter].vertex_index[(k+1)%3]!=m2):
				pass
			else: 
				#found a matching edge
				print "Found a triangle with a matching edge: ", tri_counter
				if(used[tri_counter]!=0):
					print "Poop! I can't use it!"
				else:
					print "Yeah! I can use it"
				
					if(strip_count%2==1):  #is this an odd tri
						print "odd triangle"
						m2=md2.faces[tri_counter].vertex_index[(k+2)%3]
					else:
						print "even triangle"
						m1=md2.faces[tri_counter].vertex_index[(k+2)%3]
	
					strip_verts.append(md2.faces[tri_counter].vertex_index[(k+2)%3])
					strip_st.append(md2.faces[tri_counter].texture_index[(k+2)%3])
					strip_count+=1
					strip_tris.append(tri_counter)
	
					used[tri_counter]=2
					tri_counter=start_tri+1 # restart looking

	#clear used counter
	for used_counter in range(0, md2.num_faces):
		if used[used_counter]==2:
			used[used_counter]=0

	return strip_count
"""
"""
#***********************************************
def find_fan_length(md2, start_tri, start_vert):
	print "Finding fan length"
	
	global used
	global strip_verts
	global strip_tris
	global strip_st
	
	strip_verts=[0]*0
	strip_tris=[0]*0
	strip_st=[0]*0

	m1=m2=0

	used[start_tri]=2
	
	strip_verts.append(md2.faces[start_tri].vertex_index[start_vert%3])
	strip_verts.append(md2.faces[start_tri].vertex_index[(start_vert+2)%3])
	strip_verts.append(md2.faces[start_tri].vertex_index[(start_vert+1)%3])
	
	strip_st.append(md2.faces[start_tri].texture_index[start_vert%3])
	strip_st.append(md2.faces[start_tri].texture_index[(start_vert+2)%3])
	strip_st.append(md2.faces[start_tri].texture_index[(start_vert+1)%3])

	strip_count=1
	strip_tris.append(start_tri)

	m1=md2.faces[start_tri].vertex_index[(start_vert)]
	m2=md2.faces[start_tri].vertex_index[(start_vert+1)%3]

	for tri_counter in range(start_tri+1, md2.num_faces):
		for k in range(0,3):
			if (md2.faces[tri_counter].vertex_index[k]!=m1) or (md2.faces[tri_counter].vertex_index[(k+1)%3]!=m2):
				pass
			else:
				#found a matching edge
				print "Found a triangle with a matching edge: ", tri_counter
				if(used[tri_counter]!=0):
					print "Poop! I can't use it!"
				else:
					print "Yeah! I can use it"
				
					m2=md2.faces[tri_counter].vertex_index[(k+1)%3]
	
					strip_verts.append(md2.faces[tri_counter].vertex_index[(k+1)%3])
					strip_st.append(md2.faces[tri_counter].texture_index[(k+1)%3])
					
					strip_count+=1
					strip_tris.append(tri_counter)
	
					used[tri_counter]=2
					tri_counter=start_tri+1 #restart looking
				
	#clear used counter
	for used_counter in range(0, md2.num_faces):
		if used[used_counter]==2:
			used[used_counter]=0
	
	return strip_count
"""

"""
######################################################
# Build GL command List
######################################################

def build_GL_commands(md2):
print "Building GL Commands"
	
	global used
	global strip_verts
	global strip_tris
	global strip_st
	
	#globals initialization
	used=[0]*md2.num_faces
	print "Used: ", used
	num_commands=0

	for tri_counter in range(0,md2.num_faces):
		if used[tri_counter]!=0: 
			print "Found a used triangle: ", tri_counter
		else:
			print "Found an unused triangle: ", tri_counter

			#intialization
			best_length=0
			best_type=0
			best_verts=[0]*0
			best_tris=[0]*0
			best_st=[0]*0
			

			for start_vert in range(0,3):
				fan_length=find_fan_length(md2, tri_counter, start_vert)
				#fan_length=0
				print "Triangle: ", tri_counter, " Vertex: ", start_vert, " Fan Length: ", fan_length

				if (fan_length>best_length):
					best_type=1
					best_length=fan_length
					best_verts=strip_verts
					best_tris=strip_tris
					best_st=strip_st
				
				#strip_length=find_strip_length(md2, tri_counter, start_vert)
				strip_length=0
				print "Triangle: ", tri_counter, " Vertex: ", start_vert, " Strip Length: ", strip_length
		
				if (strip_length>best_length): 
					best_type=0
					best_length=strip_length
					best_verts=strip_verts
					best_tris=strip_tris
					best_st=strip_st

			print "Best length for this triangle is: ", best_length
			print "Best type is: ", best_type
			print "Best Tris are: ", best_tris
			print "Best Verts are: ", best_verts
			print "Best ST are: ", best_st


			#mark tris as used
			for used_counter in range(0,best_length):
				used[best_tris[used_counter]]=1

			#create command list
			cmd_list=md2_GL_cmd_list()
			if best_type==0:
				cmd_list.num=(-(best_length+2))
			else:	
				cmd_list.num=best_length+2

			num_commands+=1
			for command_counter in range(0,best_length+2):
				cmd=md2_GL_command()										
				s=md2.tex_coords[best_st[command_counter]].u
				t=md2.tex_coords[best_st[command_counter]].v
				cmd.s=s/md2.skin_width
				cmd.t=t/md2.skin_height
				cmd.vert_index=best_verts[command_counter]
				num_commands+=3
				cmd_list.cmd_list.append(cmd)
			cmd_list.dump()
			md2.GL_commands.append(cmd_list)
		
		print "Used: ", used			

	#add the null command at the end
	temp_cmdlist=md2_GL_cmd_list()	
	temp_cmdlist.num=0
	md2.GL_commands.append(temp_cmdlist)  
	num_commands+=1		

	#cleanup and return
	used=best_vert=best_st=best_tris=strip_vert=strip_st=strip_tris=0
	return num_commands
"""



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
	
	header_size=8*4	#8 integers, and each integer is 4 bytes
	md2_tags.offset_names=0+header_size
	md2_tags.offset_tags=md2_tags.offset_names+0
	
	for object in mesh_objs:
		#check if it's an "Empty" mesh object
		if object.getType()!="Empty":
			print "Ignoring non-'Empty' object: ", object.getType()
		else:
			print "Found: ", object.getType()
			#ok=validation(object)
			#if ok==False:
			#	print "Ignoring invalid 'Empty' object."
			#else:
			fill_md2_tags(md2_tags, object)

			Blender.Window.DrawProgressBar(1.0, "Writing to Disk")

	md2_tags.dump()
	#actually write it to disk
	file=open(filename,"wb")
	md2_tags.save(file)
	file.close()
	
	#cleanup
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
