#!BPY

# Blender exporter for UFO:AI.

# Copyright 2008 (c) Wrwrwr <http://www.wrwrwr.org>

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# --------------------------------------------------------------------------
# Changelog
# 	0.2.1	mattn fixed missing texture and removed stepon
# 	0.2	WRWRWR Initial version
# --------------------------------------------------------------------------


"""
Name: 'UFO:AI (.map)'
Blender: 245
Group: 'Export'
Tooltip: 'Export to UFO:AI map format.'
"""

__author__ = 'Wrwrwr'
__version__ = '0.2.1'
__email__ = 'ufoai@wrwrwr.org'
__bpydoc__ = '''\
Exports current scene as an UFO:AI map.

See the full documentation at: http://www.wrwrwr.org/b2ufo/.

Simple convex meshes, lights, models and generic entities are exported.
'''

from math import *
from numpy import *
from numpy.linalg import *

from copy import copy, deepcopy
from cStringIO import StringIO
from datetime import datetime
from logging import Filter, basicConfig, debug, error, getLogger, info, warning
from os.path import exists, isabs, join, normpath, realpath, splitext
from os import sep
from sys import stderr
from time import clock

from Blender import *
import BPyMesh # for faceAngles



# planes for Q2 texture mapping; normal, its non zero indices
# "natural" texture mapping -- horrible, moreover the planes have different handedness
PLANES = (
(( 0,  0,  1), (0, 1)),			# floor, lh
(( 0,  0, -1), (0, 1)),			# ceiling, rh
(( 1,  0,  0), (1, 2)),			# west wall, lh
((-1,  0,  0), (1, 2)),			# east wall, rh
(( 0,  1,  0), (0, 2)),			# south wall, rh
(( 0, -1,  0), (0, 2))			# north wall, lh
)

# dummy object needed only for triangulation in splitting *** move to export
do = Object.New('Mesh', 'Dummy Exporter Object')

# dummy mesh used for exporting meshes without changing them *** move to export
dm = Mesh.New()

# ~rotation matrices used in texture mapping, *** move this to export (for configurable sum weighting)
a0 = matrix([[ 0,  1], [ 1,  0]])
a1 = matrix([[ 1,  0], [ 0, -1]])
asit = (a0 + a1).I.T



def normalize(vector):
	'''
	Normalize a numpy array. Should be a numpy function.
	'''
	return vector / norm(vector)



def needsSplitting(mesh, scale):
	'''
	Checks if it's ok to export mesh as it is. Must be strictly convex.
	Connectivity: checks if every edge is used by exactly two faces.
	Convexity: checks if every two normals of neighbouring faces intersect behind them.
	This should also work for all cases that are not polyhedra, but are exportable.
	Complexity: O(number of edges).
	*** check if the mesh is at all connected?
	*** are these tests enough
	'''
	# make a dictionary, where the edge is the key, and a list of faces that use it is the value
	esfs = dict([(e.key, []) for e in mesh.edges])
	for f in mesh.faces:
		for ek in f.edge_keys:
			esfs[ek].append(f)

	# check normals of every two neighbouring faces
	for ek, fs in esfs.iteritems():
		if len(fs) != 2:
			warning('\t\tNeeds splitting: An edge is not used by exactly 2 faces.')
			return True
		if dot(fs[0].no, fs[1].cent - fs[0].cent) >= 0:
			warning('\t\tNeeds splitting: Concave around: %s.' % around((fs[0].cent + fs[1].cent) / 2 / scale, 2))
			'''
			# sometimes a handy way to help in fixing those 'where is it concave?' things
			mesh.mode = Mesh.SelectModes['FACE']
			fs[0].sel = fs[1].sel = 1
			mesh = mesh.__copy__()
			sob = Object.New('Mesh', 'Concave Debug')
			Scene.getCurrent().objects.link(sob)
			sob.link(mesh)
			'''
			return True

	'''
	# check face areas, that's only needed with early coordinates rounding
	# such checks would only be needed without enlarging coordinates
	for f in mesh.faces:
		if f.area < minFaceArea:
			warning('\t\tNeeds splitting: A very small face present.')
			return True
	'''
	return False


# *** advanced, efficient splitting needed (e.g.: add as many faces as will allow to close polyhedron with a single vertex leaving it convex, like incremental convex hull)
# *** how to deal with very close opposite faces?
# *** if using rounded vertex coordinates, ascertain that the added vertices have integer coordinates too
def split(mesh, splitHeight):
	'''
	Split the mesh into tetra-, pentahedra, one for every face.
	Very close opposite faces (like a concave thin wall) will break visibility info.
	'''
	# we'll return a list of new meshes, together making the original one
	ms = []
	for f in mesh.faces:
		vs = [v.co for v in f.verts]

		# check if the face won't cause trouble
		if len(vs) != 3:
			warning('\t\tSkipping a face not being a triangle, with %d vertices.' % len(vs))
			continue
		if f.area < minFaceArea:
			warning('\t\tSkipping a face with very small area: %.2f.' % f.area)
			continue
		as = filter(lambda a: a < minFaceAngle, BPyMesh.faceAngles(f)) # *** just till the first is found
		if as:
			warning('\t\tSkipping a face with a very small angle: %.2f.' % as[0])
			continue
		es = filter(lambda ei: mesh.edges[ei].length < minEdgeLength, mesh.findEdges(f.edge_keys)) # *** same
		if es:
			warning('\t\tSkipping a face with a very short edge: %.2f.' % mesh.edges[es[0]].length)
			continue

		# make a new tetrahedron, watch vertex order not to flip any normals
		m = Mesh.New()
		m.verts.extend(vs)
		m.verts.extend(f.cent - f.no * splitHeight)
		m.faces.extend(((0, 1, 2), (1, 0, 3), (2, 1, 3), (0, 2, 3)))
		m.faces[0].image = f.image
		m.faces[0].uv = f.uv
		ms.append(m)
	return ms
	'''
	# new faces should have similar area to the original face?
	m.verts.extend(f.cent - f.no * sqrt(f.area) * 1.24 * splitHeight)

	# cuboids, a bit more precise for small faces
	m.verts.extend([v.co - f.no * sqrt(f.area) * splitHeight for v in f.verts])
	m.faces.extend(((0, 1, 2), (1, 0, 3, 4), (2, 1, 4, 5), (0, 2, 5, 3), (5, 4, 3)))
	'''



def Q2Texture(face):
	'''
	Texture mapping info in the Quake 2 format: offset u, offset v, rotation, scale u, scale v;
	all in the one of the base planes (ceiling, west, south etc.); offsets and rotation with respect to the scene center.
	Note that, because textures are actually mapped to one of the base planes and not the face's plane, it's not possible to properly map streched textures,
	or textures to faces when the projections to the base plane of the image edges' in the scene aren't perpendicular.
	Face is assumed to have an image texture, and to be strictly convex.
	*** still requires review, add lack of inverse exception handling
	*** this doesn't work well for textures stretched on quads (completely ignores one vertex)
	'''
	# a plane, the texture is actually mapped to; one of the base planes, the normal of which is the closest to the face normal
	d, (tn, tnnzis) = max(map(lambda p: (dot(face.no, p[0]), p), PLANES))

	# project face points to the base plane (any vertices in any order may be used here) *** quad with three vertices in a line problem
	vs = array([v.co for v in face.verts])[:, tnnzis]
	c = array(face.cent)[tnnzis, ...] # *** c doesn't have to be an array

	# image data, uv coordinates must match vertices
	size = face.image.getSize()
	uvs = array(face.uv)

	# face edges in the scene coordinates and in the image coordinates
	edgesScene = matrix([vs[1] - vs[0], vs[2] - vs[0]])
	edgesImage = matrix([uvs[1] - uvs[0], uvs[2] - uvs[0]])

	# bottom and left edge of the image, or uv unit vectors, in the scene coordinates
	# (uvs may lie on a line -- noninvertible, *** a better approximation of offset and rotation is needed in such cases)
	try:
		units = asarray(edgesImage.I * edgesScene)
	except LinAlgError:
		warning('\t\tA texture line to be mapped onto a face, uvs: %s.' % [tuple(around(v, 2)) for v in face.uv])
		units = asarray(pinv(edgesImage) * edgesScene) # *** does the Moore-Penrose make sense here at all?

	# face center uv coordinates (we'll try to put the same texture point on the face center as blender does)
	try:
		uvc = uvs[0] + ravel(matrix(c - vs[0]) * edgesScene.I * edgesImage)
	except LinAlgError:
		error('\t\tBUG: A line face in texture mapping.')
		return ' 0 0 0 1 1'

	# unscaled scale
	scale = apply_along_axis(norm, 1, units)

	# normalize the unit vectors
	units[0] /= scale[0]
	units[1] /= scale[1]

	# quake understands the left edge upside down, or blender does
	scale[1] *= -1
	# uvs[:, 1] *= -1, not used
	uvc[1] *= -1

	# units and base axes have the same orientation?, if not flip one of the units (projected base axes cross is always -1)
	if cross(units[0], units[1]) > 0:
		units[0] *= -1
		scale[0] *= -1

	# rotation angle (around (0,0,0)), such an angle that the base rotated by it gives the texture units' projections
	# or rather the sum of the base axes gives the sum of the units, any better idea for stretched textures? *** implement the configurable weighting
	rotation = atan2(*ravel(dot(units[0] + units[1], asit)))

	# offsets will be calculated with respect to the rotated base
	r = matrix([[sin(rotation)], [cos(rotation)]])

	# offset (from (0,0,0), in lengths of units' projections)
	# *** shouldn't this be matrix(c - vs[0])? think this over again
	offset = uvc - ravel(matrix(c) * hstack([a0 * r, a1 * r])) / scale

	# in degrees, rounded
	rotation = round(rotation * 180.0 / pi) % 360

	# in images
	scale /= size

	# in images, rounded
	offset = around((offset * size) % size)

	return '%d %d %d %f %f' % (offset[0], offset[1], rotation, scale[0], scale[1])



def writeFace(file, face, flags, coordinatesMultiplier, imagePaths, missingImage):
	'''
	Writes the face description to the file.
	Writes three vertices, texture information (name, u offset, v offset, rotation, u scale, v scale), and flags.
	'''
	# write three points in the face plane (in a counterclock-wise order)
	vs = [face.verts[-1].co, face.verts[1].co, face.verts[0].co]

	# large coordinates give better approximation of plane normal and distance, but an edge shared by two different meshes may come out as two different edges
	if coordinatesMultiplier == 1:
		ps = vs
	else:
		ps = array([vs[1] - vs[0], vs[2] - vs[1], vs[0] - vs[2]], dtype=longdouble) # some increased precision won't hurt here
		ps = apply_along_axis(normalize, 1, ps) * coordinatesMultiplier + vs
	file.write('( %d %d %d ) ( %d %d %d ) ( %d %d %d )' % tuple(around(ravel(ps))))

	# write texture information, it's assumed here that the mesh has an uv layer
	if face.image:
		file.write(' %s %s' % (imagePaths[face.image], Q2Texture(face)))
	else:
		flags = (0, 128, 0) # 16 = ~transparent, 128 = don't draw, 512 = completely skip (breaks visibility)
		file.write(' %s 0 0 0 1 1' % missingImage)

	# write flags, *** all flags handling not just level flags (handling somewhat above or at writeMesh)
	file.write(' %d %d %d\n' % tuple(flags))

	'''
	# write a lot of debugging info to the map file (should still compile)
	file.write('// normal: %s, center: %s, area: %s\n' % (face.no, face.cent, face.area))
	file.write('// vertices: %s\n' % [v.co for v in face.verts])
	file.write('// edges lengths: %f %f %f\n' % (norm(vs[2] - vs[0]), norm(vs[1] - vs[0]), norm(vs[2] - vs[1])))
	file.write('// face angles: %s\n' % str(BPyMesh.faceAngles(face)))
	v1, v2 = normalize(vs[1] - vs[0]), normalize(vs[2] - vs[0])
	pr1, pr2 = normalize(around(ps[1] - ps[0])), normalize(around(ps[2] - ps[0]))
	file.write('// original plane vectors: %s %s, quality: %f %f %f\n' % (v1, v2, dot(v1, face.no), dot(v2, face.no), dot(vs[0], face.no)))
	file.write('// written plane vectors:  %s %s, quality: %f %f %f\n// ---------------------\n' % (pr1, pr2, dot(pr1, face.no), dot(pr2, face.no), dot(ps[0], face.no)))
	'''


def writeMesh(file, mesh, scale, quadToleration, splitMethod, splitHeight, minFaceArea, minFaceAngle, minEdgeLength, coordinatesMultiplier, imagePaths, missingImage, stats):
	'''
	Write a Blender mesh to the map file. It will be split if needed (if it's not strictly concave).
	Writes game logic properties as entity properties, then writes faces of one or more brushes as needed.
	Level visibility is set to above object's geometric center (calculated, not from Blander). You can override this with the 'levels' property.
	'''
	# make a copy of the mesh data
	dm.getFromObject(mesh)
	do.link(dm)

	# *** some more checks to see if at all exportable
	if not dm.faces:
		warning('\t\tIgnoring a mesh without faces.')
		return

	if not dm.faceUV:
		warning('\t\tNo uv layer. Only uv-mapped textures are supported.')
		dm.addUVLayer('Dummy uv layer.')

	# apply object transform to all vertices to get their world coordinates
	dm.transform(mesh.matrix * scale, recalc_normals=True)

	# round vertices before performing any calculations (goes with writing them verbatim later),
	# this can turn some faces completely, however sometimes is better at avoiding breaks in split meshes
	if coordinatesMultiplier == 1:
		for v in dm.verts:
			v.co = Mathutils.Vector(around(v.co))

	# triangulate all nonplanar quads
	dm.sel = False
	for f in dm.faces:
		if len(f.verts) == 4:
			vs = [v.co for v in f.verts]
			es = [vs[i] - vs[(i+1) % 4] for i in range(4)]
			if not alltrue([abs(dot(es[i], f.no)) < quadToleration * norm(es[i]) for i in range(4)]):
				f.sel = True
	dm.quadToTriangle()

	# split the mesh if needed and requested
	if needsSplitting(dm, scale):
		if splitMethod: # *** just one (not really working) method at the moment :)
			ms = split(dm)
		else:
			warning('\t\tSplitting Disabled!')
			ms = [dm]
	else:
		ms = [dm]

	# defined game properties
	ps = dict((p.name, p.data) for p in mesh.game_properties)

	# flags property
	if 'flags' in ps:
		flags = map(int, ps['flags'].split(None, 2))
		del ps['flags']
	else:
		flags = [0, 0, 0]

	# level visibility
	if 'levels' in ps:
		lf = sum(map(lambda x: 1 << x if ps['levels'].find(str(x+1)) != -1 else 0, range(8)))
		stats['levels'] = max(stats['levels'], max(map(int, ps['levels'].split())))
		del ps['levels']
	else:
		vs = dm.verts
		l = int(max(0, min(7, floor(sum(v.co.z for v in vs) / len(vs) / scale / 2))))
		lf = sum(map(lambda x: 1 << x, range(l, 8)))
		stats['levels'] = max(stats['levels'], l+1)
	flags[0] ^= 256 * lf

	# special properties
	if 'actorclip' in ps:
		flags = [65536, 0, 0] # ignore the rest of the flags intentionally
		del ps['actorclip']
	if 'trans33' in ps:
		flags[1] ^= 16
		del ps['trans33']
	if 'trans66' in ps:
		flags[1] ^= 32
		del ps['trans66']

	# write all the brushes needed to render this mesh
	for i in range(len(ms)):
		file.write('// Brush %d (%s %d)\n{\n' % (stats['brushes'], mesh.name, i))
		for p in sorted(ps.iteritems()):
			file.write('"%s" "%s"\n' % p)
		for f in ms[i].faces:
		    writeFace(file, f, flags, coordinatesMultiplier, imagePaths, missingImage)
		file.write('}\n')
		stats['faces'] += len(ms[i].faces)
		stats['brushes'] += 1
	stats['meshes'] += 1


def writeLight(file, light, scale, energyMultiplier, stats):
	'''
	Write a light to the map file.
	Writes game logic properties as entity properties adding light, color and origin if not already defined.
	Light intensity is blenders energy multiplied by the multiplier.
	'''
	# add intensity, color and origin
	ps = dict()
	ps['classname'] = 'light'
	ps['light'] = '%f' % (light.data.energy * energyMultiplier)
	ps['_color'] = '%f %f %f' % tuple(light.data.col)
	ps['origin'] = '%d %d %d' % tuple(around(array(light.loc) * scale))
	ps.update(dict((p.name, p.data) for p in light.game_properties))

	# write the entity
	file.write('// %s\n{\n' % light.name)
	for p in sorted(ps.iteritems()):
		file.write('"%s" "%s"\n' % p)
	file.write('}\n')
	stats['lights'] += 1


def writeModel(file, model, scale, modelsFolder, stats):
	'''
	Writes model entity to the file. Path ("model" property) is required and should begin with models/.
	Adds angles, origin and spawnflags to the defined properties (if not already set).
	You can use "levels" property as with meshes, overriding spawnflags.
	'''
	# check and normalize the path, can be absolute or relative to modelsFolder
	try:
		p = model.getProperty('model')
	except RuntimeError:
		p = None
	if not p:
		warning('\t\tModel does not have path set ("model" property).')
		return
	p = p.data
	if isabs(p):
		p = realpath(p)
		if not p.startswith(modelsFolder):
			warning('\t\tModel is outside of models base: %s.' % p)
			return
	else:
		p = realpath(join(modelsFolder, p))
	if not exists(p):
		warning('\t\tModel doesn\'t exist. Bad path set (%s).' % p)
		return
	p = p[len(modelsFolder)+1:]

	# level -- spawnflags
	l = int(max(0, min(7, floor(model.LocZ / 2))))

	# define origin and angles (pitch, yaw, roll), update with user-defined game properties
	ps = dict()
	ps['origin'] = '%d %d %d' % tuple(around(array(model.loc) * scale))
	ps['angles'] = '%d %d %d' % tuple(around([a * 180.0 / pi for a in (model.RotX, model.RotZ - pi / 2, -model.RotY)]))
	ps['spawnflags'] = sum(map(lambda x: 1 << x, range(l, 8)))
	ps.update(dict((p.name, p.data) for p in model.game_properties))

	# update with the normalized path
	ps['model'] = p

	# override spawnflags with levels if present
	if 'levels' in ps:
		ps['spawnflags'] = sum(map(lambda x: 1 << x if ps['levels'].find(str(x+1)) != -1 else 0, range(8)))
		del ps['levels']

	# write it
	file.write('// %s\n{\n' % model.name)
	for p in sorted(ps.iteritems()):
		file.write('"%s" "%s"\n' % p)
	file.write('}\n')
	stats['levels'] = max(stats['levels'], l+1)
	stats['models'] += 1


def writeEntity(file, entity, scale, stats):
	'''
	Write generic entity to the map file.
	Writes game logic properties as entity properties, adds origin and angle.
	The classname must be already set as game property is required.
	'''
	# add angle and origin
	ps = dict()
	ps['origin'] = '%d %d %d' % tuple(around(array(entity.loc) * scale))
	ps['angle'] = '%d' % round(entity.RotZ * 180.0 / pi)
	ps.update(dict((p.name, p.data) for p in entity.game_properties))

	# write it out, including properties
	file.write('// %s\n{\n' % entity.name)
	for p in sorted(ps.iteritems()):
		file.write('"%s" "%s"\n' % p)
	file.write('}\n')

	# statistics have separate entries for different spawns
	if ps['classname'] == 'info_alien_start':
		stats['aliens'] += 1
	elif ps['classname'] == 'info_human_start':
		stats['humans'] += 1
	elif ps['classname'] == 'info_player_start':
		if not ps['team']:
			warning('Player spawn with no team set.')
		stats['teams'] = max(stats['teams'], ps['team'])
		stats['players'] += 1
	else:
		stats['entities'] += 1



def export(fileName):
	'''
	Exports meshes, lights and all that has classname game property as entities.
	'''
	# logging and printing configuration
	basicConfig(level=15, format='%(message)s', stream=stderr)
	set_printoptions(precision=2, suppress=True)


	# general options
	pathField = Draw.Create(normpath('../ufoai'))
	scaleField = Draw.Create(32.0)
	quadTolerationField = Draw.Create(0.01)
	coordinatesMultiplierField = Draw.Create(65536)

	# light options
	energyMultiplierField = Draw.Create(500)
	sunlightButton = Draw.Create(True)
	sunlightIntensityField = Draw.Create(160)		# day
	sunlightColorField = Draw.Create('1.0 0.8 0.8')	# white
	sunlightAnglesField = Draw.Create('30 210')		# ~noon
	ambientButton = Draw.Create(True)
	ambientColorField = Draw.Create('0.4 0.4 0.4')	# just as example

	# splitting options
	splitButton = Draw.Create(False)
	splitHeightField = Draw.Create(3.5)
	minFaceAreaField = Draw.Create(0.5) 	# 3.5 should be enough in most cases
	minFaceAngleField = Draw.Create(5) 		# 3.5 should be enough in most cases
	minEdgeLengthField = Draw.Create(1) 	# 0.8 usually is enough
	sceneCopyButton = Draw.Create(False)
	leaveDummiesButton = Draw.Create(False)
	faceWarningsButton = Draw.Create(False)

	# ***
	def globalLightButtonCallback(event, value):
		print event, value;

	# that's a popup
	fs = (
	('UFO Path:'),
	('', pathField, 0, 256, 'Path to the top ufo folder.'),
	(' '),
	('General:'),
	('Scale:', scaleField, 0.1, 100.0, 'Scale everything by this value.'),
	('Quad toleration:', quadTolerationField, 0.0, 1.0, 'Consider quad nonplanar if absolute cosine between an edge and normal is higher.'),
	('Coordinates Multiplier:', coordinatesMultiplierField, 1, 131072, 'Enlarge plane coordinates to mitigate rounding errors. Set 1 to disable.'),
	(' '),
	('Light:'),
	('Energy multiplier:', energyMultiplierField, 0, 10000, 'Light intensity for lamps, energy multiplier.'),
	('Sunlight', sunlightButton, 'Enable sunlight.'),
	('Sunlight intensity:', sunlightIntensityField, 0, 10000, 'Global, parallel light intensity.'),
	('Sunlight color:', sunlightColorField, 0, 100, 'Color of the parallel light. Three floats.'),
	('Sunlight angles:', sunlightAnglesField, 0, 100, 'Angles of the parallel light. In degrees.'),
	('Ambient', ambientButton, 'Enable ambient lighting.'),
	('Ambient color:', ambientColorField, 0, 100, 'Ambient lighting color. Three floats.'),
	('Concave splitting:'),
	('Enable', splitButton, 'Make a tetra-, pentahedron for every face, doesn\'t work too well.'),
	('Splitting Height:', splitHeightField, 0.01, 100.0, 'Pyramid height for split faces.'),
	('Min. Face Area:', minFaceAreaField, 0, 100.0, 'Skip faces with area (after scaling) lower than this value.'),
	('Min. Face Angle:', minFaceAngleField, 0, 60.0, 'Skip faces with angles lower than this value (degrees).'),
	('Min. Edge Length:', minEdgeLengthField, 0, 100.0, 'Skip faces with edges shorter than that (after scaling).'),
	(' '),
	(' '),
	('Debugging:'),
	('Copy Scene', sceneCopyButton, 'Operate on a deep scene copy.'),
	('Leave Dummy', leaveDummiesButton, 'Don\'t unlink the dummy object.'),
	('Face Warnings', faceWarningsButton, 'Warn about faces ignored during splitting.'),
	)

	if not Draw.PupBlock('UFO:AI map export', fs):
		return

	Window.WaitCursor(1)

	Window.EditMode(0) # *** reenter if enabled

	st = clock()

	info('Exporting: %s.' % fileName)

	# written things counters
	stats = { 'meshes' : 0, 'lights' : 0, 'models' : 0, 'entities' : 0, 'faces' : 0, 'brushes' : 0, 'aliens' : 0, 'humans' : 0, 'players' : 0, 'teams' : 0, 'levels' : 0 }

	# options
	#	opts = { 'scale' : scaleField.val, 'light' : lightField.val, ...

	# if requested, operate on a deep scene copy
	if sceneCopyButton.val:
		sceneToExport = Scene.GetCurrent()
		scene = sceneToExport.copy(2)
	else:
		scene = Scene.GetCurrent()

	# only textures inside base/textures folder will get exported, only models inside base, all paths will be relative to this base folders
	#*** there should be three 'missing' textures: not texture at all, outside base, split
	texturesFolder = realpath(join(pathField.val, normpath('base/textures')))
	if not exists(texturesFolder):
		error('The textures base folder (%s) does not exist. Incomplete ufo or a bad path.' % texturesFolder)
		return
	modelsFolder = realpath(join(pathField.val, normpath('base')))
	if not exists(modelsFolder):
		error('The models base folder (%s) does not exist. Incomplete ufo or a bad path.' % modelsFolder)
		return
	missingImage = normpath('tex_common/nodraw.tga')
	if not exists(realpath(join(texturesFolder, missingImage))):
		error('The missing texture (%s) is missing.' % missingImage)
		return
	missingImage = splitext(missingImage)[0]

	# preprocess image paths, find paths relative to base/textures, *** how to get images just from the current scene?
	info('Processing images.')
	imagePaths = {}
	for i in Image.Get():
		p = normpath(sys.expandpath(i.getFilename())).split(normpath('base/textures'), 1)
		if len(p) != 2:
			warning('\tAn image outside of the base/textures folder: %s.' % i.getFilename())
			p = missingImage
		else:
			p = p[1]
			if p.startswith(sep):
				p = p[len(sep):]
			p = realpath(join(texturesFolder, p))
			if not exists(p):
				warning('\tAn image doesn\'t exist: %s.' % p)
				p = missingImage
			else:
				p = splitext(p[len(texturesFolder)+1:])[0]
		imagePaths[i] = p

	# warnings about skipped faces are to verbose sometimes
	if not faceWarningsButton.val:
		class FaceWarningsFilter(Filter):
			def filter(self, record):
				return not record.getMessage().startswith('\t\tSkipping a face')
		getLogger().addFilter(FaceWarningsFilter())

	# classify and scale (look for the classname first, then decide by object type)
	info('Classifying.')
	meshes = []
	lights = []
	models = []
	entities = []
	for o in scene.objects:
		if o.name.startswith('Dummy Exporter'):
			warning('\tA probable exception leftover, ignoring: %s.' % o.name)
			continue
		try:
			cn = o.getProperty('classname')
		except RuntimeError: # hail Blender ...
			cn = None
		if cn:
			if cn.data == 'misc_model':
				models.append(o)
			else:
				entities.append(o)
		else:
			if o.type == 'Mesh':
				meshes.append(o)
			elif o.type == 'Lamp':
				lights.append(o)
			else:
				warning('\tIgnoring an object: %s.' % o.name)

	# write to memory, flush later
	world = StringIO()
	rest = StringIO()

	# export meshes
	if meshes:
		info('Exporting meshes:')
		scene.objects.link(do) # dummy object needed for triangulating
		for m in meshes:
			info('\t%s' % m.name)
			writeMesh(world, m, scaleField.val, quadTolerationField.val, splitButton.val, splitHeightField.val, minFaceAreaField.val, minFaceAngleField.val, minEdgeLengthField.val, coordinatesMultiplierField.val, imagePaths, missingImage, stats)
		if not leaveDummiesButton.val:
			scene.objects.unlink(do)

	# export lights
	if lights:
		info('Exporting lights:')
		for l in lights:
			info('\t%s' % l.name)
			writeLight(rest, l, scaleField.val, energyMultiplierField.val, stats)

	# export models
	if models:
		info('Exporting models:')
		for m in models:
			info('\t%s' % m.name)
			writeModel(rest, m, scaleField.val, modelsFolder, stats)

	# export other entities (spawns, models)
	if entities:
		info('Exporting entities:')
		for e in entities:
		    info('\t%s' % e.name)
		    writeEntity(rest, e, scaleField.val, stats)

	# get rid of the scene copy
	if sceneCopyButton.val:
		sceneToExport.makeCurrent()
		Scene.Unlink(scene)

	# check some general things
	if stats['aliens'] == 0:
		warning('Map does not have any alien spawns (classname info_alien_start).')

	if stats['humans'] == 0:
		warning('Map does not have any human spawns (classname info_human_start).')

	if stats['teams'] == 0:
		warning('Map does not have any multiplier spawns (classname info_player_start).')
	# *** check some more, any lights?

	# write to disk
	file = open(fileName, 'wb')
	file.write('// Exported on: %s.\n' % datetime.now())
	file.write('{\n')
	file.write('"classname" "worldspawn"\n')
	if sunlightButton.val:
		file.write('"light" "%d"\n' % sunlightIntensityField.val) # *** check light values
		file.write('"_color" "%s"\n' % sunlightColorField.val)
		file.write('"angles" "%s"\n' % sunlightAnglesField.val)
	if ambientButton.val:
		file.write('"ambient" "%s"\n' % ambientColorField.val)
	file.write('"maxlevel" "%d"\n' % (stats['levels'] + 1)) # need to add one in case of actor getting to the map top
	file.write('"maxteams" "%d"\n' % stats['teams'])
	file.write(world.getvalue())
	file.write('}\n')
	file.write(rest.getvalue())
	file.close()

	# bah
	s = lambda n: [n, '' if n == 1 else 's']
	es = lambda n: [n, '' if n == 1 else 'es']
	ies = lambda n: [n, 'y' if n == 1 else 'ies']
	info(str(stats))
	info('%d mesh%s, %d light%s, %d model%s, %d other entit%s, done in %.2f second%s.' % tuple(es(stats['meshes']) + s(stats['lights']) + s(stats['models']) + ies(stats['entities']) + s(clock() - st)))

	Window.WaitCursor(0)



def main():
	# *** some default reasonable for everyone
	Window.FileSelector(export, 'UFO:AI map export', realpath(normpath('../ufoai/base/maps/blenderd.map')))
	'''
	# profiling, open stats with pprofui for example
	from profile import run
	from os.path import expanduser
	run("from os.path import *; from ufoai_export import export; export(realpath(normpath('../ufoai/base/maps/blender.map')))", expanduser('~/stats'))
	'''

if __name__ == '__main__': main()
