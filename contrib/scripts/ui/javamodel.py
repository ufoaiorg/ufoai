#!/usr/bin/env python
#
# @brief extract java parsers about node behaviour from .h and .c files
# @license Public domain
# @return create files is a sub directory
#

import os, os.path, sys
from ExtractNodeBehaviour import *

def getTemplate(token):
	fileName = sys.path[0] + "/javamodel." + token + ".template"
	if not os.path.exists(fileName):
		fileName = sys.path[0] + "/javamodel.default.template"
	file = open(fileName, "rt")
	data = file.read()
	file.close()
	return data

## TODO rework that code with a list of word
def classNameTypo(name):
	result = ""
	if name.startswith("abstract"):
		result += "Abstract"
		name = name.replace("abstract", "", 1)
	if name.startswith("base"):
		result += "Base"
		name = name.replace("base", "", 1)
	if name.startswith("custom"):
		result += "Custom"
		name = name.replace("custom", "", 1)
	if name.startswith("text"):
		result += "Text"
		name = name.replace("text", "", 1)
	if name.startswith("material"):
		result += "Material"
		name = name.replace("material", "", 1)
	if name.startswith("tbar"):
		result += "TBar"
		name = name.replace("tbar", "", 1)
	if name.startswith("vscroll"):
		result += "VScroll"
		name = name.replace("vscroll", "", 1)
	if name.startswith("line"):
		result += "Line"
		name = name.replace("line", "", 1)
	if name.startswith("key"):
		result += "Key"
		name = name.replace("key", "", 1)
	if name.startswith("_"):
		result += ""
		name = name.replace("_", "", 1)
	if name.startswith("select"):
		result += "Select"
		name = name.replace("select", "", 1)
	if name.startswith("option"):
		result += "Option"
		name = name.replace("option", "", 1)
	if name.startswith("radio"):
		result += "Radio"
		name = name.replace("radio", "", 1)
	if name != "":
		result += name[0].upper() + name[1:]
	return result

## TODO finish it
def genEventPropertiesParserRegistration(package):
	events = set([])
	list = package.getBehaviourNames()
	list.sort()
	for name in list:
		behaviour = package.getBehaviour(name)
		list = behaviour.callbacks.keys()
		if len(list) != 0:
			list.sort()
			for name in list:
				e = behaviour.callbacks[name]
				if eventproperties != "":
					eventproperties += ',\n\t\t\t'
				events.add(e.name)
				pass

def getOutputPath():
	path = sys.path[0] + "/javamodel"
	if not os.path.exists(path):
		os.mkdir(path)
	return path

def genNodeParserRegistration(package):
	print "genNodeParserRegistration..."
	data = ""
	list = package.getBehaviourNames()
	list.sort()
	for name in list:
		behaviour = package.getBehaviour(name)
		javaID = classNameTypo(behaviour.name)
		token = behaviour.name
		if token == "null":
			continue
		if token == "window":
			continue
		data += "registerSubParser(" + javaID + "SubParser.FACTORY, factory);\n"

	file = open(getOutputPath() + "/nodeRegistration.txt", "wt")
	file.write(data)
	file.close()

def genBehaviourParser(node):
	global eventpropertiesparsers, nodebehaviourparsers

	javaID = classNameTypo(node.name)
	extendsJavaID = "UFONode"
	if node.extends != None:
		extendsJavaID = classNameTypo(node.extends)

	token = node.name
	print javaID + " \"" + token + "\"..."
	if token == "null":
		return
	if token == "window":
		return

	template = getTemplate(token)

	registration = False
	eventProperties = ""
	eventPropertiesRegistration = ""
	list = node.callbacks.keys()
	if len(list) != 0:
		list.sort()
		for name in list:
			e = node.callbacks[name]
			factory = "EventPropertySubParser." + e.name.upper()
			if eventProperties != "":
				eventProperties += ',\n\t\t\t'
			eventProperties += factory
			if eventPropertiesRegistration != "":
				eventPropertiesRegistration += '\n\t\t'
			eventPropertiesRegistration += "registerSubParser(" + factory + ", factory);"
			registration = True
			pass

	getFactory = "IUFOSubParserFactory factory = getNodeSubParserFactory();"

	hideRegistration = '// '
	if registration:
		hideRegistration = ''

	template = template.replace("###HIDEREGISTRATION###", hideRegistration)
	template = template.replace("###JAVAID###", javaID)
	template = template.replace("###EXTENDSJAVAID###", extendsJavaID)
	template = template.replace("###TOKEN###", token)
	template = template.replace("###EVENTPROPERTIES###", eventProperties)
	template = template.replace("###EVENTPROPERTIESREGISTRATION###", eventPropertiesRegistration)

	className = javaID + "SubParser"
	file = open(getOutputPath() + "/" + className + ".java", "wt")
	file.write(template)
	file.close()

def genParsers(package):
	list = package.getBehaviourNames()
	list.sort()
	for name in list:
		genBehaviourParser(package.getBehaviour(name))

extract = ExtractNodeBehaviour()
package = extract.getPackage()
genParsers(package)
genNodeParserRegistration(package)
print "Done"
