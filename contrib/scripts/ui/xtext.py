#!/usr/bin/env python
#
# @brief Generate an xtext fragment from source code of the UI engine
# @license Public domain
# @return an xtext fragment to stdout
#

import os, os.path, sys
from ExtractNodeBehaviour import *

def genBehaviourXText(node):
	result = ""
	label = ""

	# node properties
	result += 'UI%sNodeProperties:\n' % node.name.title()
	result += '\t('
	if node.extends != None:
		result += 'UI%sNodeProperties | ' % node.extends.title()
	result += 'UI%sNodePropertiesBase)\n' % node.name.title()
	result += '\n'

	# base properties
	result += 'UI%sNodePropertiesBase:\n' % node.name.title()
	list = node.properties.keys()
	if len(list) != 0:
		list.sort()
		next = ""
		for name in list:
			e = node.properties[name]
			result += "\t(\"%s\" %s=UIValue)?\n" % (e.name, e.name)
			next = "& "

	list = node.callbacks.keys()
	if len(list) != 0:
		list.sort()
		for name in list:
			e = node.callbacks[name]
			result += "\t(\"%s\" %s=UIFuncStatements)?\n" % (e.name, e.name)
	result += '\t;\n'

	return result

def genXText(package):
	result = ""
	list = package.getBehaviourNames()
	list.sort()
	result += """// autogen xtext from source code\n"""
	result += "\n"

	result += "//================ NODES ================\n\n"

	result += 'UINode:\n'
	next = ""
	for name in list:
		result += "\t%sUI%sNode\n" % (next, name.title())
		next = "| "
	result += "\t;\n"
	result += "\n"

	for name in list:
		result += 'UINode%s:\n' % name.title()
		result += '\t"%s" name=ID "{" (properties+=UI%sProperties)* "}";\n' % (name, name.title())
		result += "\n"

	result += "//================ PROPERTIES ================\n\n"

	for name in list:
		result += genBehaviourXText(package.getBehaviour(name))
		result += "\n"

	result += 'UINodeProperties:\n'
	next = ""
	for name in list:
		result += "\t%sUI%sNodePropertiesBase\n" % (next, name.title())
		next = "| "
	result += "\t;"


	return result

def getXText():
	extract = ExtractNodeBehaviour()
	package = extract.getPackage()
	return genXText(package)

if __name__ == "__main__":
	print getXText()
