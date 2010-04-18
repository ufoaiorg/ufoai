#!/usr/bin/python

#
# @brief functions to check and update menu_*.ufo files
# @license Public domain
# @note tested with Python 2.4.1
#

import os, sys, re
import os.path

# path where exists ufo binary
UFOAI_ROOT = os.path.realpath(sys.path[0] + '/../../..')

removeComment = re.compile('(?:\s|^)(//.*$)', re.MULTILINE)
removeEvents = re.compile('(\s)(click|change|rclick|mclick|wheel|in|out|whup|whdown)\s*\{.*?\}', re.DOTALL)
removeFunctions = re.compile('(\s)(confunc|func)(\s+)([a-zA-Z0-9_-]*)(\s*)\{.*?\}', re.DOTALL)
removeOptions = re.compile('(\s)(option)(\s+)([a-zA-Z0-9_-]*)(\s*)\{.*?\}', re.DOTALL)

mouseevent = ["click", "viewchange", "rclick", "mclick", "wheel", "in", "out", "whup", "whdown"]

parseNodes = re.compile('\s*([a-zA-Z0-9_-]+)\s+([a-zA-Z0-9_-]+)\s*\{(.*?)\}', re.DOTALL)
matchParam = re.compile("(\".*?\"|(?!\")\S+)\s+", re.DOTALL)

renameMenuParam = '(\smenu\s+%s\s*\{\s*\{.*?\s)(%s)(\s)'
updateMenuParam = '(\smenu\s+%s\s*\{\s*\{.*?\s%s\s*)([^\"\s]\S*|\".*?\")(\s)'
renameNodeParam = '(\smenu\s+%s\s*\{.*?\s%s\s+%s\s*\{.*?\s)(%s)(\s)'
updateNodeParam = '(\smenu\s+%s\s*\{.*?\s%s\s+%s\s*\{.*?%s\s+)([^\"\s]\S*|\".*?\")(\s)'


class Root:

	def __init__(self):
		self.nodes = NodeObject("", "")
		self.files = {}
		self.fileBases = {}

	def parseUFOParam(self, object, data):
		result = matchParam.findall(data.strip() + " ")
		while len(result) != 0:
			name = result.pop(0)
			value = None
			if name in mouseevent:
				if len(result) > 0:
					value = result.pop(0)
					# is event alone
					if value != '...':
						result.insert(0, value)

			else:
				if len(result) > 0:
					value = result.pop(0)
				else:
					print 'no value for the param \"' + name + '\" (' + object.name + ')'
			object.addParam(name, value)

	def parseUFOMenu(self, data, filename):
		data = removeComment.sub('', data)
		data = removeEvents.sub('\\1\\2 ...', data)
		data = removeFunctions.sub('\\1\\2\\3\\4\\5{}', data)
		data = removeOptions.sub('\\1\\2\\3\\4\\5', data)
		data = '\n\n' + data

		menus = data.split('\nmenu')
		menus.pop(0)
		for m in menus:

			m = m.split('{', 1)
			name = m[0].strip()
			menu = NodeObject("menu", name)
			menu.filename = filename
			menu.root = self
			m = m[1].strip()
			m = m[0:len(m) - 1]

			# empty menu
			if len(m) == 0:
				return

			# menu param
			if m[0] == '{':
				m = m.split('}', 1)
				params = m[0][1:]
				m = m[1].strip()
				self.parseUFOParam(menu, params)

			# nodes
			nodes = parseNodes.findall(m)
			for node in nodes:
				behaviour, name, param = node
				node = NodeObject(behaviour, name)
				node.filename = filename
				node.root = self
				node.parent = menu
				menu.addChild(node)
				self.parseUFOParam(node, param)

			self.nodes.addChild(menu)

	def loadFile(self, filename):
		print 'load ' + filename
		file = open(UFOAI_ROOT + '/' + filename, "rt")
		data = file.read()
		file.close()

		self.files[filename] = data
		self.fileBases[filename] = data

		self.parseUFOMenu(data, filename)

	def loadAll(self):
		for f in os.listdir(UFOAI_ROOT + '/base/ufos'):
			if "menu_" not in f:
				continue
			self.loadFile('/base/ufos/' + f)

	def save(self):
		for f, data in self.files.iteritems():
			if data == self.fileBases[f]:
				continue

			print 'save ' + f
			file = open(UFOAI_ROOT + '/' + f, "wt")
			file.write(data)
			file.close()

	def renameMenuParam(self, node, param, newname):
		menu = node.name
		filename = node.filename

		data = self.files[filename]
		match = re.compile(renameMenuParam % (menu, param), re.DOTALL)
		data = match.sub('\\1' + newname + '\\3', data, 1)
		self.files[filename] = data

	def updateMenuParam(self, node, param, value):
		menu = node.name
		filename = node.filename

		data = self.files[filename]
		match = re.compile(updateMenuParam % (menu, param), re.DOTALL)
		data = match.sub('\\1' + value + '\\3', data, 1)
		self.files[filename] = data

	def updateNodeParam(self, node, param, value):
		menu = node.parent.name
		filename = node.filename

		data = self.files[filename]
		match = re.compile(updateNodeParam % (menu, node.behaviour, node.name, param), re.DOTALL)
		data = match.sub('\\1' + value + '\\3', data, 1)
		self.files[filename] = data

	def renameNodeParam(self, node, param, newname):
		menu = node.parent.name
		filename = node.filename

		data = self.files[filename]
		match = re.compile(renameNodeParam % (menu, node.behaviour, node.name, param), re.DOTALL)
		data = match.sub('\\1' + newname + '\\3', data, 1)
		self.files[filename] = data


class NodeObject:
	def __init__(self, behaviour, name):
		self.behaviour = behaviour
		self.name = name
		self.param = {}
		self.child = []
		self.childname = {}
		self.mouseevent = None

	def addChild(self, node):
		self.child.append(node)
		self.childname[node.name] = node

	def addParam(self, name, value):
		if name == 'pos':
			v = value[1:len(value)-1].split(" ")
			self.pos = (int(v[0]), int(v[len(v)-1]))
		elif name == 'size':
			v = value[1:len(value)-1].split(" ")
			self.size = (int(v[0]), int(v[len(v)-1]))

		self.param[name] = value

	def __print__(self, level = 0):
		tab = '\t' * level
		print tab + self.behaviour + " " + self.name
		for name, value in self.param.iteritems():
			print tab + '\t' + name + ' ' + str(value)
		for child in self.child:
			child.__print__(level + 1)

	def existsParam(self, param):
		return param in self.param

	def getParam(self, param):
		if param in self.param:
			return self.param[param]
		return None

	def renameParam(self, old, new):
		if self.behaviour == "menu":
			self.root.renameMenuParam(self, old, new)
		else:
			self.root.renameNodeParam(self, old, new)

	def updateParam(self, old, new):
		if self.behaviour == "menu":
			self.root.updateMenuParam(self, old, new)
		else:
			self.root.updateNodeParam(self, old, new)

	def useMouseEvent(self):
		if self.mouseevent == None:
			self.mouseevent = False
			for e in mouseevent:
				if e in self.param:
					self.mouseevent = True
					break
		return self.mouseevent
