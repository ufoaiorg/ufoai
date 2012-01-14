#! /usr/bin/env python

#
# @brief extract code structure about behaviour from .h and .c files
# @license Public domain
#

import os, os.path, sys, re

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
		if self.extends == None:
			return None
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
		raise Exception("Behaviour '" + str(name) + "' not found.")

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

	def formatPropertyComment(self, codeComment):
		comments = []
		for line in codeComment.splitlines():
			line = line.strip()

			if line.startswith("/*"):
				line = line.replace('/**', '').replace('/*', '').replace('*/', '')
				line = line.strip()
				comments.append(line)
				continue

			if line.startswith("*/"):
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
		return comments

	def extractProperties2(self, code):
		result = []
		matchProperties = "(/\*.*?\*/)?\s*(?:\w*?\s*=)?\s*(UI_Register\w*?)\((.*?)\)\s*;"
		for data in re.findall(matchProperties, code, re.DOTALL):
			comment, function, param = data

			# clean up cause it can match more than one comment
			comment = comment.split("/*")
			comment = "/*" + comment[len(comment)-1]

			doc = self.formatPropertyComment(comment)
			params = param.split(",")
			name = params[1].strip()
			if name[0] == '"':
				name = name[1:len(name)-1]
			if name == 'NULL':
				continue
			if function == "UI_RegisterOveridedNodeProperty":
				element = Element(name, "...")
				element.addDoc(doc)
				element.override = True
				result.append(element)
				continue

			type = ""
			if function == "UI_RegisterNodeMethod":
				type = "V_UI_NODEMETHOD"
			else:
				type = params[2].strip()

			element = Element(name, type)
			element.addDoc(doc)
			result.append(element)

		return result

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

	def extractDataFromRegistrationFunction(self, code, doc):

		props = self.extractProperties2(code)

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
		for prop in props:
			node.addProperty(prop)
		if doc != None:
			node.doc = doc

		self.package.addBehaviour(node)

	def extractData(self):
		registrationFunctions = []
		# all nodes
		for fileName in os.listdir(dir):
			if ".c" not in fileName:
				continue

			file = open(dir + '/' + fileName, "rt")
			data = file.read()
			file.close()

			rf = self.extractRegistrationFunctions(data)
			doc = None
			if len(rf) == 1:
				doc = self.extractHeaderComments(data)

			for code in rf:
				registrationFunctions.append((code, doc))

		for data in registrationFunctions:
			code, doc = data
			self.extractDataFromRegistrationFunction(code, doc)

if __name__ == "__main__":
	extract = ExtractNodeBehaviour()
	package = extract.getPackage()
