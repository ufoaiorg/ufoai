#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

# Copyright (C) 2011 - UFO: Alien Invasion
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.

#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.

#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

import dircache
import os.path
import Image
import sys

# TODO check all icons from a set must have same size
# TODO merge similar sprites

# root of the game
UFOAI_ROOT = os.path.realpath(sys.path[0] + '/../..')

# where are single sprites
SPRITE_SRC = [
	UFOAI_ROOT + "/base/pics/icons",
	UFOAI_ROOT + "/base/pics/buttons"]

# margin between sprites
margin = 1
# format of the packed image
packFormat = 'PNG'

verbose = False

suffixes = set(["_clicked", "_hovered", "_disabled"])

#####################################################

class Sprite:
	def __init__(self, url):
		self.image = Image.open(url)
		self.filename = url
		self.clickedIcon = None
		self.hoveredIcon = None
		self.disabledIcon = None
		self.pack = None
		self.pos = None

		base = os.path.splitext(url)[0]
		extension = os.path.splitext(url)[1]

		url = base + "_clicked" + extension
		if os.path.exists(url):
			self.clickedIcon = Sprite(url)
		url = base + "_hovered" + extension
		if os.path.exists(url):
			self.hoveredIcon = Sprite(url)
		url = base + "_disabled" + extension
		if os.path.exists(url):
			self.disabledIcon = Sprite(url)

		self.name = base.split('/pics/')[-1]

	def getIconSet(self):
		icons = set([])
		icons.add(self)
		if self.clickedIcon:
			icons.add(self.clickedIcon)
		if self.hoveredIcon:
			icons.add(self.hoveredIcon)
		if self.disabledIcon:
			icons.add(self.disabledIcon)
		return icons

	def size(self):
		return self.image.size
	def width(self):
		return self.image.size[0]
	def height(self):
		return self.image.size[1]

class Pack:
	def __init__(self, name, size, margin = 1):
		transparent = (0, 0, 0, 0)
		self.image = Image.new("RGBA", [size, size], transparent)
		self.width = size
		self.height = size
		self.availableHeight = size - margin - margin
		self.availableWidth = size - margin - margin
		self.margin = margin
		self.rowHeight = 0
		self.name = name

	# return available position for a new icon
	def getAvailableIconPosition(self, icon):
		pos = None
		newRow = False

		# append to a row
		if self.availableWidth != None:
			if icon.width() <= self.availableWidth:
				if icon.height() > self.availableHeight:
					return None
				pos = self.width - self.availableWidth - self.margin, self.height - self.availableHeight - self.margin
			else:
				newRow = True

		# start a new row
		if newRow:
			if icon.height() > self.availableHeight - self.rowHeight:
				return None
			if icon.width() > self.width - self.margin - self.margin:
				return None
			pos = self.margin, self.height - self.availableHeight + self.rowHeight - self.margin

		return pos

	def _updatePackCacheToAddIcon(self, pos, icon):
		# do we start a new row
		if pos[0] == self.margin:
			self.availableWidth = self.width - self.margin - self.margin
			if self.rowHeight != 0:
				self.availableHeight -= self.rowHeight + self.margin
			self.rowHeight = 0
		if icon.height() > self.rowHeight:
			self.rowHeight = icon.height()
		self.availableWidth -= icon.width() + self.margin

	def _addIcon(self, icon, test):
		availableWidth = self.availableWidth
		availableHeight = self.availableHeight
		rowHeight = self.rowHeight

		icons = icon.getIconSet()

		added = False
		for i in icons:
			pos = self.getAvailableIconPosition(i)
			added = pos != None
			if added:
				self._updatePackCacheToAddIcon(pos, i)
				if not test:
					self.image.paste(i.image, pos)
					i.pos = pos
					i.pack = self
			else:
				break

		if test:
			self.availableWidth = availableWidth
			self.availableHeight = availableHeight
			self.rowHeight = rowHeight

		return added

	# test to add an icon, return True if the icon is added
	# the function do not change the internal state of the object
	def testAddIcon(self, icon):
		return self._addIcon(icon, True)

	# add an icon, return True if the icon is added
	def addIcon(self, icon):
		test = self.testAddIcon(icon)
		if not test:
			return
		return self._addIcon(icon, False)

def loadIcons(srcDir):
	global verbose
    print 'Load sprites from \"%s\"' % srcDir
	result = []
	list = dircache.listdir(srcDir)
	iconCount = 0
	iconsetCount = 0
	for filename in list:
		# filter extension
		extension = os.path.splitext(filename)[1]
		extension = extension.lower()
		if not (extension in '.png|.jpg|.jpeg|.tga|.gif'):
			print >> sys.stderr, "Warning: Skip \"%s\"" % filename
			continue

		# avoid pack files
		if "pack" in filename:
			print >> sys.stderr, "Warning: Skip \"%s\"" % filename
			continue

		# filter special suffixes
		name = os.path.splitext(filename)[0]
		special = False
		for suffix in suffixes:
			if name.endswith(suffix):
				special = True
				break
		if special:
			continue

		icon = Sprite(srcDir + '/' + filename)
		if verbose:
			print filename, icon.size(), "x" + str(len(icon.getIconSet()))
		iconCount += len(icon.getIconSet())
		iconsetCount += 1
		result.append(icon)
	print "    Icons count: ", iconCount
	print "    Iconsets count: ", iconsetCount
	return result

def iconsByHeight(x, y):
        return x.height() - y.height()

def iconsByName(x, y):
        return cmp(x.name, y.name)

def generateIconPacks(iconList, packSize):
	global verbose
	list = iconList[:]
	list.sort(cmp=iconsByHeight)
	packs = []

	while len(list) > 0:
		icon = list.pop()
		added = False
		for p in packs:
			added = p.addIcon(icon)
			if verbose:
				print icon.filename + " -> " + p.name, added
			if added:
				break
		if not added:
			p = Pack("pack_" + str(len(packs)), packSize)
			packs.append(p)
			added = p.addIcon(icon)
			if not added:
				raise Exception("No way to add this icon to a new pack")
	print "Iconpacks count: ", len(packs)
	return packs

def savePacks(packs, destDir, format):
	if not os.path.exists(destDir):
		os.makedirs(destDir)
	for p in packs:
		name = p.name + "." + format.lower()
		p.image.save(destDir + "/" + name, format)

def generateIconScript(icons, destDir):
	if not os.path.exists(destDir):
		os.makedirs(destDir)

	script = open(destDir + '/sprite_generated.ufo', "wt")
	script.write("// It is an autogenerated file from ./src/icons/gen_icon_pack.py\n")
	script.write("// You should not edit it, but:\n")
	script.write("// * edit icons from ./src/icons and autogenerate packs\n")
	script.write("// * or edit icons from ./base/pics/icons/ and script ./base/ufos/icons.ufo\n")
	script.write("\n")

	icons = icons[:]
	icons.sort(cmp=iconsByName)
	for i in icons:
		script.write("sprite " + i.name + " {\n")
		script.write("\timage \"banks/" + i.pack.name + "\"\n")
		script.write("\tsize \"" + str(i.width()) + " " + str(i.height()) + "\"\n")
		script.write("\ttexl \"" + str(i.pos[0]) + " " + str(i.pos[1]) + "\"\n")
		if i.hoveredIcon:
			script.write("\thoveredtexl \"" + str(i.hoveredIcon.pos[0]) + " " + str(i.hoveredIcon.pos[1]) + "\"\n")
		if i.disabledIcon:
			script.write("\tdisabledtexl \"" + str(i.disabledIcon.pos[0]) + " " + str(i.disabledIcon.pos[1]) + "\"\n")
		if i.clickedIcon:
			script.write("\tclickedtexl \"" + str(i.clickedIcon.pos[0]) + " " + str(i.clickedIcon.pos[1]) + "\"\n")

		script.write("}\n")
	script.close()

#####################################################

def gen_sprite_pack(base_destination, pack_size):
	sprites = []
	for src in SPRITE_SRC:
		sprites += loadIcons(src)
	packs = generateIconPacks(sprites, pack_size)
    pack_dest = os.path.join(base_destination, "pics", "banks")
	savePacks(packs, pack_dest, packFormat)
    script_dest = os.path.join(base_destination, "ufos")
	generateIconScript(sprites, script_dest)

if __name__ == '__main__':
    base_dest = os.path.join(UFOAI_ROOT, "base_tmp")
    gen_sprite_pack(base_dest, 512)
