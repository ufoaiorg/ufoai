#!/usr/bin/python

#
# @brief extract code structure about behaviour from .h and .c files
# @license Public domain
#

import os, os.path, sys

# path where exists ufo binary
UFOAI_ROOT = os.path.realpath(os.path.dirname(__file__) + '/../../..')

dir = UFOAI_ROOT + '/src/client/ui/node'

class Element:
	def __init__(self, name, type):
		self.doc = []
		self.name = name
		self.type = type
		self.override = False
		self.node = None
		pass

	def addDoc(self, comments):
		self.doc = comments

	def getSuperproperty(self):
		if not self.override:
			return None
		behaviour = self.node.getSuperclass()
		while behaviour != None:
			prop = behaviour.getProperty(self.name)
			if prop != None:
				return prop
			behaviour = behaviour.getSuperclass()

class NodeBehaviour:
	def __init__(self):
		self.name = ""
		self.extends = "abstractnode"
		self.properties = {}
		self.callbacks = {}
		self.methods = {}
		self.confuncs = {}
		self.doc = []
		self.package = None
		pass

	def init(self, dic):
		self.name = dic['name']
		if (self.name == "abstractnode"):
			self.extends = None
		elif 'extends' in dic:
			self.extends = dic['extends']
		if self.name == "":
			self.name = "null"

	def getSuperclass(self):
		return self.package.getBehaviour(self.extends)

	def getProperty(self, propertyName):
		if propertyName in self.methods:
			return self.methods[propertyName]
		if propertyName in self.callbacks:
			return self.callbacks[propertyName]
		if propertyName in self.properties:
			return self.properties[propertyName]
		return None

	def addProperty(self, element):
		if element.type == "V_UI_NODEMETHOD":
			self.methods[element.name] = element
		elif element.type == "V_UI_ACTION":
			self.callbacks[element.name] = element
		else:
			self.properties[element.name] = element
		element.node = self

	def initProperties(self, dic):
		self.properties = dic

	def initMethods(self, dic):
		self.methods = dic

class NodePackage:
	def __init__(self):
		self.behaviours = {}

	def getBehaviourNames(self):
		return self.behaviours.keys()

	def getBehaviours(self):
		return self.behaviours

	def getBehaviour(self, name):
		if name in self.behaviours:
			return self.behaviours[name]
		raise "Behaviour '" + name + "' not found."

	def addBehaviour(self, behaviour):
		self.behaviours[behaviour.name] = behaviour
		behaviour.package = self

# @brief Extract node behaviour from source code
class ExtractNodeBehaviour:

	def __init__(self):
		self.package = NodePackage()
		self.extractData()

	def getPackage(self):
		return self.package

	# @brief Extract each properties into a text according to the structure name
	def extractProperties(self, node, filedata, structName):
		signature = "const value_t %s[]" % structName
		properties = filedata.split(signature)
		properties = properties[1]

		i = properties.find('{')
		j = properties.find('};')
		properties = properties[i+1:j]

		result = {}
		comments = []
		override = False
		propertyName = ""

		for line in properties.splitlines():
			line = line.strip()

			if line.startswith("/*"):
				line = line.replace('/**', '').replace('/*', '').replace('*/', '')
				line = line.strip()
				if line.startswith("@override"):
					propertyName = line.replace('@override', '').strip()
					override = True
				else:
					comments.append(line)
				continue

			if line.startswith("*/"):
				if override:
					element = Element(propertyName, "...")
					element.override = True
					element.addDoc(comments)
					node.addProperty(element)
					comments = []
					override = False
				continue

			if line.startswith("*"):
				line = line.replace('*', '', 1).replace('*/', '')
				line = line.strip()
				if line.startswith("@"):
					comments.append(line)
				else:
					if len(comments) == 0:
						comments.append(line)
					else:
						comments[len(comments)-1] += ' ' + line
				continue

			i = line.find('{')
			if i == -1:
				continue

			line = line[i+1:]
			e = line.split(',')

			name = e[0].strip()
			if name[0] == '"':
				name = name[1:len(name)-1]

			if name == 'NULL':
				continue

			type = e[1].strip()

			element = Element(name, type)
			element.addDoc(comments)
			node.addProperty(element)
			comments = []
			override = False

	# @brief Extract each body of registration function into a text
	def extractRegistrationFunctions(self, filedata):
		result = []

		register = filedata.split("\nvoid UI_Register")
		register.pop(0)

		for body in register:
			body = body.split('{')[1]
			body = body.split('}')[0]
			result.append(body)

		return result

	# @brief Extract doc header
	# @todo Regex to extract the first comment
	def extractHeaderComments(self, filedata):
		comments = []
		codeBlock = False

		for line in filedata.splitlines():
			line = line.strip()

			if line.startswith("/*"):
				line = line.replace('/**', '').replace('/*', '').replace('*/', '')
				line = line.strip()
				if line != '':
					comments.append(line)
				continue

			if line.startswith("*/"):
				break

			if line.startswith("*"):
				line = line.replace('*', '', 1)
				if codeBlock:
					command = line.strip()
					if command == "@endcode":
						codeBlock = False
					else:
						comments[len(comments)-1] += '\n' + line
					continue

				line = line.strip()
				if line.startswith("@"):
					if line == "@code":
						codeBlock = True
						comments.append(line)
					else:
						comments.append(line)
				else:
					if len(comments) == 0:
						comments.append(line)
					else:
						comments[len(comments)-1] += ' ' + line
				continue

		return comments

	def extractData(self):
		# all nodes
		for f in os.listdir(dir):
			if ".c" not in f:
				continue

			file = open(dir + '/' + f, "rt")
			data = file.read()
			file.close()

			registrationFunctions = self.extractRegistrationFunctions(data)
			doc = None
			if len(registrationFunctions) == 1:
				doc = self.extractHeaderComments(data)

			for code in registrationFunctions:
				lines = code.splitlines()
				dic = {}
				for l in lines:
					l = l.replace("behaviour->", "")
					l = l.replace(";", "")
					e = l.split('=')
					if len(e) != 2:
						continue
					v = e[1].strip()
					if v[0] == '"':
						v = v[1:len(v)-1]
					dic[e[0].strip()] = v

				node = NodeBehaviour()
				node.init(dic)
				if 'properties' in dic:
					props = self.extractProperties(node, data, dic['properties'])
				if doc != None:
					node.doc = doc

				self.package.addBehaviour(node)
