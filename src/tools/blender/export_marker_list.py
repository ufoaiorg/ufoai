# Print all marker names

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C): DarkRain, Werner Höhrer
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

import bpy
import bpy.ops

#=======================
# Collect marker data
#=======================
def generate_mlist (markerData, printInfo):
	if (printInfo):
		print("Frame: Marker name")

	# Get the list of markers (includes start-frame and name)
	frameNames =[]
	for marker in bpy.context.scene.timeline_markers:
		frameNames.append(marker)
	frameNames.sort(key=lambda marker: marker.frame)
#	print(frameNames) #debug

	previousFrame = -1
	for i in range(len(frameNames)):
		frame = frameNames[i].frame
		if (previousFrame < 0):
			previousFrame = frame

		name = frameNames[i].name
		if (printInfo):
			print(str(frame) + ": " + name)
		# Set data (mainly name) of current frame
		markerData[frame] = (name, frame);
		# Set end-frame of previous frame
		markerData[previousFrame] = (markerData[previousFrame][0], frame-1)
		# Remember the previous frame
		previousFrame = frame

	if (printInfo):
		print(str(frame) + ": Last frame.")
		print(str(len(markerData)) + " markers found.")


#=======================
# Output ufoai data
#=======================
def write_mlist_ufoai (markerData):
	bpy.data.texts.new(name="framelist")
	txt = bpy.data.texts["framelist"]

	#file = open(filename, 'wb')
	try:
		ufoaiOffset = bpy.context.scene.frame_start	# Difference between Blender/UFO:AI frames (ufoai = blender - ufoaiOffset)

#		print(markerData)#debug
		dummySpeed = 20
		txt.write("// Frame list exported from blender - please check if everything is ok before using!\n")
		for frame in sorted(markerData.keys()):
			if (markerData[frame][0] == "ufoai_end"):
				break
			txt.write(markerData[frame][0] + "\t\t\t\t" + str(frame-ufoaiOffset) + "\t\t" + str(markerData[frame][1]-ufoaiOffset) + "\t\t" + str(dummySpeed) + "\n")
		txt.write("// End of the list.\n")
	finally:
		print("========================================")
		for line in txt.lines:
			print(line.body)
		print("========================================")
		print("Created new text with the frame list. See text-editor for saving.")
		print("A badly formatted version is printed above.")
		# You can normally use [Ctrl][Shift][Alt][C] to copy the text into the clipboards for use in other applications. Might not always work though.
		#file.close()

#=======================
# MAIN
#=======================
ufoaiOutput = True
mlist = {}
generate_mlist(mlist, not ufoaiOutput)

if ufoaiOutput:
	write_mlist_ufoai(mlist)
